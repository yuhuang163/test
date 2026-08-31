#include "qfreework.h"

#include "test_case.h"
#include "screen_inspect_analyzer.h"

#include "qatmanager.h"
#include "qfreeworkbox.h"
#include "fixture_uart.h"
#include "pcba_uart_codec.h"
#include "asd9026a_codec.h"
#include "asd9026a_device.h"
#include "xwd_raw_fixture_device.h"
#include "xwd_raw_uart_codec.h"
#include "jieli_bt_box_device.h"
#include "qprotocol_types.h"
#include "shared_instrument.h"
#include "serial_channel.h"
#include "visa_channel.h"
#include "multi_temp_logger_rtu.h"
#include "xinjie_plc_rtu_types.h"
#include "qmodbus_pdu.h"

#include <QFile>
#include <QRegularExpression>
#include <QTimer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMutexLocker>
#include <QThread>

#include <memory>

#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

bool isRuntimeMachineIndexPlaceholder(const QString& text) {
    const QString s = text.trimmed();
    return s.isEmpty() || s == QStringLiteral("$INDEX") || s == QStringLiteral("${INDEX}") || s == QStringLiteral("$SLOT") || s == QStringLiteral("${SLOT}") || s == QStringLiteral("{index}");
}

/** String 型发送参数：兼容叶子 QString 与 overlay 留下的 {string:xxx} 单键 map。 */
QString sendParamAsRawText(const QVariant& resolved) {
    if (resolved.userType() == QMetaType::QString)
        return resolved.toString();
    if (resolved.canConvert<QVariantMap>()) {
        const QVariantMap map = resolved.toMap();
        if (map.contains(QStringLiteral("string")))
            return map.value(QStringLiteral("string")).toString();
        if (map.size() == 1)
            return map.constBegin().value().toString();
    }
    return resolved.toString();
}

int fixtureMachineIndexFromParam(const QVariant& param) {
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        const QStringList keys = {QStringLiteral("MachineIndex"), QStringLiteral("machineIndex"),
                                  QStringLiteral("int"), QStringLiteral("value")};
        for (const QString& key : keys) {
            if (!map.contains(key))
                continue;
            const int v = map.value(key).toInt();
            if (v >= 1 && v <= 15)
                return v;
        }
    }
    bool ok = false;
    int idx = param.toInt(&ok);
    if ((!ok || idx <= 0) && param.type() == QVariant::String)
        idx = param.toString().trimmed().toInt(&ok);
    if (idx < 1)
        idx = 1;
    if (idx > 15)
        idx = 15;
    return idx;
}

double asd9026aParamDouble(const QVariantMap& map, const QString& key, double fallback) {
    if (!map.contains(key))
        return fallback;
    return map.value(key).toDouble();
}

quint8 asd9026aCurrentRangeFromMap(const QVariantMap& map, quint8 fallback = 4) {
    if (!map.contains(QStringLiteral("currentRange")))
        return fallback;
    return static_cast<quint8>(qBound(1, map.value(QStringLiteral("currentRange")).toInt(), 4));
}

QString asd9026aCurrentRangeText(quint8 rangeCode) {
    switch (rangeCode) {
    case 1:
        return QStringLiteral("大档");
    case 2:
        return QStringLiteral("中档");
    case 3:
        return QStringLiteral("小档");
    case 4:
    default:
        return QStringLiteral("自动");
    }
}

/** 步骤 Param_txHex / Param_string：空格分隔十六进制整帧（含 CRC） */
QString asd9026aTxHexFromParam(const QVariant& resolved, const QVariantMap& map) {
    if (map.contains(QStringLiteral("txHex")))
        return map.value(QStringLiteral("txHex")).toString().trimmed();
    if (map.contains(QStringLiteral("string")))
        return map.value(QStringLiteral("string")).toString().trimmed();
    return sendParamAsRawText(resolved).trimmed();
}

bool asd9026aSendConfiguredTxHex(Asd9026aDevice& dev, quint8 moduleAddr, const QString& txHex,
                                 QByteArray* response, QString* actualTxHex, QString* errorMessage) {
    if (txHex.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("txHex 为空");
        return false;
    }
    bool parsedAsHex = false;
    QByteArray request = XwdRawUartCodec::encodeRawText(txHex, &parsedAsHex);
    if (request.isEmpty() || !parsedAsHex) {
        if (errorMessage)
            *errorMessage = QStringLiteral("txHex 须为纯十六进制整帧（含CRC）");
        return false;
    }
    if (request.size() < 6) {
        if (errorMessage)
            *errorMessage = QStringLiteral("txHex 帧长度不足");
        return false;
    }
    // 一拖多共用同一份步骤配置：地址按工位 index 改写，并同步重算 CRC。
    request[0] = static_cast<char>(moduleAddr);
    const quint16 crc = Asd9026aCodec::crc16Modbus(request.left(request.size() - 2));
    request[request.size() - 2] = static_cast<char>(crc & 0xFF);
    request[request.size() - 1] = static_cast<char>((crc >> 8) & 0xFF);
    if (actualTxHex)
        *actualTxHex = QString::fromLatin1(request.toHex(' ').toUpper());
    return dev.sendRawFrame(request, response, errorMessage);
}

quint8 asd9026aModuleAddr(int channel) {
    return static_cast<quint8>(channel);
}

bool ensureAsd9026aConnected(QFreeWork* ctx, Asd9026aDevice& dev, QString* errorMessage) {
    if (dev.isOpen())
        return true;
    auto* box = qobject_cast<QFreeWorkBox*>(ctx->window());
    const QString port = box ? box->selectedFixtureComName(ctx->getIndex()) : QString();
    if (port.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 串口未选择，请在顶部“连接治具串口”窗口选择端口");
        return false;
    }
    // 顶部治具窗口只提供 ASD 端口选择；若已按普通治具协议打开，先释放占用再交给 ASD 驱动。
    if (Fixture_uart* fixtureUart = box->fixtureUartWidget();
        fixtureUart && fixtureUart->isFixtureSerialOpen()) {
        fixtureUart->closeSerialPort();
    }
    const int baudRate = SETTINGS.value(QStringLiteral("ASD9026A/BaudRate"), 115200).toInt();
    if (!dev.open(port, baudRate, errorMessage))
        return false;
    ctx->showlog(QStringLiteral("ASD9026A 已连接顶部治具串口 %1 @ %2").arg(port).arg(baudRate));
    return true;
}

bool ensureXwdJigUartOpen(QFreeWork* ctx, QString* errorMessage) {
    if (!ctx) {
        if (errorMessage)
            *errorMessage = QStringLiteral("工站上下文无效");
        return false;
    }
    QComboBox* jigCombo = ctx->getJigcomNameCombo();
    if (!jigCombo) {
        if (errorMessage)
            *errorMessage = QStringLiteral("当前工站未配置治具串口");
        return false;
    }
    const QString port = jigCombo->currentText().trimmed();
    if (port.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("请在工位界面选择治具串口");
        return false;
    }

    // 统一波特率键；兼容旧 XwdBleFixture / XwdSuctionFixture
    int baudRate = SETTINGS.value(QStringLiteral("XwdFixture/BaudRate"), 0).toInt();
    if (baudRate <= 0)
        baudRate = SETTINGS.value(QStringLiteral("XwdBleFixture/BaudRate"), 0).toInt();
    if (baudRate <= 0)
        baudRate = SETTINGS.value(QStringLiteral("XwdSuctionFixture/BaudRate"), 9600).toInt();
    if (ctx->jigBaudRate != baudRate) {
        ctx->jigBaudRate = baudRate;
        if (ctx->jigSerialPort && ctx->jigSerialPort->isOpen())
            ctx->closeJigSerialPort();
    }
    if (!ctx->jigSerialPort || !ctx->jigSerialPort->isOpen())
        ctx->openJigSerialPort();
    if (!ctx->jigSerialPort || !ctx->jigSerialPort->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("治具串口打开失败：%1 @ %2").arg(port).arg(baudRate);
        return false;
    }
    ctx->showlog(QStringLiteral("XWD治具串口已打开：%1 @ %2").arg(port).arg(baudRate));
    return true;
}

bool sendAndCollectXwdReadOnceReply(SerialChannel* channel, QSerialPort* port, const QByteArray& request,
                                    int readChannel, int timeoutMs, QByteArray* reply, QString* errorMessage) {
    if (!channel || !port || !port->isOpen() || !reply)
        return false;

    reply->clear();
    channel->clearReceiveBuffer();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    const QMetaObject::Connection frameConnection =
        QObject::connect(channel, &SerialChannel::frameReceived, &loop, [&](const QByteArray& frame) {
            reply->append(frame);
            double ch1Ma = 0;
            double ch2Ma = 0;
            bool hasCh1 = false;
            bool hasCh2 = false;
            XwdRawUartCodec::parseReadOnceReply(QString::fromUtf8(*reply), &ch1Ma, &ch2Ma, &hasCh1, &hasCh2);
            if ((readChannel == 2 && hasCh2) || (readChannel != 2 && hasCh1))
                loop.quit();
        });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    if (channel->write(request) != request.size()) {
        QObject::disconnect(frameConnection);
        if (errorMessage)
            *errorMessage = QStringLiteral("XWD治具串口写入失败");
        return false;
    }
    qDebug().noquote() << "XWD FIXTURE TX:" << QString::fromLatin1(request.toHex(' ').toUpper());
    Qlog().save_jig_uart_log(1, request);
    // 直接进入事件循环再等待回包，避免 waitForBytesWritten 内先收到 CH1 并提前触发 loop.quit()。
    timeout.start(qMax(1, timeoutMs));
    loop.exec();
    QObject::disconnect(frameConnection);
    return true;
}

bool ensureJieliBtBoxProductUartOpen(QFreeWork* ctx, QString* errorMessage) {
    if (!ctx) {
        if (errorMessage)
            *errorMessage = QStringLiteral("工站上下文无效");
        return false;
    }
    QComboBox* productCombo = ctx->getProductcomNameCombo();
    if (!productCombo) {
        if (errorMessage)
            *errorMessage = QStringLiteral("当前工站未配置产品串口(仪器)");
        return false;
    }
    const QString port = productCombo->currentText().trimmed();
    if (port.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("请在工位界面「产品串口(仪器)」下拉框选择 COM 口");
        return false;
    }

    // 杰理盒子固定 460800（可用 SETTINGS 覆盖）
    const int baudRate = SETTINGS.value(QStringLiteral("JieliBtBox/BaudRate"), 460800).toInt();
    if (ctx->productBaudRate != baudRate) {
        ctx->productBaudRate = baudRate;
        if (ctx->productSerialPort && ctx->productSerialPort->isOpen())
            ctx->closeProductSerialPort();
    }
    if (!ctx->productSerialPort || !ctx->productSerialPort->isOpen())
        ctx->openProductSerialPort();
    if (!ctx->productSerialPort || !ctx->productSerialPort->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("产品串口(仪器)打开失败：%1").arg(port);
        return false;
    }
    return true;
}

bool isRuntimeMacPlaceholder(const QString& text) {
    const QString s = text.trimmed();
    // 空字符串不是 MAC 占位符；否则未填写的 visaAddress 等字段会被误替换成界面 MAC
    return s == QStringLiteral("$MAC") || s == QStringLiteral("${MAC}") || s == QStringLiteral("{mac}");
}

bool looksLikeBluetoothMacAddress(const QString& text) {
    static const QRegularExpression macRe(
        QStringLiteral(R"(^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$)"));
    return macRe.match(text.trimmed()).hasMatch();
}

bool isRuntimePcbaSnPlaceholder(const QString& text) {
    const QString s = text.trimmed();
    return s == QStringLiteral("$SN") || s == QStringLiteral("${SN}") || s == QStringLiteral("{sn}")
           || s.compare(QStringLiteral("$PCBA_SN"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("${PCBA_SN}"), Qt::CaseInsensitive) == 0;
}

bool isRuntimeWholeMachineSnPlaceholder(const QString& text) {
    const QString s = text.trimmed();
    return s.compare(QStringLiteral("$TAIL_SN"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("${TAIL_SN}"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("{tail_sn}"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("$WHOLE_MACHINE_SN"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("${WHOLE_MACHINE_SN}"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("$WHOLE_SN"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("${WHOLE_SN}"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("$TUPLE_SN"), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("${TUPLE_SN}"), Qt::CaseInsensitive) == 0;
}

bool isRuntimeSnPlaceholder(const QString& text) {
    return isRuntimePcbaSnPlaceholder(text) || isRuntimeWholeMachineSnPlaceholder(text);
}

enum class TuplePlaceholderKind {
    None,
    ProductKey,
    DeviceName,
    DeviceSecret,
};

TuplePlaceholderKind tuplePlaceholderKind(const QString& text) {
    const QString s = text.trimmed();
    if (s.compare(QStringLiteral("$TUPLE_PRODUCT_KEY"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${TUPLE_PRODUCT_KEY}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$PRODUCT_KEY"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::ProductKey;
    if (s.compare(QStringLiteral("$TUPLE_DEVICE_NAME"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${TUPLE_DEVICE_NAME}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$DEVICE_NAME"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::DeviceName;
    if (s.compare(QStringLiteral("$TUPLE_DEVICE_SECRET"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${TUPLE_DEVICE_SECRET}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$DEVICE_SECRET"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::DeviceSecret;
    return TuplePlaceholderKind::None;
}

bool paramTreeReferencesTuplePlaceholder(const QVariant& param) {
    if (param.userType() == QMetaType::QString)
        return tuplePlaceholderKind(param.toString()) != TuplePlaceholderKind::None;
    if (param.canConvert<DeviceSnPayload>()) {
        return tuplePlaceholderKind(QString::fromUtf8(param.value<DeviceSnPayload>().sn))
            != TuplePlaceholderKind::None;
    }
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (paramTreeReferencesTuplePlaceholder(it.value()))
                return true;
        }
    }
    if (param.type() == QVariant::List) {
        const QVariantList list = param.toList();
        for (const QVariant& item : list) {
            if (paramTreeReferencesTuplePlaceholder(item))
                return true;
        }
    }
    return false;
}

PlcCmd plcCmdFromName(const QString& name) {
    if (name == QLatin1String("Connect"))
        return PlcCmd::Connect;
    if (name == QLatin1String("Disconnect"))
        return PlcCmd::Disconnect;
    if (name == QLatin1String("IsConnected"))
        return PlcCmd::IsConnected;
    if (name == QLatin1String("ReadCoil"))
        return PlcCmd::ReadCoil;
    if (name == QLatin1String("WriteCoil"))
        return PlcCmd::WriteCoil;
    if (name == QLatin1String("ReadCoils"))
        return PlcCmd::ReadCoils;
    if (name == QLatin1String("WaitCoilTrue"))
        return PlcCmd::WaitCoilTrue;
    if (name == QLatin1String("WaitCoilFalse"))
        return PlcCmd::WaitCoilFalse;
    if (name == QLatin1String("SendStepDone"))
        return PlcCmd::SendStepDone;
    return PlcCmd::IsConnected;
}

GcPlcCmd gcPlcCmdFromName(const QString& name) {
    if (name == QLatin1String("Connect"))
        return GcPlcCmd::Connect;
    if (name == QLatin1String("Disconnect"))
        return GcPlcCmd::Disconnect;
    if (name == QLatin1String("IsConnected"))
        return GcPlcCmd::IsConnected;
    if (name == QLatin1String("WriteCoil"))
        return GcPlcCmd::WriteCoil;
    return GcPlcCmd::IsConnected;
}

XinjePlcCmd xinjiePlcCmdFromName(const QString& name) {
    if (name == QLatin1String("Connect"))
        return XinjePlcCmd::Connect;
    if (name == QLatin1String("Disconnect"))
        return XinjePlcCmd::Disconnect;
    if (name == QLatin1String("IsConnected"))
        return XinjePlcCmd::IsConnected;
    if (name == QLatin1String("WriteCoil"))
        return XinjePlcCmd::WriteCoil;
    if (name == QLatin1String("ReadCoils"))
        return XinjePlcCmd::ReadCoils;
    if (name == QLatin1String("WriteRegister"))
        return XinjePlcCmd::WriteRegister;
    if (name == QLatin1String("ReadHoldingRegisters"))
        return XinjePlcCmd::ReadHoldingRegisters;
    if (name == QLatin1String("ReadDiscreteInputs"))
        return XinjePlcCmd::ReadDiscreteInputs;
    return XinjePlcCmd::IsConnected;
}

HqAmmeterRtuCmd hqAmmeterRtuCmdFromName(const QString& name) {
    if (name == QLatin1String("ReadMeasurement"))
        return HqAmmeterRtuCmd::ReadMeasurement;
    if (name == QLatin1String("SetBaud115200"))
        return HqAmmeterRtuCmd::SetBaud115200;
    return HqAmmeterRtuCmd::ReadMeasurement;
}

LxAmmeterRtuCmd lxAmmeterRtuCmdFromName(const QString& name) {
    if (name == QLatin1String("ReadMeasurement"))
        return LxAmmeterRtuCmd::ReadMeasurement;
    return LxAmmeterRtuCmd::ReadMeasurement;
}

MultiTempLoggerRtuCmd multiTempLoggerRtuCmdFromName(const QString& name) {
    if (name.compare(QLatin1String("SendRaw"), Qt::CaseInsensitive) == 0)
        return MultiTempLoggerRtuCmd::SendRaw;
    if (name.compare(QLatin1String("ReadChannelTemp"), Qt::CaseInsensitive) == 0)
        return MultiTempLoggerRtuCmd::ReadChannelTemp;
    return MultiTempLoggerRtuCmd::ReadChannelTemp;
}

HuilingScpiCmd huilingScpiCmdFromName(const QString& name) {
    if (name == QLatin1String("ConfigureMeasure"))
        return HuilingScpiCmd::ConfigureMeasure;
    if (name == QLatin1String("ReadMeasureCurrent"))
        return HuilingScpiCmd::ReadMeasureCurrent;
    if (name == QLatin1String("ReadMeasureConfiguration"))
        return HuilingScpiCmd::ReadMeasureConfiguration;
    if (name == QLatin1String("InitializeDevice"))
        return HuilingScpiCmd::InitializeDevice;
    if (name == QLatin1String("ConfigureProgrammablePower"))
        return HuilingScpiCmd::ConfigureProgrammablePower;
    if (name == QLatin1String("ProgrammablePowerOutput"))
        return HuilingScpiCmd::ProgrammablePowerOutput;
    if (name == QLatin1String("ReadProgrammableVoltage"))
        return HuilingScpiCmd::ReadProgrammableVoltage;
    if (name == QLatin1String("ReadProgrammableCurrent"))
        return HuilingScpiCmd::ReadProgrammableCurrent;
    if (name == QLatin1String("InitializeProgrammablePower"))
        return HuilingScpiCmd::InitializeProgrammablePower;
    if (name == QLatin1String("SendRawLine"))
        return HuilingScpiCmd::SendRawLine;
    return HuilingScpiCmd::InitializeDevice;
}

CmwScpiCmd cmwScpiCmdFromName(const QString& name) {
    if (name == QLatin1String("ClearStatus"))
        return CmwScpiCmd::ClearStatus;
    if (name == QLatin1String("GenOff"))
        return CmwScpiCmd::GenOff;
    if (name == QLatin1String("GenOn"))
        return CmwScpiCmd::GenOn;
    if (name == QLatin1String("ListOff"))
        return CmwScpiCmd::ListOff;
    if (name == QLatin1String("BbModeArb"))
        return CmwScpiCmd::BbModeArb;
    if (name == QLatin1String("ArbFile"))
        return CmwScpiCmd::ArbFile;
    if (name == QLatin1String("ArbRepetition"))
        return CmwScpiCmd::ArbRepetition;
    if (name == QLatin1String("ArbCycles"))
        return CmwScpiCmd::ArbCycles;
    if (name == QLatin1String("TxLevelDbm"))
        return CmwScpiCmd::TxLevelDbm;
    if (name == QLatin1String("FrequencyMhz"))
        return CmwScpiCmd::FrequencyMhz;
    if (name == QLatin1String("ManualArbTrigger"))
        return CmwScpiCmd::ManualArbTrigger;
    if (name == QLatin1String("WriteLine"))
        return CmwScpiCmd::WriteLine;
    if (name == QLatin1String("Identity"))
        return CmwScpiCmd::Identity;
    if (name == QLatin1String("ArbFilePath"))
        return CmwScpiCmd::ArbFilePath;
    if (name == QLatin1String("ArbScount"))
        return CmwScpiCmd::ArbScount;
    if (name == QLatin1String("GenState"))
        return CmwScpiCmd::GenState;
    if (name == QLatin1String("SystemError"))
        return CmwScpiCmd::SystemError;
    if (name == QLatin1String("QueryLine"))
        return CmwScpiCmd::QueryLine;
    return CmwScpiCmd::ClearStatus;
}

int paramMapSwitchValue(const QVariantMap& map) {
    if (map.contains(QStringLiteral("int")))
        return map.value(QStringLiteral("int")).toInt();
    if (map.contains(QStringLiteral("value")))
        return map.value(QStringLiteral("value")).toInt();
    if (map.size() == 1)
        return map.constBegin().value().toInt();
    return 0;
}

/** 读电流连续采样窗口：Param_sampleDurationMs > Timing/CommandTimeoutMs > 默认 3000ms */
int currentSampleDurationMs(const TestCaseDefinition& def, const QVariantMap& map) {
    if (map.contains(QStringLiteral("sampleDurationMs"))) {
        const int v = map.value(QStringLiteral("sampleDurationMs")).toInt();
        if (v > 0)
            return v;
    }
    const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    if (timeoutMs > 0)
        return timeoutMs;
    return 3000;
}

int currentSampleIntervalMs(const QVariantMap& map) {
    const int v = map.value(QStringLiteral("sampleIntervalMs"), 200).toInt();
    return qMax(50, v);
}

QVariantMap huilingVisaLinkKeysFromMap(const QVariantMap& map) {
    static const QStringList keys = {
        QStringLiteral("visaAddress"),
        QStringLiteral("voltage"),
        QStringLiteral("current"),
        QStringLiteral("scpiSetVoltageCmd"),
        QStringLiteral("scpiSetCurrentCmd"),
        QStringLiteral("scpiOutputOnCmd"),
        QStringLiteral("scpiOutputOffCmd"),
        QStringLiteral("scpiReadVoltageCmd"),
        QStringLiteral("scpiReadCurrentCmd"),
        QStringLiteral("scpiSetCurrentRangeCmd"),
        QStringLiteral("currentRange"),
        QStringLiteral("powerChannel"),
        QStringLiteral("scpiChannelSelectCmd"),
        QStringLiteral("visaDeviceIndex"),
        QStringLiteral("sharedPair"),
        QStringLiteral("stationsPerDevice"),
        QStringLiteral("visaAddress0"),
        QStringLiteral("visaAddress1"),
        QStringLiteral("visaAddress_0"),
        QStringLiteral("visaAddress_1"),
    };
    QVariantMap out;
    for (const QString& key : keys) {
        if (map.contains(key))
            out.insert(key, map.value(key));
    }
    return out;
}

/** 合并 link（visa/scpi 模板）与 command（电压电流等），供配置步落盘缓存与 load profile。 */
QVariantMap mergeVisaPowerStepParamMap(const QVariantMap& linkMap, const QVariant& commandParam) {
    QVariantMap merged = linkMap;
    if (!commandParam.canConvert<QVariantMap>()) {
        return merged;
    }
    const QVariantMap cmd = commandParam.toMap();
    for (auto it = cmd.constBegin(); it != cmd.constEnd(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return merged;
}

struct HuilingScpiStepParams {
    QVariantMap linkMap;
    QVariant commandParam;
};

HuilingScpiStepParams splitHuilingScpiStepParam(const QVariant& resolved, const QString& deviceCmd) {
    HuilingScpiStepParams out;
    if (!resolved.canConvert<QVariantMap>()) {
        out.commandParam = resolved;
        return out;
    }
    const QVariantMap map = resolved.toMap();
    out.linkMap = huilingVisaLinkKeysFromMap(map);
    if (deviceCmd == QLatin1String("ProgrammablePowerOutput")) {
        out.commandParam = paramMapSwitchValue(map);
    } else if (deviceCmd == QLatin1String("ConfigureProgrammablePower")) {
        QVariantMap cmd;
        if (map.contains(QStringLiteral("voltage")))
            cmd.insert(QStringLiteral("voltage"), map.value(QStringLiteral("voltage")));
        if (map.contains(QStringLiteral("current")))
            cmd.insert(QStringLiteral("current"), map.value(QStringLiteral("current")));
        if (map.contains(QStringLiteral("currentRange")))
            cmd.insert(QStringLiteral("currentRange"), map.value(QStringLiteral("currentRange")));
        out.commandParam = cmd;
    } else if (deviceCmd == QLatin1String("ReadProgrammableCurrent")) {
        if (map.contains(QStringLiteral("currentRange"))) {
            out.commandParam =
                QVariantMap{{QStringLiteral("currentRange"), map.value(QStringLiteral("currentRange"))}};
        }
    } else if (deviceCmd == QLatin1String("SendRawLine")) {
        if (map.contains(QStringLiteral("string")))
            out.commandParam = map.value(QStringLiteral("string"));
        else if (map.contains(QStringLiteral("line")))
            out.commandParam = map.value(QStringLiteral("line"));
        else
            out.commandParam = map.value(QStringLiteral("value"));
    } else {
        out.commandParam = QVariant();
    }
    return out;
}

double huilingParamDouble(const QVariantMap& map, const QString& key, double fallback) {
    return map.contains(key) ? map.value(key).toDouble() : fallback;
}

/** 是否为「等待蓝牙连上」类 Dongle 指令（扫连/直连/按名/OTA/App/主连）。
 *  超时宜 ≥18s；sendCommandWithRetry 第三参 allowResend=false（只发一次，勿窗口内重发 DCON）。
 *  是否须等连接成功才过步，由 TestCaseRunner::needAsyncDone（流程侧）决定，不是第三参。 */
bool isDongleBleConnectCmd(DongleCmd cmd) {
    return cmd == DongleCmd::BleScanConnect || cmd == DongleCmd::BleDirectConnect
        || cmd == DongleCmd::BleScanConnectByName || cmd == DongleCmd::BleOtaConnect
        || cmd == DongleCmd::BleAppConnect || cmd == DongleCmd::BleMainConnect;
}

} // namespace

int QFreeWork::resolveFixtureMachineIndex(const QVariant& param) const {
    const QVariant resolved = resolveTestCaseSendParamTree(param);
    if (resolved.userType() == QMetaType::QString && isRuntimeMachineIndexPlaceholder(resolved.toString())) {
        return qBound(1, getIndex(), 15);
    }
    int idx = fixtureMachineIndexFromParam(resolved);
    if (idx <= 0)
        return qBound(1, getIndex(), 15);
    return idx;
}

QVariantMap QFreeWork::cachedHuilingVisaLink() const {
    return huilingVisaLinkCache_;
}

void QFreeWork::updateHuilingVisaLinkCache(const QVariantMap& link) {
    const QString addr = link.value(QStringLiteral("visaAddress")).toString().trimmed();
    if (addr.isEmpty() || looksLikeBluetoothMacAddress(addr))
        return;
    huilingVisaLinkCache_ = link;
}

void QFreeWork::seedHuilingVisaLinkCacheFromFlowOrSettings() {
    auto trySeedFromConfigureStep = [this](const TestCaseDefinition& def) {
        if (def.send.channel != TestCaseSendChannel::Scpi) {
            return false;
        }
        if (def.send.deviceCmd.compare(QLatin1String("ConfigureProgrammablePower"), Qt::CaseInsensitive) != 0) {
            return false;
        }
        if (!def.send.param.canConvert<QVariantMap>()) {
            return false;
        }
        const QVariantMap map = def.send.param.toMap();
        QString visaAddr = map.value(QStringLiteral("visaAddress")).toString().trimmed();
        if (visaAddr.isEmpty())
            visaAddr = SharedInstrument::visaAddressFromParam(map, 0);
        if (visaAddr.isEmpty() || looksLikeBluetoothMacAddress(visaAddr))
            return false;
        QVariantMap seedMap = huilingVisaLinkKeysFromMap(map);
        seedMap.insert(QStringLiteral("visaAddress"), visaAddr);
        updateHuilingVisaLinkCache(seedMap);
        return true;
    };

    const QVector<TestFlowItemEntry> flowItems = TestCaseStore::loadStationFlowItems(activeFlowStationKey_);
    for (const TestFlowItemEntry& entry : flowItems) {
        if (!entry.enabled) {
            continue;
        }
        TestCaseDefinition def;
        if (!TestCaseStore::loadCaseForStation(activeFlowStationKey_, entry.caseName, def)) {
            continue;
        }
        if (trySeedFromConfigureStep(def)) {
            return;
        }
    }

    TestCaseDefinition libraryDef;
    if (TestCaseStore::loadCaseForStation(activeFlowStationKey_, QStringLiteral("配置Visa程控电源"), libraryDef)
        && trySeedFromConfigureStep(libraryDef)) {
        return;
    }
}

QString QFreeWork::resolveTestCaseSendPlaceholder(const QString& text) const {
    if (isRuntimeMacPlaceholder(text))
        return currentMacAddress();
    if (isRuntimePcbaSnPlaceholder(text))
        return resolvedPcbaSnText();
    if (isRuntimeWholeMachineSnPlaceholder(text))
        return resolvedPcbaSnText();
    switch (tuplePlaceholderKind(text)) {
    case TuplePlaceholderKind::ProductKey:
        return tupleData_.productKey;
    case TuplePlaceholderKind::DeviceName:
        return tupleData_.deviceName;
    case TuplePlaceholderKind::DeviceSecret:
        return tupleData_.deviceSecret;
    default:
        break;
    }
    if (text.startsWith(QStringLiteral("$SETTINGS:"))) {
        const QString key = text.mid(10).trimmed();
        if (!key.isEmpty())
            return SETTINGS.value(key).toString();
    }
    return text;
}

QVariant QFreeWork::resolveTestCaseSendParamTree(const QVariant& param) const {
    if (param.userType() == QMetaType::QString)
        return resolveTestCaseSendPlaceholder(param.toString());
    // 加载期曾把 Sn 归一成 DeviceSnPayload，此处仍要对 sn 字段做 $TUPLE_* 展开
    if (param.canConvert<DeviceSnPayload>()) {
        DeviceSnPayload payload = param.value<DeviceSnPayload>();
        payload.sn = resolveTestCaseSendPlaceholder(QString::fromUtf8(payload.sn)).toUtf8();
        return QVariant::fromValue(payload);
    }
    if (param.canConvert<QVariantMap>()) {
        QVariantMap map = param.toMap();
        for (auto it = map.begin(); it != map.end(); ++it)
            it.value() = resolveTestCaseSendParamTree(it.value());
        return map;
    }
    if (param.type() == QVariant::List) {
        QVariantList list = param.toList();
        for (int i = 0; i < list.size(); ++i)
            list[i] = resolveTestCaseSendParamTree(list[i]);
        return list;
    }
    return param;
}

bool QFreeWork::prepareTupleProductWriteForTestCase(const TestCaseDefinition& def, DeviceCmd cmd,
                                                    const QVariant& wireParam) {
    if (!paramTreeReferencesTuplePlaceholder(def.send.param))
        return true;

    const QString stepName = def.meta.displayName.trimmed().isEmpty() ? def.meta.name.trimmed()
                                                                      : def.meta.displayName.trimmed();
    if (failTupleWriteIfNoValidField(stepName, tupleData_.success, QStringLiteral("云端三元组未获取成功")))
        return false;

    QString writeText;
    if (cmd == DeviceCmd::Sn && wireParam.canConvert<DeviceSnPayload>()) {
        writeText = QString::fromUtf8(wireParam.value<DeviceSnPayload>().sn);
    } else if (cmd == DeviceCmd::WriteKey) {
        const QVariantMap map = wireParam.toMap();
        writeText = QString::fromUtf8(map.value(QStringLiteral("value")).toByteArray());
        if (writeText.trimmed().isEmpty())
            writeText = map.value(QStringLiteral("value")).toString();
        // Qaiot：可单字段写 productId/deviceId/deviceSecret（或一次写多个）
        if (writeText.trimmed().isEmpty()) {
            const QString productId =
                map.value(QStringLiteral("productId"), map.value(QStringLiteral("productKey"))).toString().trimmed();
            const QString deviceId =
                map.value(QStringLiteral("deviceId"), map.value(QStringLiteral("deviceName"))).toString().trimmed();
            const QString deviceSecret =
                map.value(QStringLiteral("deviceSecret"), map.value(QStringLiteral("key"))).toString().trimmed();
            QStringList parts;
            auto appendPart = [&](const QString& label, const QString& text) {
                if (text.isEmpty())
                    return true;
                if (tuplePlaceholderKind(text) != TuplePlaceholderKind::None)
                    return false;
                parts.append(QStringLiteral("%1:%2").arg(label, text));
                return true;
            };
            if (!appendPart(QStringLiteral("productId"), productId) || !appendPart(QStringLiteral("deviceId"), deviceId) ||
                !appendPart(QStringLiteral("deviceSecret"), deviceSecret)) {
                failTupleWriteIfNoValidField(stepName, false, QStringLiteral("三元组占位符未展开"));
                return false;
            }
            if (!parts.isEmpty()) {
                writeText = parts.join(QLatin1Char(' '));
                stepRuntime_.testData = writeText;
                return true;
            }
        }
    }

    if (writeText.trimmed().isEmpty() || tuplePlaceholderKind(writeText) != TuplePlaceholderKind::None) {
        failTupleWriteIfNoValidField(stepName, false,
                                     writeText.trimmed().isEmpty() ? QStringLiteral("三元组字段为空")
                                                                   : QStringLiteral("三元组占位符未展开"));
        return false;
    }
    stepRuntime_.testData = writeText;
    return true;
}

bool QFreeWork::prepareTailSnWriteForTestCase(const TestCaseDefinition& def, DeviceCmd cmd, const QVariant& wireParam) {
    if (cmd != DeviceCmd::Sn || def.send.action != TestCaseSendAction::Set)
        return true;

    FacDevInfoType whichSn = FacDevInfoType_TAIL_SN;
    QString snText;
    if (wireParam.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = wireParam.value<DeviceSnPayload>();
        whichSn = payload.which_sn;
        snText = QString::fromUtf8(payload.sn).trimmed();
    } else if (wireParam.canConvert<QVariantMap>()) {
        const QVariantMap map = wireParam.toMap();
        whichSn = static_cast<FacDevInfoType>(map.value(QStringLiteral("which_sn"), FacDevInfoType_TAIL_SN).toInt());
        snText = map.value(QStringLiteral("sn")).toString().trimmed();
    } else {
        return true;
    }
    if (whichSn != FacDevInfoType_TAIL_SN)
        return true;

    const QString stepName = def.meta.displayName.trimmed().isEmpty() ? def.meta.name.trimmed()
                                                                    : def.meta.displayName.trimmed();
    if (snText.isEmpty()) {
        stepRuntime_.testData = QStringLiteral("整机SN为空");
        showlog(QStringLiteral("%1失败：界面SN为空，请先获取三元组或扫入整机SN").arg(stepName));
        return false;
    }
    stepRuntime_.testData = snText;
    return true;
}

QString QFreeWork::currentMacAddress() const {
    // 单步/连蓝牙优先用界面当前值，避免成员 macAddress 残留覆盖空的输入框
    auto usable = [](const QString& s) {
        const QString t = s.trimmed();
        return !t.isEmpty() && t != QStringLiteral("没有mac地址");
    };
    if (ui && ui->macInput && usable(ui->macInput->text()))
        return ui->macInput->text().trimmed();
    if (ui && ui->mac_combo && usable(ui->mac_combo->currentText()))
        return ui->mac_combo->currentText().trimmed();
    if (usable(macAddress))
        return macAddress.trimmed();
    return {};
}

bool QFreeWork::useTestCaseFlow(const QString& stationKey) const {
    QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        key = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (TestCaseStore::loadStationItems(key).isEmpty()) {
        const QString byName = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationName());
        if (!byName.isEmpty())
            key = byName;
    }
    if (key.isEmpty())
        key = QStringLiteral("default");
    if (!QFile::exists(TestCasePaths::flowIniPath()))
        return false;
    return !TestCaseStore::loadStationItems(key).isEmpty();
}

QStringList QFreeWork::testCaseFlowItems(const QString& stationKey) const {
    QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        key = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (TestCaseStore::loadStationItems(key).isEmpty()) {
        const QString byName = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationName());
        if (!byName.isEmpty())
            key = byName;
    }
    if (key.isEmpty())
        key = QStringLiteral("default");
    return TestCaseStore::loadStationItems(key);
}

void QFreeWork::setActiveTestCase(const TestCaseDefinition& def) {
    activeTestCase_ = def;
    activeTestCaseStepLabel_ = def.meta.name.trimmed();
    testCaseStepActive_ = true;
    testCaseStepResult_ = {};
    testCaseMultiGateTableEmitted_ = false;
    currentSampleAnyMatchActive_ = false;
    currentSampleCount_ = 0;
    currentSampleLastValueText_.clear();
}

void QFreeWork::clearActiveTestCase() {
    activeTestCase_ = {};
    activeTestCaseStepLabel_.clear();
    testCaseStepActive_ = false;
    testCaseStepResult_ = {};
    testCaseMultiGateTableEmitted_ = false;
    currentSampleAnyMatchActive_ = false;
    currentSampleCount_ = 0;
    currentSampleLastValueText_.clear();
}

void QFreeWork::applyRuntimeSnGateExpected(QVector<TestCaseGate>& gates) {
    if (gates.size() != 1 || gates.first().field != QStringLiteral("value") || !gates.first().expected.trimmed().isEmpty())
        return;
    gates[0].expected = resolvedPcbaSnText();
}

void QFreeWork::applyRuntimePlaceholderGateExpected(QVector<TestCaseGate>& gates) {
    for (TestCaseGate& g : gates) {
        const QString expected = g.expected.trimmed();
        if (expected.isEmpty())
            continue;
        if (isRuntimeMacPlaceholder(expected)) {
            // $MAC = 界面 MAC 框（开局解析 SN / MES 已填入）；不用 macAddress 成员，避免读 MAC 回包后自比自过
            QString uiMac;
            if (ui && ui->macInput)
                uiMac = ui->macInput->text().trimmed();
            if (uiMac.isEmpty() || uiMac == QStringLiteral("没有mac地址"))
                g.expected.clear();
            else
                g.expected = uiMac;
            continue;
        }
        if (isRuntimePcbaSnPlaceholder(expected) || isRuntimeWholeMachineSnPlaceholder(expected)) {
            g.expected = resolveTestCaseSendPlaceholder(expected);
            continue;
        }
        if (tuplePlaceholderKind(expected) != TuplePlaceholderKind::None
            || expected.startsWith(QStringLiteral("$SETTINGS:"))) {
            g.expected = resolveTestCaseSendPlaceholder(expected);
        }
    }
}

void QFreeWork::emitFixtureMultiGateTableRows(const QVector<TestCaseGate>& gates, const QString& reportType,
                                              const QVariant& payload, bool& allPass, QString& detailOut) {
    allPass = true;
    detailOut.clear();
    QVector<TestItem> rows;
    rows.reserve(gates.size());
    QStringList detailParts;
    for (const TestCaseGate& g : gates) {
        if (!g.enabled)
            continue;
        TestCaseGate ge = g;
        ge.reportType = reportType;
        bool subPass = true;
        QString subDetail;
        GateRegistry::evaluate(ge, reportType, payload, subPass, subDetail);
        if (!subPass)
            allPass = false;
        detailParts.append(QStringLiteral("%1(%2)")
                               .arg(GateRegistry::fieldDisplayName(reportType, ge.field), subDetail));
        TestItem item;
        // 多字段卡控分项表格只显示判定项（如 RSSI/频偏），不再重复步骤名前缀
        item.testItem = GateRegistry::fieldDisplayName(reportType, ge.field);
        // 实测/要求统一走 formatStepDisplay（屏幕纯色等会转成中文，避免表里出现 0~5）
        const GateStepDisplay disp =
            GateRegistry::formatStepDisplay(ge, QVector<TestCaseGate>{ge}, reportType, payload, false);
        item.testData = disp.testData;
        item.ask = disp.ask;
        if (item.testData.isEmpty()) {
            // 兼容旧详情句式：当前值= / 当前=
            const QStringList prefixes = {QStringLiteral("当前值="), QStringLiteral("当前=")};
            for (const QString& curPrefix : prefixes) {
                const int curPos = subDetail.indexOf(curPrefix);
                if (curPos < 0)
                    continue;
                const int start = curPos + curPrefix.size();
                const int comma = subDetail.indexOf(QLatin1Char(','), start);
                item.testData = (comma > start) ? subDetail.mid(start, comma - start).trimmed()
                                                : subDetail.mid(start).trimmed();
                break;
            }
            if (item.testData.isEmpty())
                item.testData = subDetail;
        }
        item.testResult = subPass ? passValue : failValue;
        rows.append(item);
    }
    detailOut = detailParts.join(QStringLiteral("; "));
    if (!rows.isEmpty()) {
        testResultTableUpdate(rows);
        testCaseMultiGateTableEmitted_ = true;
        // 与结果表同步：RSSI/频偏等分项各写一条 MES，避免整步只上报主字段
        appendMultiGateTestCaseMes(gates, reportType, payload);
    }
}

bool QFreeWork::isActiveTestCaseStep(const QString& stepLabel) const {
    return testCaseStepActive_ && activeTestCaseStepLabel_ == stepLabel.trimmed();
}

bool QFreeWork::evaluateActiveTestCaseGate(const QString& reportType, const QVariant& payload) {
    if (!testCaseStepActive_ || !activeTestCase_.gate.enabled)
        return false;
    if (activeTestCase_.gate.reportType != reportType)
        return false;

    if (reportType == QStringLiteral("ProtocolSnData")) {
        if (activeTestCase_.send.deviceCmd != QStringLiteral("Sn") || activeTestCase_.send.action != TestCaseSendAction::Get)
            return false;
        if (!payload.canConvert<ProtocolSnData>()) {
            markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
            showlog(QStringLiteral("卡控失败：SN 回传数据类型无效"));
            if (commandRetryTimer)
                finishCommandRetryWait(false, QStringLiteral("卡控失败：SN 回传数据类型无效"));
            return true;
        }
    }

    QVector<TestCaseGate> gatesForEval = TestCaseStore::activeGatesForEvaluation(activeTestCase_);
    if (gatesForEval.isEmpty()) {
        markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
        showlog(QStringLiteral("卡控失败：未启用任何判定项（请在 case ini 的 Gate/ItemN_Enabled 勾选）"));
        if (commandRetryTimer)
            finishCommandRetryWait(false, QStringLiteral("卡控失败：未启用任何判定项"));
        return true;
    }

    applyRuntimeSnGateExpected(gatesForEval);
    applyRuntimePlaceholderGateExpected(gatesForEval);

    bool pass = true;
    QString detail;
    const bool multiFieldMode = gatesForEval.size() > 1;
    // 凡启用多项判定：一律写分项结果表 + 分项 MES；步骤收尾靠 testCaseMultiGateTableEmitted_ 跳过汇总，避免重复
    if (multiFieldMode) {
        emitFixtureMultiGateTableRows(gatesForEval, reportType, payload, pass, detail);
    } else {
        GateRegistry::evaluate(gatesForEval.first(), reportType, payload, pass, detail);
    }

    GateStepDisplay display =
        GateRegistry::formatStepDisplay(gatesForEval.first(), gatesForEval, reportType, payload, multiFieldMode);
    if (display.testData.isEmpty())
        display.testData = detail;

    // 屏幕图像识别：自动卡控失败时弹窗确认是否显示对应颜色（是=通过，否=不通过）
    bool humanOverrodePass = false;
    if (!pass && reportType == QStringLiteral("ProtocolScreenInspectData")) {
        const QString failDetail = detail.isEmpty() ? QStringLiteral("未通过") : detail;
        showlog(QStringLiteral("屏幕检测自动识别未通过：%1").arg(failDetail));
        // 弹窗要用本步具体颜色，避免笼统「目标颜色」
        QString expectColor;
        QVariantMap paramMap;
        if (activeTestCase_.send.param.canConvert<QVariantMap>())
            paramMap = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();
        if (paramMap.contains(QStringLiteral("expectedColor"))) {
            bool colorOk = false;
            const int ci = ScreenInspectAnalyzer::parseColorIndex(
                paramMap.value(QStringLiteral("expectedColor")).toString(), &colorOk);
            if (colorOk && ci >= 0)
                expectColor = ScreenInspectAnalyzer::colorName(ci);
        }
        if (expectColor.isEmpty()) {
            for (const TestCaseGate& g : gatesForEval) {
                if (!g.enabled || g.field.trimmed() != QLatin1String("detectedColor"))
                    continue;
                const QString e = g.expected.trimmed();
                if (e.isEmpty())
                    continue;
                bool colorOk = false;
                const int ci = ScreenInspectAnalyzer::parseColorIndex(e, &colorOk);
                expectColor = (colorOk && ci >= 0) ? ScreenInspectAnalyzer::colorName(ci) : e;
                break;
            }
        }
        if (expectColor.isEmpty()) {
            const QString hintName = activeTestCase_.meta.displayName.trimmed().isEmpty()
                                         ? activeTestCase_.meta.name.trimmed()
                                         : activeTestCase_.meta.displayName.trimmed();
            // 确认屏幕(白色) / 确认屏幕（灰度条）
            const QRegularExpression re(QStringLiteral("[（(]([^）)]+)[）)]"));
            const QRegularExpressionMatch m = re.match(hintName);
            if (m.hasMatch())
                expectColor = m.captured(1).trimmed();
            else {
                const int us = hintName.lastIndexOf(QLatin1Char('_'));
                if (us >= 0)
                    expectColor = hintName.mid(us + 1).trimmed();
            }
        }
        if (expectColor.isEmpty()) {
            const QString prompt = activeTestCase_.meta.promptText.trimmed();
            const QString prefix = QStringLiteral("请确认屏幕是否显示");
            if (prompt.startsWith(prefix))
                expectColor = prompt.mid(prefix.size()).trimmed();
        }
        if (screenInspectAskHumanPassOnAutoFail(expectColor)) {
            pass = true;
            humanOverrodePass = true;
            if (!display.testData.contains(QStringLiteral("人工确认")))
                display.testData += QStringLiteral("（人工确认通过）");
            showlog(QStringLiteral("人工确认：屏幕显示%1，本步通过").arg(
                expectColor.isEmpty() ? QStringLiteral("目标颜色") : expectColor));
        } else {
            showlog(QStringLiteral("人工确认：屏幕未显示%1，本步不通过").arg(
                expectColor.isEmpty() ? QStringLiteral("目标颜色") : expectColor));
        }
    }

    markActiveTestCaseStepDone(pass, display.testData, display.ask);
    if (commandRetryTimer) {
        finishCommandRetryWait(pass,
                               pass ? QStringLiteral("卡控通过，步骤完成")
                                    : QStringLiteral("卡控失败：%1").arg(detail.isEmpty() ? QStringLiteral("未通过")
                                                                                          : detail));
    }
    if (!pass) {
        result = failValue;
        showlog(QStringLiteral("卡控失败：%1").arg(detail));
    } else if (!humanOverrodePass) {
        showlog(QStringLiteral("卡控通过：%1").arg(detail));
    }
    return true;
}

void QFreeWork::runScpiProgrammableCurrentSampleAnyMatch(const TestCaseDefinition& def, const QVariant& commandParam) {
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    const int durationMs = currentSampleDurationMs(def, map);
    const int intervalMs = currentSampleIntervalMs(map);

    currentSampleAnyMatchActive_ = true;
    currentSampleCount_ = 0;
    currentSampleLastValueText_.clear();
    showlog(QStringLiteral("读电流连续采样：窗口 %1ms，间隔 %2ms，期间任一值卡控合格即通过")
                .arg(durationMs)
                .arg(intervalMs));

    QElapsedTimer sampleTimer;
    sampleTimer.start();
    while (sampleTimer.elapsed() < durationMs && !isActiveTestCaseStepDone()) {
        if (!isTestContinue) {
            currentSampleAnyMatchActive_ = false;
            markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            showlog(QStringLiteral("读电流连续采样已中止"));
            return;
        }
        QString errStr;
        bool ok = scpiVisaManager()->exec(HuilingScpiCmd::ReadProgrammableCurrent, commandParam, &errStr);
        if (!ok) {
            VisaChannel::waitWork(200);
            ok = scpiVisaManager()->exec(HuilingScpiCmd::ReadProgrammableCurrent, commandParam, &errStr);
        }
        if (!ok) {
            VisaChannel::waitWork(300);
            ok = scpiVisaManager()->exec(HuilingScpiCmd::ReadProgrammableCurrent, commandParam, &errStr);
        }
        if (!ok) {
            showlog(QStringLiteral("读电流采样失败（继续）：%1").arg(errStr));
        }
        if (isActiveTestCaseStepDone())
            break;
        const int remain = durationMs - static_cast<int>(sampleTimer.elapsed());
        if (remain <= 0)
            break;
        waitWork(qMin(intervalMs, remain));
    }

    currentSampleAnyMatchActive_ = false;
    if (isActiveTestCaseStepDone())
        return;

    const QString failData =
        currentSampleLastValueText_.isEmpty() ? QStringLiteral("无有效读数") : currentSampleLastValueText_;
    showlog(QStringLiteral("读电流采样结束：共 %1 次，无一值落入卡控范围，判定失败（末次 %2）")
                .arg(currentSampleCount_)
                .arg(failData));
    markActiveTestCaseStepDone(false, failData, QStringLiteral("失败"));
}

void QFreeWork::runAsdProgrammableCurrentSampleAnyMatch(const TestCaseDefinition& def, quint8 moduleAddr,
                                                        int channel) {
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    const int durationMs = currentSampleDurationMs(def, map);
    const int intervalMs = currentSampleIntervalMs(map);
    const QString channelText = QStringLiteral("CH%1").arg(channel);
    auto* box = qobject_cast<QFreeWorkBox*>(window());
    Asd9026aDevice* asdDevice = box ? box->sharedAsd9026aDevice() : nullptr;
    if (!asdDevice) {
        markActiveTestCaseStepDone(false, QStringLiteral("共享设备不存在"), QStringLiteral("失败"));
        showlog(QStringLiteral("ASD9026A 共享设备对象不存在"));
        return;
    }

    currentSampleAnyMatchActive_ = true;
    currentSampleCount_ = 0;
    currentSampleLastValueText_.clear();
    showlog(QStringLiteral("ASD9026A 读电流连续采样：窗口 %1ms，间隔 %2ms，期间任一值卡控合格即通过")
                .arg(durationMs)
                .arg(intervalMs));

    QElapsedTimer sampleTimer;
    sampleTimer.start();
    while (sampleTimer.elapsed() < durationMs && !isActiveTestCaseStepDone()) {
        if (!isTestContinue) {
            currentSampleAnyMatchActive_ = false;
            markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            showlog(QStringLiteral("ASD9026A 读电流连续采样已中止"));
            return;
        }
        Asd9026aAnalogStatus status;
        QString errStr;
        if (!asdDevice->readAnalogStatus(moduleAddr, &status, &errStr)) {
            showlog(QStringLiteral("ASD9026A 读电流采样失败（继续）：%1").arg(errStr));
        } else {
            const double currentMa = status.current * 1000.0;
            ProtocolMeasureData measureData;
            measureData.deviceName = QStringLiteral("ASD9026A");
            measureData.channel = channelText;
            measureData.type = QStringLiteral("Current");
            measureData.value = currentMa;
            measureData.valueText = QStringLiteral("%1 mA").arg(currentMa, 0, 'f', 4);
            measureData.unit = QStringLiteral("mA");
            measureData.isOk = true;
            onUsbInstrumentReport(
                ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(measureData)));
        }
        if (isActiveTestCaseStepDone())
            break;
        const int remain = durationMs - static_cast<int>(sampleTimer.elapsed());
        if (remain <= 0)
            break;
        waitWork(qMin(intervalMs, remain));
    }

    currentSampleAnyMatchActive_ = false;
    if (isActiveTestCaseStepDone())
        return;

    const QString failData =
        currentSampleLastValueText_.isEmpty() ? QStringLiteral("无有效读数") : currentSampleLastValueText_;
    showlog(QStringLiteral("ASD9026A 读电流采样结束：共 %1 次，无一值落入卡控范围，判定失败（末次 %2）")
                .arg(currentSampleCount_)
                .arg(failData));
    markActiveTestCaseStepDone(false, failData, QStringLiteral("失败"));
}

void QFreeWork::runJigAmmeterCurrentSampleAnyMatch() {
    QVariantMap map;
    if (activeTestCase_.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();
    int durationMs = 0;
    if (map.contains(QStringLiteral("sampleDurationMs")))
        durationMs = map.value(QStringLiteral("sampleDurationMs")).toInt();
    if (durationMs <= 0 && measure_wait_time > 0)
        durationMs = measure_wait_time;
    if (durationMs <= 0)
        durationMs = currentSampleDurationMs(activeTestCase_, map);
    // ini 里 CommandTimeoutMs=300 过短，至少 1s
    durationMs = qMax(1000, durationMs);
    const int intervalMs = currentSampleIntervalMs(map);

    currentSampleAnyMatchActive_ = true;
    currentSampleCount_ = 0;
    currentSampleLastValueText_.clear();
    showlog(QStringLiteral("治具电流连续采样：窗口 %1ms，间隔 %2ms，期间任一值卡控合格即通过")
                .arg(durationMs)
                .arg(intervalMs));

    QElapsedTimer sampleTimer;
    sampleTimer.start();
    while (sampleTimer.elapsed() < durationMs && !stepRuntime_.done && !isActiveTestCaseStepDone()) {
        if (!isTestContinue) {
            currentSampleAnyMatchActive_ = false;
            markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            showlog(QStringLiteral("治具电流连续采样已中止"));
            return;
        }
        QString errStr;
        if (!execAmmeterMeasure(&errStr))
            showlog(QStringLiteral("治具电流采样下发失败（继续）：%1").arg(errStr));
        const int remain = durationMs - static_cast<int>(sampleTimer.elapsed());
        if (remain <= 0)
            break;
        waitWork(qMin(intervalMs, remain));
    }

    currentSampleAnyMatchActive_ = false;
    if (stepRuntime_.done || isActiveTestCaseStepDone())
        return;

    const QString failData =
        currentSampleLastValueText_.isEmpty() ? QStringLiteral("无有效读数") : currentSampleLastValueText_;
    const QString ask =
        QStringLiteral("[%1,%2]ma").arg(QString::number(LowCurrent), QString::number(HighCurrent));
    showlog(QStringLiteral("治具电流采样结束：共 %1 次，无一值落入卡控范围，判定失败（末次 %2，范围 %3）")
                .arg(currentSampleCount_)
                .arg(failData, ask));
    markActiveTestCaseStepDone(false, failData, ask);
}

void QFreeWork::runModbusAmmeterCurrentSampleAnyMatch(const TestCaseDefinition& def, ModbusDeviceRoute route) {
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    const int durationMs = qMax(1000, currentSampleDurationMs(def, map));
    const int intervalMs = currentSampleIntervalMs(map);

    currentSampleAnyMatchActive_ = true;
    currentSampleCount_ = 0;
    currentSampleLastValueText_.clear();
    showlog(QStringLiteral("Modbus 电流表连续采样：窗口 %1ms，间隔 %2ms，期间任一值卡控合格即通过")
                .arg(durationMs)
                .arg(intervalMs));

    QElapsedTimer sampleTimer;
    sampleTimer.start();
    while (sampleTimer.elapsed() < durationMs && !isActiveTestCaseStepDone()) {
        if (!isTestContinue) {
            currentSampleAnyMatchActive_ = false;
            markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            showlog(QStringLiteral("Modbus 电流表连续采样已中止"));
            return;
        }
        QString errStr;
        bool ok = false;
        if (route == ModbusDeviceRoute::HqAmmeterRtu)
            ok = modbusManager.exec(hqAmmeterRtuCmdFromName(def.send.deviceCmd), &errStr);
        else if (route == ModbusDeviceRoute::LxAmmeterRtu)
            ok = modbusManager.exec(lxAmmeterRtuCmdFromName(def.send.deviceCmd), &errStr);
        else if (route == ModbusDeviceRoute::MultiTempLoggerRtu) {
            QVariantMap tempParam = map;
            SharedInstrument::applyTempLoggerParamsForStation(getIndex(), &tempParam);
            const QString sharedCom = tempParam.value(QStringLiteral("sharedComName")).toString().trimmed();
            if (!sharedCom.isEmpty()) {
                auto* box = qobject_cast<QFreeWorkBox*>(window());
                QString openErr;
                const int tempDeviceIndex = tempParam.value(QStringLiteral("tempDeviceIndex"), 0).toInt();
                const int baud = tempParam.value(QStringLiteral("tempBaudRate"), 115200).toInt();
                const auto rtsMode = SharedInstrument::tempRtsModeFromParam(tempParam);
                SerialChannel* sharedCh = box ? box->ensureSharedTempLoggerChannel(tempDeviceIndex, sharedCom, &openErr,
                                                                                    baud, rtsMode)
                                              : nullptr;
                if (!sharedCh) {
                    showlog(QStringLiteral("共享温度仪串口打开失败（继续）：%1").arg(openErr));
                } else {
                    QMutexLocker locker(box->sharedTempLoggerMutex(tempDeviceIndex));
                    const QByteArray request = MultiTempLoggerModbusRtu().buildRequest(
                        static_cast<int>(multiTempLoggerRtuCmdFromName(def.send.deviceCmd)), tempParam);
                    QByteArray reply;
                    if (request.isEmpty() || !sharedCh->exchangeCollect(request, &reply, 2000)) {
                        showlog(QStringLiteral("Modbus 温度仪采样收发失败（继续）"));
                    } else {
                        double celsius = 0.0;
                        QString valueText;
                        const int slaveAddr = tempParam.value(QStringLiteral("slaveAddr"), 1).toInt();
                        if (MultiTempLoggerModbusRtu::parseTemperatureFrame(reply, &celsius, &valueText, slaveAddr,
                                                                            request)) {
                            ProtocolMeasureData measureData;
                            measureData.deviceName = QStringLiteral("MultiTempLogger");
                            measureData.channel = QStringLiteral("CH%1").arg(
                                tempParam.value(QStringLiteral("channel"), 1).toInt());
                            measureData.type = QStringLiteral("Temperature");
                            measureData.value = celsius;
                            measureData.valueText = valueText;
                            measureData.unit = QStringLiteral("C");
                            measureData.isOk = true;
                            onUsbInstrumentReport(ProtocolReport(QStringLiteral("ProtocolMeasureData"),
                                                                 QVariant::fromValue(measureData)));
                        }
                    }
                    ok = true;
                }
            } else {
                ok = modbusManager.exec(multiTempLoggerRtuCmdFromName(def.send.deviceCmd), tempParam, &errStr);
            }
        }
        else
            ok = false;
        if (!ok)
            showlog(QStringLiteral("Modbus 电流表采样下发失败（继续）：%1").arg(errStr));
        if (isActiveTestCaseStepDone())
            break;
        const int remain = durationMs - static_cast<int>(sampleTimer.elapsed());
        if (remain <= 0)
            break;
        waitWork(qMin(intervalMs, remain));
    }

    currentSampleAnyMatchActive_ = false;
    if (isActiveTestCaseStepDone())
        return;

    const QString failData =
        currentSampleLastValueText_.isEmpty() ? QStringLiteral("无有效读数") : currentSampleLastValueText_;
    showlog(QStringLiteral("Modbus 电流表采样结束：共 %1 次，无一值落入卡控范围，判定失败（末次 %2）")
                .arg(currentSampleCount_)
                .arg(failData));
    markActiveTestCaseStepDone(false, failData, QStringLiteral("失败"));
}

void QFreeWork::runMultiTempLoggerChannelsWindowAllMatch(const TestCaseDefinition& def) {
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    QString shareDetail;
    SharedInstrument::applyTempLoggerParamsForStation(getIndex(), &map, &shareDetail);
    if (!shareDetail.isEmpty())
        showlog(QStringLiteral("共享温度仪：%1").arg(shareDetail));

    QVector<int> channels = SharedInstrument::tempChannelListForStation(getIndex(), map);
    if (channels.isEmpty())
        channels = {qMax(1, map.value(QStringLiteral("channel"), 1).toInt())};

    const int durationMs = qMax(1000, currentSampleDurationMs(def, map));
    const int intervalMs = qMax(100, currentSampleIntervalMs(map));
    const int perReadMs = qMax(500, TestCaseRunner::commandTimeoutMs(def));
    int readTimeoutMs = map.value(QStringLiteral("tempReadTimeoutMs"), 0).toInt();
    if (readTimeoutMs <= 0)
        readTimeoutMs = qBound(400, qMin(perReadMs, 1200), 2000);
    const int interReadDelayMs = qMax(0, map.value(QStringLiteral("tempInterReadDelayMs"), 80).toInt());
    const bool logHexEveryRead =
        map.value(QStringLiteral("tempLogHex"), true).toString().trimmed().compare(QLatin1String("false"),
                                                                                     Qt::CaseInsensitive)
        != 0;

    double lowC = def.gate.low;
    double highC = def.gate.high;
    if (map.contains(QStringLiteral("tempLowC")))
        lowC = map.value(QStringLiteral("tempLowC")).toDouble();
    if (map.contains(QStringLiteral("tempHighC")))
        highC = map.value(QStringLiteral("tempHighC")).toDouble();
    if (highC < lowC)
        qSwap(lowC, highC);

    // any：同轮任一路落入范围即过；all（默认）：同轮全部通道落入范围才过
    const QString passMode =
        map.value(QStringLiteral("tempPassMode"), QStringLiteral("all")).toString().trimmed().toLower();
    const bool anyChannelPass = (passMode == QLatin1String("any") || passMode == QLatin1String("one"));

    QStringList chNames;
    for (int ch : channels)
        chNames.append(QStringLiteral("CH%1").arg(ch));
    showlog(QStringLiteral("多路温度卡控：通道[%1]，窗口 %2ms，间隔 %3ms，单通道超时 %4ms，%5 [%6,%7]℃")
                .arg(chNames.join(QLatin1Char(',')))
                .arg(durationMs)
                .arg(intervalMs)
                .arg(readTimeoutMs)
                .arg(anyChannelPass ? QStringLiteral("任一路落入") : QStringLiteral("要求同轮全部落在"))
                .arg(lowC, 0, 'f', 1)
                .arg(highC, 0, 'f', 1));

    const QString sharedCom = map.value(QStringLiteral("sharedComName")).toString().trimmed();
    const int tempDeviceIndex = map.value(QStringLiteral("tempDeviceIndex"), 0).toInt();
    const int baud = map.value(QStringLiteral("tempBaudRate"), 115200).toInt();
    const auto rtsMode = SharedInstrument::tempRtsModeFromParam(map);
    const int slaveAddr = map.contains(QStringLiteral("slaveAddr"))
                              ? map.value(QStringLiteral("slaveAddr")).toInt()
                              : map.value(QStringLiteral("addr"), 1).toInt();

    auto* box = qobject_cast<QFreeWorkBox*>(window());
    SerialChannel* sharedCh = nullptr;
    if (!sharedCom.isEmpty()) {
        QString openErr;
        sharedCh = box ? box->ensureSharedTempLoggerChannel(tempDeviceIndex, sharedCom, &openErr, baud, rtsMode)
                       : nullptr;
        if (!sharedCh) {
            showlog(QStringLiteral("共享温度仪串口打开失败：%1").arg(openErr));
            markActiveTestCaseStepDone(false, QStringLiteral("共享串口失败"), QStringLiteral("失败"));
            return;
        }
        showlog(QStringLiteral("温度仪串口 %1 波特率 %2 RTS=%3（协议默认 RS232；RS485 转换器步骤填 Param_tempRtsMode=rs485）")
                    .arg(sharedCom)
                    .arg(baud)
                    .arg(SharedInstrument::tempRtsModeLabel(rtsMode)));
    } else {
        modbusManager.setDeviceRoute(ModbusDeviceRoute::MultiTempLoggerRtu);
    }

    auto readOne = [&](int channel, int sampleRound, double* outC, QString* errOut) -> bool {
        QVariantMap one = map;
        one.insert(QStringLiteral("channel"), channel);
        one.insert(QStringLiteral("slaveAddr"), slaveAddr);
        const QByteArray request =
            MultiTempLoggerModbusRtu().buildRequest(static_cast<int>(MultiTempLoggerRtuCmd::ReadChannelTemp), one);
        if (request.isEmpty()) {
            if (errOut)
                *errOut = QStringLiteral("组帧失败");
            return false;
        }
        const QString txHex = QString::fromLatin1(request.toHex(' ').toUpper());
        QByteArray reply;
        auto doExchange = [&](SerialChannel* ch) -> bool {
            return ch->exchangeCollect(request, &reply, readTimeoutMs);
        };
        if (sharedCh) {
            QMutexLocker locker(box->sharedTempLoggerMutex(tempDeviceIndex));
            if (!doExchange(sharedCh)) {
                if (errOut)
                    *errOut = QStringLiteral("收发超时");
                if (logHexEveryRead) {
                    const QString rxHex = reply.isEmpty() ? QStringLiteral("(空)")
                                                          : QString::fromLatin1(reply.toHex(' ').toUpper());
                    showlog(QStringLiteral("多路温度#%1 CH%2 TX=%3 RX=%4 → 收发超时")
                                .arg(sampleRound)
                                .arg(channel)
                                .arg(txHex)
                                .arg(rxHex));
                }
                return false;
            }
        } else {
            if (!modbusManager.serialChannel() || !modbusManager.serialChannel()->isOpen()) {
                if (errOut)
                    *errOut = QStringLiteral("万用表串口未打开");
                return false;
            }
            if (!doExchange(modbusManager.serialChannel())) {
                if (errOut)
                    *errOut = QStringLiteral("收发超时");
                if (logHexEveryRead) {
                    const QString rxHex = reply.isEmpty() ? QStringLiteral("(空)")
                                                          : QString::fromLatin1(reply.toHex(' ').toUpper());
                    showlog(QStringLiteral("多路温度#%1 CH%2 TX=%3 RX=%4 → 收发超时")
                                .arg(sampleRound)
                                .arg(channel)
                                .arg(txHex)
                                .arg(rxHex));
                }
                return false;
            }
        }
        const QString rxHex =
            reply.isEmpty() ? QStringLiteral("(空)") : QString::fromLatin1(reply.toHex(' ').toUpper());
        QByteArray rtuFrame;
        const bool hasRtu =
            QModbusPdu::extractFc03Read2RegsResponse(reply, slaveAddr, &rtuFrame, request);
        const QString rtuHex = hasRtu ? QString::fromLatin1(rtuFrame.toHex(' ').toUpper()) : QStringLiteral("(无)");

        QString valueText;
        if (!MultiTempLoggerModbusRtu::parseTemperatureFrame(reply, outC, &valueText, slaveAddr, request)) {
            if (errOut)
                *errOut = QStringLiteral("解析失败");
            if (logHexEveryRead) {
                showlog(QStringLiteral("多路温度#%1 CH%2 TX=%3 RX=%4 RTU=%5 → 解析失败")
                            .arg(sampleRound)
                            .arg(channel)
                            .arg(txHex)
                            .arg(rxHex)
                            .arg(rtuHex));
            }
            return false;
        }
        if (logHexEveryRead) {
            showlog(QStringLiteral("多路温度#%1 CH%2 TX=%3 RX=%4 RTU=%5 → %6℃")
                        .arg(sampleRound)
                        .arg(channel)
                        .arg(txHex)
                        .arg(rxHex)
                        .arg(rtuHex)
                        .arg(*outC, 0, 'f', 2));
        }
        return true;
    };

    QString lastRoundText;
    int round = 0;
    QElapsedTimer sampleTimer;
    sampleTimer.start();
    while (sampleTimer.elapsed() < durationMs && !isActiveTestCaseStepDone()) {
        if (!isTestContinue) {
            markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            showlog(QStringLiteral("多路温度卡控已中止"));
            return;
        }
        ++round;
        QStringList parts;
        QString firstPassText;
        bool allOk = true;
        bool anyInRange = false;
        bool anyFailRead = false;
        for (int ch : channels) {
            double c = 0.0;
            QString err;
            if (!readOne(ch, round, &c, &err)) {
                parts.append(QStringLiteral("CH%1=读失败(%2)").arg(ch).arg(err));
                allOk = false;
                anyFailRead = true;
                if (interReadDelayMs > 0)
                    QThread::msleep(interReadDelayMs);
                continue;
            }
            const bool inRange = (c >= lowC && c <= highC);
            parts.append(QStringLiteral("CH%1=%2℃%3")
                             .arg(ch)
                             .arg(c, 0, 'f', 2)
                             .arg(inRange ? QString() : QStringLiteral("(超)")));
            if (inRange) {
                anyInRange = true;
                // 任一路模式：结果表只保留最先达标的一路
                if (firstPassText.isEmpty())
                    firstPassText = QStringLiteral("CH%1=%2℃").arg(ch).arg(c, 0, 'f', 2);
            } else {
                allOk = false;
            }
            if (interReadDelayMs > 0)
                QThread::msleep(interReadDelayMs);
        }
        lastRoundText = parts.join(QLatin1Char(' '));
        const bool roundPass = anyChannelPass ? anyInRange : allOk;
        if (anyChannelPass) {
            showlog(QStringLiteral("多路温度采样#%1：%2")
                        .arg(round)
                        .arg(roundPass ? (firstPassText + QStringLiteral(" → 达标"))
                                       : (anyFailRead ? QStringLiteral("有读失败(继续)")
                                                      : QStringLiteral("均未达标(继续)"))));
        } else {
            showlog(QStringLiteral("多路温度采样#%1：%2 → %3")
                        .arg(round)
                        .arg(lastRoundText)
                        .arg(roundPass ? QStringLiteral("全部达标")
                                       : (anyFailRead ? QStringLiteral("有读失败(继续)")
                                                      : QStringLiteral("未全达标(继续)"))));
        }
        if (roundPass) {
            const QString ask =
                QStringLiteral("[%1,%2]℃").arg(QString::number(lowC, 'f', 1)).arg(QString::number(highC, 'f', 1));
            const QString passData =
                (anyChannelPass && !firstPassText.isEmpty()) ? firstPassText : lastRoundText;
            markActiveTestCaseStepDone(true, passData, ask);
            if (anyChannelPass)
                showlog(QStringLiteral("多路温度卡控通过：%1 落入 %2").arg(passData, ask));
            else
                showlog(QStringLiteral("多路温度卡控通过：%1 点全部落入 %2").arg(channels.size()).arg(ask));
            return;
        }
        const int remain = durationMs - static_cast<int>(sampleTimer.elapsed());
        if (remain <= 0)
            break;
        waitWork(qMin(intervalMs, remain));
    }

    if (isActiveTestCaseStepDone())
        return;
    const QString ask =
        QStringLiteral("[%1,%2]℃").arg(QString::number(lowC, 'f', 1)).arg(QString::number(highC, 'f', 1));
    const QString failData = lastRoundText.isEmpty() ? QStringLiteral("无有效读数") : lastRoundText;
    if (anyChannelPass) {
        showlog(QStringLiteral("多路温度卡控失败：窗口 %1ms 内无一通道落入 %2；末轮 %3")
                    .arg(durationMs)
                    .arg(ask)
                    .arg(failData));
    } else {
        showlog(QStringLiteral("多路温度卡控失败：窗口 %1ms 内未出现「全部通道」同时落入 %2；末轮 %3")
                    .arg(durationMs)
                    .arg(ask)
                    .arg(failData));
    }
    markActiveTestCaseStepDone(false, failData, ask);
}

void QFreeWork::runXwdFixtureCurrentSampleAnyMatch(const TestCaseDefinition& def, const QByteArray& request,
                                                   int readChannel, bool dualFixture, int perReadTimeoutMs) {
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    const int durationMs = qMax(1000, currentSampleDurationMs(def, map));
    const int intervalMs = currentSampleIntervalMs(map);
    const int timeoutMs = qMax(2000, perReadTimeoutMs);

    currentSampleAnyMatchActive_ = true;
    currentSampleCount_ = 0;
    currentSampleLastValueText_.clear();
    showlog(QStringLiteral("XWD 治具读电流连续采样：窗口 %1ms，间隔 %2ms，单次超时 %3ms，期间任一值卡控合格即通过")
                .arg(durationMs)
                .arg(intervalMs)
                .arg(timeoutMs));

    QElapsedTimer sampleTimer;
    sampleTimer.start();
    while (sampleTimer.elapsed() < durationMs && !isActiveTestCaseStepDone()) {
        if (!isTestContinue) {
            currentSampleAnyMatchActive_ = false;
            markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            showlog(QStringLiteral("XWD 治具读电流连续采样已中止"));
            return;
        }

        QByteArray reply;
        QString receiveError;
        if (!sendAndCollectXwdReadOnceReply(jigSerialChannel_, jigSerialPort, request, readChannel, timeoutMs,
                                            &reply, &receiveError)) {
            showlog(receiveError + QStringLiteral("（继续）"));
        } else {
            if (reply.isEmpty()) {
                showlog(QStringLiteral("XWD治具采样：本次无回包（继续）"));
            } else {
                const QString replyText = QString::fromUtf8(reply).trimmed();
                double ch1Ma = 0;
                double ch2Ma = 0;
                bool hasCh1 = false;
                bool hasCh2 = false;
                if (XwdRawUartCodec::parseReadOnceReply(replyText, &ch1Ma, &ch2Ma, &hasCh1, &hasCh2)) {
                    const bool channelOk = (readChannel == 2) ? hasCh2 : hasCh1;
                    const double valueMa = (readChannel == 2) ? ch2Ma : ch1Ma;
                    if (dualFixture)
                        showlog(QStringLiteral("XWD治具电流：CH1=%1mA CH2=%2mA，本工位取 CH%3")
                                    .arg(hasCh1 ? QString::number(ch1Ma, 'f', 2) : QStringLiteral("-"))
                                    .arg(hasCh2 ? QString::number(ch2Ma, 'f', 2) : QStringLiteral("-"))
                                    .arg(readChannel));
                    else
                        showlog(QStringLiteral("XWD治具电流：CH1=%1mA").arg(ch1Ma, 0, 'f', 2));
                    if (channelOk) {
                        ProtocolMeasureData measureData;
                        measureData.deviceName = QStringLiteral("XWD");
                        measureData.channel = QStringLiteral("CH%1").arg(readChannel);
                        measureData.type = QStringLiteral("Current");
                        measureData.value = valueMa;
                        measureData.valueText = QString::number(valueMa, 'f', 2);
                        measureData.unit = QStringLiteral("mA");
                        measureData.isOk = true;
                        onUsbInstrumentReport(ProtocolReport(QStringLiteral("ProtocolMeasureData"),
                                                             QVariant::fromValue(measureData)));
                    } else {
                        showlog(QStringLiteral("XWD治具采样：缺 CH%1（继续）").arg(readChannel));
                    }
                } else {
                    showlog(QStringLiteral("XWD治具采样：回包无法解析（继续）：%1").arg(replyText));
                }
            }
        }

        if (isActiveTestCaseStepDone())
            break;
        const int remain = durationMs - static_cast<int>(sampleTimer.elapsed());
        if (remain <= 0)
            break;
        waitWork(qMin(intervalMs, remain));
    }

    currentSampleAnyMatchActive_ = false;
    if (isActiveTestCaseStepDone())
        return;

    const QString failData =
        currentSampleLastValueText_.isEmpty() ? QStringLiteral("无有效读数") : currentSampleLastValueText_;
    showlog(QStringLiteral("XWD 治具读电流采样结束：共 %1 次，无一值落入卡控范围，判定失败（末次 %2）")
                .arg(currentSampleCount_)
                .arg(failData));
    markActiveTestCaseStepDone(false, failData, QStringLiteral("失败"));
}

void QFreeWork::markActiveTestCaseStepDone(bool pass, const QString& testData, const QString& ask) {
    testCaseStepResult_.done = true;
    testCaseStepResult_.pass = pass;
    testCaseStepResult_.testData = testData;
    onTestCaseStepMarkedDone(pass, testData, ask);
}

void TestCaseRunner::beginStep(QFreeWork* ctx, const TestCaseDefinition& def) {
    if (!ctx)
        return;
    ctx->setActiveTestCase(def);
    ctx->showlog(QStringLiteral("执行 case：%1").arg(stepLabel(def)));
    if (def.timing.delayBeforeMs > 0)
        ctx->waitWork(def.timing.delayBeforeMs);

    if (def.hook.enabled) {
        TestCaseHookRegistry::invoke(def.hook.hookId, ctx);
        return;
    }

    // 纯空白提醒 / 仅等 Gate 上报（如按键）：不执行 Send
    if (def.meta.promptOnly) {
        if (def.gate.enabled) {
            const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
            const QString expectHint = def.gate.expected.trimmed().isEmpty()
                                           ? def.gate.field
                                           : QStringLiteral("%1=%2").arg(def.gate.field, def.gate.expected.trimmed());
            ctx->showlog(QStringLiteral("本步提示并等待协议上报卡控（%1，超时 %2ms，不发送指令）")
                             .arg(expectHint)
                             .arg(timeoutMs));
            QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                    return;
                ctx->showlog(QStringLiteral("等待上报超时：%1").arg(TestCaseRunner::stepLabel(def)));
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时未收到匹配上报"), QStringLiteral("失败"));
            });
        } else {
            ctx->showlog(QStringLiteral("本步为提示，确认后继续（不发送指令）"));
        }
        return;
    }

    if (def.send.channel == TestCaseSendChannel::Product && !ctx->at->getConnected() && !TestCaseRunner::stepRequiresProductBle(def)) {
        ctx->showlog(QStringLiteral("本步不要求蓝牙连接，已跳过产品协议"));
        // 无卡控提示步此时已点过「是」才进入 beginStep；有卡控则弹窗与发送同时，跳过协议后直接过步
        ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
        return;
    }

    if (def.send.channel == TestCaseSendChannel::Fixture) {
        if (def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera) {
            ctx->runScreenInspectStep();
            return;
        }
        if (def.send.fixtureProtocol == TestCaseFixtureProtocol::VesLight) {
            ctx->executeFixtureVesLightCase(def);
            return;
        }
        if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Asd9026a)
            ctx->executeFixtureAsd9026aCase(def);
        else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Xwd)
            ctx->executeFixtureXwdCase(def);
        else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox)
            ctx->executeFixtureJieliBtBoxCase(def);
        else
            ctx->executeFixturePcbaCase(def);
        return;
    }

    if (def.send.channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (!TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd)) {
            ctx->showlog(QStringLiteral("未知云端指令：%1").arg(def.send.deviceCmd));
            ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        if (!TupleCmdCatalog::isCmdForAction(tupleCmd, def.send.action)) {
            ctx->showlog(QStringLiteral("云端指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
            ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        ctx->executeCloudTupleCase(def);
        return;
    }

    if (def.send.channel == TestCaseSendChannel::ProductSerial) {
        ctx->executeProductSerialCase(def);
        return;
    }

    if (def.send.channel == TestCaseSendChannel::Modbus) {
        const QString deviceKey = def.send.device;
        ModbusDeviceRoute devRoute = ModbusPeriphCmdCatalog::deviceFromIni(deviceKey);
        ctx->modbusManager.setDeviceRoute(devRoute);

        const QVariant resolvedParam = ctx->resolveTestCaseSendParamTree(def.send.param);
        QString errStr;

        if (devRoute == ModbusDeviceRoute::InovanceH5uTcp) {
            PlcCmd plcCmd = plcCmdFromName(def.send.deviceCmd);
            // 1拖2：WriteCoil 可用 mLeft/mRight，按工位选一侧写入 m
            QVariant plcParam = resolvedParam;
            if (plcCmd == PlcCmd::WriteCoil && resolvedParam.canConvert<QVariantMap>()) {
                QVariantMap map = resolvedParam.toMap();
                if (map.contains(QStringLiteral("mLeft")) && map.contains(QStringLiteral("mRight"))) {
                    const int m = (ctx->getIndex() <= 1) ? map.value(QStringLiteral("mLeft")).toInt()
                                                        : map.value(QStringLiteral("mRight")).toInt();
                    map.insert(QStringLiteral("m"), m);
                    plcParam = map;
                    ctx->showlog(QStringLiteral("PLC WriteCoil 工位%1 选用 M%2")
                                     .arg(ctx->getIndex())
                                     .arg(m));
                }
            }
            QVariant resultVal;
            bool ok = ctx->modbusManager.exec(plcCmd, plcParam, &resultVal, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("PLC 指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else {
                if (def.send.action == TestCaseSendAction::Get) {
                    ProtocolMeasureData measureData;
                    measureData.deviceName = deviceKey;
                    measureData.type = QStringLiteral("PLC_Register");
                    measureData.value = resultVal.toDouble();
                    measureData.valueText = resultVal.toString();
                    measureData.unit = QStringLiteral("");
                    measureData.isOk = true;
                    ctx->onUsbInstrumentReport(ProtocolReport(QStringLiteral("ProtocolMeasureData"),
                                                              QVariant::fromValue(measureData)));
                } else {
                    ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
                }
            }
        } else if (devRoute == ModbusDeviceRoute::GcSeriesTcp) {
            GcPlcCmd gcCmd = gcPlcCmdFromName(def.send.deviceCmd);
            QVariant gcParam = resolvedParam;
            if (gcCmd == GcPlcCmd::WriteCoil && resolvedParam.canConvert<QVariantMap>()) {
                QVariantMap map = resolvedParam.toMap();
                if (map.contains(QStringLiteral("mLeft")) && map.contains(QStringLiteral("mRight"))) {
                    const int m = (ctx->getIndex() <= 1) ? map.value(QStringLiteral("mLeft")).toInt()
                                                        : map.value(QStringLiteral("mRight")).toInt();
                    map.insert(QStringLiteral("m"), m);
                    gcParam = map;
                    ctx->showlog(QStringLiteral("GC WriteCoil 工位%1 选用 M%2")
                                     .arg(ctx->getIndex())
                                     .arg(m));
                }
            }
            QVariant resultVal;
            bool ok = ctx->modbusManager.exec(gcCmd, gcParam, &resultVal, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("GC PLC 指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else {
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
            }
        } else if (devRoute == ModbusDeviceRoute::XinjiePlcRtu) {
            XinjePlcCmd xjCmd = xinjiePlcCmdFromName(def.send.deviceCmd);
            QVariant resultVal;
            bool ok = ctx->modbusManager.exec(xjCmd, resolvedParam, &resultVal, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("信捷 PLC 指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else if (def.send.action == TestCaseSendAction::Get) {
                ProtocolMeasureData measureData;
                measureData.deviceName = deviceKey;
                measureData.type = QStringLiteral("XinjiePlc");
                measureData.valueText = resultVal.toString();
                measureData.value = resultVal.toDouble();
                measureData.isOk = true;
                ctx->onUsbInstrumentReport(ProtocolReport(QStringLiteral("ProtocolMeasureData"),
                                                          QVariant::fromValue(measureData)));
            } else {
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
            }
        } else if (devRoute == ModbusDeviceRoute::HqAmmeterRtu) {
            if (def.send.action == TestCaseSendAction::Get && def.gate.enabled) {
                ctx->runModbusAmmeterCurrentSampleAnyMatch(def, devRoute);
                return;
            }
            HqAmmeterRtuCmd cmd = hqAmmeterRtuCmdFromName(def.send.deviceCmd);
            bool ok = ctx->modbusManager.exec(cmd, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("HQ 电流表指令 [%1] 下发失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else if (def.send.action == TestCaseSendAction::Get) {
                const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                    if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                        return;
                    ctx->showlog(QStringLiteral("HQ 电流表等待超时：%1").arg(def.send.deviceCmd));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
                });
                ctx->showlog(QStringLiteral("等待 HQ 电流表回包：%1（超时 %2ms）").arg(def.send.deviceCmd).arg(timeoutMs));
            } else {
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
            }
        } else if (devRoute == ModbusDeviceRoute::LxAmmeterRtu) {
            if (def.send.action == TestCaseSendAction::Get && def.gate.enabled) {
                ctx->runModbusAmmeterCurrentSampleAnyMatch(def, devRoute);
                return;
            }
            LxAmmeterRtuCmd cmd = lxAmmeterRtuCmdFromName(def.send.deviceCmd);
            bool ok = ctx->modbusManager.exec(cmd, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("LX 电流表指令 [%1] 下发失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else if (def.send.action == TestCaseSendAction::Get) {
                const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                    if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                        return;
                    ctx->showlog(QStringLiteral("LX 电流表等待超时：%1").arg(def.send.deviceCmd));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
                });
                ctx->showlog(QStringLiteral("等待 LX 电流表回包：%1（超时 %2ms）").arg(def.send.deviceCmd).arg(timeoutMs));
            } else {
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
            }
        } else if (devRoute == ModbusDeviceRoute::MultiTempLoggerRtu) {
            QVariantMap tempParam =
                resolvedParam.canConvert<QVariantMap>() ? resolvedParam.toMap() : QVariantMap{};
            // 多通道窗口卡控（法兰加热 6 点）：channelsPerStation>1 或显式 channels 列表
            const int channelsPerStation = SharedInstrument::channelsPerStationFromParam(tempParam);
            const QVector<int> channelList =
                SharedInstrument::tempChannelListForStation(ctx->getIndex(), tempParam);
            if (def.send.action == TestCaseSendAction::Get && def.gate.enabled
                && def.send.deviceCmd.compare(QLatin1String("SendRaw"), Qt::CaseInsensitive) != 0
                && (channelsPerStation > 1 || channelList.size() > 1)) {
                ctx->runMultiTempLoggerChannelsWindowAllMatch(def);
                return;
            }

            QString shareDetail;
            if (SharedInstrument::applyTempLoggerParamsForStation(ctx->getIndex(), &tempParam, &shareDetail))
                ctx->showlog(QStringLiteral("共享温度仪：%1").arg(shareDetail));

            const QString sharedCom = tempParam.value(QStringLiteral("sharedComName")).toString().trimmed();
            const int tempDeviceIndex = tempParam.value(QStringLiteral("tempDeviceIndex"), 0).toInt();
            if (!sharedCom.isEmpty()) {
                // 共享串口：同步 exchange，避免两工位异步抢 RX
                auto* box = qobject_cast<QFreeWorkBox*>(ctx->window());
                QString openErr;
                const int baud = tempParam.value(QStringLiteral("tempBaudRate"), 115200).toInt();
                const auto rtsMode = SharedInstrument::tempRtsModeFromParam(tempParam);
                SerialChannel* sharedCh = box ? box->ensureSharedTempLoggerChannel(tempDeviceIndex, sharedCom, &openErr,
                                                                                    baud, rtsMode)
                                              : nullptr;
                if (!sharedCh) {
                    ctx->showlog(QStringLiteral("共享温度仪串口打开失败：%1").arg(openErr));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("共享串口失败"), QStringLiteral("失败"));
                    return;
                }
                QMutexLocker locker(box->sharedTempLoggerMutex(tempDeviceIndex));
                MultiTempLoggerRtuCmd cmd = multiTempLoggerRtuCmdFromName(def.send.deviceCmd);
                const QByteArray request =
                    MultiTempLoggerModbusRtu().buildRequest(static_cast<int>(cmd), tempParam);
                if (request.isEmpty()) {
                    ctx->showlog(QStringLiteral("温度记录仪组帧失败：%1").arg(def.send.deviceCmd));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("组帧失败"), QStringLiteral("失败"));
                    return;
                }
                const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                QByteArray reply;
                if (!sharedCh->exchangeCollect(request, &reply, timeoutMs)) {
                    ctx->showlog(QStringLiteral("温度记录仪共享串口收发超时/失败：%1 TX=%2 RX=%3")
                                     .arg(def.send.deviceCmd)
                                     .arg(QString::fromLatin1(request.toHex(' ').toUpper()))
                                     .arg(reply.isEmpty() ? QStringLiteral("(空)")
                                                          : QString::fromLatin1(reply.toHex(' ').toUpper())));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("收发失败"), QStringLiteral("失败"));
                    return;
                }
                if (def.send.action == TestCaseSendAction::Get
                    && def.send.deviceCmd.compare(QLatin1String("SendRaw"), Qt::CaseInsensitive) != 0) {
                    double celsius = 0.0;
                    QString valueText;
                    const int slaveAddr = tempParam.value(QStringLiteral("slaveAddr"), 1).toInt();
                    if (!MultiTempLoggerModbusRtu::parseTemperatureFrame(reply, &celsius, &valueText, slaveAddr,
                                                                         request)) {
                        ctx->showlog(QStringLiteral("温度记录仪回包解析失败"));
                        ctx->markActiveTestCaseStepDone(false, QStringLiteral("解析失败"), QStringLiteral("失败"));
                        return;
                    }
                    ProtocolMeasureData measureData;
                    measureData.deviceName = QStringLiteral("MultiTempLogger");
                    measureData.channel =
                        QStringLiteral("CH%1").arg(tempParam.value(QStringLiteral("channel"), 1).toInt());
                    measureData.type = QStringLiteral("Temperature");
                    measureData.value = celsius;
                    measureData.valueText = valueText;
                    measureData.unit = QStringLiteral("C");
                    measureData.isOk = true;
                    ctx->onUsbInstrumentReport(
                        ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(measureData)));
                    if (!def.gate.enabled && !ctx->isActiveTestCaseStepDone())
                        ctx->markActiveTestCaseStepDone(true, valueText, QStringLiteral("通过"));
                } else {
                    ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
                }
                return;
            }

            if (def.send.action == TestCaseSendAction::Get && def.gate.enabled
                && def.send.deviceCmd.compare(QLatin1String("SendRaw"), Qt::CaseInsensitive) != 0) {
                ctx->runModbusAmmeterCurrentSampleAnyMatch(def, devRoute);
                return;
            }
            MultiTempLoggerRtuCmd cmd = multiTempLoggerRtuCmdFromName(def.send.deviceCmd);
            bool ok =
                ctx->modbusManager.exec(cmd, tempParam.isEmpty() ? resolvedParam : QVariant(tempParam), &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("温度记录仪指令 [%1] 下发失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else if (def.send.action == TestCaseSendAction::Get) {
                const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                    if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                        return;
                    ctx->showlog(QStringLiteral("温度记录仪等待超时：%1").arg(def.send.deviceCmd));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
                });
                ctx->showlog(QStringLiteral("等待温度记录仪回包：%1（超时 %2ms）").arg(def.send.deviceCmd).arg(timeoutMs));
            } else {
                // SendRaw 设置：只下发开放报文
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
            }
        } else {
            ctx->showlog(QStringLiteral("未知 Modbus 设备路由: %1").arg(deviceKey));
            ctx->markActiveTestCaseStepDone(false, deviceKey, QStringLiteral("失败"));
        }
        return;
    }

    if (def.send.channel == TestCaseSendChannel::Scpi) {
        const QString deviceKey = def.send.device;
        ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(deviceKey);
        ctx->scpiVisaManager()->setDeviceRoute(devRoute);

        const QVariant resolvedParam = ctx->resolveTestCaseSendParamTree(def.send.param);
        QString errStr;

        if (devRoute == ScpiDeviceRoute::HuilingWfp60h || devRoute == ScpiDeviceRoute::Agilent66319d) {
            const HuilingScpiStepParams stepParams =
                splitHuilingScpiStepParam(resolvedParam, def.send.deviceCmd);
            QVariantMap linkMap = stepParams.linkMap;
            if (linkMap.value(QStringLiteral("visaAddress")).toString().trimmed().isEmpty()) {
                linkMap = ctx->cachedHuilingVisaLink();
            }
            const QVariantMap loadMap = mergeVisaPowerStepParamMap(linkMap, stepParams.commandParam);
            QVariantMap visaLoadMap = loadMap;
            QString shareDetail;
            if (SharedInstrument::applyVisaParamsForStation(ctx->getIndex(), &visaLoadMap, &shareDetail)) {
                const QString visaAddr = visaLoadMap.value(QStringLiteral("visaAddress")).toString().trimmed();
                if (!visaAddr.isEmpty() && !looksLikeBluetoothMacAddress(visaAddr))
                    ctx->showlog(QStringLiteral("共享程控电源：%1").arg(shareDetail));
                else
                    ctx->showlog(QStringLiteral("共享程控电源：%1（VISA 地址无效，请检查配置Visa程控电源步骤的 visaAddress/visaAddress0）")
                                     .arg(shareDetail));
            }
            const int visaTimeoutMs = TestCaseRunner::commandTimeoutMs(def);
            const QString resolvedVisaAddr = visaLoadMap.value(QStringLiteral("visaAddress")).toString().trimmed();
            if (looksLikeBluetoothMacAddress(resolvedVisaAddr)) {
                ctx->showlog(QStringLiteral("%1：VISA 地址被误解析为蓝牙 MAC（%2），请检查步骤 Param_visaAddress 或 Param_visaAddress0")
                                 .arg(ScpiPeriphCmdCatalog::deviceUiLabel(devRoute), resolvedVisaAddr));
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("visaAddress无效"), QStringLiteral("失败"));
                return;
            }
            // 「配置Visa程控电源」：开局先作废该地址共享句柄，避免上一轮僵死会话占线
            if (def.send.deviceCmd.compare(QLatin1String("ConfigureProgrammablePower"), Qt::CaseInsensitive) == 0
                && !resolvedVisaAddr.isEmpty()) {
                ctx->scpiVisaManager()->closeConnection();
                VisaChannel::discardIdleSharedSession(resolvedVisaAddr);
            }
            const bool visaReady =
                devRoute == ScpiDeviceRoute::Agilent66319d
                    ? ctx->scpiVisaManager()->loadAgilent66319dVisaFromParamMap(visaLoadMap, visaTimeoutMs)
                    : ctx->scpiVisaManager()->loadHuilingVisaFromParamMap(visaLoadMap, visaTimeoutMs);
            if (!visaReady) {
                ctx->showlog(QStringLiteral("%1：请在「配置Visa程控电源」步骤填写 Param_visaAddress，"
                                           "或一拖多共享时填 Param_sharedPair=true 与 Param_visaAddress0/1")
                                 .arg(ScpiPeriphCmdCatalog::deviceUiLabel(devRoute)));
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("visaAddress缺失"), QStringLiteral("失败"));
                return;
            }
            const bool gpibAddr = resolvedVisaAddr.startsWith(QStringLiteral("GPIB"), Qt::CaseInsensitive);
            if (gpibAddr) {
                ctx->suppressProductBleAutoReconnect_ = true;
                ctx->scpiVisaManager()->ensureConnected();
                VisaChannel::waitWork(200);
            }
            if (!stepParams.linkMap.value(QStringLiteral("visaAddress")).toString().trimmed().isEmpty()
                || !visaLoadMap.value(QStringLiteral("visaAddress")).toString().trimmed().isEmpty()) {
                ctx->updateHuilingVisaLinkCache(huilingVisaLinkKeysFromMap(visaLoadMap));
            }
            HuilingScpiCmd cmd = huilingScpiCmdFromName(def.send.deviceCmd);
            // 读电流+卡控：连续采样，期间任一合格即通过（不再单次读数立刻判失败）
            if (cmd == HuilingScpiCmd::ReadProgrammableCurrent && def.gate.enabled) {
                ctx->runScpiProgrammableCurrentSampleAnyMatch(def, stepParams.commandParam);
                return;
            }
            // 底层已有同会话/重开恢复；上层 GPIB 再试 1 次，非 GPIB 保留原重试
            bool ok = ctx->scpiVisaManager()->exec(cmd, stepParams.commandParam, &errStr);
            if (!ok) {
                if (gpibAddr)
                    VisaChannel::waitWork(300);
                else
                    ctx->waitWork(300);
                ok = ctx->scpiVisaManager()->exec(cmd, stepParams.commandParam, &errStr);
            }
            if (!ok && !gpibAddr) {
                ctx->waitWork(500);
                ok = ctx->scpiVisaManager()->exec(cmd, stepParams.commandParam, &errStr);
            }
            if (!ok && !gpibAddr) {
                ctx->waitWork(300);
                ctx->resetVisaBackend();
                ctx->waitWork(500);
                ok = ctx->scpiVisaManager()->exec(cmd, stepParams.commandParam, &errStr);
            }
            if (!ok) {
                ctx->showlog(QStringLiteral("%1 指令 [%2] 执行失败: %3")
                                 .arg(ScpiPeriphCmdCatalog::deviceUiLabel(devRoute), def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else {
                QString testData = QStringLiteral("-");
                const HuilingWfp60hScpiProfile profile = ctx->scpiVisaManager()->huilingActiveProfile();
                if (def.send.deviceCmd == QLatin1String("ProgrammablePowerOutput")) {
                    const bool enable = stepParams.commandParam.toBool();
                    testData = enable ? QStringLiteral("ON") : QStringLiteral("OFF");
                    ctx->showlog(QStringLiteral("程控电源输出%1").arg(enable ? QStringLiteral("已打开") : QStringLiteral("已关闭")));
                } else if (def.send.deviceCmd == QLatin1String("ConfigureProgrammablePower")) {
                    const QVariantMap cmdMap = stepParams.commandParam.toMap();
                    const double voltageV =
                        huilingParamDouble(cmdMap, QStringLiteral("voltage"), profile.scpiPowerVoltageV);
                    const double currentA =
                        huilingParamDouble(cmdMap, QStringLiteral("current"), profile.scpiPowerCurrentA);
                    testData = QStringLiteral("V=%1V,I=%2A").arg(voltageV, 0, 'f', 2).arg(currentA, 0, 'f', 3);
                    ctx->showlog(QStringLiteral("程控电源已配置：%1 V，限流 %2 A").arg(voltageV, 0, 'f', 2).arg(currentA, 0, 'f', 3));
                }
                if (def.send.action == TestCaseSendAction::Get) {
                    if (!def.gate.enabled) {
                        ctx->markActiveTestCaseStepDone(true, testData, QStringLiteral("通过"));
                    } else {
                        const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                        QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                            if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                                return;
                            ctx->showlog(QStringLiteral("SCPI 设备等待超时：%1").arg(def.send.deviceCmd));
                            ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
                        });
                    }
                } else {
                    ctx->markActiveTestCaseStepDone(true, testData, QStringLiteral("通过"));
                }
            }
        } else if (devRoute == ScpiDeviceRoute::RsCmw100) {
            CmwScpiCmd cmd = cmwScpiCmdFromName(def.send.deviceCmd);
            bool ok = ctx->scpiVisaManager()->exec(cmd, resolvedParam, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("CMW100 指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else {
                if (def.send.action == TestCaseSendAction::Get) {
                    QString response = ctx->scpiVisaManager()->lastQueryResponse();
                    ProtocolMeasureData measureData;
                    measureData.deviceName = deviceKey;
                    measureData.type = def.send.deviceCmd;
                    measureData.value = response.toDouble();
                    measureData.valueText = response;
                    measureData.unit = QStringLiteral("");
                    measureData.isOk = true;
                    ctx->onUsbInstrumentReport(ProtocolReport(QStringLiteral("ProtocolMeasureData"),
                                                              QVariant::fromValue(measureData)));
                } else {
                    ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
                }
            }
        } else {
            ctx->showlog(QStringLiteral("未知 SCPI 设备路由: %1").arg(deviceKey));
            ctx->markActiveTestCaseStepDone(false, deviceKey, QStringLiteral("失败"));
        }
        return;
    }

    DongleCmd dongleCmd = DongleCmd::BleScanConnect;
    if (def.send.channel == TestCaseSendChannel::Dongle) {
        if (!DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd)) {
            ctx->showlog(QStringLiteral("未知 Dongle 指令：%1").arg(def.send.deviceCmd));
            ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }

        if (dongleCmd == DongleCmd::BleDisconnect) {
            int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
            if (timeoutMs <= 0)
                timeoutMs = 3000;
            // 禁止 startTask 在后续步骤用 MAC 偷偷 AT+MAC=xxx 拉回连接
            ctx->suppressProductBleAutoReconnect_ = true;
            ctx->showlog(QStringLiteral("主动断开 dongle 与产品蓝牙（AT+MAC=00:00:00:00:00:00），本轮禁止自动重连"));
            ctx->at->set(DongleCmd::BleDisconnect);
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < timeoutMs) {
                if (!ctx->at->getConnected()) {
                    ctx->markActiveTestCaseStepDone(true, QStringLiteral("已断开"), QStringLiteral("通过"));
                    ctx->showlog(QStringLiteral("蓝牙已断开（保持断开，不自动重连）"));
                    return;
                }
                ctx->waitWork(50);
            }
            ctx->at->resetConnected();
            ctx->markActiveTestCaseStepDone(true, QStringLiteral("已下发断开"), QStringLiteral("通过"));
            ctx->showlog(QStringLiteral("已下发断开指令（等待 AT+DISCONNECT 超时，本地已清连接态，本轮禁止自动重连）"));
            return;
        }

        if (dongleCmd == DongleCmd::SampleSuctionDual) {
            ctx->runDongleSuctionSampleStep();
            return;
        }
        if (dongleCmd == DongleCmd::SampleSuctionSingle) {
            ctx->runDongleSuctionSampleSingleStep();
            return;
        }

        // 流程里显式再连时，允许恢复自动重连辅助
        if (dongleCmd == DongleCmd::BleScanConnect || dongleCmd == DongleCmd::BleDirectConnect
            || dongleCmd == DongleCmd::BleScanConnectByName || dongleCmd == DongleCmd::BleOtaConnect
            || dongleCmd == DongleCmd::BleAppConnect || dongleCmd == DongleCmd::BleMainConnect) {
            ctx->suppressProductBleAutoReconnect_ = false;
        }

        if (dongleCmd == DongleCmd::BleScanConnectByName) {
            const QVariant param = ctx->resolveTestCaseSendParamTree(def.send.param);
            QString targetName = QStringLiteral("M5 Ultra");
            int rssiThreshold = -50;

            if (param.canConvert<QVariantMap>()) {
                QVariantMap map = param.toMap();
                if (map.contains(QStringLiteral("name"))) targetName = map.value(QStringLiteral("name")).toString();
                if (map.contains(QStringLiteral("rssi"))) rssiThreshold = map.value(QStringLiteral("rssi")).toInt();
            } else if (param.type() == QVariant::String) {
                const QString s = param.toString().trimmed();
                if (!s.isEmpty()) targetName = s;
            }

            ctx->showlog(QStringLiteral("开始搜索广播名称: '%1', 最低信号要求: %2").arg(targetName).arg(rssiThreshold));

            // 本步开始前再清一次，避免沿用开测前或上一步残留的扫描结果
            ctx->deviceMap.clear();
            if (ctx->ui && ctx->ui->mac_combo)
                ctx->ui->mac_combo->clear();

            int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
            if (timeoutMs <= 0) timeoutMs = 6000;

            QElapsedTimer timer;
            timer.start();
            QString bestMac;
            int bestRssi = -999;

            while (timer.elapsed() < timeoutMs) {
                bestMac.clear();
                bestRssi = -999;

                for (auto it = ctx->deviceMap.begin(); it != ctx->deviceMap.end(); ++it) {
                    const QString deviceAddress = it.key();
                    const QString deviceName = it.value().value(QStringLiteral("Name"));
                    const int deviceRssi = it.value().value(QStringLiteral("Rssi")).toInt();

                    if (deviceName.contains(targetName) && deviceRssi > rssiThreshold && deviceAddress.length() == 17) {
                        if (deviceRssi > bestRssi) {
                            bestRssi = deviceRssi;
                            bestMac = deviceAddress;
                        }
                    }
                }

                if (!bestMac.isEmpty()) {
                    break;
                }
                ctx->waitWork(100);
            }

            if (bestMac.isEmpty()) {
                ctx->stepRuntime_.done = true;
                ctx->stepRuntime_.pass = false;
                ctx->stepRuntime_.testData = QStringLiteral("未找到匹配广播: ") + targetName;
                ctx->TestResult = ctx->failValue;
                ctx->showlog(QStringLiteral("按名称自动连接失败：轮询了 %3 毫秒，依然没扫到名称包含 %1 且信号大于 %2 的设备")
                                .arg(targetName)
                                .arg(rssiThreshold)
                                .arg(timeoutMs));
                return;
            }

            ctx->showlog(QStringLiteral("按广播名称找到最佳设备，MAC: %1, 信号: %2, 发起连接...")
                            .arg(bestMac)
                            .arg(bestRssi));

            ctx->stepRuntime_.testData = bestMac;
            ctx->macAddress = bestMac;
            if (ctx->ui && ctx->ui->macInput)
                ctx->ui->macInput->setText(bestMac);
            const auto sendFn = [ctx, bestMac]() {
                ctx->at->set(DongleCmd::BleScanConnect, bestMac);
                if (ctx->ui && ctx->ui->mac_combo) {
                    ctx->ui->mac_combo->setCurrentText(bestMac);
                }
            };
            // 连接前清 RX / 本地连接态，避免单步复用脏缓冲或误判已连接
            if (ctx->dongleSerialChannel_)
                ctx->dongleSerialChannel_->clearReceiveBuffer();
            if (ctx->at)
                ctx->at->resetConnected();
            ctx->setCommandWaitSource(CommandWaitSource::DongleAt);
            const int bleTimeoutMs = qMax(timeoutMs > 0 ? timeoutMs : 18000, 18000);
            // allowResend=false：扫连后只发一次连接指令
            ctx->sendCommandWithRetry(sendFn, bleTimeoutMs, false);
            return;
        }

        if (isDongleBleConnectCmd(dongleCmd)) {
            if (!ctx->dongleSerialPort || !ctx->dongleSerialPort->isOpen()) {
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("Dongle串口未打开"), QStringLiteral("失败"));
                ctx->showlog(QStringLiteral("蓝牙连接失败：请先打开 Dongle 串口后再单步/开测"));
                return;
            }
            QVariant param = ctx->resolveTestCaseSendParamTree(def.send.param);
            QString mac = param.toString().trimmed();
            if (mac.isEmpty() && param.canConvert<QVariantMap>())
                mac = param.toMap().value(QStringLiteral("string")).toString().trimmed();
            if (mac.isEmpty() || mac == QStringLiteral("没有mac地址"))
                mac = ctx->currentMacAddress();
            if (mac.isEmpty() || mac == QStringLiteral("没有mac地址")) {
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("MAC为空"), QStringLiteral("失败"));
                ctx->showlog(QStringLiteral("蓝牙连接失败：MAC 为空，请在界面填写/选择 MAC，或步骤 Param 填写目标地址"));
                return;
            }
            ctx->macAddress = mac;
            if (ctx->ui && ctx->ui->macInput && ctx->ui->macInput->text().trimmed().isEmpty())
                ctx->ui->macInput->setText(mac);
            // 连接前清 RX / 本地连接态，避免单步时误判已连接或 AT 应答错乱
            if (ctx->dongleSerialChannel_)
                ctx->dongleSerialChannel_->clearReceiveBuffer();
            if (ctx->at)
                ctx->at->resetConnected();
            ctx->showlog(QStringLiteral("发起蓝牙连接：%1").arg(mac));
            const auto sendFn = [ctx, dongleCmd, mac]() { ctx->at->set(dongleCmd, mac); };
            int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
            timeoutMs = qMax(timeoutMs > 0 ? timeoutMs : 18000, 18000);
            ctx->setCommandWaitSource(CommandWaitSource::DongleAt);
            // allowResend=false：连接过程只发一次，避免窗口内重发打断 Dongle；结案靠连接态/needAsyncDone
            ctx->sendCommandWithRetry(sendFn, timeoutMs, false);
            return;
        }

        const auto sendFn = [ctx, def, dongleCmd]() {
            QVariant param = ctx->resolveTestCaseSendParamTree(def.send.param);
            if (def.send.action == TestCaseSendAction::Get)
                ctx->at->get(dongleCmd, param);
            else
                ctx->at->set(dongleCmd, param);
        };
        // AT+BLEMTU / AT+HSADC / AT+BOMB / AT+GMAC 等 dongle 侧无应答解析：
        // Timing/WaitReply=false 时只发不收，发完即过步；此时没有回包可判失败，故先卡串口是否已打开
        if (!def.timing.waitReply) {
            if (!ctx->dongleSerialPort || !ctx->dongleSerialPort->isOpen()) {
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("Dongle串口未打开"), QStringLiteral("失败"));
                ctx->showlog(QStringLiteral("发送失败：请先打开 Dongle 串口后再单步/开测"));
                return;
            }
            sendFn();
            ctx->canGoNext = true;
            ctx->sendRetryOver = false;
            ctx->lastCommandRetryCount = 1;
            ctx->lastCommandFailReason.clear();
            ctx->showlog(QStringLiteral("已发送（不等待回包）"));
            return;
        }
        int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
        if (timeoutMs <= 0)
            timeoutMs = 3000;
        ctx->setCommandWaitSource(CommandWaitSource::DongleAt);
        ctx->sendCommandWithRetry(sendFn, timeoutMs, true);
        return;
    }

    ctx->applyTestCaseProductProtocol(def.send.productProtocol);

    DeviceCmd cmd = DeviceCmd::FacMode;
    if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
        ctx->showlog(QStringLiteral("未知指令：%1").arg(def.send.deviceCmd));
        ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    const QVariant resolvedParam = ctx->resolveTestCaseSendParamTree(def.send.param);
    const QVariant wireParam = DeviceCmdCatalog::normalizeSendParam(cmd, resolvedParam);
    if (def.send.action == TestCaseSendAction::Set && !ctx->prepareTupleProductWriteForTestCase(def, cmd, wireParam)) {
        ctx->markActiveTestCaseStepDone(false, ctx->activeTestCaseStepTestData(), QStringLiteral("失败"));
        return;
    }
    if (def.send.action == TestCaseSendAction::Set && !ctx->prepareTailSnWriteForTestCase(def, cmd, wireParam)) {
        ctx->markActiveTestCaseStepDone(false, ctx->activeTestCaseStepTestData(), QStringLiteral("失败"));
        ctx->TestResult = ctx->failValue;
        return;
    }

    const auto sendFn = [ctx, def, cmd, wireParam]() {
        if (def.send.action == TestCaseSendAction::Get)
            ctx->protocolManager.get(cmd, wireParam);
        else
            ctx->protocolManager.set(cmd, wireParam);
    };

    // 进蓝牙非信令等会关机：Timing/WaitReply=false 时只发不收，发完即过步
    if (!def.timing.waitReply) {
        sendFn();
        ctx->canGoNext = true;
        ctx->sendRetryOver = false;
        ctx->lastCommandRetryCount = 1;
        ctx->lastCommandFailReason.clear();
        ctx->showlog(QStringLiteral("已发送（不等待回包）"));
        return;
    }

    const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    ctx->setCommandWaitSource(CommandWaitSource::ProductProtocol);
    ctx->sendCommandWithRetry(sendFn, timeoutMs);
}

void QFreeWork::executeFixturePcbaCase(const TestCaseDefinition& def) {
    if (def.send.fixtureProtocol != TestCaseFixtureProtocol::Pcba) {
        showlog(QStringLiteral("治具协议类型不匹配，请检查 Send/Protocol"));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    FixturePcbaCmd cmd;
    if (!FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, cmd)) {
        showlog(QStringLiteral("未知治具 PCBA 指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!FixturePcbaCmdCatalog::isCmdForAction(cmd, def.send.action)) {
        showlog(QStringLiteral("治具指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    auto* box = qobject_cast<QFreeWorkBox*>(window());
    QString fixtureConnectDetail;
    bool fixtureAutoConnected = false;
    Fixture_uart* uart =
        box ? box->ensureFixtureUartConnected(getIndex(), &fixtureConnectDetail, &fixtureAutoConnected) : nullptr;
    if (!uart || !uart->isFixtureSerialOpen()) {
        const QString msg = fixtureConnectDetail.isEmpty()
            ? QStringLiteral("治具串口未连接，且无法自动连接（请检查配置或菜单「连接治具串口」）")
            : fixtureConnectDetail;
        showlog(msg);
        markActiveTestCaseStepDone(false, QStringLiteral("治具未连接"), QStringLiteral("失败"));
        return;
    }
    if (fixtureAutoConnected)
        showlog(QStringLiteral("已自动连接治具串口：%1").arg(fixtureConnectDetail));

    const int machineIndex = resolveFixtureMachineIndex(def.send.param);

    if (def.send.action == TestCaseSendAction::Set) {
        QByteArray frame;
        switch (cmd) {
        case FixturePcbaCmd::StartTest:
            frame = FixturePcbaUartProtocol::buildStartTestCommand(machineIndex);
            break;
        case FixturePcbaCmd::StartSleep:
            frame = FixturePcbaUartProtocol::buildSleepCommand(machineIndex);
            break;
        case FixturePcbaCmd::StartWhiteMode:
            frame = FixturePcbaUartProtocol::buildWhiteModeCommand(machineIndex);
            break;
        default:
            markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        if (frame.isEmpty()) {
            showlog(QStringLiteral("治具 PCBA 组包失败：机号 %1（有效范围 1~15）").arg(machineIndex));
            markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        uart->sendPcbaFrame(frame);
        showlog(QStringLiteral("已发送治具 PCBA：%1，机号 %2，帧 %3")
                    .arg(FixturePcbaCmdCatalog::fixturePcbaCmdUiLabel(def.send.deviceCmd))
                    .arg(machineIndex)
                    .arg(QString::fromLatin1(frame.toHex(' ').toUpper())));
        if (!def.gate.enabled)
            markActiveTestCaseStepDone(true, QString::number(machineIndex), QStringLiteral("通过"));
        return;
    }

    const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    auto* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(timeoutMs);
    const auto waitTimerStopped = std::make_shared<bool>(false);

    const auto stopWaitTimer = [timeoutTimer, waitTimerStopped]() {
        if (*waitTimerStopped)
            return;
        *waitTimerStopped = true;
        timeoutTimer->disconnect();
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
    };

    const auto onFixtureTimeout = [this, def, stopWaitTimer]() {
        if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
            return;
        stopWaitTimer();
        showlog(QStringLiteral("治具等待超时：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
    };
    connect(timeoutTimer, &QTimer::timeout, this, onFixtureTimeout);

    const auto handlePacket = [this, def, stopWaitTimer](const FixturePacketData& pack) {
        if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
            return;
        stopWaitTimer();
        const QVariant payload = QVariant::fromValue(pack);
        if (def.gate.enabled && evaluateActiveTestCaseGate(QStringLiteral("ProtocolFixturePcbaData"), payload))
            return;
        const QString detail =
            QStringLiteral("机号=%1 静态 %2uA 工作=%3mA")
                .arg(pack.machineNumber)
                .arg(pack.staticCurrent)
                .arg(pack.workingCurrent);
        markActiveTestCaseStepDone(true, detail, QStringLiteral("通过"));
        showlog(QStringLiteral("治具回包：%1").arg(detail));
    };

    if (cmd == FixturePcbaCmd::WaitFixturePacket) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine, this,
                           [this, connPtr, handlePacket](const FixturePacketData& pack) {
                               QObject::disconnect(*connPtr);
                               handlePacket(pack);
                           });
    } else if (cmd == FixturePcbaCmd::WaitSleepRequest) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine_sleep, this,
                           [this, connPtr, handlePacket](const FixturePacketData& pack) {
                               QObject::disconnect(*connPtr);
                               handlePacket(pack);
                           });
    } else if (cmd == FixturePcbaCmd::WaitStartTestAck) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine_start, this,
                           [this, def, stopWaitTimer, connPtr]() {
                               QObject::disconnect(*connPtr);
                               if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
                                   return;
                               stopWaitTimer();
                               if (!def.gate.enabled) {
                                   markActiveTestCaseStepDone(true, QStringLiteral("开始测试应答"),
                                                              QStringLiteral("通过"));
                                   showlog(QStringLiteral("收到治具开始测试应答"));
                               } else {
                                   showlog(QStringLiteral(
                                       "WaitStartTestAck 不支持卡控，请关闭卡控或改用 WaitFixturePacket"));
                                   markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
                               }
                           });
    } else if (cmd == FixturePcbaCmd::WaitWorkCurrentDoneAck) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine_sleep, this,
                           [this, def, stopWaitTimer, connPtr](const FixturePacketData& pack) {
                               QObject::disconnect(*connPtr);
                               if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
                                   return;
                               stopWaitTimer();
                               const QString detail =
                                   QStringLiteral("工作电流测量完成 机号=%1").arg(pack.machineNumber);
                               markActiveTestCaseStepDone(true, detail, QStringLiteral("通过"));
                               showlog(QStringLiteral("收到治具短包 55 01 05 CC AA：%1").arg(detail));
                           });
    } else {
        stopWaitTimer();
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    timeoutTimer->start();
    showlog(QStringLiteral("等待治具回包：%1（超时 %2ms）")
                .arg(FixturePcbaCmdCatalog::fixturePcbaCmdUiLabel(def.send.deviceCmd))
                .arg(timeoutMs));
}

void QFreeWork::executeFixtureAsd9026aCase(const TestCaseDefinition& def) {
    if (def.send.fixtureProtocol != TestCaseFixtureProtocol::Asd9026a) {
        showlog(QStringLiteral("ASD9026A 协议类型不匹配，请检查 Send/Protocol"));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    Asd9026aCmd cmd;
    if (!Asd9026aCmdCatalog::asd9026aCmdFromName(def.send.deviceCmd, cmd)) {
        showlog(QStringLiteral("未知 ASD9026A 指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!Asd9026aCmdCatalog::isCmdForAction(cmd, def.send.action)) {
        showlog(QStringLiteral("ASD9026A 指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    auto* box = qobject_cast<QFreeWorkBox*>(window());
    Asd9026aDevice* asdDevice = box ? box->sharedAsd9026aDevice() : nullptr;
    if (!asdDevice) {
        showlog(QStringLiteral("ASD9026A 共享设备对象不存在"));
        markActiveTestCaseStepDone(false, QStringLiteral("共享设备不存在"), QStringLiteral("失败"));
        return;
    }

    QString errStr;
    if (!ensureAsd9026aConnected(this, *asdDevice, &errStr)) {
        showlog(errStr);
        markActiveTestCaseStepDone(false, QStringLiteral("串口未连接"), QStringLiteral("失败"));
        return;
    }

    const QVariant resolvedParam = resolveTestCaseSendParamTree(def.send.param);
    const QVariantMap cmdMap = resolvedParam.canConvert<QVariantMap>() ? resolvedParam.toMap() : QVariantMap{};
    const int channel = getIndex();
    if (channel < 1 || channel > 2) {
        const QString message = QStringLiteral("ASD9026A 仅支持通道1/2，当前工位 index=%1").arg(channel);
        showlog(message);
        markActiveTestCaseStepDone(false, message, QStringLiteral("失败"));
        return;
    }
    const quint8 moduleAddr = asd9026aModuleAddr(channel);
    const QString channelText = QStringLiteral("CH%1").arg(channel);
    const QString txHexOverride = asd9026aTxHexFromParam(resolvedParam, cmdMap);

    auto reportMeasure = [this, def, channelText](const QString& type, double value, const QString& unit,
                                                  const QString& valueText) {
        ProtocolMeasureData measureData;
        measureData.deviceName = QStringLiteral("ASD9026A");
        measureData.channel = channelText;
        measureData.type = type;
        measureData.value = value;
        measureData.valueText = valueText.isEmpty() ? QString::number(value, 'f', 4) : valueText;
        measureData.unit = unit;
        measureData.isOk = true;
        onUsbInstrumentReport(
            ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(measureData)));
    };

    bool ok = false;
    QString testData = QStringLiteral("-");
    QString actualTxHex;

    switch (cmd) {
    case Asd9026aCmd::SendRaw: {
        const QString rawText = txHexOverride.isEmpty() ? sendParamAsRawText(resolvedParam).trimmed() : txHexOverride;
        if (rawText.isEmpty()) {
            showlog(QStringLiteral("ASD9026A SendRaw 内容为空，请配置 Param_string 或 Param_txHex"));
            markActiveTestCaseStepDone(false, QStringLiteral("参数为空"), QStringLiteral("失败"));
            return;
        }
        QByteArray response;
        QByteArray* respPtr = (def.send.action == TestCaseSendAction::Get) ? &response : nullptr;
        ok = asd9026aSendConfiguredTxHex(*asdDevice, moduleAddr, rawText, respPtr, &actualTxHex, &errStr);
        testData = actualTxHex;
        if (ok) {
            showlog(QStringLiteral("ASD9026A %1 已按十六进制发送：%2").arg(channelText, actualTxHex));
            if (def.send.action == TestCaseSendAction::Get) {
                const QString rxHex = QString::fromLatin1(response.toHex(' ').toUpper());
                showlog(QStringLiteral("ASD9026A 回包：%1").arg(rxHex));
                testData = rxHex;
            }
        }
        break;
    }
    case Asd9026aCmd::ProgrammablePowerOutput: {
        const bool enable = cmdMap.value(QStringLiteral("enable"), 1).toInt() != 0;
        if (!txHexOverride.isEmpty()) {
            ok = asd9026aSendConfiguredTxHex(*asdDevice, moduleAddr, txHexOverride, nullptr, &actualTxHex, &errStr);
            testData = actualTxHex;
            if (ok)
                showlog(QStringLiteral("ASD9026A %1 输出%2（txHex）")
                            .arg(channelText, enable ? QStringLiteral("已打开") : QStringLiteral("已关闭")));
        } else {
            ok = asdDevice->setOutputEnabled(moduleAddr, enable, &errStr);
            testData = enable ? QStringLiteral("ON") : QStringLiteral("OFF");
            if (ok)
                showlog(QStringLiteral("ASD9026A %1 输出%2")
                            .arg(channelText, enable ? QStringLiteral("已打开") : QStringLiteral("已关闭")));
        }
        break;
    }
    case Asd9026aCmd::ConfigureProgrammablePower: {
        const double voltageV = asd9026aParamDouble(cmdMap, QStringLiteral("voltage"), 4.0);
        const double currentA = asd9026aParamDouble(cmdMap, QStringLiteral("current"), 2.0);
        const quint8 currentRange = asd9026aCurrentRangeFromMap(cmdMap, 4);
        if (!txHexOverride.isEmpty()) {
            ok = asd9026aSendConfiguredTxHex(*asdDevice, moduleAddr, txHexOverride, nullptr, &actualTxHex, &errStr);
            testData = actualTxHex;
            if (ok)
                showlog(QStringLiteral("ASD9026A %1 已配置（txHex）：%2").arg(channelText, actualTxHex));
        } else {
            ok = asdDevice->configureConstantVoltage(moduleAddr, voltageV, currentA, currentRange, &errStr);
            testData = QStringLiteral("V=%1V,I=%2A,量程=%3")
                           .arg(voltageV, 0, 'f', 2)
                           .arg(currentA, 0, 'f', 3)
                           .arg(asd9026aCurrentRangeText(currentRange));
            if (ok) {
                showlog(QStringLiteral("ASD9026A %1 已配置：%2 V，限流 %3 A，电流测量量程 %4")
                            .arg(channelText)
                            .arg(voltageV, 0, 'f', 2)
                            .arg(currentA, 0, 'f', 3)
                            .arg(asd9026aCurrentRangeText(currentRange)));
            }
        }
        break;
    }
    case Asd9026aCmd::ConfigureCurrentMeasureRange: {
        const quint8 currentRange = asd9026aCurrentRangeFromMap(cmdMap, 4);
        if (!txHexOverride.isEmpty()) {
            ok = asd9026aSendConfiguredTxHex(*asdDevice, moduleAddr, txHexOverride, nullptr, &actualTxHex, &errStr);
            testData = actualTxHex;
            if (ok)
                showlog(QStringLiteral("ASD9026A %1 量程切换（txHex）：%2").arg(channelText, actualTxHex));
        } else {
            ok = asdDevice->setCurrentMeasureRange(moduleAddr, currentRange, &errStr);
            testData = asd9026aCurrentRangeText(currentRange);
            if (ok)
                showlog(QStringLiteral("ASD9026A %1 电流测量量程已切换为 %2").arg(channelText, testData));
        }
        break;
    }
    case Asd9026aCmd::ReadProgrammableVoltage:
    case Asd9026aCmd::ReadProgrammableCurrent: {
        if (cmd == Asd9026aCmd::ReadProgrammableCurrent && def.gate.enabled) {
            runAsdProgrammableCurrentSampleAnyMatch(def, moduleAddr, channel);
            return;
        }
        Asd9026aAnalogStatus status;
        if (!txHexOverride.isEmpty()) {
            QByteArray response;
            ok = asd9026aSendConfiguredTxHex(*asdDevice, moduleAddr, txHexOverride, &response, &actualTxHex, &errStr);
            if (ok)
                ok = asdDevice->parseAnalogStatusResponse(moduleAddr, response, &status, &errStr);
            if (ok)
                showlog(QStringLiteral("ASD9026A %1 已按 txHex 读取：%2").arg(channelText, actualTxHex));
        } else {
            ok = asdDevice->readAnalogStatus(moduleAddr, &status, &errStr);
        }
        if (!ok)
            break;
        if (cmd == Asd9026aCmd::ReadProgrammableVoltage) {
            testData = QStringLiteral("%1 V").arg(status.voltage, 0, 'f', 4);
            showlog(QStringLiteral("ASD9026A %1 电压：raw=%2 → %3")
                        .arg(channelText)
                        .arg(status.voltageRaw)
                        .arg(testData));
            reportMeasure(QStringLiteral("Voltage"), status.voltage, QStringLiteral("V"), testData);
        } else {
            const double currentMa = status.current * 1000.0;
            testData = QStringLiteral("%1 mA").arg(currentMa, 0, 'f', 4);
            showlog(QStringLiteral("ASD9026A %1 电流：raw=%2 → %3 A → %4")
                        .arg(channelText)
                        .arg(status.currentRaw)
                        .arg(status.current, 0, 'g', 10)
                        .arg(testData));
            reportMeasure(QStringLiteral("Current"), currentMa, QStringLiteral("mA"), testData);
        }
        if (!def.gate.enabled)
            markActiveTestCaseStepDone(true, testData, QStringLiteral("通过"));
        return;
    }
    default:
        errStr = QStringLiteral("未实现的 ASD9026A 指令");
        break;
    }

    if (!ok) {
        showlog(QStringLiteral("ASD9026A 指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
        markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
        return;
    }

    if (!def.gate.enabled)
        markActiveTestCaseStepDone(true, testData, QStringLiteral("通过"));
}

void QFreeWork::executeFixtureXwdCase(const TestCaseDefinition& def) {
    if (def.send.fixtureProtocol != TestCaseFixtureProtocol::Xwd) {
        showlog(QStringLiteral("XWD治具协议类型不匹配，请检查 Send/Protocol"));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    XwdRawFixtureCmd cmd;
    if (!XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(def.send.deviceCmd, cmd)) {
        showlog(QStringLiteral("未知 XWD治具指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!XwdRawFixtureCmdCatalog::isCmdForAction(cmd, def.send.action)) {
        showlog(QStringLiteral("XWD治具指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    QString errStr;
    if (!ensureXwdJigUartOpen(this, &errStr)) {
        showlog(errStr);
        markActiveTestCaseStepDone(false, QStringLiteral("串口未连接"), QStringLiteral("失败"));
        return;
    }

    const QString rawText = sendParamAsRawText(resolveTestCaseSendParamTree(def.send.param));
    if (rawText.isEmpty()) {
        showlog(QStringLiteral("XWD治具发送内容为空，请配置 Send/Param/string"));
        markActiveTestCaseStepDone(false, QStringLiteral("参数为空"), QStringLiteral("失败"));
        return;
    }

    bool parsedAsHex = false;
    const QByteArray request = XwdRawUartCodec::encodeRawText(rawText, &parsedAsHex);
    if (request.isEmpty()) {
        showlog(QStringLiteral("XWD治具发送内容编码为空"));
        markActiveTestCaseStepDone(false, QStringLiteral("参数为空"), QStringLiteral("失败"));
        return;
    }

    // 「设置」只下发；「读取」下发后等回包并解析电流（吸力工站）；蓝牙盒电源等步骤一般用设置
    if (def.send.action != TestCaseSendAction::Get) {
        if (!XwdRawFixtureDevice::sendRawText(jigSerialPort, rawText, &errStr)) {
            showlog(QStringLiteral("XWD治具指令 [%1] 发送失败: %2").arg(def.send.deviceCmd, errStr));
            markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            return;
        }
        showlog(parsedAsHex ? QStringLiteral("XWD治具已按十六进制发送：%1").arg(rawText)
                            : QStringLiteral("XWD治具已原文发送：%1").arg(rawText));
        if (!def.gate.enabled)
            markActiveTestCaseStepDone(true, rawText, QStringLiteral("通过"));
        return;
    }

    if (parsedAsHex)
        showlog(QStringLiteral("XWD治具读取下发（十六进制）：%1").arg(rawText));
    else
        showlog(QStringLiteral("XWD治具读取下发（原文）：%1").arg(rawText));

    int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    // 该治具实测回包常 >300ms；过短超时会“设备已回、上位机已放弃”
    if (timeoutMs < 2000)
        timeoutMs = 2000;
    // 可由步骤指定读取通道；未指定时，一拖二按工位号取通道，一拖一取 CH1。
    const int stationSlots = qMax(1, SETTINGS.value(QStringLiteral("User/formColumn"), 1).toInt()
                                         * SETTINGS.value(QStringLiteral("User/formRow"), 1).toInt());
    const bool dualFixture = stationSlots >= 2;
    const QVariant resolvedParam = resolveTestCaseSendParamTree(def.send.param);
    const QVariantMap paramMap = resolvedParam.canConvert<QVariantMap>() ? resolvedParam.toMap() : QVariantMap{};
    const int configuredReadChannel = paramMap.value(QStringLiteral("readChannel"), 0).toInt();
    const int readChannel = (configuredReadChannel == 1 || configuredReadChannel == 2)
                                ? configuredReadChannel
                                : ((dualFixture && getIndex() >= 2) ? 2 : 1);
    showlog(QStringLiteral("XWD治具等待回包超时上限：%1ms，取通道 CH%2（%3）")
                .arg(timeoutMs)
                .arg(readChannel)
                .arg(configuredReadChannel == readChannel
                         ? QStringLiteral("步骤配置")
                         : (dualFixture ? QStringLiteral("一拖二") : QStringLiteral("一拖一"))));
    if (!jigSerialChannel_) {
        showlog(QStringLiteral("XWD治具串口通道未初始化"));
        markActiveTestCaseStepDone(false, QStringLiteral("串口未连接"), QStringLiteral("失败"));
        return;
    }

    if (def.gate.enabled) {
        runXwdFixtureCurrentSampleAnyMatch(def, request, readChannel, dualFixture, timeoutMs);
        return;
    }

    // 回包为逐行文本；发送前持续监听，避免 CH1 与下一行 CH2 之间切换监听造成丢帧。
    QByteArray reply;
    QString receiveError;
    if (!sendAndCollectXwdReadOnceReply(jigSerialChannel_, jigSerialPort, request, readChannel, timeoutMs,
                                        &reply, &receiveError)) {
        showlog(receiveError);
        markActiveTestCaseStepDone(false, QStringLiteral("写入失败"), QStringLiteral("失败"));
        return;
    }
    if (reply.isEmpty()) {
        showlog(QStringLiteral("XWD治具等待回包超时（%1ms）").arg(timeoutMs));
        markActiveTestCaseStepDone(false, QStringLiteral("接收超时"), QStringLiteral("失败"));
        return;
    }

    const QString replyText = QString::fromUtf8(reply).trimmed();
    showlog(QStringLiteral("XWD治具已收到回包：%1").arg(replyText));

    double ch1Ma = 0;
    double ch2Ma = 0;
    bool hasCh1 = false;
    bool hasCh2 = false;
    if (XwdRawUartCodec::parseReadOnceReply(replyText, &ch1Ma, &ch2Ma, &hasCh1, &hasCh2)) {
        const bool channelOk = (readChannel == 2) ? hasCh2 : hasCh1;
        const double valueMa = (readChannel == 2) ? ch2Ma : ch1Ma;
        if (dualFixture)
            showlog(QStringLiteral("XWD治具电流：CH1=%1mA%2 CH2=%3mA%4，本工位取 CH%5")
                        .arg(hasCh1 ? QString::number(ch1Ma, 'f', 2) : QStringLiteral("-"))
                        .arg(hasCh1 ? QString() : QStringLiteral("(无)"))
                        .arg(hasCh2 ? QString::number(ch2Ma, 'f', 2) : QStringLiteral("-"))
                        .arg(hasCh2 ? QString() : QStringLiteral("(无)"))
                        .arg(readChannel));
        else
            showlog(QStringLiteral("XWD治具电流：CH1=%1mA").arg(ch1Ma, 0, 'f', 2));

        if (!channelOk) {
            showlog(QStringLiteral("XWD治具回包缺少 CH%1 电流").arg(readChannel));
            markActiveTestCaseStepDone(false, QStringLiteral("缺通道"), QStringLiteral("失败"));
            return;
        }

        ProtocolMeasureData measureData;
        measureData.deviceName = QStringLiteral("XWD");
        measureData.channel = QStringLiteral("CH%1").arg(readChannel);
        measureData.type = QStringLiteral("Current");
        measureData.value = valueMa;
        measureData.valueText = QString::number(valueMa, 'f', 2);
        measureData.unit = QStringLiteral("mA");
        measureData.isOk = true;
        onUsbInstrumentReport(
            ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(measureData)));
        if (!def.gate.enabled)
            markActiveTestCaseStepDone(true, measureData.valueText + QStringLiteral("mA"), QStringLiteral("通过"));
        return;
    }

    if (def.gate.enabled) {
        showlog(QStringLiteral("XWD治具回包无法解析电流(mA)：%1").arg(replyText));
        markActiveTestCaseStepDone(false, replyText, QStringLiteral("失败"));
        return;
    }
    markActiveTestCaseStepDone(true, replyText, QStringLiteral("通过"));
}

void QFreeWork::executeFixtureJieliBtBoxCase(const TestCaseDefinition& def) {
    if (def.send.fixtureProtocol != TestCaseFixtureProtocol::JieliBtBox) {
        showlog(QStringLiteral("杰理蓝牙盒子协议类型不匹配，请检查 Send/Protocol"));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    JieliBtBoxCmd cmd;
    if (!JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(def.send.deviceCmd, cmd)) {
        showlog(QStringLiteral("未知杰理蓝牙盒子指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!JieliBtBoxCmdCatalog::isCmdForAction(cmd, def.send.action)) {
        showlog(QStringLiteral("杰理蓝牙盒子指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    QString errStr;
    if (!ensureJieliBtBoxProductUartOpen(this, &errStr)) {
        showlog(errStr);
        markActiveTestCaseStepDone(false, QStringLiteral("串口未连接"), QStringLiteral("失败"));
        return;
    }
    if (!productSerialChannel_) {
        showlog(QStringLiteral("产品串口(仪器)通道未初始化"));
        markActiveTestCaseStepDone(false, QStringLiteral("串口未连接"), QStringLiteral("失败"));
        return;
    }

    int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    if (cmd == JieliBtBoxCmd::WaitRfInfo)
        timeoutMs = qMax(timeoutMs, 10000);
    else if (timeoutMs < 3000)
        timeoutMs = 3000;

    const QString portName = getProductcomNameCombo() ? getProductcomNameCombo()->currentText().trimmed() : QString();
    showlog(QStringLiteral("等待杰理蓝牙盒子频偏/RSSI（产品串口 %1，超时 %2ms）")
                .arg(portName.isEmpty() ? QStringLiteral("-") : portName)
                .arg(timeoutMs));
    JieliBtBoxRfInfo info;
    if (!JieliBtBoxDevice::waitForRfInfo(productSerialChannel_, timeoutMs, &info, &errStr)) {
        showlog(QStringLiteral("杰理蓝牙盒子读取失败：%1").arg(errStr));
        markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
        return;
    }

    const QString macText = QString::fromLatin1(info.mac.toHex(':')).toUpper();
    const QString detail = QStringLiteral("频偏=%1 RSSI=%2%3")
                               .arg(info.freqOffset)
                               .arg(info.rssi)
                               .arg(macText.isEmpty() ? QString() : QStringLiteral(" MAC=%1").arg(macText));
    showlog(QStringLiteral("杰理蓝牙盒子：%1").arg(detail));

    // 同步到工站 RSSI 显示（与产品 BLE RSSI 共用上限下限卡控时可叠加 Gate）
    BLE_RSSI = QString::number(info.rssi);
    if (ui && ui->BLE_RSSI)
        ui->BLE_RSSI->setText(QStringLiteral("BLE的RSSI:") + BLE_RSSI);

    ProtocolJieliBtBoxData report;
    report.freqOffset = info.freqOffset;
    report.rssi = info.rssi;
    report.mac = macText;
    const QVariant payload = QVariant::fromValue(report);
    if (def.gate.enabled && evaluateActiveTestCaseGate(QStringLiteral("ProtocolJieliBtBoxData"), payload))
        return;
    markActiveTestCaseStepDone(true, detail, QStringLiteral("通过"));
}

bool QFreeWork::sendVesCh1BrightnessOnFixture(int brightness, QString* failReason) {
    auto* box = qobject_cast<QFreeWorkBox*>(window());
    QString fixtureConnectDetail;
    bool fixtureAutoConnected = false;
    Fixture_uart* uart =
        box ? box->ensureFixtureUartConnected(getIndex(), &fixtureConnectDetail, &fixtureAutoConnected) : nullptr;
    if (!uart || !uart->isFixtureSerialOpen()) {
        const QString msg = fixtureConnectDetail.isEmpty()
            ? QStringLiteral("治具串口未连接，且无法自动连接（请检查配置或菜单「连接治具串口」）")
            : fixtureConnectDetail;
        if (failReason)
            *failReason = msg;
        return false;
    }
    if (fixtureAutoConnected)
        showlog(QStringLiteral("已自动连接治具串口：%1").arg(fixtureConnectDetail));

    const quint8 ch = 1;
    const quint8 cur = static_cast<quint8>(qBound(0, brightness, 255));
    const quint8 xorv = static_cast<quint8>(0x24 ^ ch ^ cur);
    QByteArray pkt(4, '\0');
    pkt[0] = char(0x24);
    pkt[1] = char(ch);
    pkt[2] = char(cur);
    pkt[3] = char(xorv);
    uart->sendPcbaFrame(pkt);
    showlog(QStringLiteral("VES 光源 CH1 亮度=%1 帧 %2")
                .arg(cur)
                .arg(QString::fromLatin1(pkt.toHex(' ').toUpper())));
    return true;
}

void QFreeWork::executeFixtureVesLightCase(const TestCaseDefinition& def) {
    if (def.send.fixtureProtocol != TestCaseFixtureProtocol::VesLight) {
        showlog(QStringLiteral("VES 光源协议类型不匹配，请检查 Send/Protocol"));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    VesLightCmd cmd;
    if (!VesLightCmdCatalog::vesLightCmdFromName(def.send.deviceCmd, cmd)) {
        showlog(QStringLiteral("未知 VES 光源指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!VesLightCmdCatalog::isCmdForAction(cmd, def.send.action)) {
        showlog(QStringLiteral("VES 光源指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    int brightness = 22;
    if (map.contains(QStringLiteral("brightness")))
        brightness = map.value(QStringLiteral("brightness")).toInt();
    else if (map.contains(QStringLiteral("current")))
        brightness = map.value(QStringLiteral("current")).toInt();
    brightness = qBound(0, brightness, 255);

    QString failReason;
    if (!sendVesCh1BrightnessOnFixture(brightness, &failReason)) {
        showlog(QStringLiteral("VES 设亮度失败：%1").arg(failReason));
        markActiveTestCaseStepDone(false, failReason, QStringLiteral("失败"));
        return;
    }
    const QString data = QStringLiteral("CH1=%1").arg(brightness);
    markActiveTestCaseStepDone(true, data, QStringLiteral("通过"));
}

void QFreeWork::executeProductSerialCase(const TestCaseDefinition& def) {
    ProductSerialCmd serialCmd;
    if (!ProductSerialCmdCatalog::productSerialCmdFromName(def.send.deviceCmd, serialCmd)) {
        showlog(QStringLiteral("未知产品串口指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!ProductSerialCmdCatalog::isCmdForAction(serialCmd, def.send.action)) {
        showlog(QStringLiteral("产品串口指令仅支持设置：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    // WaitReply=false：只发不等应答；停止接收+PER 必须等收包数，仍走等待
    const int waitMs = def.timing.waitReply ? timeoutMs : -1;

    switch (serialCmd) {
    case ProductSerialCmd::InstrumentReset:
        startProductInstrumentResetAndWaitAck(QString(), waitMs);
        break;
    case ProductSerialCmd::StopRxAndPer:
        startProductInstrumentStopReceiveAndPer(QString(), timeoutMs);
        break;
    default: {
        const int profile = ProductSerialCmdCatalog::brushProfileForCmd(serialCmd);
        if (profile < 0) {
            markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        startProductInstrumentStartReceiveForCatalog(QString(), profile, waitMs);
        break;
    }
    }
}

void registerFreeWorkTestCaseHooks() {
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    TestCaseHookRegistry::registerHook(QStringLiteral("NoOp"), [](QFreeWork* fw) {
        if (!fw)
            return;
        fw->showlog(QStringLiteral("钩子 NoOp 已执行"));
        fw->markActiveTestCaseStepDone(true, QStringLiteral("noop"), QStringLiteral("通过"));
    });
    TestCaseHookRegistry::registerHook(QStringLiteral("FreeWorkNoOpDemo"), [](QFreeWork* fw) {
        if (!fw)
            return;
        fw->showlog(QStringLiteral("示例钩子 FreeWorkNoOpDemo 已执行"));
        fw->markActiveTestCaseStepDone(true, QStringLiteral("hook_ok"), QStringLiteral("通过"));
    });
}
