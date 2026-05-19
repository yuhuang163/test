#include "qvisa.h"

#include <QDebug>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

Qvisa::Qvisa(QObject* parent) : QObject(parent) {}

Qvisa::~Qvisa()
{
    closeConnection();
}

void Qvisa::setProtocolConfig(const Qvisa::ProtocolConfig& config)
{
    if (config_.useVisa != config.useVisa || config_.visaAddress != config.visaAddress) {
        closeConnection();
    }
    config_ = config;
}

Qvisa::ProtocolConfig Qvisa::protocolConfig() const
{
    return config_;
}

bool Qvisa::configured() const
{
    return config_.useVisa && !config_.visaAddress.trimmed().isEmpty();
}

bool Qvisa::ensureConnected()
{
    if (!configured()) {
        qDebug() << "VISA未启用或地址为空";
        return false;
    }
#ifdef HAVE_NI_VISA
    if (visaInst_ != VI_NULL) {
        return true;
    }
    if (visaRm_ == VI_NULL) {
        const ViStatus rmStatus = viOpenDefaultRM(&visaRm_);
        if (rmStatus < VI_SUCCESS) {
            qDebug() << "VISA打开资源管理器失败，status=" << rmStatus;
            visaRm_ = VI_NULL;
            return false;
        }
    }

    const QByteArray addr = config_.visaAddress.trimmed().toLocal8Bit();
    const ViStatus openStatus = viOpen(visaRm_, (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &visaInst_);
    if (openStatus < VI_SUCCESS) {
        qDebug() << "VISA打开设备失败，address=" << config_.visaAddress << "status=" << openStatus;
        visaInst_ = VI_NULL;
        return false;
    }
    viSetAttribute(visaInst_, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(config_.timeoutMs));
    qDebug() << "VISA设备已连接:" << config_.visaAddress;
    return true;
#else
    qDebug() << "当前构建未启用NI-VISA（HAVE_NI_VISA），无法使用VISA地址通讯";
    return false;
#endif
}

void Qvisa::closeConnection()
{
#ifdef HAVE_NI_VISA
    if (visaInst_ != VI_NULL) {
        viClose(visaInst_);
        visaInst_ = VI_NULL;
    }
    if (visaRm_ != VI_NULL) {
        viClose(visaRm_);
        visaRm_ = VI_NULL;
    }
#endif
}

bool Qvisa::writeCommand(const QString& cmd)
{
    if (!ensureConnected()) {
        return false;
    }
#ifdef HAVE_NI_VISA
    QByteArray payload = cmd.toLocal8Bit();
    if (!payload.endsWith('\n')) {
        payload.append('\n');
    }
    ViUInt32 writeCount = 0;
    const ViStatus status = viWrite(visaInst_, reinterpret_cast<ViBuf>(payload.data()),
                                    static_cast<ViUInt32>(payload.size()), &writeCount);
    if (status < VI_SUCCESS) {
        qDebug() << "VISA写入失败:" << cmd << "status=" << status;
        return false;
    }
    return true;
#else
    Q_UNUSED(cmd);
    return false;
#endif
}

bool Qvisa::queryCommand(const QString& cmd, QString* response)
{
#ifdef HAVE_NI_VISA
    if (!writeCommand(cmd)) {
        return false;
    }
    char buffer[1024] = {0};
    ViUInt32 readCount = 0;
    const ViStatus status = viRead(visaInst_, reinterpret_cast<ViBuf>(buffer), sizeof(buffer) - 1, &readCount);
    if (status < VI_SUCCESS) {
        qDebug() << "VISA读取失败:" << cmd << "status=" << status;
        return false;
    }
    if (response) {
        *response = QString::fromLocal8Bit(buffer, static_cast<int>(readCount)).trimmed();
    }
    return true;
#else
    Q_UNUSED(cmd);
    Q_UNUSED(response);
    return false;
#endif
}

bool Qvisa::sendPowerInstruction(Qvisa::PowerAction action)
{
    switch (action) {
    case PowerAction::ConfigurePowerSupply:
        return configureProgrammablePower(config_.powerVoltageV, config_.powerCurrentA);
    case PowerAction::ReadVoltage:
        return readProgrammablePowerVoltage();
    case PowerAction::ReadCurrent:
        return readProgrammablePowerCurrent();
    case PowerAction::OutputOn:
        return setProgrammablePowerOutput(true);
    case PowerAction::OutputOff:
        return setProgrammablePowerOutput(false);
    case PowerAction::InitializeDevice:
        return initializeProgrammablePower();
    }
    return false;
}

bool Qvisa::configureProgrammablePower(double voltageV, double currentA)
{
    return writeCommand(config_.setVoltageCmd.arg(QString::number(voltageV, 'f', 3))) &&
           writeCommand(config_.setCurrentCmd.arg(QString::number(currentA, 'f', 3)));
}

bool Qvisa::setProgrammablePowerOutput(bool enable)
{
    return writeCommand(enable ? config_.outputOnCmd : config_.outputOffCmd);
}

bool Qvisa::readProgrammablePowerVoltage()
{
    QString resp;
    if (!queryCommand(config_.readVoltageCmd, &resp)) {
        emit programmablePowerVoltageRead(0.0, false);
        return false;
    }
    bool ok = false;
    const double v = resp.trimmed().toDouble(&ok);
    emit programmablePowerVoltageRead(v, ok);
    return true;
}

bool Qvisa::readProgrammablePowerCurrent()
{
    QString resp;
    if (!queryCommand(config_.readCurrentCmd, &resp)) {
        emit programmablePowerCurrentRead(0.0, false);
        return false;
    }
    bool ok = false;
    const double a = resp.trimmed().toDouble(&ok);
    emit programmablePowerCurrentRead(a, ok);
    return true;
}

bool Qvisa::initializeProgrammablePower()
{
    if (!writeCommand(QStringLiteral("*RST"))) {
        return false;
    }
    return configureProgrammablePower(config_.powerVoltageV, config_.powerCurrentA);
}
