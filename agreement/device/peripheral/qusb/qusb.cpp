#include "qusb.h"
#include "qmodbus_pdu.h"

#include <QDebug>
#include <QSerialPort>
#include <QStringList>
#include <QVector>
#include <cmath>

namespace {
constexpr int kModbusMaxBufferedLen = 15;
constexpr int kReg0HighIndex = 3;
constexpr int kReg0LowIndex = 4;
constexpr int kReg1HighIndex = 5;
constexpr int kReg1LowIndex = 6;
constexpr int kUnitExponentIndex = 8;

quint16 readBigEndianU16(const QByteArray &buffer, int highIndex, int lowIndex)
{
    return (static_cast<quint8>(buffer.at(highIndex)) << 8) |
           static_cast<quint8>(buffer.at(lowIndex));
}

bool isLikelyAsciiLine(const QByteArray &buffer)
{
    if (buffer.isEmpty())
    {
        return false;
    }
    for (char c : buffer)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc == '\r' || uc == '\n' || uc == '\t')
        {
            continue;
        }
        if (uc < 0x20 || uc > 0x7E)
        {
            return false;
        }
    }
    return true;
}
}   // namespace
#if _MSC_VER >= 1600
    #pragma execution_character_set(push, "utf-8")
#endif

Qusb::Qusb(QSerialPort *parent) :
QSerialPort(parent), serialPort(parent)
{

    if (!serialPort) {
        qDebug() << "Qat指向一个空指针";
        return;
    }


    registerCommand();
}

Qusb::~Qusb() = default;

void Qusb::registerCommand()
{

    // 在这里注册命令即可
    commandList["CURR"] = std::bind(&Qusb::CONFigureFUNCtion, this, std::placeholders::_1);

}
void Qusb::CONFigureFUNCtion(QString p)
{
    Q_UNUSED(p);
    qDebug() << "当前是电流模式";
}

void Qusb::parseCmd(const QByteArray &byte)
{
    if (!byte.isEmpty())
    {
        qDebug().noquote() << "USB RX:" << QString::fromLatin1(byte.toHex(' ').toUpper());
    }
    switch (resolveProtocolForInput(byte))
    {
    case ProtocolType::Scpi:
        processScpiData(byte);
        break;
    case ProtocolType::HqModbus:
        processModbusRTUData(byte);
        break;
    case ProtocolType::LxModbus:
        processlxModbusRTUData(byte);
        break;
    case ProtocolType::Auto:
        processScpiData(byte);
        break;
    }
}

bool Qusb::configureProgrammablePower(double voltageV, double currentA)
{
    if (!serialPort || !serialPort->isOpen())
    {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi)
    {
        qDebug() << "当前协议不支持程控电源SCPI配置";
        return false;
    }

    sendCmd(protocolConfig_.scpiSetVoltageCmd.arg(QString::number(voltageV, 'f', 3)));
    sendCmd(protocolConfig_.scpiSetCurrentCmd.arg(QString::number(currentA, 'f', 3)));
    return true;
}

bool Qusb::setProgrammablePowerOutput(bool enable)
{
    if (!serialPort || !serialPort->isOpen())
    {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi)
    {
        qDebug() << "当前协议不支持程控电源输出控制";
        return false;
    }
    sendCmd(enable ? protocolConfig_.scpiOutputOnCmd : protocolConfig_.scpiOutputOffCmd);
    return true;
}

bool Qusb::readProgrammablePowerVoltage()
{
    if (!serialPort || !serialPort->isOpen())
    {
        return false;
    }
    // 串口：下一行 SCPI 回包在 processCmd 中按 pending 解析并 emit
    pendingProgPowerRead_ = ProgrammablePowerReadPending::Voltage;
    sendCmd(protocolConfig_.scpiReadVoltageCmd);
    return true;
}

bool Qusb::readProgrammablePowerCurrent()
{
    if (!serialPort || !serialPort->isOpen())
    {
        return false;
    }
    pendingProgPowerRead_ = ProgrammablePowerReadPending::Current;
    sendCmd(protocolConfig_.scpiReadCurrentCmd);
    return true;
}

bool Qusb::initializeProgrammablePower()
{
    if (!serialPort || !serialPort->isOpen())
    {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi)
    {
        qDebug() << "当前协议不支持程控电源初始化";
        return false;
    }
    sendCmd("*RST");
    return configureProgrammablePower(protocolConfig_.scpiPowerVoltageV, protocolConfig_.scpiPowerCurrentA);
}

void Qusb::processScpiData(const QByteArray &byte)
{
    foreach(char c, byte)
    {
        if (c == '\r' || c == '\n')
        {
            const QString line = cmd.trimmed();
            cmd.clear();
            if (!line.isEmpty()) {
                processCmd(line, parameter);
            }
            continue;
        }
        cmd += c;
        if (c == ';')
        {
            const QString line = cmd.trimmed();
            cmd.clear();
            if (!line.isEmpty()) {
                processCmd(line, parameter);
            }
        }
    }
}

void Qusb::setProtocolConfig(const ProtocolConfig &config)
{
    protocolConfig_ = config;
}

Qusb::ProtocolConfig Qusb::protocolConfig() const
{
    return protocolConfig_;
}

void Qusb::set(UsbCmd cmd, const QVariant& data)
{
    switch (cmd)
    {
    case UsbCmd::PowerActionCmd:
        sendPowerInstruction(static_cast<PowerAction>(data.toInt()));
        return;
    case UsbCmd::ProtocolConfigCmd:
    {
        ProtocolConfig cfg = protocolConfig_;
        const QVariantMap m = data.toMap();
        if (m.contains(QStringLiteral("protocol")))
            cfg.protocol = static_cast<ProtocolType>(m.value(QStringLiteral("protocol")).toInt());
        if (m.contains(QStringLiteral("luxshareMachineId")))
            cfg.luxshareMachineId = m.value(QStringLiteral("luxshareMachineId")).toInt();
        setProtocolConfig(cfg);
        return;
    }
    case UsbCmd::SendScpiLine:
        sendCmd(data.toString());
        return;
    case UsbCmd::ConfigureProgrammablePower:
    {
        const QVariantMap m = data.toMap();
        configureProgrammablePower(m.value(QStringLiteral("voltage")).toDouble(),
                                   m.value(QStringLiteral("current")).toDouble());
        return;
    }
    case UsbCmd::ProgrammablePowerOutput:
        setProgrammablePowerOutput(data.toBool());
        return;
    case UsbCmd::ReadProgrammablePowerVoltageCmd:
        readProgrammablePowerVoltage();
        return;
    case UsbCmd::ReadProgrammablePowerCurrentCmd:
        readProgrammablePowerCurrent();
        return;
    case UsbCmd::InitializeProgrammablePowerCmd:
        initializeProgrammablePower();
        return;
    }
}

void Qusb::get(UsbCmd cmd, const QVariant& param)
{
    Q_UNUSED(param);
    switch (cmd)
    {
    case UsbCmd::PowerActionCmd:
        return;
    case UsbCmd::ProtocolConfigCmd:
        return;
    case UsbCmd::SendScpiLine:
        return;
    case UsbCmd::ConfigureProgrammablePower:
        return;
    case UsbCmd::ProgrammablePowerOutput:
        return;
    case UsbCmd::ReadProgrammablePowerVoltageCmd:
        readProgrammablePowerVoltage();
        return;
    case UsbCmd::ReadProgrammablePowerCurrentCmd:
        readProgrammablePowerCurrent();
        return;
    case UsbCmd::InitializeProgrammablePowerCmd:
        initializeProgrammablePower();
        return;
    }
}

bool Qusb::sendCustomMessage(const QVariantMap& map)
{
    const QString action = map.value(QStringLiteral("action")).toString().trimmed().toLower();
    if (action == QStringLiteral("poweraction")) {
        set(UsbCmd::PowerActionCmd, map.value(QStringLiteral("value")));
        return true;
    }
    if (action == QStringLiteral("sendscpiline")) {
        set(UsbCmd::SendScpiLine, map.value(QStringLiteral("line")));
        return true;
    }
    if (action == QStringLiteral("configureprogrammablepower")) {
        QVariantMap payload;
        payload.insert(QStringLiteral("voltage"), map.value(QStringLiteral("voltage")));
        payload.insert(QStringLiteral("current"), map.value(QStringLiteral("current")));
        set(UsbCmd::ConfigureProgrammablePower, payload);
        return true;
    }
    if (action == QStringLiteral("programmablepoweroutput")) {
        set(UsbCmd::ProgrammablePowerOutput, map.value(QStringLiteral("enable")));
        return true;
    }
    if (action == QStringLiteral("readprogrammablepowervoltage")) {
        get(UsbCmd::ReadProgrammablePowerVoltageCmd);
        return true;
    }
    if (action == QStringLiteral("readprogrammablepowercurrent")) {
        get(UsbCmd::ReadProgrammablePowerCurrentCmd);
        return true;
    }
    if (action == QStringLiteral("initializeprogrammablepower")) {
        get(UsbCmd::InitializeProgrammablePowerCmd);
        return true;
    }
    if (action == QStringLiteral("setprotocolconfig")) {
        set(UsbCmd::ProtocolConfigCmd, map);
        return true;
    }
    return false;
}

bool Qusb::sendPowerInstruction(PowerAction action)
{
    if (!serialPort || !serialPort->isOpen())
    {
        qDebug() << "Qusb: 未打开 usb 串口，无法发送数据";
        return false;
    }

    switch (resolveProtocolForOutput())
    {
    case ProtocolType::Scpi:
        return handleScpiAction(action);
    case ProtocolType::HqModbus:
        return handleHqAction(action);
    case ProtocolType::LxModbus:
        return handleLxAction(action);
    case ProtocolType::Auto:
        return handleScpiAction(action);
    }
    return false;
}

Qusb::ProtocolType Qusb::resolveProtocolForInput(const QByteArray &input) const
{
    if (protocolConfig_.protocol != ProtocolType::Auto)
    {
        return protocolConfig_.protocol;
    }
    if (isLikelyAsciiLine(input))
    {
        return ProtocolType::Scpi;
    }
    if (protocolConfig_.luxshareMachineId > 0)
    {
        return ProtocolType::LxModbus;
    }
    return ProtocolType::HqModbus;
}

Qusb::ProtocolType Qusb::resolveProtocolForOutput() const
{
    if (protocolConfig_.protocol != ProtocolType::Auto)
    {
        return protocolConfig_.protocol;
    }
    return ProtocolType::Scpi;
}

bool Qusb::handleScpiAction(PowerAction action)
{
    switch (action)
    {
    case PowerAction::ConfigurePowerSupply:
        sendCONF(protocolConfig_.scpiCurrentType, protocolConfig_.scpiCurrentMode, protocolConfig_.scpiRange);
        return true;
    case PowerAction::ReadMeasurement:
        getMEASure(QString());
        return true;
    case PowerAction::ReadConfiguration:
        getCONF(QString());
        return true;
    case PowerAction::InitializeDevice:
        sendCmd("*RST");
        return true;
    case PowerAction::ReadProgrammablePowerVoltage:
        return readProgrammablePowerVoltage();
    case PowerAction::ReadProgrammablePowerCurrent:
        return readProgrammablePowerCurrent();
    case PowerAction::ReadsuctionData:
        qDebug() << "SCPI 未实现 ReadsuctionData";
        return false;
    }
    return false;
}

bool Qusb::handleHqAction(PowerAction action)
{
    switch (action)
    {
    case PowerAction::ConfigurePowerSupply:
    case PowerAction::InitializeDevice:
        sethqMEASure();
        return true;
    case PowerAction::ReadMeasurement:
        gethqMEASure();
        return true;
    case PowerAction::ReadConfiguration:
        qDebug() << "华勤Modbus协议暂无读取配置指令，已跳过";
        return false;
    case PowerAction::ReadsuctionData:
    case PowerAction::ReadProgrammablePowerVoltage:
    case PowerAction::ReadProgrammablePowerCurrent:
        qDebug() << "华勤Modbus协议不支持该程控电源动作";
        return false;
    }
    return false;
}

bool Qusb::handleLxAction(PowerAction action)
{
    switch (action)
    {
    case PowerAction::ReadMeasurement:
        getlxMEASure(protocolConfig_.luxshareMachineId);
        return true;
    case PowerAction::ConfigurePowerSupply:
    case PowerAction::ReadConfiguration:
    case PowerAction::InitializeDevice:
        qDebug() << "立讯协议当前仅支持读取测量值";
        return false;
    case PowerAction::ReadsuctionData:
    case PowerAction::ReadProgrammablePowerVoltage:
    case PowerAction::ReadProgrammablePowerCurrent:
        qDebug() << "立讯协议不支持该程控电源动作";
        return false;
    }
    return false;
}

void Qusb::emitProgrammablePowerReadIfPending(const QString& scpiLine)
{
    if (pendingProgPowerRead_ == ProgrammablePowerReadPending::None) {
        return;
    }
    const QString t = scpiLine.trimmed();
    bool ok = false;
    const double v = t.toDouble(&ok);
    const ProgrammablePowerReadPending p = pendingProgPowerRead_;
    pendingProgPowerRead_ = ProgrammablePowerReadPending::None;
    if (p == ProgrammablePowerReadPending::Voltage) {
        emit programmablePowerVoltageRead(v, ok);
    } else if (p == ProgrammablePowerReadPending::Current) {
        emit programmablePowerCurrentRead(v, ok);
    }
}

void Qusb::processCmd(QString cmd, QString parameter)
{
    auto it = commandList.find(cmd);
    if (it != commandList.end())
    {
        // 调用回调函数
        it->second(parameter);
    }
    else if (pendingProgPowerRead_ != ProgrammablePowerReadPending::None)
    {
        // 程控电源读 V/I 后发「?」得到的纯数值行，勿走电流表 send_ammeter_data
        emitProgrammablePowerReadIfPending(cmd);
    }
    else
    {
        emit send_ammeter_data(cmd);   // 发给欣旺达静态电流处理
        qDebug() << "不支持的命令 : " << cmd;
    }
}
void Qusb::sendCmd(QString cmd)
{
    if (!serialPort || !serialPort->isOpen())
    {
        qDebug() << "Qusb: 未打开 usb 串口，无法发送数据";
        return;
    }

    cmd += "\r\n";

    const QByteArray data = cmd.toLocal8Bit();
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    serialPort->write(data);
}

void Qusb::sendCONF(QString current, QString dc, QString range)
{
    // 构建 CONF 命令字符串
    QString command = "CONF:" + current + ":" + dc + " " + range;

    // 调用 sendCmd 函数发送命令
    sendCmd(command);
}

void Qusb::getCONF(QString mac)
{
    Q_UNUSED(mac);
    QString s;
    // s = "CONFigure:RANGe?";
    // sendCmd(s);
    s = "CONFigure:FUNCtion?";
    sendCmd(s);
}
void Qusb::getMEASure(QString mac)
{
    Q_UNUSED(mac);
    QString s = "MEASure:CURRent:DC? 500e-3";
        sendCmd(s);
}
void Qusb::gethqMEASure()
{
    QString s = "01030000000305CB";   // 测量数值

    QByteArray data = QByteArray::fromHex(s.toLatin1());
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    serialPort->write(data);
}
void Qusb::getlxMEASure(int mechine)
{
    static const QStringList machineCmdList = {
        "",                    "010300000002c40b", "020300000002C438", "030300000002C5E9",
        "040300000002C45E",    "050300000002C58F", "060300000002C5BC"};
    if (mechine <= 0 || mechine >= machineCmdList.size())
    {
        qDebug() << "不支持的立讯设备号:" << mechine;
        return;
    }

    QByteArray data = QByteArray::fromHex(machineCmdList.at(mechine).toLatin1());
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    serialPort->write(data);
}
void Qusb::sethqMEASure()
{
    QString s = "010600030006F9C8";   // 设置波特率115200
    QByteArray data = QByteArray::fromHex(s.toLatin1());
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    serialPort->write(data);
}
quint16 calculateCRC(const QByteArray &data)
{
    return QModbusPdu::crc16ModbusRtuBigEndian(data);
}
void Qusb::processModbusRTUData(const QByteArray &response)
{
    qDebug() << "Received Modbus RTU response frame: " << response.toHex().toUpper();
    quint8 byteCount = 0;
    const bool valid = QModbusPdu::validateRtuFrame(response, &byteCount);
    const quint16 crcExpected = readBigEndianU16(response, response.length() - 2, response.length() - 1);
    const quint16 crcCalculated = calculateCRC(response.left(response.length() - 2));

    qDebug() << "Length: " << response.length() << " Byte Count: " << byteCount
             << " Expected CRC: " << QString::number(crcExpected, 16).toUpper()
             << " Calculated CRC: " << QString::number(crcCalculated, 16).toUpper();

    if (!valid)
    {
        qDebug() << "Invalid Modbus RTU frame";
        return;
    }

    const quint16 reg0 = readBigEndianU16(response, kReg0HighIndex, kReg0LowIndex);
    const quint16 reg1 = readBigEndianU16(response, kReg1HighIndex, kReg1LowIndex);
    const qint8 unitExponent = static_cast<qint8>(response.at(kUnitExponentIndex));
    const qint32 currentValue = (reg0 << 16) | reg1;
    const double current_uA = currentValue * std::pow(10, unitExponent) * 0.000000001;
    qDebug() << ": Current Value = " << current_uA << "A";

    emit send_ammeter_data(QString::number(current_uA));
}

void Qusb::processlxModbusRTUData(const QByteArray &response)
{
    qDebug() << "收到的数据为: " << response.toHex().toUpper();
    data = data + response;
    if (data.length() > kModbusMaxBufferedLen)
        data = 0;

    qDebug() << "Received Modbus RTU data frame: " << data.toHex().toUpper();
    quint8 byteCount = 0;
    const bool valid = QModbusPdu::validateRtuFrame(data, &byteCount);
    qDebug() << "数据长度：" << byteCount;

    const quint16 crcExpected = readBigEndianU16(data, data.length() - 2, data.length() - 1);
    const quint16 crcCalculated = calculateCRC(data.left(data.length() - 2));
    qDebug() << "Length: " << data.length()
             << " Expected CRC: " << QString::number(crcExpected, 16).toUpper()
             << " Calculated CRC: " << QString::number(crcCalculated, 16).toUpper();
    if (!valid)
    {
        qDebug() << "Invalid Modbus RTU frame";
        return;
    }

    const quint16 reg0 = readBigEndianU16(data, kReg0HighIndex, kReg0LowIndex);
    const quint16 reg1 = readBigEndianU16(data, kReg1HighIndex, kReg1LowIndex);
    const qint32 currentValue = (reg0 << 16) | reg1;
    const double current_uA = currentValue;
    qDebug() << "Current Value = " << current_uA << "uA";

    emit send_ammeter_data(QString::number(current_uA));   // 立讯的发送
    data = 0;
}
