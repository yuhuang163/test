#include "qusb.h"
#include <QDebug>
#include <QMessageBox>
#include <QSerialPort>
#include <QStringList>
#include <QVector>
#include <cmath>

namespace {
constexpr int kModbusMinFrameLen = 5;
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

bool isExpectedModbusLength(const QByteArray &buffer, quint8 byteCount)
{
    return buffer.length() == kModbusMinFrameLen + byteCount;
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

// 避免未连接时 sendPowerInstruction / sendCmd 在循环里反复弹 QMessageBox
bool g_qusbUsbNotOpenDialogShown = false;
bool g_qusbVisaNotReadyDialogShown = false;

void qusbResetUsbNotOpenDialogIfOpen(QSerialPort* sp)
{
    if (sp && sp->isOpen())
        g_qusbUsbNotOpenDialogShown = false;
}

void qusbWarnUsbSerialNotOpenOnce()
{
    qDebug() << "Qusb: 未打开 usb 串口，无法发送数据";
    if (g_qusbUsbNotOpenDialogShown)
        return;
    g_qusbUsbNotOpenDialogShown = true;
    QMessageBox::warning(nullptr, QStringLiteral("警告"),
                          QStringLiteral("未打开 usb 串口\n无法发送数据！"));
}

void qusbWarnVisaNotConnectedOnce()
{
    qDebug() << "Qusb: VISA 资源未连接，无法发送电源指令";
    if (g_qusbVisaNotReadyDialogShown)
        return;
    g_qusbVisaNotReadyDialogShown = true;
    QMessageBox::warning(nullptr, QStringLiteral("警告"),
                          QStringLiteral("VISA 资源未连接，无法发送电源指令！"));
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

Qusb::~Qusb()
{
    closeVisaConnection();
}

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
    case ProtocolType::Byd:
        processModbusRTUData(byte);
        break;
    case ProtocolType::Auto:
        processScpiData(byte);
        break;
    }
}

bool Qusb::configureProgrammablePower(double voltageV, double currentA)
{
    if (!isVisaScpiEnabled() && (!serialPort || !serialPort->isOpen()))
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
    if (!isVisaScpiEnabled() && (!serialPort || !serialPort->isOpen()))
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
    if (!isVisaScpiEnabled() && (!serialPort || !serialPort->isOpen()))
    {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi)
    {
        qDebug() << "当前协议不支持程控电源电压读取";
        return false;
    }
    sendCmd(protocolConfig_.scpiReadVoltageCmd);
    return true;
}

bool Qusb::readProgrammablePowerCurrent()
{
    if (!isVisaScpiEnabled() && (!serialPort || !serialPort->isOpen()))
    {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi)
    {
        qDebug() << "当前协议不支持程控电源电流读取";
        return false;
    }
    sendCmd(protocolConfig_.scpiReadCurrentCmd);
    return true;
}

bool Qusb::initializeProgrammablePower()
{
    if (!isVisaScpiEnabled() && (!serialPort || !serialPort->isOpen()))
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
            // 遇到\r或\n时停止追加字符
            break;
        }
        dataQueue.push_back((uint8_t)c);
    }
    while (dataQueue.isEmpty() == false)
    {
        char c = dataQueue.dequeue();
        cmd += c;
    }
    processCmd(cmd, parameter);
    cmd.clear();
}

void Qusb::setProtocolConfig(const ProtocolConfig &config)
{
    if (protocolConfig_.scpiUseVisa != config.scpiUseVisa ||
        protocolConfig_.scpiVisaAddress != config.scpiVisaAddress)
    {
        closeVisaConnection();
    }
    protocolConfig_ = config;
}

Qusb::ProtocolConfig Qusb::protocolConfig() const
{
    return protocolConfig_;
}

bool Qusb::sendPowerInstruction(PowerAction action)
{
    qusbResetUsbNotOpenDialogIfOpen(serialPort);

    if (isVisaScpiEnabled())
    {
        if (!ensureVisaConnected())
        {
            qusbWarnVisaNotConnectedOnce();
            return false;
        }
        g_qusbVisaNotReadyDialogShown = false;
    }
    else if (!serialPort || !serialPort->isOpen())
    {
        qusbWarnUsbSerialNotOpenOnce();
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
    case ProtocolType::Byd:
        return handlebydAction(action);
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
    }
    return false;
}

bool Qusb::handlebydAction(PowerAction action)
{
    switch (action)
    {
    case PowerAction::ReadMeasurement:
        getMEASure(QString());
        return true;
    case PowerAction::ConfigurePowerSupply:
        // BYD 电源配置：统一入口调度到 SCPI 指令链（WFP60H 等）
        sendCONF(protocolConfig_.scpiCurrentType, protocolConfig_.scpiCurrentMode, protocolConfig_.scpiRange);
        sendCmd(protocolConfig_.scpiSetVoltageCmd.arg(QString::number(protocolConfig_.scpiPowerVoltageV, 'f', 3)));
        sendCmd(protocolConfig_.scpiSetCurrentCmd.arg(QString::number(protocolConfig_.scpiPowerCurrentA, 'f', 3)));
        return true;
    case PowerAction::ReadConfiguration:
        // BYD 电源读配置
        getCONF(QString());
        return true;
    case PowerAction::InitializeDevice:
        // BYD 电源初始化
        sendCmd("*RST");
        return true;
    }
    return false;
}

void Qusb::processCmd(QString cmd, QString parameter)
{
    auto it = commandList.find(cmd);
    if (it != commandList.end())
    {
        // 调用回调函数
        it->second(parameter);
    }
    else
    {
        emit send_ammeter_data(cmd);   // 发给欣旺达静态电流处理
        qDebug() << "不支持的命令 : " << cmd;
    }
}
void Qusb::sendCmd(QString cmd)
{
    if (isVisaScpiEnabled())
    {
        visaWrite(cmd);
        return;
    }

    qusbResetUsbNotOpenDialogIfOpen(serialPort);
    if (!serialPort || !serialPort->isOpen())
    {
        qusbWarnUsbSerialNotOpenOnce();
        return;
    }

    cmd += "\r\n";

    serialPort->write(cmd.toLocal8Bit());
}

bool Qusb::isVisaScpiEnabled() const
{
    return protocolConfig_.scpiUseVisa &&
           !protocolConfig_.scpiVisaAddress.trimmed().isEmpty() &&
           resolveProtocolForOutput() == ProtocolType::Scpi;
}

bool Qusb::ensureVisaConnected()
{
    if (!isVisaScpiEnabled())
    {
        return false;
    }
#ifdef HAVE_NI_VISA
    if (visaInst_ != VI_NULL)
    {
        return true;
    }
    if (visaRm_ == VI_NULL)
    {
        const ViStatus rmStatus = viOpenDefaultRM(&visaRm_);
        if (rmStatus < VI_SUCCESS)
        {
            qDebug() << "VISA打开资源管理器失败，status=" << rmStatus;
            visaRm_ = VI_NULL;
            return false;
        }
    }

    QByteArray addr = protocolConfig_.scpiVisaAddress.trimmed().toLocal8Bit();
    const ViStatus openStatus = viOpen(visaRm_, (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &visaInst_);
    if (openStatus < VI_SUCCESS)
    {
        qDebug() << "VISA打开设备失败，address=" << protocolConfig_.scpiVisaAddress << "status=" << openStatus;
        visaInst_ = VI_NULL;
        return false;
    }
    viSetAttribute(visaInst_, VI_ATTR_TMO_VALUE, 3000);
    qDebug() << "VISA设备已连接:" << protocolConfig_.scpiVisaAddress;
    return true;
#else
    qDebug() << "当前构建未启用NI-VISA（HAVE_NI_VISA），无法使用VISA地址通讯";
    return false;
#endif
}

void Qusb::closeVisaConnection()
{
#ifdef HAVE_NI_VISA
    if (visaInst_ != VI_NULL)
    {
        viClose(visaInst_);
        visaInst_ = VI_NULL;
    }
    if (visaRm_ != VI_NULL)
    {
        viClose(visaRm_);
        visaRm_ = VI_NULL;
    }
#endif
}

bool Qusb::visaWrite(const QString &cmd)
{
    if (!ensureVisaConnected())
    {
        return false;
    }
#ifdef HAVE_NI_VISA
    QByteArray payload = cmd.toLocal8Bit();
    if (!payload.endsWith('\n'))
    {
        payload.append('\n');
    }
    ViUInt32 writeCount = 0;
    const ViStatus status = viWrite(visaInst_, reinterpret_cast<ViBuf>(payload.data()),
                                    static_cast<ViUInt32>(payload.size()), &writeCount);
    if (status < VI_SUCCESS)
    {
        qDebug() << "VISA写入失败:" << cmd << "status=" << status;
        return false;
    }
    return true;
#else
    Q_UNUSED(cmd);
    return false;
#endif
}

bool Qusb::visaQuery(const QString &cmd, QString *response)
{
#ifdef HAVE_NI_VISA
    if (!visaWrite(cmd))
    {
        return false;
    }
    char buffer[1024] = {0};
    ViUInt32 readCount = 0;
    const ViStatus status = viRead(visaInst_, reinterpret_cast<ViBuf>(buffer), sizeof(buffer) - 1, &readCount);
    if (status < VI_SUCCESS)
    {
        qDebug() << "VISA读取失败:" << cmd << "status=" << status;
        return false;
    }
    if (response)
    {
        *response = QString::fromLocal8Bit(buffer, static_cast<int>(readCount)).trimmed();
    }
    return true;
#else
    Q_UNUSED(cmd);
    Q_UNUSED(response);
    return false;
#endif
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
    const QString s = "CONFigure:FUNCtion?";
    if (isVisaScpiEnabled())
    {
        QString resp;
        if (visaQuery(s, &resp))
        {
            qDebug().noquote() << "VISA配置查询返回:" << resp;
        }
        return;
    }
    sendCmd(s);
}
void Qusb::getMEASure(QString mac)
{
    Q_UNUSED(mac);
    QString s = protocolConfig_.scpiReadCurrentCmd;
    if (isVisaScpiEnabled())
    {
        QString resp;
        if (visaQuery(s, &resp))
        {
            qDebug().noquote() << "VISA测量返回:" << resp;
            emit send_ammeter_data(resp);
        }
        return;
    }
    sendCmd(s);
}
void Qusb::gethqMEASure()
{
    QString s = "01030000000305CB";   // 测量数值

    QByteArray data = QByteArray::fromHex(s.toLatin1());
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
    serialPort->write(data);
}

void Qusb::sethqMEASure()
{
    QString s = "010600030006F9C8";   // 设置波特率115200
    QByteArray data = QByteArray::fromHex(s.toLatin1());
    serialPort->write(data);
}
quint16 calculateCRC(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (int i = 0; i < data.length(); ++i)
    {
        crc ^= static_cast<quint8>(data.at(i));
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    // Correct the endianness
    return ((crc & 0x00FF) << 8) | ((crc & 0xFF00) >> 8);
}
void Qusb::processModbusRTUData(const QByteArray &response)
{
    if (response.length() < kModbusMinFrameLen)
    {
        qDebug() << "processModbus RTUDataInvalid Modbus RTU response frame";
        return;
    }

    qDebug() << "Received Modbus RTU response frame: " << response.toHex().toUpper();

    const quint8 byteCount = static_cast<quint8>(response.at(2));
    if (!isExpectedModbusLength(response, byteCount))
    {
        qDebug() << "Invalid data length";
        return;
    }

    const quint16 crcExpected =
        readBigEndianU16(response, response.length() - 2, response.length() - 1);
    const quint16 crcCalculated = calculateCRC(response.left(response.length() - 2));

    qDebug() << "Length: " << response.length() << " Byte Count: " << byteCount
             << " Expected CRC: " << QString::number(crcExpected, 16).toUpper()
             << " Calculated CRC: " << QString::number(crcCalculated, 16).toUpper();

    if (crcCalculated != crcExpected)
    {
        qDebug() << "CRC check failed";
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

    if (data.length() < kModbusMinFrameLen)
    {
        qDebug() << "processlxModb usRTUData Invalid Modbus RTU response frame";
        return;
    }

    qDebug() << "Received Modbus RTU data frame: " << data.toHex().toUpper();
    const quint8 byteCount = static_cast<quint8>(data.at(2));
    qDebug() << "数据长度：" << byteCount;
    if (!isExpectedModbusLength(data, byteCount))
    {
        qDebug() << "Invalid data length";
        return;
    }

    const quint16 crcExpected = readBigEndianU16(data, data.length() - 2, data.length() - 1);
    const quint16 crcCalculated = calculateCRC(data.left(data.length() - 2));
    qDebug() << "Length: " << data.length()
             << " Expected CRC: " << QString::number(crcExpected, 16).toUpper()
             << " Calculated CRC: " << QString::number(crcCalculated, 16).toUpper();
    if (crcCalculated != crcExpected)
    {
        qDebug() << "CRC check failed";
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
