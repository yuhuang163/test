#include "qusb.h"

#include "huiling_wfp60h_profile.h"
#include "modbus_types.h"
#include "qmodbusmanager.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

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

HuilingWfp60hScpiProfile toProfilePatch(const Qusb::ProtocolConfig& cfg) {
    HuilingWfp60hScpiProfile profile;
    profile.scpiCurrentType = cfg.scpiCurrentType;
    profile.scpiCurrentMode = cfg.scpiCurrentMode;
    profile.scpiRange = cfg.scpiRange;
    profile.scpiPowerVoltageV = cfg.scpiPowerVoltageV;
    profile.scpiPowerCurrentA = cfg.scpiPowerCurrentA;
    profile.scpiSetVoltageCmd = cfg.scpiSetVoltageCmd;
    profile.scpiSetCurrentCmd = cfg.scpiSetCurrentCmd;
    profile.scpiOutputOnCmd = cfg.scpiOutputOnCmd;
    profile.scpiOutputOffCmd = cfg.scpiOutputOffCmd;
    profile.scpiReadVoltageCmd = cfg.scpiReadVoltageCmd;
    profile.scpiReadCurrentCmd = cfg.scpiReadCurrentCmd;
    return profile;
}

Qusb::ProtocolConfig fromActiveProfile(const QusbLinkConfig& route, const HuilingWfp60hScpiProfile& profile) {
    Qusb::ProtocolConfig out;
    out.protocol = route.protocol;
    out.luxshareMachineId = route.luxshareMachineId;
    out.scpiCurrentType = profile.scpiCurrentType;
    out.scpiCurrentMode = profile.scpiCurrentMode;
    out.scpiRange = profile.scpiRange;
    out.scpiPowerVoltageV = profile.scpiPowerVoltageV;
    out.scpiPowerCurrentA = profile.scpiPowerCurrentA;
    out.scpiSetVoltageCmd = profile.scpiSetVoltageCmd;
    out.scpiSetCurrentCmd = profile.scpiSetCurrentCmd;
    out.scpiOutputOnCmd = profile.scpiOutputOnCmd;
    out.scpiOutputOffCmd = profile.scpiOutputOffCmd;
    out.scpiReadVoltageCmd = profile.scpiReadVoltageCmd;
    out.scpiReadCurrentCmd = profile.scpiReadCurrentCmd;
    return out;
}
} // namespace

Qusb::Qusb(QSerialPort* port, QObject* parent) : QObject(parent), serialPort_(port), scpiManager_(this) {
    scpiManager_.attachSerialPort(port);
    syncScpiDeviceRoute();
    connect(&scpiManager_, &QScpiManager::ammeterReadingReceived, this, [this](const QString& valueText) {
        emit reportReceived(ProtocolReport(QStringLiteral("ProtocolAmmeterReadingData"),
                                           QVariant::fromValue(ProtocolAmmeterReadingData{valueText})));
    });
    connect(&scpiManager_, &QScpiManager::programmablePowerVoltageRead, this, &Qusb::programmablePowerVoltageRead);
    connect(&scpiManager_, &QScpiManager::programmablePowerCurrentRead, this, &Qusb::programmablePowerCurrentRead);
}

void Qusb::setModbusManager(QModbusManager* manager) {
    modbusManager_ = manager;
    syncModbusManagerRtuRoute();
}

void Qusb::syncModbusManagerRtuRoute() {
    if (!modbusManager_) {
        return;
    }
    switch (linkConfig_.protocol) {
    case QusbProtocolRoute::HqModbus:
        modbusManager_->setDeviceRoute(ModbusDeviceRoute::HqAmmeterRtu);
        break;
    case QusbProtocolRoute::LxModbus:
        modbusManager_->setDeviceRoute(ModbusDeviceRoute::LxAmmeterRtu);
        break;
    default:
        modbusManager_->setDeviceRoute(ModbusDeviceRoute::None);
        break;
    }
    modbusManager_->setLuxshareMachineId(linkConfig_.luxshareMachineId);
}

void Qusb::syncScpiDeviceRoute() {
    switch (linkConfig_.protocol) {
    case QusbProtocolRoute::Scpi:
    case QusbProtocolRoute::Auto:
        scpiManager_.setDeviceRoute(ScpiDeviceRoute::HuilingWfp60h);
        break;
    default:
        scpiManager_.setDeviceRoute(ScpiDeviceRoute::None);
        break;
    }
}

void Qusb::setLinkConfig(const QusbLinkConfig& config) {
    linkConfig_ = config;
    syncModbusManagerRtuRoute();
    syncScpiDeviceRoute();
}

QusbLinkConfig Qusb::linkConfig() const {
    return linkConfig_;
}

void Qusb::setProtocolConfig(const ProtocolConfig& config) {
    QusbLinkConfig route;
    route.protocol = config.protocol;
    route.luxshareMachineId = config.luxshareMachineId;
    setLinkConfig(route);
    scpiManager_.setHuilingProfilePatch(toProfilePatch(config));
}

Qusb::ProtocolConfig Qusb::protocolConfig() const {
    return fromActiveProfile(linkConfig_, scpiManager_.huilingActiveProfile());
}

void Qusb::parseCmd(const QByteArray& byte) {
    if (!byte.isEmpty()) {
        qDebug().noquote() << "USB RX:" << QString::fromLatin1(byte.toHex(' ').toUpper());
    }
    if (modbusManager_ && modbusManager_->isRtuAmmeterRoute()) {
        return;
    }
    switch (resolveRouteForInput(byte)) {
    case QusbProtocolRoute::Scpi:
    case QusbProtocolRoute::Auto:
        scpiManager_.feedRx(byte);
        break;
    case QusbProtocolRoute::HqModbus:
    case QusbProtocolRoute::LxModbus:
        qDebug() << "Modbus RTU 收包应由 modbusManager.feedRtuRx 处理";
        break;
    }
}

bool Qusb::dispatchScpiPowerAction(QusbPowerAction action) {
    switch (action) {
    case QusbPowerAction::ConfigurePowerSupply:
        return scpiManager_.exec(HuilingScpiCmd::ConfigureMeasure);
    case QusbPowerAction::ReadMeasurement:
        return scpiManager_.exec(HuilingScpiCmd::ReadMeasureCurrent);
    case QusbPowerAction::ReadConfiguration:
        return scpiManager_.exec(HuilingScpiCmd::ReadMeasureConfiguration);
    case QusbPowerAction::InitializeDevice:
        return scpiManager_.exec(HuilingScpiCmd::InitializeDevice);
    case QusbPowerAction::ReadProgrammablePowerVoltage:
        return scpiManager_.exec(HuilingScpiCmd::ReadProgrammableVoltage);
    case QusbPowerAction::ReadProgrammablePowerCurrent:
        return scpiManager_.exec(HuilingScpiCmd::ReadProgrammableCurrent);
    case QusbPowerAction::ReadsuctionData:
        qDebug() << "SCPI 未实现 ReadsuctionData";
        return false;
    }
    return false;
}

bool Qusb::sendPowerInstruction(QusbPowerAction action) {
    if (!serialPort_ || !serialPort_->isOpen()) {
        qDebug() << "Qusb: 串口未打开";
        return false;
    }
    switch (resolveRouteForOutput()) {
    case QusbProtocolRoute::Scpi:
    case QusbProtocolRoute::Auto:
        return dispatchScpiPowerAction(action);
    case QusbProtocolRoute::HqModbus:
    case QusbProtocolRoute::LxModbus:
        return handleModbusAction(action);
    }
    return false;
}

QusbProtocolRoute Qusb::resolveRouteForInput(const QByteArray& input) const {
    if (linkConfig_.protocol != QusbProtocolRoute::Auto) {
        return linkConfig_.protocol;
    }
    if (isLikelyAsciiLine(input)) {
        return QusbProtocolRoute::Scpi;
    }
    if (linkConfig_.luxshareMachineId > 0) {
        return QusbProtocolRoute::LxModbus;
    }
    return QusbProtocolRoute::HqModbus;
}

QusbProtocolRoute Qusb::resolveRouteForOutput() const {
    if (linkConfig_.protocol != QusbProtocolRoute::Auto) {
        return linkConfig_.protocol;
    }
    return QusbProtocolRoute::Scpi;
}

bool Qusb::handleModbusAction(QusbPowerAction action) {
    if (!modbusManager_) {
        qDebug() << "Modbus RTU 未绑定 modbusManager";
        return false;
    }
    switch (action) {
    case QusbPowerAction::ConfigurePowerSupply:
    case QusbPowerAction::InitializeDevice:
        if (modbusManager_->deviceRoute() == ModbusDeviceRoute::HqAmmeterRtu) {
            return modbusManager_->exec(HqAmmeterRtuCmd::SetBaud115200);
        }
        return false;
    case QusbPowerAction::ReadMeasurement:
        if (modbusManager_->deviceRoute() == ModbusDeviceRoute::HqAmmeterRtu) {
            return modbusManager_->exec(HqAmmeterRtuCmd::ReadMeasurement);
        }
        if (modbusManager_->deviceRoute() == ModbusDeviceRoute::LxAmmeterRtu) {
            return modbusManager_->exec(LxAmmeterRtuCmd::ReadMeasurement);
        }
        return false;
    case QusbPowerAction::ReadConfiguration:
        qDebug() << "Modbus RTU 电流表暂无读取配置指令";
        return false;
    case QusbPowerAction::ReadsuctionData:
    case QusbPowerAction::ReadProgrammablePowerVoltage:
    case QusbPowerAction::ReadProgrammablePowerCurrent:
        qDebug() << "Modbus RTU 电流表不支持程控电源动作";
        return false;
    }
    return false;
}

HuilingScpiCmd Qusb::scpiCmdFromUsbCmd(QusbCmd cmd) const {
    switch (cmd) {
    case QusbCmd::SendScpiLine:
        return HuilingScpiCmd::SendRawLine;
    case QusbCmd::ConfigureProgrammablePower:
        return HuilingScpiCmd::ConfigureProgrammablePower;
    case QusbCmd::ProgrammablePowerOutput:
        return HuilingScpiCmd::ProgrammablePowerOutput;
    case QusbCmd::ReadProgrammablePowerVoltageCmd:
        return HuilingScpiCmd::ReadProgrammableVoltage;
    case QusbCmd::ReadProgrammablePowerCurrentCmd:
        return HuilingScpiCmd::ReadProgrammableCurrent;
    case QusbCmd::InitializeProgrammablePowerCmd:
        return HuilingScpiCmd::InitializeProgrammablePower;
    default:
        return HuilingScpiCmd::SendRawLine;
    }
}

void Qusb::set(QusbCmd cmd, const QVariant& data) {
    switch (cmd) {
    case QusbCmd::PowerActionCmd:
        sendPowerInstruction(static_cast<QusbPowerAction>(data.toInt()));
        return;
    case QusbCmd::ProtocolConfigCmd: {
        QusbLinkConfig cfg = linkConfig();
        const QVariantMap m = data.toMap();
        if (m.contains(QStringLiteral("protocol")))
            cfg.protocol = static_cast<QusbProtocolRoute>(m.value(QStringLiteral("protocol")).toInt());
        if (m.contains(QStringLiteral("luxshareMachineId")))
            cfg.luxshareMachineId = m.value(QStringLiteral("luxshareMachineId")).toInt();
        setLinkConfig(cfg);
        return;
    }
    case QusbCmd::SendScpiLine:
    case QusbCmd::ConfigureProgrammablePower:
    case QusbCmd::ProgrammablePowerOutput:
    case QusbCmd::ReadProgrammablePowerVoltageCmd:
    case QusbCmd::ReadProgrammablePowerCurrentCmd:
    case QusbCmd::InitializeProgrammablePowerCmd:
        scpiManager_.exec(scpiCmdFromUsbCmd(cmd), data);
        return;
    }
}

void Qusb::get(QusbCmd cmd, const QVariant& param) {
    if (cmd == QusbCmd::ReadProgrammablePowerVoltageCmd || cmd == QusbCmd::ReadProgrammablePowerCurrentCmd ||
        cmd == QusbCmd::InitializeProgrammablePowerCmd) {
        scpiManager_.exec(scpiCmdFromUsbCmd(cmd), param);
    }
}

bool Qusb::sendCustomMessage(const QVariantMap& map) {
    const QString action = map.value(QStringLiteral("action")).toString().trimmed().toLower();
    if (action == QStringLiteral("poweraction")) {
        set(QusbCmd::PowerActionCmd, map.value(QStringLiteral("value")));
        return true;
    }
    if (action == QStringLiteral("setprotocolconfig")) {
        set(QusbCmd::ProtocolConfigCmd, map);
        return true;
    }
    if (scpiManager_.sendCustomMessage(map)) {
        return true;
    }
    return false;
}

bool Qusb::configureProgrammablePower(double voltageV, double currentA) {
    QVariantMap m;
    m.insert(QStringLiteral("voltage"), voltageV);
    m.insert(QStringLiteral("current"), currentA);
    return scpiManager_.exec(HuilingScpiCmd::ConfigureProgrammablePower, m);
}

bool Qusb::setProgrammablePowerOutput(bool enable) {
    return scpiManager_.exec(HuilingScpiCmd::ProgrammablePowerOutput, enable);
}

bool Qusb::readProgrammablePowerVoltage() {
    return scpiManager_.exec(HuilingScpiCmd::ReadProgrammableVoltage);
}

bool Qusb::readProgrammablePowerCurrent() {
    return scpiManager_.exec(HuilingScpiCmd::ReadProgrammableCurrent);
}

bool Qusb::initializeProgrammablePower() {
    return scpiManager_.exec(HuilingScpiCmd::InitializeProgrammablePower);
}
