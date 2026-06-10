#include "qusb.h"

#include "hq_ammeter_rtu.h"
#include "huiling_wfp60h_profile.h"
#include "lx_ammeter_rtu.h"
#include "modbus_types.h"
#include "qmodbus_rtu_rx_buffer.h" // qusb 无 manager 时的 RTU 回退
#include "qmodbusmanager.h"

#include <QDebug>
#include <QSerialPort>

namespace {

bool isLikelyAsciiLine(const QByteArray& buffer) {
    if (buffer.isEmpty()) {
        return false;
    }
    for (char c : buffer) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc == '\r' || uc == '\n' || uc == '\t') {
            continue;
        }
        if (uc < 0x20 || uc > 0x7E) {
            return false;
        }
    }
    return true;
}
} // namespace
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Qusb::Qusb(QSerialPort* parent) : QSerialPort(parent), serialPort(parent) {

    if (!serialPort) {
        qDebug() << "Qat指向一个空指针";
        return;
    }

    registerCommand();
}

Qusb::~Qusb() = default;

void Qusb::registerCommand() {

    // 在这里注册命令即可
    commandList["CURR"] = std::bind(&Qusb::CONFigureFUNCtion, this, std::placeholders::_1);
}
void Qusb::CONFigureFUNCtion(QString p) {
    Q_UNUSED(p);
    qDebug() << "当前是电流模式";
}

void Qusb::parseCmd(const QByteArray& byte) {
    if (!byte.isEmpty()) {
        qDebug().noquote() << "USB RX:" << QString::fromLatin1(byte.toHex(' ').toUpper());
    }
    if (modbusManager_ && modbusManager_->isRtuAmmeterRoute()) {
        return;
    }
    switch (resolveProtocolForInput(byte)) {
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

void Qusb::mergeHuilingScpiProfile() {
    const HuilingWfp60hScpiProfile profile = HuilingWfp60hScpiProfile::fromSettings();
    protocolConfig_.scpiCurrentType = profile.scpiCurrentType;
    protocolConfig_.scpiCurrentMode = profile.scpiCurrentMode;
    protocolConfig_.scpiRange = profile.scpiRange;
    protocolConfig_.scpiPowerVoltageV = profile.scpiPowerVoltageV;
    protocolConfig_.scpiPowerCurrentA = profile.scpiPowerCurrentA;
    protocolConfig_.scpiSetVoltageCmd = profile.scpiSetVoltageCmd;
    protocolConfig_.scpiSetCurrentCmd = profile.scpiSetCurrentCmd;
    protocolConfig_.scpiOutputOnCmd = profile.scpiOutputOnCmd;
    protocolConfig_.scpiOutputOffCmd = profile.scpiOutputOffCmd;
    protocolConfig_.scpiReadVoltageCmd = profile.scpiReadVoltageCmd;
    protocolConfig_.scpiReadCurrentCmd = profile.scpiReadCurrentCmd;
}

bool Qusb::configureProgrammablePower(double voltageV, double currentA) {
    if (!serialPort || !serialPort->isOpen()) {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi) {
        qDebug() << "当前协议不支持程控电源SCPI配置";
        return false;
    }
    mergeHuilingScpiProfile();

    sendCmd(protocolConfig_.scpiSetVoltageCmd.arg(QString::number(voltageV, 'f', 3)));
    sendCmd(protocolConfig_.scpiSetCurrentCmd.arg(QString::number(currentA, 'f', 3)));
    return true;
}

bool Qusb::setProgrammablePowerOutput(bool enable) {
    if (!serialPort || !serialPort->isOpen()) {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi) {
        qDebug() << "当前协议不支持程控电源输出控制";
        return false;
    }
    mergeHuilingScpiProfile();
    sendCmd(enable ? protocolConfig_.scpiOutputOnCmd : protocolConfig_.scpiOutputOffCmd);
    return true;
}

bool Qusb::readProgrammablePowerVoltage() {
    if (!serialPort || !serialPort->isOpen()) {
        return false;
    }
    mergeHuilingScpiProfile();
    // 串口：下一行 SCPI 回包在 processCmd 中按 pending 解析并 emit
    pendingProgPowerRead_ = ProgrammablePowerReadPending::Voltage;
    sendCmd(protocolConfig_.scpiReadVoltageCmd);
    return true;
}

bool Qusb::readProgrammablePowerCurrent() {
    if (!serialPort || !serialPort->isOpen()) {
        return false;
    }
    mergeHuilingScpiProfile();
    pendingProgPowerRead_ = ProgrammablePowerReadPending::Current;
    sendCmd(protocolConfig_.scpiReadCurrentCmd);
    return true;
}

bool Qusb::initializeProgrammablePower() {
    if (!serialPort || !serialPort->isOpen()) {
        return false;
    }
    if (resolveProtocolForOutput() != ProtocolType::Scpi) {
        qDebug() << "当前协议不支持程控电源初始化";
        return false;
    }
    mergeHuilingScpiProfile();
    sendCmd("*RST");
    return configureProgrammablePower(protocolConfig_.scpiPowerVoltageV, protocolConfig_.scpiPowerCurrentA);
}

void Qusb::processScpiData(const QByteArray& byte) {
    scpiLineCodec_.feed(byte, [this](const QString& line) { processCmd(line, parameter); });
}

void Qusb::setModbusManager(QModbusManager* manager) {
    modbusManager_ = manager;
    syncModbusManagerRtuRoute();
}

void Qusb::syncModbusManagerRtuRoute() {
    if (!modbusManager_) {
        return;
    }
    switch (protocolConfig_.protocol) {
    case ProtocolType::HqModbus:
        modbusManager_->setRtuRoute(ModbusRtuRoute::Hq);
        break;
    case ProtocolType::LxModbus:
        modbusManager_->setRtuRoute(ModbusRtuRoute::Lx);
        break;
    default:
        modbusManager_->setRtuRoute(ModbusRtuRoute::None);
        break;
    }
    modbusManager_->setLuxshareMachineId(protocolConfig_.luxshareMachineId);
}

void Qusb::setProtocolConfig(const ProtocolConfig& config) {
    protocolConfig_ = config;
    syncModbusManagerRtuRoute();
}

Qusb::ProtocolConfig Qusb::protocolConfig() const {
    return protocolConfig_;
}

void Qusb::set(UsbCmd cmd, const QVariant& data) {
    switch (cmd) {
    case UsbCmd::PowerActionCmd:
        sendPowerInstruction(static_cast<PowerAction>(data.toInt()));
        return;
    case UsbCmd::ProtocolConfigCmd: {
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
    case UsbCmd::ConfigureProgrammablePower: {
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

void Qusb::get(UsbCmd cmd, const QVariant& param) {
    Q_UNUSED(param);
    switch (cmd) {
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

bool Qusb::sendCustomMessage(const QVariantMap& map) {
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

bool Qusb::sendPowerInstruction(PowerAction action) {
    if (!serialPort || !serialPort->isOpen()) {
        qDebug() << "Qusb: 未打开 usb 串口，无法发送数据";
        return false;
    }

    switch (resolveProtocolForOutput()) {
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

Qusb::ProtocolType Qusb::resolveProtocolForInput(const QByteArray& input) const {
    if (protocolConfig_.protocol != ProtocolType::Auto) {
        return protocolConfig_.protocol;
    }
    if (isLikelyAsciiLine(input)) {
        return ProtocolType::Scpi;
    }
    if (protocolConfig_.luxshareMachineId > 0) {
        return ProtocolType::LxModbus;
    }
    return ProtocolType::HqModbus;
}

Qusb::ProtocolType Qusb::resolveProtocolForOutput() const {
    if (protocolConfig_.protocol != ProtocolType::Auto) {
        return protocolConfig_.protocol;
    }
    return ProtocolType::Scpi;
}

bool Qusb::handleScpiAction(PowerAction action) {
    switch (action) {
    case PowerAction::ConfigurePowerSupply:
        mergeHuilingScpiProfile();
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

bool Qusb::handleHqAction(PowerAction action) {
    if (modbusManager_) {
        switch (action) {
        case PowerAction::ConfigurePowerSupply:
        case PowerAction::InitializeDevice:
            return modbusManager_->exec(ModbusRtuAmmeterCmd::InitializeBaud115200);
        case PowerAction::ReadMeasurement:
            return modbusManager_->exec(ModbusRtuAmmeterCmd::ReadMeasurement);
        default:
            break;
        }
    }
    switch (action) {
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

bool Qusb::handleLxAction(PowerAction action) {
    if (modbusManager_ && action == PowerAction::ReadMeasurement) {
        return modbusManager_->exec(ModbusRtuAmmeterCmd::ReadMeasurement);
    }
    switch (action) {
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

void Qusb::emitProgrammablePowerReadIfPending(const QString& scpiLine) {
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

void Qusb::processCmd(QString cmd, QString parameter) {
    auto it = commandList.find(cmd);
    if (it != commandList.end()) {
        // 调用回调函数
        it->second(parameter);
    } else if (pendingProgPowerRead_ != ProgrammablePowerReadPending::None) {
        // 程控电源读 V/I 后发「?」得到的纯数值行，勿走电流表 send_ammeter_data
        emitProgrammablePowerReadIfPending(cmd);
    } else {
        emitReport(QStringLiteral("ProtocolAmmeterReadingData"),
                   QVariant::fromValue(ProtocolAmmeterReadingData{cmd}));
        qDebug() << "不支持的命令 : " << cmd;
    }
}
void Qusb::sendCmd(QString cmd) {
    if (!serialPort || !serialPort->isOpen()) {
        qDebug() << "Qusb: 未打开 usb 串口，无法发送数据";
        return;
    }

    cmd += "\r\n";

    const QByteArray data = cmd.toLocal8Bit();
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    serialPort->write(data);
}

void Qusb::sendCONF(QString current, QString dc, QString range) {
    // 构建 CONF 命令字符串
    QString command = "CONF:" + current + ":" + dc + " " + range;

    // 调用 sendCmd 函数发送命令
    sendCmd(command);
}

void Qusb::getCONF(QString mac) {
    Q_UNUSED(mac);
    QString s;
    // s = "CONFigure:RANGe?";
    // sendCmd(s);
    s = "CONFigure:FUNCtion?";
    sendCmd(s);
}
void Qusb::getMEASure(QString mac) {
    Q_UNUSED(mac);
    QString s = "MEASure:CURRent:DC? 500e-3";
    sendCmd(s);
}
void Qusb::gethqMEASure() {
    const QByteArray payload = HqAmmeterModbusRtu::buildReadMeasurementRequest();
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(payload.toHex(' ').toUpper());
    serialPort->write(payload);
}
void Qusb::getlxMEASure(int mechine) {
    const QByteArray payload = LxAmmeterModbusRtu::buildReadMeasurementRequest(mechine);
    if (payload.isEmpty()) {
        return;
    }
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(payload.toHex(' ').toUpper());
    serialPort->write(payload);
}
void Qusb::sethqMEASure() {
    const QByteArray payload = HqAmmeterModbusRtu::buildSetBaud115200Request();
    qDebug().noquote() << "USB TX:" << QString::fromLatin1(payload.toHex(' ').toUpper());
    serialPort->write(payload);
}
void Qusb::processModbusRTUData(const QByteArray& response) {
    qDebug() << "Received Modbus RTU response frame: " << response.toHex().toUpper();
    const ModbusRtuCodec::AmmeterReading reading = HqAmmeterModbusRtu::parseResponse(response);
    if (!reading.ok) {
        qDebug() << "Invalid Modbus RTU frame";
        return;
    }
    qDebug() << ": Current Value = " << reading.valueText << "A";
    emitReport(QStringLiteral("ProtocolAmmeterReadingData"),
               QVariant::fromValue(ProtocolAmmeterReadingData{reading.valueText}));
}

void Qusb::processlxModbusRTUData(const QByteArray& response) {
    qDebug() << "收到的数据为: " << response.toHex().toUpper();
    ModbusRtuRxBuffer lxBuffer;
    lxBuffer.feed(response);
    const QByteArray& frame = lxBuffer.buffer();
    if (frame.isEmpty()) {
        return;
    }
    qDebug() << "Received Modbus RTU data frame: " << frame.toHex().toUpper();
    const ModbusRtuCodec::AmmeterReading reading = LxAmmeterModbusRtu::parseResponse(frame);
    if (!reading.ok) {
        qDebug() << "Invalid Modbus RTU frame";
        return;
    }
    qDebug() << "Current Value = " << reading.valueText << "uA";
    emitReport(QStringLiteral("ProtocolAmmeterReadingData"),
               QVariant::fromValue(ProtocolAmmeterReadingData{reading.valueText}));
}
