#include "xinjie_plc_rtu_device.h"

#include "Abini.h"
#include "qmodbus_pdu.h"
#include "serial_channel.h"
#include "xinjie_plc_address.h"

#include <QSerialPort>
#include <QVariantMap>
#include <QVector>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr quint8 kFcReadDiscreteInputs = 0x02;
constexpr quint8 kFcReadHoldingRegisters = 0x03;
constexpr quint8 kFcWriteSingleRegister = 0x06;

// 兼容界面只填数字或 "COM 4"：纯数字或 "COMx" 归一为 "COMx"，避免打不开串口
QString normalizeComPort(const QString& raw) {
    QString s = raw.trimmed();
    if (s.isEmpty())
        return s;
    if (s.startsWith(QStringLiteral("COM"), Qt::CaseInsensitive)) {
        const QString num = s.mid(3).trimmed();
        bool ok = false;
        const int n = num.toInt(&ok);
        return ok ? QStringLiteral("COM%1").arg(n) : s;
    }
    bool ok = false;
    const int n = s.toInt(&ok);
    return ok ? QStringLiteral("COM%1").arg(n) : s;
}

QByteArray appendRtuCrc(const QByteArray& body) {
    QByteArray frame = body;
    const quint16 crcBe = QModbusPdu::crc16ModbusRtuBigEndian(body);
    frame.append(char(quint8(crcBe >> 8)));
    frame.append(char(quint8(crcBe & 0xFF)));
    return frame;
}

QByteArray buildRtuRequest(quint8 slaveId, const QByteArray& pdu) {
    QByteArray body;
    body.append(char(slaveId));
    body.append(pdu);
    return appendRtuCrc(body);
}

bool validateRtuFrame(const QByteArray& frame, quint8 slaveId, quint8 expectedFc, QByteArray* pduOut,
                      QString* errorMessage) {
    if (!pduOut) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("内部参数错误");
        }
        return false;
    }
    pduOut->clear();
    if (frame.size() < 5) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RTU 应答过短");
        }
        return false;
    }
    if (static_cast<quint8>(frame.at(0)) != slaveId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RTU 从站地址不匹配");
        }
        return false;
    }
    const quint16 crcExpected = QModbusPdu::readUint16Be(frame.constData() + frame.size() - 2);
    const quint16 crcCalculated = QModbusPdu::crc16ModbusRtuBigEndian(frame.left(frame.size() - 2));
    if (crcCalculated != crcExpected) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RTU CRC 校验失败");
        }
        return false;
    }
    *pduOut = frame.mid(1, frame.size() - 3);
    if (QModbusPdu::isExceptionResponse(*pduOut)) {
        if (errorMessage) {
            *errorMessage = QModbusPdu::formatExceptionMessage(*pduOut);
        }
        return false;
    }
    if (!pduOut->isEmpty() && static_cast<quint8>((*pduOut)[0]) != expectedFc) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RTU 功能码不匹配，期望 0x%1")
                                .arg(expectedFc, 2, 16, QChar('0'));
        }
        return false;
    }
    return true;
}

QString addressFromParam(const QVariant& param) {
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        if (map.contains(QStringLiteral("address")))
            return map.value(QStringLiteral("address")).toString().trimmed();
        if (map.contains(QStringLiteral("addr")))
            return map.value(QStringLiteral("addr")).toString().trimmed();
    }
    return QString();
}

int quantityFromParam(const QVariant& param, int fallback = 1) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("quantity")))
        return qMax(1, map.value(QStringLiteral("quantity")).toInt());
    return fallback;
}

bool valueFromParam(const QVariant& param, bool fallback = true) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("value")))
        return map.value(QStringLiteral("value")).toBool();
    return fallback;
}

quint16 registerValueFromParam(const QVariant& param, quint16 fallback = 0) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("value")))
        return quint16(map.value(QStringLiteral("value")).toUInt());
    return fallback;
}

/** 写类指令是否等待并校验回帧；信捷现场多为只发不收，默认 false。 */
bool expectReplyFromParam(const QVariant& param, bool fallback) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (!map.contains(QStringLiteral("expectReply")))
        return fallback;
    const QString t = map.value(QStringLiteral("expectReply")).toString().trimmed().toLower();
    if (t.isEmpty())
        return fallback;
    return t == QStringLiteral("1") || t == QStringLiteral("true") || t == QStringLiteral("yes");
}

QSerialPort::Parity parityFromText(const QString& text, QSerialPort::Parity fallback) {
    const QString t = text.trimmed().toLower();
    if (t.isEmpty())
        return fallback;
    if (t.startsWith(QLatin1Char('e')))
        return QSerialPort::EvenParity;
    if (t.startsWith(QLatin1Char('o')))
        return QSerialPort::OddParity;
    if (t.startsWith(QLatin1Char('n')))
        return QSerialPort::NoParity;
    return fallback;
}

QString parityText(QSerialPort::Parity parity) {
    if (parity == QSerialPort::EvenParity)
        return QStringLiteral("E");
    if (parity == QSerialPort::OddParity)
        return QStringLiteral("O");
    return QStringLiteral("N");
}

SerialChannel::RtsDtrMode rtsModeFromText(const QString& text) {
    const QString t = text.trimmed().toLower();
    if (t == QStringLiteral("rs485") || t == QStringLiteral("halfduplex"))
        return SerialChannel::RtsDtrMode::Rs485HalfDuplex;
    if (t == QStringLiteral("none") || t == QStringLiteral("off"))
        return SerialChannel::RtsDtrMode::None;
    return SerialChannel::RtsDtrMode::Enable;
}

} // namespace

XinjePlcRtuDevice::XinjePlcRtuDevice(QObject* parent) : QObject(parent) {
    registerXinjePlcCmdMetaTypes();
}

void XinjePlcRtuDevice::setSerialChannel(SerialChannel* channel) {
    serial_ = channel;
}

void XinjePlcRtuDevice::setStationIndex(int stationIndex) {
    stationIndex_ = qMax(1, stationIndex);
}

void XinjePlcRtuDevice::setLogFn(LogFn fn) {
    log_ = std::move(fn);
}

void XinjePlcRtuDevice::logLine(const QString& line) const {
    if (log_) {
        log_(line);
    }
}

bool XinjePlcRtuDevice::isQueryCmd(XinjePlcCmd cmd) {
    return cmd == XinjePlcCmd::IsConnected || cmd == XinjePlcCmd::ReadCoils || cmd == XinjePlcCmd::ReadHoldingRegisters
           || cmd == XinjePlcCmd::ReadDiscreteInputs;
}

XinjePlcRtuDevice::Config XinjePlcRtuDevice::configFromSettings() const {
    Config cfg;
    const int st = stationIndex_;
    QString comPort =
        SETTINGS.value(QStringLiteral("XINJE_PLC/ComPort_Station%1").arg(st), QString()).toString().trimmed();
    if (comPort.isEmpty())
        comPort = SETTINGS.value(QStringLiteral("XINJE_PLC/ComPort"), QString()).toString().trimmed();
    // 未单独配置时复用工位界面「万用表串口」（mechine/0、mechine/1…）
    if (comPort.isEmpty()) {
        const int mechineIdx = qMax(0, st - 1);
        comPort = SETTINGS.value(QStringLiteral("mechine/%1/usbcomName").arg(mechineIdx)).toString().trimmed();
    }
    cfg.comPort = normalizeComPort(comPort);
    cfg.baudRate = SETTINGS.value(QStringLiteral("XINJE_PLC/BaudRate_Station%1").arg(st),
                                  SETTINGS.value(QStringLiteral("XINJE_PLC/BaudRate"), 19200))
                       .toInt();
    cfg.slaveId = quint8(SETTINGS.value(QStringLiteral("XINJE_PLC/SlaveId_Station%1").arg(st),
                                        SETTINGS.value(QStringLiteral("XINJE_PLC/SlaveId"), 1))
                             .toUInt());
    cfg.requestTimeoutMs =
        SETTINGS.value(QStringLiteral("XINJE_PLC/RequestTimeoutMs_Station%1").arg(st),
                       SETTINGS.value(QStringLiteral("XINJE_PLC/RequestTimeoutMs"), 2000))
            .toInt();
    // 默认不做 RTS 控制（与信捷官方演示 Open(com,baud,8,"N",1) 一致）；
    // 仅在手动 RS485 收发切换的转换器上才需要把 rtsMode 配成 rs485
    cfg.rtsMode = rtsModeFromText(
        SETTINGS.value(QStringLiteral("XINJE_PLC/RtsMode_Station%1").arg(st),
                       SETTINGS.value(QStringLiteral("XINJE_PLC/RtsMode"), QStringLiteral("none")))
            .toString());
    cfg.parity = parityFromText(SETTINGS.value(QStringLiteral("XINJE_PLC/Parity_Station%1").arg(st),
                                               SETTINGS.value(QStringLiteral("XINJE_PLC/Parity"), QString()))
                                    .toString(),
                                cfg.parity);
    return cfg;
}

XinjePlcRtuDevice::Config XinjePlcRtuDevice::configFromParam(const QVariant& param) const {
    Config cfg = configFromSettings();
    if (!param.canConvert<QVariantMap>())
        return cfg;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("comPort")))
        cfg.comPort = normalizeComPort(map.value(QStringLiteral("comPort")).toString());
    if (map.contains(QStringLiteral("portName")))
        cfg.comPort = normalizeComPort(map.value(QStringLiteral("portName")).toString());
    if (map.contains(QStringLiteral("baudRate")))
        cfg.baudRate = map.value(QStringLiteral("baudRate")).toInt();
    if (map.contains(QStringLiteral("slaveId")))
        cfg.slaveId = quint8(map.value(QStringLiteral("slaveId")).toUInt());
    if (map.contains(QStringLiteral("requestTimeoutMs")))
        cfg.requestTimeoutMs = map.value(QStringLiteral("requestTimeoutMs")).toInt();
    if (map.contains(QStringLiteral("rtsMode")))
        cfg.rtsMode = rtsModeFromText(map.value(QStringLiteral("rtsMode")).toString());
    if (map.contains(QStringLiteral("parity")))
        cfg.parity = parityFromText(map.value(QStringLiteral("parity")).toString(), cfg.parity);
    return cfg;
}

bool XinjePlcRtuDevice::ensureOpen(const Config& cfg, QString* errorMessage) {
    if (!serial_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("信捷 PLC 串口通道未绑定");
        }
        return false;
    }
    if (opened_ && serial_->isOpen() && serial_->portName() == cfg.comPort && activeConfig_.baudRate == cfg.baudRate
        && activeConfig_.slaveId == cfg.slaveId && activeConfig_.parity == cfg.parity) {
        return true;
    }
    if (cfg.comPort.isEmpty()) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("未配置信捷 PLC 串口：请在步骤 Param 填 comPort，或工位选万用表串口，或 ini 设 XINJE_PLC/ComPort");
        }
        return false;
    }
    if (serial_->isOpen()) {
        serial_->close();
        opened_ = false;
    }
    SerialChannel::OpenParams params;
    params.portName = cfg.comPort;
    params.baudRate = cfg.baudRate;
    params.readDebounceMs = 10;
    params.flowControl = QSerialPort::NoFlowControl;
    params.rtsDtrMode = cfg.rtsMode;
    params.parity = cfg.parity;
    if (!serial_->open(params)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("信捷 PLC 串口打开失败(%1): %2").arg(cfg.comPort, serial_->errorString());
        }
        return false;
    }
    activeConfig_ = cfg;
    opened_ = true;
    return true;
}

bool XinjePlcRtuDevice::transact(const QByteArray& request, QByteArray* response, int timeoutMs,
                                 QString* errorMessage) const {
    if (!serial_ || !serial_->isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("信捷 PLC 串口未打开");
        }
        return false;
    }
    QByteArray raw;
    if (!serial_->exchangeCollect(request, &raw, timeoutMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("信捷 PLC 等待应答超时");
        }
        return false;
    }
    if (response) {
        *response = raw;
    }
    return true;
}

bool XinjePlcRtuDevice::transactPdu(quint8 slaveId, const QByteArray& pdu, QByteArray* responsePdu, int timeoutMs,
                                    QString* errorMessage) const {
    const QByteArray request = buildRtuRequest(slaveId, pdu);
    QByteArray raw;
    if (!transact(request, &raw, timeoutMs, errorMessage)) {
        return false;
    }
    QByteArray frame;
    if (!QModbusPdu::extractValidRtuFrame(raw, &frame)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("信捷 PLC 应答帧无效");
        }
        return false;
    }
    return validateRtuFrame(frame, slaveId, static_cast<quint8>(pdu.at(0)), responsePdu, errorMessage);
}

bool XinjePlcRtuDevice::sendPduNoReply(quint8 slaveId, const QByteArray& pdu, QString* errorMessage) const {
    if (!serial_ || !serial_->isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("信捷 PLC 串口未打开");
        }
        return false;
    }
    const QByteArray request = buildRtuRequest(slaveId, pdu);
    // 先清缓冲，避免上一次残留字节影响后续读指令
    serial_->clearReceiveBuffer();
    if (serial_->write(request) != request.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("信捷 PLC 串口写入失败");
        }
        return false;
    }
    if (QSerialPort* p = serial_->port()) {
        p->waitForBytesWritten(500);
    }
    return true;
}

bool XinjePlcRtuDevice::set(XinjePlcCmd cmd, const QVariant& param, QString* errorMessage) {
    switch (cmd) {
    case XinjePlcCmd::Connect: {
        const Config cfg = configFromParam(param);
        logLine(QStringLiteral("信捷 PLC 连接: %1 %2bps 8-%3-1 Slave=%4 RTS=%5")
                    .arg(cfg.comPort)
                    .arg(cfg.baudRate)
                    .arg(parityText(cfg.parity))
                    .arg(cfg.slaveId)
                    .arg(static_cast<int>(cfg.rtsMode)));
        return ensureOpen(cfg, errorMessage);
    }
    case XinjePlcCmd::Disconnect:
        if (serial_ && serial_->isOpen()) {
            serial_->close();
        }
        opened_ = false;
        return true;
    case XinjePlcCmd::WriteCoil: {
        const Config cfg = activeConfig_.comPort.isEmpty() ? configFromSettings() : activeConfig_;
        if (!ensureOpen(cfg, errorMessage)) {
            return false;
        }
        const QString addrText = addressFromParam(param);
        const XinjePlcAddress addr = parseXinjePlcAddress(addrText, true);
        if (!addr.ok || !addr.isCoil) {
            if (errorMessage) {
                *errorMessage = addr.error.isEmpty() ? QStringLiteral("WriteCoil 地址非法: %1").arg(addrText) : addr.error;
            }
            return false;
        }
        const bool value = valueFromParam(param, true);
        const QByteArray pdu = QModbusPdu::buildWriteSingleCoilRequestPdu(addr.modbusAddress, value);
        const bool expectReply = expectReplyFromParam(param, false);
        logLine(QStringLiteral("信捷 WriteCoil %1 addr=0x%2 value=%3%4")
                    .arg(addrText)
                    .arg(addr.modbusAddress, 4, 16, QChar('0'))
                    .arg(value ? 1 : 0)
                    .arg(expectReply ? QString() : QStringLiteral(" (不校验回包)")));
        if (!expectReply)
            return sendPduNoReply(cfg.slaveId, pdu, errorMessage);
        QByteArray responsePdu;
        return transactPdu(cfg.slaveId, pdu, &responsePdu, cfg.requestTimeoutMs, errorMessage);
    }
    case XinjePlcCmd::WriteRegister: {
        const Config cfg = activeConfig_.comPort.isEmpty() ? configFromSettings() : activeConfig_;
        if (!ensureOpen(cfg, errorMessage)) {
            return false;
        }
        const QString addrText = addressFromParam(param);
        const XinjePlcAddress addr = parseXinjePlcAddress(addrText, false);
        if (!addr.ok || !addr.isHoldingRegister) {
            if (errorMessage) {
                *errorMessage =
                    addr.error.isEmpty() ? QStringLiteral("WriteRegister 地址非法: %1").arg(addrText) : addr.error;
            }
            return false;
        }
        const quint16 value = registerValueFromParam(param, 0);
        QByteArray pdu;
        pdu.append(char(kFcWriteSingleRegister));
        QModbusPdu::appendUint16Be(pdu, addr.modbusAddress);
        QModbusPdu::appendUint16Be(pdu, value);
        const bool expectReply = expectReplyFromParam(param, false);
        logLine(QStringLiteral("信捷 WriteRegister %1 addr=0x%2 value=%3%4")
                    .arg(addrText)
                    .arg(addr.modbusAddress, 4, 16, QChar('0'))
                    .arg(value)
                    .arg(expectReply ? QString() : QStringLiteral(" (不校验回包)")));
        if (!expectReply)
            return sendPduNoReply(cfg.slaveId, pdu, errorMessage);
        QByteArray responsePdu;
        return transactPdu(cfg.slaveId, pdu, &responsePdu, cfg.requestTimeoutMs, errorMessage);
    }
    default:
        if (errorMessage) {
            *errorMessage = QStringLiteral("XinjePlcCmd 非 set 类指令: %1").arg(static_cast<int>(cmd));
        }
        return false;
    }
}

bool XinjePlcRtuDevice::get(XinjePlcCmd cmd, const QVariant& param, QVariant* result, QString* errorMessage) {
    if (cmd == XinjePlcCmd::IsConnected) {
        if (result) {
            *result = serial_ && serial_->isOpen();
        }
        return true;
    }

    const Config cfg = activeConfig_.comPort.isEmpty() ? configFromSettings() : activeConfig_;
    if (!ensureOpen(cfg, errorMessage)) {
        return false;
    }

    const QString addrText = addressFromParam(param);
    const int quantity = quantityFromParam(param, 1);

    if (cmd == XinjePlcCmd::ReadCoils) {
        const XinjePlcAddress addr = parseXinjePlcAddress(addrText, true);
        if (!addr.ok || !addr.isCoil) {
            if (errorMessage) {
                *errorMessage = addr.error.isEmpty() ? QStringLiteral("ReadCoils 地址非法: %1").arg(addrText) : addr.error;
            }
            return false;
        }
        const QByteArray pdu = QModbusPdu::buildReadCoilsRequestPdu(addr.modbusAddress, quint16(quantity));
        QByteArray responsePdu;
        if (!transactPdu(cfg.slaveId, pdu, &responsePdu, cfg.requestTimeoutMs, errorMessage)) {
            return false;
        }
        QVector<bool> bits;
        if (!QModbusPdu::parseReadCoilsPdu(responsePdu, quantity, &bits, errorMessage)) {
            return false;
        }
        if (result) {
            if (quantity == 1 && !bits.isEmpty()) {
                *result = bits.first();
            } else {
                QVariantList list;
                for (bool b : bits) {
                    list.append(b);
                }
                *result = list;
            }
        }
        return true;
    }

    if (cmd == XinjePlcCmd::ReadDiscreteInputs) {
        const XinjePlcAddress addr = parseXinjePlcAddress(addrText, true);
        if (!addr.ok || !addr.isDiscreteInput) {
            if (errorMessage) {
                *errorMessage =
                    addr.error.isEmpty() ? QStringLiteral("ReadDiscreteInputs 地址非法: %1").arg(addrText) : addr.error;
            }
            return false;
        }
        QByteArray pdu;
        pdu.append(char(kFcReadDiscreteInputs));
        QModbusPdu::appendUint16Be(pdu, addr.modbusAddress);
        QModbusPdu::appendUint16Be(pdu, quint16(quantity));
        QByteArray responsePdu;
        if (!transactPdu(cfg.slaveId, pdu, &responsePdu, cfg.requestTimeoutMs, errorMessage)) {
            return false;
        }
        QVector<bool> bits;
        if (!QModbusPdu::parseReadCoilsPdu(responsePdu, quantity, &bits, errorMessage)) {
            return false;
        }
        if (result) {
            if (quantity == 1 && !bits.isEmpty()) {
                *result = bits.first();
            } else {
                QVariantList list;
                for (bool b : bits) {
                    list.append(b);
                }
                *result = list;
            }
        }
        return true;
    }

    if (cmd == XinjePlcCmd::ReadHoldingRegisters) {
        const XinjePlcAddress addr = parseXinjePlcAddress(addrText, false);
        if (!addr.ok || !addr.isHoldingRegister) {
            if (errorMessage) {
                *errorMessage =
                    addr.error.isEmpty() ? QStringLiteral("ReadHoldingRegisters 地址非法: %1").arg(addrText) : addr.error;
            }
            return false;
        }
        QByteArray pdu;
        pdu.append(char(kFcReadHoldingRegisters));
        QModbusPdu::appendUint16Be(pdu, addr.modbusAddress);
        QModbusPdu::appendUint16Be(pdu, quint16(quantity));
        QByteArray responsePdu;
        if (!transactPdu(cfg.slaveId, pdu, &responsePdu, cfg.requestTimeoutMs, errorMessage)) {
            return false;
        }
        if (responsePdu.size() < 2 + quantity * 2) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("读保持寄存器应答长度不足");
            }
            return false;
        }
        QVariantList values;
        for (int i = 0; i < quantity; ++i) {
            const quint16 v = QModbusPdu::readUint16Be(responsePdu.constData() + 2 + i * 2);
            values.append(v);
        }
        if (result) {
            *result = quantity == 1 ? values.first() : QVariant(values);
        }
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("XinjePlcCmd 非 get 类指令: %1").arg(static_cast<int>(cmd));
    }
    return false;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
