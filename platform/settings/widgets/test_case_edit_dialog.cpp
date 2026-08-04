#include "test_case_edit_dialog.h"
#include "ui_test_case_edit_dialog.h"

#include "test_case.h"
#include "manifest/modbus_cmd_manifest.h"
#include "manifest/scpi_cmd_manifest.h"
#include "qprotocol_types.h"

#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QItemSelectionModel>

#include <QHash>
#include <QInputDialog>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QColor>

#include <algorithm>
#include <functional>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

QVariantMap sendParamAsJsonMap(const QVariant& param) {
    // DeviceSnPayload 必须先于 canConvert/toMap：payload 的 toMap() 恒为空，UI 会显示成 {}
    if (param.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = param.value<DeviceSnPayload>();
        QVariantMap map;
        map.insert(QStringLiteral("which_sn"), static_cast<int>(payload.which_sn));
        map.insert(QStringLiteral("sn"), QString::fromUtf8(payload.sn));
        return map;
    }
    if (param.type() == QVariant::Map || param.type() == QVariant::Hash)
        return param.toMap();
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        if (!map.isEmpty())
            return map;
    }
    return {};
}

/** 步骤 Param 键 → 界面中文说明（保存仍用英文键）。 */
QString sendParamKeyZhLabel(const QString& key) {
    const QString k = key.trimmed();
    if (k.isEmpty())
        return {};
    static const QHash<QString, QString> kMap = {
        {QStringLiteral("visaAddress"), QStringLiteral("VISA 资源地址（单电源）")},
        {QStringLiteral("voltage"), QStringLiteral("电压 (V)")},
        {QStringLiteral("current"), QStringLiteral("限流 (A)")},
        {QStringLiteral("currentRange"), QStringLiteral("电流量程")},
        {QStringLiteral("scpiSetVoltageCmd"), QStringLiteral("设电压 SCPI（含 %1）")},
        {QStringLiteral("scpiSetCurrentCmd"), QStringLiteral("设限流 SCPI（含 %1）")},
        {QStringLiteral("scpiOutputOnCmd"), QStringLiteral("打开输出 SCPI")},
        {QStringLiteral("scpiOutputOffCmd"), QStringLiteral("关闭输出 SCPI")},
        {QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("读电压 SCPI")},
        {QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("读电流 SCPI")},
        {QStringLiteral("scpiSetCurrentRangeCmd"), QStringLiteral("设电流量程 SCPI（含 %1）")},
        {QStringLiteral("scpiChannelSelectCmd"), QStringLiteral("选通道 SCPI（含 %1，如 INST OUT%1）")},
        {QStringLiteral("sharedPair"), QStringLiteral("启用多工位共享外设（填 false 关闭二拖二）")},
        {QStringLiteral("shareInstrument"), QStringLiteral("启用多工位共享外设（同 sharedPair，填 false 可关闭）")},
        {QStringLiteral("stationsPerDevice"), QStringLiteral("每台设备对应工位数（如 2 或 3）")},
        {QStringLiteral("powerChannel"), QStringLiteral("电源通道号（一般自动填）")},
        {QStringLiteral("powerChannelLock"), QStringLiteral("锁定电源通道（不按工位改）")},
        {QStringLiteral("visaDeviceIndex"), QStringLiteral("电源设备序号（从 0，一般自动）")},
        {QStringLiteral("visaDeviceIndexLock"), QStringLiteral("锁定电源设备序号")},
        {QStringLiteral("enable"), QStringLiteral("输出开关（1开/0关）")},
        {QStringLiteral("sampleDurationMs"), QStringLiteral("连续采样窗口 (ms)")},
        {QStringLiteral("sampleIntervalMs"), QStringLiteral("连续采样间隔 (ms)")},
        {QStringLiteral("channel"), QStringLiteral("温度/采样通道号")},
        {QStringLiteral("channels"), QStringLiteral("温度通道列表（如 1,2,3,4,5,6 或 1-6）")},
        {QStringLiteral("channelsPerStation"), QStringLiteral("每工位占用温度通道数（法兰加热填 6）")},
        {QStringLiteral("channelLock"), QStringLiteral("锁定通道号（不按工位改）")},
        {QStringLiteral("slaveAddr"), QStringLiteral("Modbus 从站地址")},
        {QStringLiteral("addr"), QStringLiteral("Modbus 从站地址")},
        {QStringLiteral("tempDeviceIndex"), QStringLiteral("温度仪设备序号（从 0）")},
        {QStringLiteral("tempDeviceIndexLock"), QStringLiteral("锁定温度仪设备序号")},
        {QStringLiteral("tempBaudRate"), QStringLiteral("温度仪串口波特率")},
        {QStringLiteral("tempRtsMode"), QStringLiteral("RTS模式：默认 RS232；RS485 转换器填 rs485，无控制填 none")},
        {QStringLiteral("tempLowC"), QStringLiteral("温度下限 ℃（可覆盖 Gate）")},
        {QStringLiteral("tempHighC"), QStringLiteral("温度上限 ℃（可覆盖 Gate）")},
        {QStringLiteral("sharedComName"), QStringLiteral("共享串口名（运行时解析）")},
        {QStringLiteral("txHex"), QStringLiteral("发送十六进制报文")},
        {QStringLiteral("string"), QStringLiteral("原文/字符串参数")},
        {QStringLiteral("line"), QStringLiteral("SCPI 原始行")},
        {QStringLiteral("mLeft"), QStringLiteral("PLC 左工位线圈")},
        {QStringLiteral("mRight"), QStringLiteral("PLC 右工位线圈")},
        {QStringLiteral("readChannel"), QStringLiteral("治具读电流通道 CH1/CH2")},
        {QStringLiteral("MachineIndex"), QStringLiteral("治具机号")},
        {QStringLiteral("machineIndex"), QStringLiteral("治具机号")},
    };
    if (kMap.contains(k))
        return kMap.value(k);
    {
        QRegularExpression re(QStringLiteral(R"(^visaAddress_?(\d+)$)"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(k);
        if (m.hasMatch())
            return QStringLiteral("第 %1 台程控电源 VISA 地址").arg(m.captured(1).toInt() + 1);
    }
    {
        QRegularExpression re(QStringLiteral(R"(^(?:tempComName|sharedComName)_?(\d+)$)"),
                              QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(k);
        if (m.hasMatch())
            return QStringLiteral("第 %1 台温度仪串口名").arg(m.captured(1).toInt() + 1);
    }
    // 未登记键：界面仍显示英文键本身，悬停同样提示
    return k;
}

enum {
    SendParamKeyRole = Qt::UserRole + 31,
    SendParamSavedRole = Qt::UserRole + 32,
    SendParamTouchedRole = Qt::UserRole + 33,
    SendParamPlaceholderRole = Qt::UserRole + 34,
};

bool isSendParamExplicitEmptySentinel(const QString& text) {
    const QString t = text.trimmed();
    return t == QLatin1String("-") || t == QStringLiteral("(空)") || t == QStringLiteral("（空）");
}

void applySendParamValueCell(QTableWidgetItem* valItem, const QString& text, bool fromSavedIni, bool asPlaceholder) {
    if (!valItem)
        return;
    valItem->setText(text);
    valItem->setData(SendParamTouchedRole, false);
    valItem->setData(SendParamPlaceholderRole, asPlaceholder);
    QFont font = valItem->font();
    font.setItalic(asPlaceholder);
    valItem->setFont(font);
    if (asPlaceholder) {
        valItem->setForeground(QBrush(QColor(0x88, 0x88, 0x88)));
        valItem->setToolTip(QStringLiteral("灰色斜体为参考默认值，尚未写入 ini；直接保存不会写入该项。"
                                             "若需显式空值：改过后留空保存，或填「-」。"));
    } else {
        valItem->setForeground(QBrush());
        if (fromSavedIni && text.trimmed().isEmpty())
            valItem->setToolTip(QStringLiteral("已显式保存为空值（Param_键=）"));
        else
            valItem->setToolTip(QString());
    }
}

QTableWidgetItem* makeSendParamValueItem(const QString& text, bool fromSavedIni, bool asPlaceholder) {
    auto* item = new QTableWidgetItem();
    applySendParamValueCell(item, text, fromSavedIni, asPlaceholder);
    return item;
}

void applySendParamNameCell(QTableWidgetItem* nameItem, const QString& key) {
    if (!nameItem)
        return;
    const QString k = key.trimmed();
    const QString zh = sendParamKeyZhLabel(k);
    nameItem->setData(SendParamKeyRole, k);
    if (k.isEmpty()) {
        nameItem->setText(QString());
    } else if (zh == k) {
        nameItem->setText(k);
    } else {
        nameItem->setText(QStringLiteral("%1  (%2)").arg(zh, k));
    }
    nameItem->setFlags((nameItem->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable) & ~Qt::ItemIsEditable);
    nameItem->setToolTip(k.isEmpty() ? QStringLiteral("双击可填写英文参数名")
                                      : QStringLiteral("英文参数名：%1").arg(k));
}

QTableWidgetItem* makeSendParamNameItem(const QString& key) {
    auto* item = new QTableWidgetItem();
    applySendParamNameCell(item, key);
    return item;
}

void configureSendParamTable(QTableWidget* table, bool namedKeys) {
    if (!table)
        return;
    table->clear();
    table->setRowCount(0);
    if (namedKeys) {
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels(
            {QStringLiteral("参数说明 (英文键)"), QStringLiteral("参数值（灰斜体=未写入；改后空=显式空）")});
        table->setColumnWidth(0, 320);
    } else {
        table->setColumnCount(1);
        table->setHorizontalHeaderLabels({QStringLiteral("参数值")});
    }
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void setSendParamTableFromMap(QTableWidget* table, const QVariantMap& map) {
    QSignalBlocker blocker(table);
    configureSendParamTable(table, true);
    QStringList keys = map.keys();
    keys.sort(Qt::CaseInsensitive);
    for (const QString& key : keys) {
        const int r = table->rowCount();
        table->insertRow(r);
        QTableWidgetItem* nameItem = makeSendParamNameItem(key);
        nameItem->setData(SendParamSavedRole, true);
        table->setItem(r, 0, nameItem);
        table->setItem(r, 1, makeSendParamValueItem(map.value(key).toString(), true, false));
    }
    if (table->rowCount() == 0) {
        table->insertRow(0);
        table->setItem(0, 0, makeSendParamNameItem(QString()));
        table->setItem(0, 1, makeSendParamValueItem(QString(), false, false));
    }
}

/** 按模板列出全部参数字段；userMap 覆盖已有值，模板缺省值仅作灰色占位提示。 */
void setSendParamTableFromMapWithTemplate(QTableWidget* table, const QVariantMap& userMap,
                                          const QVariantMap& templateMap) {
    QSignalBlocker blocker(table);
    configureSendParamTable(table, true);
    QStringList ordered;
    for (const QString& k : templateMap.keys()) {
        if (!k.isEmpty() && !ordered.contains(k))
            ordered.append(k);
    }
    QStringList extras;
    for (const QString& k : userMap.keys()) {
        if (!k.isEmpty() && !ordered.contains(k))
            extras.append(k);
    }
    extras.sort(Qt::CaseInsensitive);
    ordered.append(extras);

    for (const QString& key : ordered) {
        const int r = table->rowCount();
        table->insertRow(r);
        QTableWidgetItem* nameItem = makeSendParamNameItem(key);
        if (userMap.contains(key)) {
            nameItem->setData(SendParamSavedRole, true);
            table->setItem(r, 0, nameItem);
            table->setItem(r, 1, makeSendParamValueItem(userMap.value(key).toString(), true, false));
            continue;
        }
        table->setItem(r, 0, nameItem);
        const QString tmplVal = templateMap.value(key).toString();
        const bool asPlaceholder = templateMap.contains(key) && !tmplVal.isEmpty();
        table->setItem(r, 1, makeSendParamValueItem(asPlaceholder ? tmplVal : QString(), false, asPlaceholder));
    }
    if (table->rowCount() == 0) {
        table->insertRow(0);
        table->setItem(0, 0, makeSendParamNameItem(QString()));
        table->setItem(0, 1, makeSendParamValueItem(QString(), false, false));
    }
}

/** 配置程控电源：设备缺省 SCPI 参数模板。 */
QVariantMap defaultVisaConfigureParamMap(ScpiDeviceRoute route) {
    QVariantMap map;
    if (route == ScpiDeviceRoute::Agilent66319d) {
        map.insert(QStringLiteral("visaAddress"), QStringLiteral("GPIB0::7::INSTR"));
        map.insert(QStringLiteral("voltage"), QStringLiteral("5.0"));
        map.insert(QStringLiteral("current"), QStringLiteral("3.0"));
        map.insert(QStringLiteral("currentRange"), QStringLiteral("3"));
        map.insert(QStringLiteral("scpiSetVoltageCmd"), QStringLiteral("VOLT %1"));
        map.insert(QStringLiteral("scpiSetCurrentCmd"), QStringLiteral("CURR %1"));
        map.insert(QStringLiteral("scpiOutputOnCmd"), QStringLiteral("OUTP ON"));
        map.insert(QStringLiteral("scpiOutputOffCmd"), QStringLiteral("OUTP OFF"));
        map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS:VOLT:DC?"));
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS:CURR:DC?"));
        map.insert(QStringLiteral("scpiSetCurrentRangeCmd"), QStringLiteral("SENS:CURR:RANG %1"));
        // 一拖多两工位共一台双通道电源：在步骤里配 sharedPair + visaAddress0/1（不走上位机设置）
        map.insert(QStringLiteral("sharedPair"), QStringLiteral("true"));
        map.insert(QStringLiteral("stationsPerDevice"), QStringLiteral("2"));
        map.insert(QStringLiteral("visaAddress0"), QStringLiteral("GPIB0::7::INSTR"));
        map.insert(QStringLiteral("visaAddress1"), QStringLiteral("GPIB0::8::INSTR"));
        map.insert(QStringLiteral("scpiChannelSelectCmd"), QStringLiteral("INST OUT%1"));
        return map;
    }
    map.insert(QStringLiteral("visaAddress"), QStringLiteral("TCPIP::localhost::5026::SOCKET"));
    map.insert(QStringLiteral("voltage"), QStringLiteral("12.0"));
    map.insert(QStringLiteral("current"), QStringLiteral("2.5"));
    map.insert(QStringLiteral("scpiSetVoltageCmd"),
               QStringLiteral("SOURce1:VOLTage:LEVel:IMMediate:AMPLitude %1"));
    map.insert(QStringLiteral("scpiSetCurrentCmd"), QStringLiteral("SOURce1:CURRent:LIMit:VALue %1"));
    map.insert(QStringLiteral("scpiOutputOnCmd"), QStringLiteral("OUTPut1:STATe ON"));
    map.insert(QStringLiteral("scpiOutputOffCmd"), QStringLiteral("OUTPut1:STATe OFF"));
    map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEASure1:VOLTage:DC?"));
    map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEASure1:CURRent:DC?"));
    return map;
}

QVariantMap defaultVisaReadCurrentParamMap(ScpiDeviceRoute route) {
    QVariantMap map;
    if (route == ScpiDeviceRoute::Agilent66319d) {
        map.insert(QStringLiteral("currentRange"), QStringLiteral("3"));
        map.insert(QStringLiteral("scpiSetCurrentRangeCmd"), QStringLiteral("SENS:CURR:RANG %1"));
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS:CURR:DC?"));
    } else {
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEASure1:CURRent:DC?"));
    }
    map.insert(QStringLiteral("sampleDurationMs"), QStringLiteral("3000"));
    map.insert(QStringLiteral("sampleIntervalMs"), QStringLiteral("200"));
    return map;
}

QVariantMap sendParamDefaultMapForCmd(TestCaseSendChannel channel, const QString& device, const QString& cmdName) {
    if (channel == TestCaseSendChannel::Scpi) {
        const ScpiDeviceRoute route = ScpiPeriphCmdCatalog::deviceFromIni(device);
        if (route == ScpiDeviceRoute::HuilingWfp60h || route == ScpiDeviceRoute::Agilent66319d) {
            if (cmdName == QLatin1String("ConfigureProgrammablePower"))
                return defaultVisaConfigureParamMap(route);
            if (cmdName == QLatin1String("ReadProgrammableCurrent"))
                return defaultVisaReadCurrentParamMap(route);
            if (cmdName == QLatin1String("ReadProgrammableVoltage")) {
                QVariantMap map;
                if (route == ScpiDeviceRoute::Agilent66319d)
                    map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS:VOLT:DC?"));
                else
                    map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEASure1:VOLTage:DC?"));
                return map;
            }
            if (cmdName == QLatin1String("SendRawLine")) {
                return QVariantMap{{QStringLiteral("line"), QString()}};
            }
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Modbus
        && ModbusPeriphCmdCatalog::deviceFromIni(device) == ModbusDeviceRoute::MultiTempLoggerRtu) {
        if (cmdName == QLatin1String("ReadChannelTemp")) {
            QVariantMap map;
            map.insert(QStringLiteral("slaveAddr"), QStringLiteral("1"));
            map.insert(QStringLiteral("sharedPair"), QStringLiteral("true"));
            map.insert(QStringLiteral("stationsPerDevice"), QStringLiteral("2"));
            map.insert(QStringLiteral("channelsPerStation"), QStringLiteral("6"));
            map.insert(QStringLiteral("tempComName0"), QStringLiteral("COM10"));
            map.insert(QStringLiteral("tempComName1"), QStringLiteral("COM11"));
            map.insert(QStringLiteral("tempBaudRate"), QStringLiteral("115200"));
            map.insert(QStringLiteral("sampleDurationMs"), QStringLiteral("20000"));
            map.insert(QStringLiteral("sampleIntervalMs"), QStringLiteral("500"));
            return map;
        }
        if (cmdName == QLatin1String("SendRaw")) {
            return QVariantMap{{QStringLiteral("txHex"), QStringLiteral("01 03 00 12 00 02 64 0E")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Product) {
        if (cmdName == QLatin1String("RootSuctionTest")) {
            return QVariantMap{{QStringLiteral("switch"), QStringLiteral("1")},
                               {QStringLiteral("mode"), QStringLiteral("1")},
                               {QStringLiteral("level"), QStringLiteral("8")}};
        }
        if (cmdName == QLatin1String("SuctionMode")) {
            return QVariantMap{{QStringLiteral("enter"), QStringLiteral("1")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Dongle) {
        if (cmdName == QLatin1String("SampleSuctionSingle")) {
            return QVariantMap{{QStringLiteral("sampleDurationMs"), QStringLiteral("10000")},
                               {QStringLiteral("sampleIntervalMs"), QStringLiteral("20")},
                               {QStringLiteral("channel"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("SampleSuctionDual")) {
            return QVariantMap{{QStringLiteral("sampleDurationMs"), QStringLiteral("10000")},
                               {QStringLiteral("sampleIntervalMs"), QStringLiteral("20")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Fixture
        && FixturePcbaCmdCatalog::fixtureProtocolFromIni(device) == TestCaseFixtureProtocol::Asd9026a) {
        if (cmdName == QLatin1String("ConfigureProgrammablePower")) {
            return QVariantMap{{QStringLiteral("voltage"), QStringLiteral("4.0")},
                               {QStringLiteral("current"), QStringLiteral("2.0")},
                               {QStringLiteral("currentRange"), QStringLiteral("4")},
                               {QStringLiteral("txHex"),
                                QStringLiteral("02 21 10 0D 00 00 3D 09 00 00 00 07 D0 04 01 00 00 91 8F")}};
        }
        if (cmdName == QLatin1String("ConfigureCurrentMeasureRange")) {
            return QVariantMap{
                {QStringLiteral("currentRange"), QStringLiteral("4")},
                {QStringLiteral("txHex"), QStringLiteral("02 21 10 0D 00 00 00 00 00 00 00 00 00 04 00 00 00 14 A9")}};
        }
        if (cmdName == QLatin1String("ProgrammablePowerOutput")) {
            return QVariantMap{{QStringLiteral("enable"), QStringLiteral("1")},
                               {QStringLiteral("txHex"), QStringLiteral("02 11 04 03 01 00 00 9B C5")}};
        }
        if (cmdName == QLatin1String("ReadProgrammableVoltage")
            || cmdName == QLatin1String("ReadProgrammableCurrent")) {
            QVariantMap map{{QStringLiteral("txHex"), QStringLiteral("02 20 10 00 0D 96")}};
            if (cmdName == QLatin1String("ReadProgrammableCurrent")) {
                map.insert(QStringLiteral("sampleDurationMs"), QStringLiteral("3000"));
                map.insert(QStringLiteral("sampleIntervalMs"), QStringLiteral("200"));
            }
            return map;
        }
        if (cmdName == QLatin1String("SendRaw")) {
            return QVariantMap{{QStringLiteral("txHex"), QStringLiteral("02 11 04 03 01 00 00 9B C5")}};
        }
        return {};
    }
    return {};
}

void applySendParamTableWithTemplate(QTableWidget* table, TestCaseSendChannel channel, const QString& device,
                                     const QString& cmdName, const QVariantMap& userMap) {
    if (!table)
        return;
    const QVariantMap tmpl = sendParamDefaultMapForCmd(channel, device, cmdName);
    if (!tmpl.isEmpty())
        setSendParamTableFromMapWithTemplate(table, userMap, tmpl);
    else
        setSendParamTableFromMap(table, userMap);
}

void setSendParamTableFromString(QTableWidget* table, const QString& value) {
    configureSendParamTable(table, false);
    table->insertRow(0);
    table->setItem(0, 0, new QTableWidgetItem(value));
}

QVariantMap readSendParamMapFromTable(const QTableWidget* table) {
    QVariantMap map;
    if (!table || table->columnCount() < 2)
        return map;
    for (int r = 0; r < table->rowCount(); ++r) {
        const QTableWidgetItem* nameItem = table->item(r, 0);
        const QTableWidgetItem* valItem = table->item(r, 1);
        QString name;
        if (nameItem) {
            name = nameItem->data(SendParamKeyRole).toString().trimmed();
            if (name.isEmpty())
                name = nameItem->text().trimmed();
        }
        if (name.isEmpty())
            continue;

        QString val = valItem ? valItem->text().trimmed() : QString();
        const bool explicitEmptySentinel = isSendParamExplicitEmptySentinel(val);
        if (explicitEmptySentinel)
            val.clear();

        const bool fromSavedIni = nameItem && nameItem->data(SendParamSavedRole).toBool();
        const bool touched = valItem && valItem->data(SendParamTouchedRole).toBool();
        const bool placeholder = valItem && valItem->data(SendParamPlaceholderRole).toBool();

        // 未改动的灰色模板占位：保存时不写入 ini
        if (placeholder && !touched && !explicitEmptySentinel)
            continue;

        if (val.isEmpty()) {
            if (fromSavedIni || touched || explicitEmptySentinel)
                map.insert(name, QString());
            continue;
        }
        map.insert(name, val);
    }
    return map;
}

QString readSendParamStringFromTable(const QTableWidget* table) {
    if (!table || table->rowCount() <= 0)
        return {};
    const QTableWidgetItem* item = table->item(0, 0);
    return item ? item->text().trimmed() : QString();
}

bool isFixtureMachineIndexPlaceholder(const QVariant& param) {
    if (param.userType() == QMetaType::QString) {
        const QString s = param.toString().trimmed();
        return s.isEmpty() || s.compare(QStringLiteral("$INDEX"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${INDEX}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$SLOT"), Qt::CaseInsensitive) == 0;
    }
    return !param.isValid() || param.toInt() == 0;
}

int comboIndexByData(const QComboBox* box, const QString& data) {
    for (int i = 0; i < box->count(); ++i) {
        if (box->itemData(i).toString() == data)
            return i;
    }
    return -1;
}

QString comboData(const QComboBox* box) {
    return box->currentData().toString();
}

void fillActionCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("设置"), QStringLiteral("Set"));
    box->addItem(QStringLiteral("读取"), QStringLiteral("Get"));
}

void fillSendChannelCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("产品蓝牙通信"), QStringLiteral("Product"));
    box->addItem(QStringLiteral("产品串口通信"), QStringLiteral("ProductSerial"));
    box->addItem(QStringLiteral("Dongle通信"), QStringLiteral("Dongle"));
    box->addItem(QStringLiteral("云端交互"), QStringLiteral("Cloud"));
    box->addItem(QStringLiteral("治具通信"), QStringLiteral("Fixture"));
    box->addItem(QStringLiteral("Modbus通信"), QStringLiteral("Modbus"));
    box->addItem(QStringLiteral("SCPI通信"), QStringLiteral("Scpi"));
}

void fillProductProtocolCombo(QComboBox* box) {
    box->clear();
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qfctp),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qfctp));
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qpb),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qpb));
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qroot),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qroot));
}

void fillFixtureProtocolCombo(QComboBox* box) {
    box->clear();
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::Pcba),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Pcba));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::Asd9026a),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Asd9026a));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::Xwd),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Xwd));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::JieliBtBox),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::JieliBtBox));
}

void fillProtocolComboForChannel(QComboBox* box, TestCaseSendChannel channel) {
    box->clear();
    if (channel == TestCaseSendChannel::Fixture) {
        fillFixtureProtocolCombo(box);
    } else if (channel == TestCaseSendChannel::Modbus) {
        for (const QString& dev : ModbusPeriphCmdCatalog::allDeviceKeys()) {
            box->addItem(ModbusPeriphCmdCatalog::deviceUiLabel(ModbusPeriphCmdCatalog::deviceFromIni(dev)), dev);
        }
    } else if (channel == TestCaseSendChannel::Scpi) {
        for (const QString& dev : ScpiPeriphCmdCatalog::allDeviceKeys()) {
            box->addItem(ScpiPeriphCmdCatalog::deviceUiLabel(ScpiPeriphCmdCatalog::deviceFromIni(dev)), dev);
        }
    } else {
        fillProductProtocolCombo(box);
    }
}

TestCaseProductProtocol productProtocolFromComboData(const QString& data) {
    return DeviceCmdCatalog::productProtocolFromIni(data);
}

TestCaseSendAction sendActionFromComboData(const QString& data) {
    return data.compare(QStringLiteral("Get"), Qt::CaseInsensitive) == 0 ? TestCaseSendAction::Get
                                                                         : TestCaseSendAction::Set;
}

void fillDeviceCmdCombo(QComboBox* box, TestCaseSendChannel channel, TestCaseSendAction action,
                        const QString& device, const QString& keepCmdIfMissing = QString()) {
    box->clear();
    QVector<QPair<QString, QString>> items;
    if (channel == TestCaseSendChannel::Dongle) {
        items.reserve(DongleCmdCatalog::allDongleCmdNames(action).size());
        for (const QString& name : DongleCmdCatalog::allDongleCmdNames(action))
            items.append({DongleCmdCatalog::dongleCmdUiLabel(name), name});
    } else if (channel == TestCaseSendChannel::Cloud) {
        items.reserve(TupleCmdCatalog::allTupleCmdNames(action).size());
        for (const QString& name : TupleCmdCatalog::allTupleCmdNames(action))
            items.append({TupleCmdCatalog::tupleCmdUiLabel(name), name});
    } else if (channel == TestCaseSendChannel::ProductSerial) {
        items.reserve(ProductSerialCmdCatalog::allProductSerialCmdNames().size());
        for (const QString& name : ProductSerialCmdCatalog::allProductSerialCmdNames()) {
            if (ProductSerialCmd cmd; ProductSerialCmdCatalog::productSerialCmdFromName(name, cmd) && ProductSerialCmdCatalog::isCmdForAction(cmd, action))
                items.append({ProductSerialCmdCatalog::productSerialCmdUiLabel(name), name});
        }
    } else if (channel == TestCaseSendChannel::Fixture) {
        const TestCaseFixtureProtocol proto = FixturePcbaCmdCatalog::fixtureProtocolFromIni(device);
        if (proto == TestCaseFixtureProtocol::Asd9026a) {
            items.reserve(Asd9026aCmdCatalog::allAsd9026aCmdNames(action).size());
            for (const QString& name : Asd9026aCmdCatalog::allAsd9026aCmdNames(action))
                items.append({Asd9026aCmdCatalog::asd9026aCmdUiLabel(name), name});
        } else if (proto == TestCaseFixtureProtocol::Xwd) {
            items.reserve(XwdRawFixtureCmdCatalog::allXwdRawFixtureCmdNames(action).size());
            for (const QString& name : XwdRawFixtureCmdCatalog::allXwdRawFixtureCmdNames(action))
                items.append({XwdRawFixtureCmdCatalog::xwdRawFixtureCmdUiLabel(name), name});
        } else if (proto == TestCaseFixtureProtocol::JieliBtBox) {
            items.reserve(JieliBtBoxCmdCatalog::allJieliBtBoxCmdNames(action).size());
            for (const QString& name : JieliBtBoxCmdCatalog::allJieliBtBoxCmdNames(action))
                items.append({JieliBtBoxCmdCatalog::jieliBtBoxCmdUiLabel(name), name});
        } else {
            items.reserve(FixturePcbaCmdCatalog::allFixturePcbaCmdNames(action).size());
            for (const QString& name : FixturePcbaCmdCatalog::allFixturePcbaCmdNames(action))
                items.append({FixturePcbaCmdCatalog::fixturePcbaCmdUiLabel(name), name});
        }
    } else if (channel == TestCaseSendChannel::Modbus) {
        ModbusDeviceRoute devRoute = ModbusPeriphCmdCatalog::deviceFromIni(device);
        const QStringList names = ModbusPeriphCmdCatalog::allCmdNames(devRoute, action);
        items.reserve(names.size());
        for (const QString& name : names) {
            items.append({ModbusPeriphCmdCatalog::cmdUiLabel(devRoute, name), name});
        }
    } else if (channel == TestCaseSendChannel::Scpi) {
        ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(device);
        const QStringList names = ScpiPeriphCmdCatalog::allCmdNames(devRoute, action);
        items.reserve(names.size());
        for (const QString& name : names) {
            items.append({ScpiPeriphCmdCatalog::cmdUiLabel(devRoute, name), name});
        }
    } else {
        const QStringList names = DeviceCmdCatalog::allDeviceCmdNames(action);
        items.reserve(names.size());
        for (const QString& name : names)
            items.append({DeviceCmdCatalog::deviceCmdUiLabel(name), name});
    }
    std::sort(items.begin(), items.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    if (channel == TestCaseSendChannel::Product && !keepCmdIfMissing.isEmpty()) {
        bool found = false;
        for (const auto& item : items) {
            if (item.second == keepCmdIfMissing) {
                found = true;
                break;
            }
        }
        if (!found) {
            const QString label = DeviceCmdCatalog::deviceCmdUiLabel(keepCmdIfMissing) + QStringLiteral("（未登记）");
            items.prepend({label, keepCmdIfMissing});
        }
    }
    for (const auto& item : items)
        box->addItem(item.first, item.second);
}

TestCaseSendChannel sendChannelFromComboData(const QString& data) {
    if (data.compare(QStringLiteral("Dongle"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Dongle;
    if (data.compare(QStringLiteral("Cloud"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Cloud;
    if (data.compare(QStringLiteral("ProductSerial"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::ProductSerial;
    if (data.compare(QStringLiteral("Fixture"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Fixture;
    if (data.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Modbus;
    if (data.compare(QStringLiteral("Scpi"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Scpi;
    return TestCaseSendChannel::Product;
}

QString sendChannelComboData(TestCaseSendChannel channel) {
    if (channel == TestCaseSendChannel::Dongle)
        return QStringLiteral("Dongle");
    if (channel == TestCaseSendChannel::Cloud)
        return QStringLiteral("Cloud");
    if (channel == TestCaseSendChannel::ProductSerial)
        return QStringLiteral("ProductSerial");
    if (channel == TestCaseSendChannel::Fixture)
        return QStringLiteral("Fixture");
    if (channel == TestCaseSendChannel::Modbus)
        return QStringLiteral("Modbus");
    if (channel == TestCaseSendChannel::Scpi)
        return QStringLiteral("Scpi");
    return QStringLiteral("Product");
}

enum class SendCmdParamKind { None,
                              Int,
                              UInt,
                              JsonMap,
                              String };

struct SendCmdParamUi {
    bool valid = false;
    SendCmdParamKind kind = SendCmdParamKind::None;
    QString hint;
};

SendCmdParamKind sendParamUiKindFromSchema(DeviceCmdParamKind kind) {
    switch (kind) {
    case DeviceCmdParamKind::None:
        return SendCmdParamKind::None;
    case DeviceCmdParamKind::Int:
        return SendCmdParamKind::Int;
    case DeviceCmdParamKind::UInt:
        return SendCmdParamKind::UInt;
    case DeviceCmdParamKind::String:
        return SendCmdParamKind::String;
    case DeviceCmdParamKind::JsonMap:
        return SendCmdParamKind::JsonMap;
    }
    return SendCmdParamKind::None;
}

QString sendParamProtocolContext(TestCaseSendChannel channel, const QString& protocolComboData) {
    if (channel == TestCaseSendChannel::Fixture || channel == TestCaseSendChannel::Modbus
        || channel == TestCaseSendChannel::Scpi)
        return protocolComboData;
    return QString();
}

SendCmdParamUi sendCmdParamUiForName(const QString& name, TestCaseSendChannel channel, const QString& device = QString()) {
    SendCmdParamUi out;
    if (channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (DongleCmdCatalog::dongleCmdFromName(name, dongleCmd)) {
            DeviceCmdParamSchema schema;
            if (DongleCmdCatalog::paramSchemaFor(dongleCmd, schema)) {
                out.valid = true;
                out.hint = DongleCmdCatalog::paramUiHint(name);
                out.kind = sendParamUiKindFromSchema(schema.kind);
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (TupleCmdCatalog::tupleCmdFromName(name, tupleCmd)) {
            DeviceCmdParamSchema schema;
            if (TupleCmdCatalog::paramSchemaFor(tupleCmd, schema)) {
                out.valid = true;
                out.hint = TupleCmdCatalog::paramUiHint(name);
                out.kind = sendParamUiKindFromSchema(schema.kind);
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::ProductSerial) {
        ProductSerialCmd serialCmd;
        if (ProductSerialCmdCatalog::productSerialCmdFromName(name, serialCmd)) {
            DeviceCmdParamSchema schema;
            if (ProductSerialCmdCatalog::paramSchemaFor(serialCmd, schema)) {
                out.valid = true;
                out.hint = ProductSerialCmdCatalog::paramUiHint(name);
                out.kind = SendCmdParamKind::None;
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Fixture) {
        const TestCaseFixtureProtocol proto = FixturePcbaCmdCatalog::fixtureProtocolFromIni(device);
        if (proto == TestCaseFixtureProtocol::Asd9026a) {
            Asd9026aCmd asdCmd;
            if (Asd9026aCmdCatalog::asd9026aCmdFromName(name, asdCmd)) {
                DeviceCmdParamSchema schema;
                if (Asd9026aCmdCatalog::paramSchemaFor(asdCmd, schema)) {
                    out.valid = true;
                    out.hint = Asd9026aCmdCatalog::paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (proto == TestCaseFixtureProtocol::Xwd) {
            XwdRawFixtureCmd xwdCmd;
            if (XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(name, xwdCmd)) {
                DeviceCmdParamSchema schema;
                if (XwdRawFixtureCmdCatalog::paramSchemaFor(xwdCmd, schema)) {
                    out.valid = true;
                    out.hint = XwdRawFixtureCmdCatalog::paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (proto == TestCaseFixtureProtocol::JieliBtBox) {
            JieliBtBoxCmd jieliCmd;
            if (JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(name, jieliCmd)) {
                DeviceCmdParamSchema schema;
                if (JieliBtBoxCmdCatalog::paramSchemaFor(jieliCmd, schema)) {
                    out.valid = true;
                    out.hint = JieliBtBoxCmdCatalog::paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        FixturePcbaCmd fixtureCmd;
        if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(name, fixtureCmd)) {
            DeviceCmdParamSchema schema;
            if (FixturePcbaCmdCatalog::paramSchemaFor(fixtureCmd, schema)) {
                out.valid = true;
                out.hint = FixturePcbaCmdCatalog::paramUiHint(name);
                out.kind = sendParamUiKindFromSchema(schema.kind);
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Modbus) {
        ModbusDeviceRoute devRoute = ModbusPeriphCmdCatalog::deviceFromIni(device);
        const auto* row = ModbusCmdManifest::findByDeviceAndName(devRoute, name);
        if (row) {
            out.valid = true;
            out.hint = ModbusPeriphCmdCatalog::paramUiHint(devRoute, name);
            if (devRoute == ModbusDeviceRoute::InovanceH5uTcp || devRoute == ModbusDeviceRoute::GcSeriesTcp) {
                if (name == QLatin1String("ReadCoil")) {
                    out.kind = SendCmdParamKind::Int;
                } else if (name == QLatin1String("WriteCoil") ||
                           name == QLatin1String("ReadCoils") ||
                           name == QLatin1String("WaitCoilTrue") ||
                           name == QLatin1String("WaitCoilFalse") ||
                           name == QLatin1String("Connect")) {
                    out.kind = SendCmdParamKind::JsonMap;
                } else {
                    out.kind = SendCmdParamKind::None;
                }
            } else if (devRoute == ModbusDeviceRoute::MultiTempLoggerRtu) {
                // 开放报文与读温通道均走 Param_* map
                out.kind = SendCmdParamKind::JsonMap;
            } else {
                out.kind = SendCmdParamKind::None;
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Scpi) {
        ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(device);
        const auto* row = ScpiCmdManifest::findByDeviceAndName(devRoute, name);
        if (row) {
            out.valid = true;
            out.hint = ScpiPeriphCmdCatalog::paramUiHint(devRoute, name);
            if ((devRoute == ScpiDeviceRoute::HuilingWfp60h || devRoute == ScpiDeviceRoute::Agilent66319d)
                && (name == QLatin1String("ConfigureProgrammablePower")
                    || name == QLatin1String("ReadProgrammableVoltage")
                    || name == QLatin1String("ReadProgrammableCurrent")
                    || name == QLatin1String("InitializeProgrammablePower"))) {
                out.kind = SendCmdParamKind::JsonMap;
            } else if (name == QLatin1String("ProgrammablePowerOutput") ||
                       name == QLatin1String("ArbCycles")) {
                out.kind = SendCmdParamKind::Int;
            } else if (name == QLatin1String("ArbFile") ||
                       name == QLatin1String("SendRawLine") ||
                       name == QLatin1String("WriteLine") ||
                       name == QLatin1String("QueryLine") ||
                       name == QLatin1String("TxLevelDbm") ||
                       name == QLatin1String("FrequencyMhz")) {
                out.kind = SendCmdParamKind::String;
            } else {
                out.kind = SendCmdParamKind::None;
            }
        }
        return out;
    }
    DeviceCmd cmd;
    if (DeviceCmdCatalog::deviceCmdFromName(name, cmd)) {
        DeviceCmdParamSchema schema;
        if (DeviceCmdCatalog::paramSchemaFor(cmd, schema)) {
            out.valid = true;
            out.hint = DeviceCmdCatalog::paramUiHint(name);
            out.kind = sendParamUiKindFromSchema(schema.kind);
        }
    }
    return out;
}

void applySendParamHintToUi(const SendCmdParamUi& uiSchema, bool hasParam, QLabel* hintLabel, QTableWidget* paramTable,
                            QSpinBox* spinBox, QWidget* addRowBtn, QWidget* removeRowBtn, QWidget* restoreBtn,
                            bool hasDefaultTemplate) {
    if (!hintLabel)
        return;
    QString hintText = uiSchema.hint;
    const bool namedMap = uiSchema.valid && uiSchema.kind == SendCmdParamKind::JsonMap;
    if (namedMap) {
        const QString saveNote =
            QStringLiteral("灰色斜体为参考默认，保存时不写入；改过后留空保存=显式空值（写入 Param_键=）。"
                           "也可填「-」表示显式空值。误删字段可点「恢复默认参数表」。");
        if (!hintText.isEmpty())
            hintText += QStringLiteral("\n") + saveNote;
        else
            hintText = saveNote;
    }
    hintLabel->setText(hintText);
    hintLabel->setVisible(hasParam && uiSchema.valid && !hintText.isEmpty());
    hintLabel->setToolTip(hintText.isEmpty() ? QString() : hintText);

    if (addRowBtn)
        addRowBtn->setVisible(namedMap);
    if (removeRowBtn)
        removeRowBtn->setVisible(namedMap);
    if (restoreBtn)
        restoreBtn->setVisible(namedMap && hasDefaultTemplate);

    if (paramTable) {
        if (uiSchema.valid && !uiSchema.hint.isEmpty())
            paramTable->setToolTip(uiSchema.hint);
        else if (namedMap)
            paramTable->setToolTip(QStringLiteral("灰色斜体=未写入 ini 的参考默认；改过后空=显式空值。"));
        else
            paramTable->setToolTip(QString());
    }
    if (spinBox) {
        if (uiSchema.valid && uiSchema.kind == SendCmdParamKind::Int)
            spinBox->setToolTip(uiSchema.hint);
        else
            spinBox->setToolTip(QString());
    }
}

void applySendParamToUi(const SendCmdParamUi& uiSchema, const QVariant& param, QWidget* pageNone, QWidget* pageInt,
                        QWidget* pageJson, QStackedWidget* stack, QSpinBox* spinBox, QTableWidget* paramTable) {
    if (!uiSchema.valid || uiSchema.kind == SendCmdParamKind::None) {
        stack->setCurrentWidget(pageNone);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::Int || uiSchema.kind == SendCmdParamKind::UInt) {
        stack->setCurrentWidget(pageInt);
        int intVal = param.toInt();
        if (param.canConvert<QVariantMap>()) {
            const QVariantMap map = param.toMap();
            if (map.contains(QStringLiteral("int")))
                intVal = map.value(QStringLiteral("int")).toInt();
            else if (map.contains(QStringLiteral("value")))
                intVal = map.value(QStringLiteral("value")).toInt();
            else if (map.size() == 1)
                intVal = map.constBegin().value().toInt();
        }
        spinBox->setValue(intVal);
        return;
    }
    stack->setCurrentWidget(pageJson);
    if (uiSchema.kind == SendCmdParamKind::String) {
        // overlay 可能把 Param_string 读成 {string:xxx}，直接 toString() 会是空
        QString text;
        if (param.canConvert<QVariantMap>()) {
            const QVariantMap map = param.toMap();
            if (map.contains(QStringLiteral("string")))
                text = map.value(QStringLiteral("string")).toString();
            else if (map.size() == 1)
                text = map.constBegin().value().toString();
            else
                text = param.toString();
        } else {
            text = param.toString();
        }
        setSendParamTableFromString(paramTable, text);
    } else {
        setSendParamTableFromMap(paramTable, sendParamAsJsonMap(param));
    }
}

QVariant readSendParamFromUi(const SendCmdParamUi& uiSchema, QSpinBox* spinBox, QTableWidget* paramTable) {
    switch (uiSchema.kind) {
    case SendCmdParamKind::Int:
    case SendCmdParamKind::UInt:
        return spinBox->value();
    case SendCmdParamKind::String:
        return readSendParamStringFromTable(paramTable);
    case SendCmdParamKind::JsonMap:
        return readSendParamMapFromTable(paramTable);
    default:
        return {};
    }
}

void fillGateReportTypeCombo(QComboBox* box) {
    box->clear();
    for (const GateTypeDescriptor& t : GateRegistry::allTypeDescriptors())
        box->addItem(t.displayName, t.reportType);
}

void fillGateFieldCombo(QComboBox* box, const QString& reportType) {
    box->clear();
    GateTypeDescriptor desc;
    if (!GateRegistry::descriptorFor(reportType, desc))
        return;
    for (const GateFieldDescriptor& f : desc.fields)
        box->addItem(f.displayName, f.field);
}

QString gateOpToTableText(TestCaseGateOp op) {
    switch (op) {
    case TestCaseGateOp::Gt:
        return QStringLiteral("gt");
    case TestCaseGateOp::Lt:
        return QStringLiteral("lt");
    case TestCaseGateOp::Eq:
        return QStringLiteral("eq");
    case TestCaseGateOp::CompareVersions:
        return QStringLiteral("compareVersions");
    default:
        return QStringLiteral("range");
    }
}

TestCaseGateOp gateOpFromTableText(const QString& text) {
    const QString t = text.trimmed();
    if (t == QLatin1String("gt"))
        return TestCaseGateOp::Gt;
    if (t == QLatin1String("lt"))
        return TestCaseGateOp::Lt;
    if (t == QLatin1String("eq"))
        return TestCaseGateOp::Eq;
    if (t == QLatin1String("compareVersions"))
        return TestCaseGateOp::CompareVersions;
    return TestCaseGateOp::Range;
}

void initPeriphGateTable(QTableWidget* table) {
    GateTypeDescriptor desc;
    if (!GateRegistry::descriptorFor(QStringLiteral("ProtocolPeriphStateData"), desc))
        return;
    table->clear();
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("外设项"), QStringLiteral("期望值（等于）")});
    table->setRowCount(desc.fields.size());
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    for (int i = 0; i < desc.fields.size(); ++i) {
        auto* nameItem = new QTableWidgetItem(desc.fields.at(i).displayName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, desc.fields.at(i).field);
        table->setItem(i, 0, nameItem);
        table->setItem(i, 1, new QTableWidgetItem(QStringLiteral("0")));
    }
}

void initRangeMultiGateTable(QTableWidget* table, const QString& reportType) {
    GateTypeDescriptor desc;
    if (!GateRegistry::descriptorFor(reportType, desc))
        return;
    table->clear();
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({QStringLiteral("启用"), QStringLiteral("判定项"), QStringLiteral("方式(range/gt/lt/eq)"),
                                      QStringLiteral("最小值"), QStringLiteral("最大值"),
                                      QStringLiteral("期望值")});
    table->setRowCount(desc.fields.size());
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    for (int i = 0; i < desc.fields.size(); ++i) {
        auto* enableItem = new QTableWidgetItem();
        enableItem->setFlags((enableItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        enableItem->setCheckState(Qt::Unchecked);
        table->setItem(i, 0, enableItem);
        // read/writeMultiGates 从「判定项」列 UserRole 取字段名；须与这里一致
        auto* nameItem = new QTableWidgetItem(desc.fields.at(i).displayName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, desc.fields.at(i).field);
        table->setItem(i, 1, nameItem);
        table->setItem(i, 2, new QTableWidgetItem(QStringLiteral("range")));
        table->setItem(i, 3, new QTableWidgetItem(QStringLiteral("0")));
        table->setItem(i, 4, new QTableWidgetItem(QStringLiteral("0")));
        table->setItem(i, 5, new QTableWidgetItem());
    }
}

void initFixturePcbaGateTable(QTableWidget* table) {
    initRangeMultiGateTable(table, QStringLiteral("ProtocolFixturePcbaData"));
}

void fillGateOpCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("在范围内"), QStringLiteral("range"));
    box->addItem(QStringLiteral("大于"), QStringLiteral("gt"));
    box->addItem(QStringLiteral("小于"), QStringLiteral("lt"));
    box->addItem(QStringLiteral("等于"), QStringLiteral("eq"));
    box->addItem(QStringLiteral("版本比对"), QStringLiteral("compareVersions"));
}

const QHash<QString, QString>& hookDisplayNameMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("NoOp"), QStringLiteral("空操作（示例）")},
        {QStringLiteral("FreeWorkNoOpDemo"), QStringLiteral("示例步骤")},
        {QStringLiteral("JIG_CURRENT_READ"), QStringLiteral("读取治具电流测量值")},
        {QStringLiteral("DONGLE_SUCTION_ENABLE"), QStringLiteral("开启 dongle 吸力读取")},
        {QStringLiteral("DONGLE_SUCTION_DISABLE"), QStringLiteral("关闭 dongle 吸力读取")},
        {QStringLiteral("DONGLE_SUCTION_SAMPLE"), QStringLiteral("采集双通道吸力(旧Hook，请改用Dongle指令)")},
        {QStringLiteral("DONGLE_SUCTION_SAMPLE_SINGLE"), QStringLiteral("采集单通道吸力(旧Hook，请改用Dongle指令)")},
        {QStringLiteral("SN_WRITE_TAIL"), QStringLiteral("写入 SN 码")},
        {QStringLiteral("MAC_WRITE_ROOT"), QStringLiteral("写入 MAC 地址（Qroot）")},
        {QStringLiteral("PRINT_WHOLE_MACHINE_SN"), QStringLiteral("打印整机 SN 二维码")},
        {QStringLiteral("QR_SN_CONSISTENCY_CHECK"), QStringLiteral("二维码一致性校验（与开局SN比对）")},
        {QStringLiteral("PLC_MODBUS_CONN"), QStringLiteral("PLC Modbus 连接")},
        {QStringLiteral("PLC_V3_SWITCH_RIGHT_WHOLE"), QStringLiteral("PLC+V3 旋钮整步右旋")},
        {QStringLiteral("PLC_V3_SWITCH_DONE_RESET_M"), QStringLiteral("PLC+V3 旋钮测试完成 M 复位")},
        {QStringLiteral("PLC_V3_KEY_MODE"), QStringLiteral("PLC+V3 模式键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_PROGRAM"), QStringLiteral("PLC+V3 程序键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_SPEED"), QStringLiteral("PLC+V3 速度键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_RIGHT"), QStringLiteral("PLC+V3 右键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_START_PAUSE"), QStringLiteral("PLC+V3 开始暂停键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_LEFT"), QStringLiteral("PLC+V3 左键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_POWER"), QStringLiteral("PLC+V3 电源键触摸整步")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2402_1M"), QStringLiteral("并联 CMW 播放 2402 BLE1M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2440_1M"), QStringLiteral("并联 CMW 播放 2440 BLE1M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2480_1M"), QStringLiteral("并联 CMW 播放 2480 BLE1M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2402_2M"), QStringLiteral("并联 CMW 播放 2402 BLE2M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2440_2M"), QStringLiteral("并联 CMW 播放 2440 BLE2M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2480_2M"), QStringLiteral("并联 CMW 播放 2480 BLE2M")},
    };
    return map;
}

QString hookUiLabel(const QString& hookId) {
    const QString key = hookId.trimmed();
    const auto it = hookDisplayNameMap().constFind(key);
    if (it != hookDisplayNameMap().cend())
        return it.value();
    if (key.isEmpty())
        return QString();
    // 已注册但未维护中文名时显示 HookId，避免与「未实现」混淆
    if (TestCaseHookRegistry::contains(key))
        return QStringLiteral("%1（待补全显示名）").arg(key);
    return QStringLiteral("未登记流程：%1").arg(key);
}

void fillHookCombo(QComboBox* box) {
    box->clear();
    QVector<QPair<QString, QString>> items;
    for (const QString& id : TestCaseHookRegistry::hookIds())
        items.append({hookUiLabel(id), id});
    std::sort(items.begin(), items.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    for (const auto& item : items)
        box->addItem(item.first, item.second);
}

} // namespace

TestCaseEditDialog::TestCaseEditDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TestCaseEditDialog) {
    ui->setupUi(this);

    // 主体可滚动，底部保存/取消始终可见，避免矮屏点不到按钮
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* scrollContent = new QWidget(scroll);
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(ui->verticalLayout_root->spacing());
    const QList<QWidget*> sections = {ui->groupBox_meta, ui->groupBox_send, ui->groupBox_timing, ui->groupBox_gate,
                                      ui->groupBox_hook};
    for (QWidget* section : sections) {
        ui->verticalLayout_root->removeWidget(section);
        scrollLayout->addWidget(section);
    }
    scrollLayout->addStretch(1);
    scroll->setWidget(scrollContent);
    ui->verticalLayout_root->insertWidget(0, scroll);
    ui->verticalLayout_root->setStretch(0, 1);

    fillSendChannelCombo(ui->comboBox_sendChannel);
    fillProductProtocolCombo(ui->comboBox_productProtocol);
    fillActionCombo(ui->comboBox_action);
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, TestCaseSendChannel::Product, TestCaseSendAction::Set, QString());
    fillGateReportTypeCombo(ui->comboBox_gateReportType);
    fillGateOpCombo(ui->comboBox_gateOp);
    registerFreeWorkTestCaseHooks();
    registerQFreeWorkCatalogTestCaseHooks();
    fillHookCombo(ui->comboBox_hookId);

    if (QPushButton* saveBtn = ui->buttonBox->button(QDialogButtonBox::Save)) {
        saveBtn->setText(QStringLiteral("保存"));
    }
    if (QPushButton* cancelBtn = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelBtn->setText(QStringLiteral("取消"));
    }

    connect(ui->comboBox_sendChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onSendChannelChanged);
    connect(ui->comboBox_productProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onProductProtocolChanged);
    connect(ui->comboBox_action, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onSendActionChanged);
    connect(ui->comboBox_deviceCmd, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onDeviceCmdChanged);
    connect(ui->comboBox_gateReportType, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onGateReportTypeChanged);
    connect(ui->checkBox_gateEnabled, &QCheckBox::toggled, this, &TestCaseEditDialog::updateGateFieldsEnabled);
    connect(ui->checkBox_promptEnabled, &QCheckBox::toggled, this, &TestCaseEditDialog::updatePromptFieldsEnabled);
    connect(ui->checkBox_promptOnly, &QCheckBox::toggled, this, &TestCaseEditDialog::updatePromptFieldsEnabled);
    connect(ui->checkBox_hookEnabled, &QCheckBox::toggled, this, &TestCaseEditDialog::updateHookFieldsEnabled);
    connect(ui->comboBox_hookId, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onHookIdChanged);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (saveValidated())
            accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(ui->pushButton_addParamRow, &QPushButton::clicked, this, [this]() {
        if (!ui->tableWidget_sendParam || ui->tableWidget_sendParam->columnCount() < 2)
            return;
        bool ok = false;
        const QString key = QInputDialog::getText(this, QStringLiteral("添加参数"),
                                                  QStringLiteral("英文参数名（保存到步骤 ini）："),
                                                  QLineEdit::Normal, QString(), &ok)
                                .trimmed();
        if (!ok || key.isEmpty())
            return;
        const int r = ui->tableWidget_sendParam->rowCount();
        ui->tableWidget_sendParam->insertRow(r);
        ui->tableWidget_sendParam->setItem(r, 0, makeSendParamNameItem(key));
        ui->tableWidget_sendParam->setItem(r, 1, makeSendParamValueItem(QString(), false, false));
        ui->tableWidget_sendParam->setCurrentCell(r, 1);
        ui->tableWidget_sendParam->editItem(ui->tableWidget_sendParam->item(r, 1));
    });
    connect(ui->tableWidget_sendParam, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!item || !ui->tableWidget_sendParam || item->column() != 1)
            return;
        item->setData(SendParamTouchedRole, true);
        if (item->data(SendParamPlaceholderRole).toBool()) {
            item->setData(SendParamPlaceholderRole, false);
            QFont font = item->font();
            font.setItalic(false);
            item->setFont(font);
            item->setForeground(QBrush());
            item->setToolTip(QString());
        }
    });
    connect(ui->pushButton_removeParamRow, &QPushButton::clicked, this, [this]() {
        if (!ui->tableWidget_sendParam)
            return;
        const auto ranges = ui->tableWidget_sendParam->selectionModel()->selectedRows();
        QList<int> rows;
        for (const QModelIndex& idx : ranges)
            rows.append(idx.row());
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int r : rows)
            ui->tableWidget_sendParam->removeRow(r);
        if (ui->tableWidget_sendParam->columnCount() >= 2 && ui->tableWidget_sendParam->rowCount() == 0) {
            ui->tableWidget_sendParam->insertRow(0);
            ui->tableWidget_sendParam->setItem(0, 0, makeSendParamNameItem(QString()));
            ui->tableWidget_sendParam->setItem(0, 1, new QTableWidgetItem());
        }
    });
    connect(ui->pushButton_restoreParamDefaults, &QPushButton::clicked, this, [this]() {
        if (!ui->tableWidget_sendParam || ui->tableWidget_sendParam->columnCount() < 2)
            return;
        const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
        const QString device = comboData(ui->comboBox_productProtocol);
        const QString cmdName = comboData(ui->comboBox_deviceCmd);
        const QVariantMap tmpl = sendParamDefaultMapForCmd(channel, device, cmdName);
        if (tmpl.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("恢复默认参数表"),
                                     QStringLiteral("当前指令没有预置参数模板，请用「添加一行」自行填写。"));
            return;
        }
        const QVariantMap current = readSendParamMapFromTable(ui->tableWidget_sendParam);
        setSendParamTableFromMapWithTemplate(ui->tableWidget_sendParam, current, tmpl);
    });
    // 双击参数名列：修改英文键（界面仍只显示中文）
    connect(ui->tableWidget_sendParam, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (!ui->tableWidget_sendParam || ui->tableWidget_sendParam->columnCount() != 2 || column != 0)
            return;
        QTableWidgetItem* nameItem = ui->tableWidget_sendParam->item(row, 0);
        if (!nameItem)
            return;
        const QString oldKey = nameItem->data(SendParamKeyRole).toString();
        bool ok = false;
        const QString key = QInputDialog::getText(this, QStringLiteral("修改参数名"),
                                                  QStringLiteral("英文参数名（保存到步骤 ini）："),
                                                  QLineEdit::Normal, oldKey, &ok)
                                .trimmed();
        if (!ok || key.isEmpty())
            return;
        applySendParamNameCell(nameItem, key);
    });

    tableWidget_multiGates_ = new QTableWidget(ui->groupBox_gate);
    initPeriphGateTable(tableWidget_multiGates_);
    ui->formLayout_gate->addRow(tableWidget_multiGates_);
    tableWidget_multiGates_->setVisible(false);

    updateGateFieldsEnabled();
    updatePromptFieldsEnabled();
    updateHookFieldsEnabled();
    updateProductProtocolRowVisible();
    onDeviceCmdChanged(ui->comboBox_deviceCmd->currentIndex());
}

TestCaseEditDialog::~TestCaseEditDialog() {
    delete ui;
}

void TestCaseEditDialog::fitDialogToScreen() {
    QScreen* scr = screen();
    if (!scr && QGuiApplication::primaryScreen())
        scr = QGuiApplication::primaryScreen();
    if (!scr)
        return;
    const QRect avail = scr->availableGeometry();
    const int maxH = qMax(420, avail.height() - 48);
    const int maxW = qMax(480, avail.width() - 48);
    if (height() > maxH || width() > maxW)
        resize(qMin(width(), maxW), qMin(height(), maxH));
    QRect geo = geometry();
    if (geo.bottom() > avail.bottom())
        geo.moveBottom(avail.bottom());
    if (geo.top() < avail.top())
        geo.moveTop(avail.top());
    if (geo.right() > avail.right())
        geo.moveRight(avail.right());
    if (geo.left() < avail.left())
        geo.moveLeft(avail.left());
    setGeometry(geo);
}

void TestCaseEditDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    fitDialogToScreen();
}

bool TestCaseEditDialog::isFixturePcbaMultiGateMode() const {
    return ui->checkBox_gateEnabled->isChecked()
           && (comboData(ui->comboBox_gateReportType) == QLatin1String("ProtocolFixturePcbaData")
               || comboData(ui->comboBox_gateReportType) == QLatin1String("ProtocolJieliBtBoxData")
               || comboData(ui->comboBox_gateReportType) == QLatin1String("ProtocolDongleSuctionPeakData"));
}

bool TestCaseEditDialog::isPeriphMultiGateMode() const {
    return ui->checkBox_gateEnabled->isChecked() && comboData(ui->comboBox_gateReportType) == QLatin1String("ProtocolPeriphStateData");
}

bool TestCaseEditDialog::isMultiGateTableMode() const {
    return isPeriphMultiGateMode() || isFixturePcbaMultiGateMode();
}

void TestCaseEditDialog::rebuildMultiGateTable() {
    if (!tableWidget_multiGates_)
        return;
    if (isFixturePcbaMultiGateMode()) {
        const QString reportType = comboData(ui->comboBox_gateReportType);
        initRangeMultiGateTable(tableWidget_multiGates_, reportType);
    } else if (isPeriphMultiGateMode())
        initPeriphGateTable(tableWidget_multiGates_);
}

void TestCaseEditDialog::writePeriphGatesToTable(const QVector<TestCaseGate>& gates) {
    if (!tableWidget_multiGates_)
        return;
    for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
        QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 0);
        if (!nameItem)
            continue;
        const QString field = nameItem->data(Qt::UserRole).toString();
        QString expected = QStringLiteral("0");
        for (const TestCaseGate& g : gates) {
            if (g.field == field) {
                expected = g.expected.trimmed();
                if (expected.isEmpty() && g.op == TestCaseGateOp::Eq)
                    expected = QString::number(static_cast<int>(g.low));
                break;
            }
        }
        if (QTableWidgetItem* valItem = tableWidget_multiGates_->item(row, 1))
            valItem->setText(expected);
        else
            tableWidget_multiGates_->setItem(row, 1, new QTableWidgetItem(expected));
    }
}

void TestCaseEditDialog::writeMultiGatesToTable(const QVector<TestCaseGate>& gates) {
    if (!tableWidget_multiGates_)
        return;
    if (isFixturePcbaMultiGateMode()) {
        for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
            QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 1);
            if (!nameItem)
                continue;
            QString field = nameItem->data(Qt::UserRole).toString();
            if (field.isEmpty()) {
                if (QTableWidgetItem* enableItem = tableWidget_multiGates_->item(row, 0))
                    field = enableItem->data(Qt::UserRole).toString();
            }
            TestCaseGate matched;
            bool found = false;
            for (const TestCaseGate& g : gates) {
                if (g.field == field) {
                    matched = g;
                    found = true;
                    break;
                }
            }
            if (QTableWidgetItem* enableItem = tableWidget_multiGates_->item(row, 0)) {
                enableItem->setCheckState(found && matched.enabled ? Qt::Checked : Qt::Unchecked);
                // 保存时回写 SettingsKey，避免 UI 往返丢掉 BLE/LowRssi 等绑定
                enableItem->setData(Qt::UserRole + 1, found ? matched.lowSettingsKey : QString());
                enableItem->setData(Qt::UserRole + 2, found ? matched.highSettingsKey : QString());
                enableItem->setData(Qt::UserRole + 3, found ? matched.expectedSettingsKey : QString());
            }
            if (!found)
                continue;
            auto setCell = [&](int col, const QString& text) {
                if (QTableWidgetItem* item = tableWidget_multiGates_->item(row, col))
                    item->setText(text);
                else
                    tableWidget_multiGates_->setItem(row, col, new QTableWidgetItem(text));
            };
            setCell(2, gateOpToTableText(matched.op));
            // 有 LowSettingsKey/HighSettingsKey 时显示 SETTINGS 解析后的实际卡控范围
            double lowShow = matched.low;
            double highShow = matched.high;
            GateRegistry::resolveRangeBounds(matched, lowShow, highShow);
            setCell(3, QString::number(lowShow));
            setCell(4, QString::number(highShow));
            setCell(5, matched.expected);
        }
        return;
    }
    writePeriphGatesToTable(gates);
}

QVector<TestCaseGate> TestCaseEditDialog::readPeriphGatesFromTable() const {
    QVector<TestCaseGate> gates;
    if (!tableWidget_multiGates_)
        return gates;
    const QString reportType = comboData(ui->comboBox_gateReportType);
    for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
        QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 0);
        QTableWidgetItem* valItem = tableWidget_multiGates_->item(row, 1);
        if (!nameItem || !valItem)
            continue;
        TestCaseGate g;
        g.enabled = true;
        g.reportType = reportType;
        g.field = nameItem->data(Qt::UserRole).toString();
        g.op = TestCaseGateOp::Eq;
        g.expected = valItem->text().trimmed();
        g.low = g.expected.toDouble();
        g.high = g.low;
        gates.append(g);
    }
    return gates;
}

QVector<TestCaseGate> TestCaseEditDialog::readMultiGatesFromTable() const {
    if (!tableWidget_multiGates_)
        return {};
    if (isFixturePcbaMultiGateMode()) {
        QVector<TestCaseGate> gates;
        const QString reportType = comboData(ui->comboBox_gateReportType);
        for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
            QTableWidgetItem* enableItem = tableWidget_multiGates_->item(row, 0);
            QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 1);
            if (!nameItem)
                continue;
            const bool rowEnabled = enableItem && enableItem->checkState() == Qt::Checked;
            QString field = nameItem->data(Qt::UserRole).toString();
            if (field.isEmpty() && enableItem)
                field = enableItem->data(Qt::UserRole).toString();
            auto cellText = [&](int col) -> QString {
                if (QTableWidgetItem* item = tableWidget_multiGates_->item(row, col))
                    return item->text().trimmed();
                return QString();
            };
            const QString opText = cellText(2);
            const QString lowText = cellText(3);
            const QString highText = cellText(4);
            const QString expectedText = cellText(5);
            TestCaseGate g;
            g.enabled = rowEnabled;
            g.reportType = reportType;
            g.field = field;
            g.op = gateOpFromTableText(opText.isEmpty() ? QStringLiteral("range") : opText);
            g.low = lowText.toDouble();
            g.high = highText.toDouble();
            g.expected = expectedText;
            if (enableItem) {
                g.lowSettingsKey = enableItem->data(Qt::UserRole + 1).toString();
                g.highSettingsKey = enableItem->data(Qt::UserRole + 2).toString();
                g.expectedSettingsKey = enableItem->data(Qt::UserRole + 3).toString();
            }
            if (g.op == TestCaseGateOp::Eq && g.expected.isEmpty())
                g.expected = QString::number(static_cast<int>(g.low));
            const bool unusedDefault =
                (g.op == TestCaseGateOp::Range && g.expected.isEmpty() && qFuzzyIsNull(g.low) && qFuzzyIsNull(g.high)
                 && g.lowSettingsKey.isEmpty() && g.highSettingsKey.isEmpty());
            if (!rowEnabled || unusedDefault)
                continue;
            gates.append(g);
        }
        return gates;
    }
    return readPeriphGatesFromTable();
}

void TestCaseEditDialog::updateGateFieldsEnabled() {
    const bool on = ui->checkBox_gateEnabled->isChecked();
    const bool multiTable = on && isMultiGateTableMode();
    ui->label_gateReportType->setVisible(on);
    ui->comboBox_gateReportType->setVisible(on);
    if (tableWidget_multiGates_)
        tableWidget_multiGates_->setVisible(multiTable);
    const bool single = on && !multiTable;
    ui->label_gateField->setVisible(single);
    ui->comboBox_gateField->setVisible(single);
    ui->label_gateOp->setVisible(single);
    ui->comboBox_gateOp->setVisible(single);
    ui->label_gateLow->setVisible(single);
    ui->lineEdit_gateLow->setVisible(single);
    ui->label_gateHigh->setVisible(single);
    ui->lineEdit_gateHigh->setVisible(single);
    ui->label_gateExpected->setVisible(single);
    ui->lineEdit_gateExpected->setVisible(single);
}

void TestCaseEditDialog::updatePromptFieldsEnabled() {
    const bool on = ui->checkBox_promptEnabled->isChecked();
    ui->checkBox_promptOnly->setVisible(on);
    ui->label_promptText->setVisible(on);
    ui->plainTextEdit_promptText->setVisible(on);
    if (!on)
        ui->checkBox_promptOnly->setChecked(false);
    // 纯空白提醒时隐藏测试指令区；Hook 本身也会隐藏，二者互不覆盖
    if (!ui->checkBox_hookEnabled->isChecked())
        ui->groupBox_send->setVisible(!on || !ui->checkBox_promptOnly->isChecked());
}

void TestCaseEditDialog::updateHookFieldsEnabled() {
    const bool on = ui->checkBox_hookEnabled->isChecked();
    const bool promptOnly =
        ui->checkBox_promptEnabled->isChecked() && ui->checkBox_promptOnly->isChecked();
    ui->label_hookId->setVisible(on);
    ui->comboBox_hookId->setVisible(on);
    // Hook 或纯空白提醒时隐藏测试指令区（吸力采样请用 Dongle/SampleSuction* + Gate）
    ui->groupBox_send->setTitle(QStringLiteral("测试指令"));
    ui->label_param->setText(QStringLiteral("指令参数"));
    ui->groupBox_send->setVisible(!on && !promptOnly);
    if (!on) {
        updateProductProtocolRowVisible();
        onDeviceCmdChanged(ui->comboBox_deviceCmd->currentIndex());
        return;
    }
    updateSendParamVisibility(false);
}

void TestCaseEditDialog::onHookIdChanged(int) {
    updateHookFieldsEnabled();
}

void TestCaseEditDialog::updateSendParamVisibility(bool hasParam) {
    ui->label_param->setVisible(hasParam);
    ui->stackedWidget_param->setVisible(hasParam);
}

void TestCaseEditDialog::onGateReportTypeChanged(int) {
    fillGateFieldCombo(ui->comboBox_gateField, comboData(ui->comboBox_gateReportType));
    rebuildMultiGateTable();
    updateGateFieldsEnabled();
}

void TestCaseEditDialog::setStationContext(const QString& stationKey) {
    stationKey_ = stationKey.trimmed();
}

void TestCaseEditDialog::setFlowContext(const QVector<TestFlowItemEntry>& entries) {
    flowEntries_ = entries;
}

void TestCaseEditDialog::setDefinition(const TestCaseDefinition& def, const QString& storageKey) {
    originalCaseName_ = storageKey.trimmed().isEmpty() ? def.meta.name.trimmed() : storageKey.trimmed();
    ui->lineEdit_caseName->setText(def.meta.name);
    ui->lineEdit_mesTag->setText(def.meta.mesTag);
    ui->checkBox_promptEnabled->setChecked(def.meta.promptEnabled);
    ui->checkBox_promptOnly->setChecked(def.meta.promptOnly);
    ui->plainTextEdit_promptText->setPlainText(def.meta.promptText);

    const TestCaseSendAction action = def.send.action;
    ui->comboBox_action->blockSignals(true);
    const int actionIdx = comboIndexByData(ui->comboBox_action,
                                           action == TestCaseSendAction::Get ? QStringLiteral("Get")
                                                                             : QStringLiteral("Set"));
    if (actionIdx >= 0)
        ui->comboBox_action->setCurrentIndex(actionIdx);
    ui->comboBox_action->blockSignals(false);

    const TestCaseSendChannel channel = def.send.channel;
    ui->comboBox_sendChannel->blockSignals(true);
    ui->comboBox_deviceCmd->blockSignals(true);
    const int channelIdx = comboIndexByData(ui->comboBox_sendChannel, sendChannelComboData(channel));
    if (channelIdx >= 0)
        ui->comboBox_sendChannel->setCurrentIndex(channelIdx);
    ui->comboBox_action->setEnabled(channel != TestCaseSendChannel::ProductSerial);

    ui->comboBox_productProtocol->blockSignals(true);
    fillProtocolComboForChannel(ui->comboBox_productProtocol, channel);
    QString protoIni;
    if (channel == TestCaseSendChannel::Fixture)
        protoIni = FixturePcbaCmdCatalog::fixtureProtocolToIni(def.send.fixtureProtocol);
    else if (channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi)
        protoIni = def.send.device;
    else
        protoIni = DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol);
    const int protoIdx = comboIndexByData(ui->comboBox_productProtocol, protoIni);
    if (protoIdx >= 0)
        ui->comboBox_productProtocol->setCurrentIndex(protoIdx);
    ui->comboBox_productProtocol->blockSignals(false);
    updateProductProtocolRowVisible();

    fillDeviceCmdCombo(ui->comboBox_deviceCmd, channel, action, def.send.device, def.send.deviceCmd);
    const int cmdIdx = comboIndexByData(ui->comboBox_deviceCmd, def.send.deviceCmd);
    if (cmdIdx >= 0)
        ui->comboBox_deviceCmd->setCurrentIndex(cmdIdx);
    ui->comboBox_sendChannel->blockSignals(false);
    ui->comboBox_deviceCmd->blockSignals(false);
    onDeviceCmdChanged(cmdIdx);

    const QString protocolCtx = sendParamProtocolContext(channel, comboData(ui->comboBox_productProtocol));
    const SendCmdParamUi uiSchema = sendCmdParamUiForName(def.send.deviceCmd, channel, protocolCtx);
    QVariant paramForUi = def.send.param;
    if (channel == TestCaseSendChannel::Fixture && uiSchema.kind == SendCmdParamKind::Int && isFixtureMachineIndexPlaceholder(def.send.param))
        paramForUi = 0;
    applySendParamToUi(uiSchema, paramForUi, ui->page_paramNone, ui->page_paramInt, ui->page_paramJson,
                       ui->stackedWidget_param, ui->spinBox_intParam, ui->tableWidget_sendParam);
    if (uiSchema.kind == SendCmdParamKind::JsonMap) {
        applySendParamTableWithTemplate(ui->tableWidget_sendParam, channel, comboData(ui->comboBox_productProtocol),
                                        def.send.deviceCmd, sendParamAsJsonMap(paramForUi));
    }
    const bool hasParamTemplate =
        !sendParamDefaultMapForCmd(channel, comboData(ui->comboBox_productProtocol), def.send.deviceCmd).isEmpty();
    applySendParamHintToUi(uiSchema, uiSchema.valid && uiSchema.kind != SendCmdParamKind::None,
                           ui->label_sendParamHint, ui->tableWidget_sendParam, ui->spinBox_intParam,
                           ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                           ui->pushButton_restoreParamDefaults, hasParamTemplate);
    lastSendParamCmdKey_ = sendChannelComboData(channel) + QLatin1Char('|')
        + comboData(ui->comboBox_productProtocol) + QLatin1Char('|') + def.send.deviceCmd;

    ui->spinBox_delayBefore->setValue(def.timing.delayBeforeMs);
    ui->spinBox_delayAfter->setValue(def.timing.delayAfterMs);
    {
        int timeoutMs = def.timing.commandTimeoutMs;
        if (timeoutMs <= 0)
            timeoutMs = def.gate.enabled ? 8000 : 300;
        ui->spinBox_commandTimeout->setValue(timeoutMs);
    }

    ui->checkBox_gateEnabled->setChecked(def.gate.enabled);
    const int typeIdx = comboIndexByData(ui->comboBox_gateReportType, def.gate.reportType);
    if (typeIdx >= 0)
        ui->comboBox_gateReportType->setCurrentIndex(typeIdx);
    onGateReportTypeChanged(0);
    const int fieldIdx = comboIndexByData(ui->comboBox_gateField, def.gate.field);
    if (fieldIdx >= 0)
        ui->comboBox_gateField->setCurrentIndex(fieldIdx);

    const QString opKey = def.gate.op == TestCaseGateOp::Gt ? QStringLiteral("gt")
        : def.gate.op == TestCaseGateOp::Lt                 ? QStringLiteral("lt")
        : def.gate.op == TestCaseGateOp::Eq                 ? QStringLiteral("eq")
        : def.gate.op == TestCaseGateOp::CompareVersions    ? QStringLiteral("compareVersions")
                                                            : QStringLiteral("range");
    const int opIdx = comboIndexByData(ui->comboBox_gateOp, opKey);
    if (opIdx >= 0)
        ui->comboBox_gateOp->setCurrentIndex(opIdx);

    ui->lineEdit_gateLow->setText(QString::number(def.gate.low));
    ui->lineEdit_gateHigh->setText(QString::number(def.gate.high));
    ui->lineEdit_gateExpected->setText(def.gate.expected);
    if ((def.gate.reportType == QLatin1String("ProtocolPeriphStateData")
         || def.gate.reportType == QLatin1String("ProtocolFixturePcbaData")
         || def.gate.reportType == QLatin1String("ProtocolJieliBtBoxData")
         || def.gate.reportType == QLatin1String("ProtocolDongleSuctionPeakData"))
        && !def.gates.isEmpty()) {
        rebuildMultiGateTable();
        writeMultiGatesToTable(def.gates);
    } else if ((def.gate.reportType == QLatin1String("ProtocolPeriphStateData")
                || def.gate.reportType == QLatin1String("ProtocolFixturePcbaData")
                || def.gate.reportType == QLatin1String("ProtocolJieliBtBoxData")
                || def.gate.reportType == QLatin1String("ProtocolDongleSuctionPeakData"))
               && def.gate.enabled) {
        rebuildMultiGateTable();
        writeMultiGatesToTable({def.gate});
    }
    ui->checkBox_hookEnabled->setChecked(def.hook.enabled);
    const int hookIdx = comboIndexByData(ui->comboBox_hookId, def.hook.hookId);
    if (hookIdx >= 0)
        ui->comboBox_hookId->setCurrentIndex(hookIdx);

    updateGateFieldsEnabled();
    updatePromptFieldsEnabled();
    updateHookFieldsEnabled();
}

TestCaseDefinition TestCaseEditDialog::definition() const {
    TestCaseDefinition def;
    def.meta.name = ui->lineEdit_caseName->text().trimmed();
    def.meta.displayName = def.meta.name;
    def.meta.mesTag = ui->lineEdit_mesTag->text().trimmed();
    def.meta.promptEnabled = ui->checkBox_promptEnabled->isChecked();
    def.meta.promptOnly = def.meta.promptEnabled && ui->checkBox_promptOnly->isChecked();
    def.meta.promptText = ui->plainTextEdit_promptText->toPlainText();

    def.send.action = comboData(ui->comboBox_action) == QLatin1String("Get") ? TestCaseSendAction::Get
                                                                             : TestCaseSendAction::Set;
    def.send.channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    if (def.send.channel == TestCaseSendChannel::Fixture) {
        def.send.fixtureProtocol =
            FixturePcbaCmdCatalog::fixtureProtocolFromIni(comboData(ui->comboBox_productProtocol));
    } else if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        def.send.device = comboData(ui->comboBox_productProtocol);
    } else {
        def.send.productProtocol = productProtocolFromComboData(comboData(ui->comboBox_productProtocol));
    }
    def.send.deviceCmd = comboData(ui->comboBox_deviceCmd);
    def.hook.enabled = ui->checkBox_hookEnabled->isChecked();
    def.hook.hookId = comboData(ui->comboBox_hookId);
    const QString protocolCtx =
        sendParamProtocolContext(def.send.channel, comboData(ui->comboBox_productProtocol));
    const SendCmdParamUi uiSchema =
        sendCmdParamUiForName(def.send.deviceCmd, def.send.channel, protocolCtx);
    def.send.param = readSendParamFromUi(uiSchema, ui->spinBox_intParam, ui->tableWidget_sendParam);
    if (def.send.channel == TestCaseSendChannel::Fixture && uiSchema.kind == SendCmdParamKind::Int
        && ui->spinBox_intParam->value() == 0)
        def.send.param = QStringLiteral("$INDEX");

    def.timing.delayBeforeMs = ui->spinBox_delayBefore->value();
    def.timing.delayAfterMs = ui->spinBox_delayAfter->value();
    def.timing.commandTimeoutMs = ui->spinBox_commandTimeout->value();
    def.gate.enabled = ui->checkBox_gateEnabled->isChecked();
    def.gate.reportType = comboData(ui->comboBox_gateReportType);
    def.gates.clear();
    if (def.gate.enabled && isMultiGateTableMode()) {
        def.gates = readMultiGatesFromTable();
        if (!def.gates.isEmpty()) {
            def.gate = def.gates.first();
            def.gate.enabled = true;
            def.gate.reportType = comboData(ui->comboBox_gateReportType);
            if (def.gates.size() > 1)
                def.gate.field = QStringLiteral("multi");
        }
    } else {
        def.gate.field = comboData(ui->comboBox_gateField);
        const QString op = comboData(ui->comboBox_gateOp);
        def.gate.op = op == QLatin1String("gt")      ? TestCaseGateOp::Gt
            : op == QLatin1String("lt")              ? TestCaseGateOp::Lt
            : op == QLatin1String("eq")              ? TestCaseGateOp::Eq
            : op == QLatin1String("compareVersions") ? TestCaseGateOp::CompareVersions
                                                     : TestCaseGateOp::Range;
        def.gate.low = ui->lineEdit_gateLow->text().toDouble();
        def.gate.high = ui->lineEdit_gateHigh->text().toDouble();
        def.gate.expected = ui->lineEdit_gateExpected->text();
        if (def.gate.enabled)
            def.gates.append(def.gate);
    }
    return def;
}

void TestCaseEditDialog::updateProductProtocolRowVisible() {
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const bool showProtocol =
        channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::Fixture ||
        channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi;
    ui->label_productProtocol->setVisible(showProtocol);
    ui->comboBox_productProtocol->setVisible(showProtocol);
    if (showProtocol) {
        if (channel == TestCaseSendChannel::Fixture)
            ui->label_productProtocol->setText(QStringLiteral("治具协议"));
        else if (channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi)
            ui->label_productProtocol->setText(QStringLiteral("目标外设"));
        else
            ui->label_productProtocol->setText(QStringLiteral("产品协议"));
    }
}

void TestCaseEditDialog::onSendChannelChanged(int) {
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    fillProtocolComboForChannel(ui->comboBox_productProtocol, channel);
    updateProductProtocolRowVisible();
    const bool serial = channel == TestCaseSendChannel::ProductSerial;
    ui->comboBox_action->setEnabled(!serial);
    if (serial) {
        const int setIdx = comboIndexByData(ui->comboBox_action, QStringLiteral("Set"));
        if (setIdx >= 0)
            ui->comboBox_action->setCurrentIndex(setIdx);
    }
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::onProductProtocolChanged(int) {
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::onSendActionChanged(int) {
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::refreshDeviceCmdCombo() {
    const QString previousCmd = comboData(ui->comboBox_deviceCmd);
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const TestCaseSendAction action = sendActionFromComboData(comboData(ui->comboBox_action));
    const QString device = comboData(ui->comboBox_productProtocol);
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, channel, action, device, previousCmd);
    int cmdIdx = comboIndexByData(ui->comboBox_deviceCmd, previousCmd);
    if (cmdIdx < 0 && ui->comboBox_deviceCmd->count() > 0)
        cmdIdx = 0;
    if (cmdIdx >= 0)
        ui->comboBox_deviceCmd->setCurrentIndex(cmdIdx);
    onDeviceCmdChanged(cmdIdx);
}

void TestCaseEditDialog::onDeviceCmdChanged(int) {
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const QString cmdName = comboData(ui->comboBox_deviceCmd);
    const QString device = comboData(ui->comboBox_productProtocol);
    const SendCmdParamUi uiSchema = sendCmdParamUiForName(cmdName, channel, device);
    const bool hasParam = uiSchema.valid && uiSchema.kind != SendCmdParamKind::None;
    updateSendParamVisibility(hasParam);
    if (!uiSchema.valid) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::None) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::Int || uiSchema.kind == SendCmdParamKind::UInt) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramInt);
        if (channel == TestCaseSendChannel::Fixture) {
            ui->spinBox_intParam->setRange(0, 15);
            ui->spinBox_intParam->setSpecialValueText(QStringLiteral("当前工位"));
        } else {
            ui->spinBox_intParam->setSpecialValueText(QString());
        }
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    if (uiSchema.kind != SendCmdParamKind::JsonMap && uiSchema.kind != SendCmdParamKind::String) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    ui->stackedWidget_param->setCurrentWidget(ui->page_paramJson);
    const QString cmdKey =
        sendChannelComboData(channel) + QLatin1Char('|') + device + QLatin1Char('|') + cmdName;
    const bool cmdChanged = (cmdKey != lastSendParamCmdKey_);
    lastSendParamCmdKey_ = cmdKey;
    const int expectCols = (uiSchema.kind == SendCmdParamKind::String) ? 1 : 2;
    // 指令切换时重置表格；同指令重复进入（如 setDefinition 后再调）则保留已填内容
    if (cmdChanged || ui->tableWidget_sendParam->columnCount() != expectCols) {
        if (uiSchema.kind == SendCmdParamKind::String)
            setSendParamTableFromString(ui->tableWidget_sendParam, QString());
        else
            applySendParamTableWithTemplate(ui->tableWidget_sendParam, channel, device, cmdName, {});
    }
    const bool hasParamTemplate = !sendParamDefaultMapForCmd(channel, device, cmdName).isEmpty();
    applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam, ui->spinBox_intParam,
                           ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                           ui->pushButton_restoreParamDefaults, hasParamTemplate);
}

bool TestCaseEditDialog::saveValidated() {
    const TestCaseDefinition def = definition();
    QStringList errors;
    if (!TestCaseValidator::validateCase(def, errors)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), errors.join(QStringLiteral("\r\n")));
        return false;
    }
    if (stationKey_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("请先在「测试流程编排」顶部选择工站，再保存步骤参数到该工站 profiles/.../steps/ 目录。"));
        return false;
    }
    QVector<TestFlowItemEntry> flowEntries = flowEntries_;
    if (flowEntries.isEmpty())
        flowEntries = TestCaseStore::loadStationFlowItems(stationKey_);
    QStringList flowErrors;
    if (!TestCaseValidator::validateFlowMesTags(stationKey_, flowEntries, flowErrors, originalCaseName_, &def)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), flowErrors.join(QStringLiteral("\r\n")));
        return false;
    }
    if (!TestCaseStore::saveCaseForStation(stationKey_, def)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入配置文件"));
        return false;
    }
    // 工站内改名：只清理本工站 profile 旧步骤文件，禁止删除总步骤库模板
    if (!originalCaseName_.isEmpty() && originalCaseName_ != def.meta.name) {
        const QString oldOverride = TestCasePaths::profileStepOverridePath(stationKey_, originalCaseName_);
        if (QFile::exists(oldOverride))
            QFile::remove(oldOverride);
    }
    return true;
}
