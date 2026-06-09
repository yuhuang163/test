#include "qvisa.h"

#include <QDebug>
#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Qvisa::Qvisa(QObject* parent) : QObject(parent) {
    config_.commandProfile = VisaDeviceProfile::DefaultPower;
}

Qvisa::~Qvisa() {
    closeConnection();
}

QStringList Qvisa::enumerateResources(const QString& expression)
{
    QStringList result;
#ifdef HAVE_NI_VISA
    ViSession rm = VI_NULL;
    if (viOpenDefaultRM(&rm) < VI_SUCCESS) {
        qDebug() << "VISA枚举：打开资源管理器失败";
        return result;
    }

    ViFindList findList = VI_NULL;
    ViUInt32 resourceCount = 0;
    char buffer[VI_FIND_BUFLEN] = {0};
    const QByteArray expr = expression.trimmed().isEmpty() ? QByteArray("?*INSTR") : expression.toLocal8Bit();
    ViStatus status = viFindRsrc(rm, expr.constData(), &findList, &resourceCount, buffer);
    if (status >= VI_SUCCESS && resourceCount > 0) {
        result.append(QString::fromLocal8Bit(buffer).trimmed());
        for (ViUInt32 i = 1; i < resourceCount; ++i) {
            status = viFindNext(findList, buffer);
            if (status < VI_SUCCESS) {
                break;
            }
            const QString next = QString::fromLocal8Bit(buffer).trimmed();
            if (!next.isEmpty()) {
                result.append(next);
            }
        }
    }
    if (findList != VI_NULL) {
        viClose(findList);
    }
    viClose(rm);
#else
    Q_UNUSED(expression);
    qDebug() << "VISA枚举：当前构建未启用 HAVE_NI_VISA";
#endif
    return result;
}

bool Qvisa::applyVisaAddress(const QString& address)
{
    const QString trimmed = address.trimmed();
    if (config_.visaAddress != trimmed) {
        closeConnection();
    }
    config_.visaAddress = trimmed;
    config_.useVisa = !trimmed.isEmpty();
    return true;
}

bool Qvisa::prepareSession(VisaDeviceProfile profile, const QString& address)
{
    const QString preservedAddress = address.trimmed();
    loadCommandSet(profile, false);
    return applyVisaAddress(preservedAddress);
}

void Qvisa::loadCommandSet(VisaDeviceProfile profile, bool loadAddressFromSettings)
{
    const QString preservedAddress = config_.visaAddress;
    const bool preservedUseVisa = config_.useVisa;

    if (profile == VisaDeviceProfile::ProgrammablePower) {
        ProtocolConfig next;
        next.useVisa = loadAddressFromSettings
                           ? SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool()
                           : preservedUseVisa;
        next.visaAddress = loadAddressFromSettings
                               ? SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QStringLiteral("GPIB0::7::INSTR"))
                                     .toString()
                                     .trimmed()
                               : preservedAddress;
        next.timeoutMs = SETTINGS.value(QStringLiteral("VisaPower/TimeoutMs"), 3000).toInt();
        next.powerVoltageV = SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), 12.0).toDouble();
        next.powerCurrentA = SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), 2.5).toDouble();
        next.commandProfile = VisaDeviceProfile::ProgrammablePower;
        if (config_.useVisa != next.useVisa || config_.visaAddress != next.visaAddress) {
            closeConnection();
        }
        config_ = next;

        commandSet_.setVoltageCmd =
            SETTINGS.value(QStringLiteral("VisaPower/ScpiSetVoltageCmd"), QStringLiteral("VOLT %1")).toString();
        commandSet_.setCurrentCmd =
            SETTINGS.value(QStringLiteral("VisaPower/ScpiSetCurrentCmd"), QStringLiteral("CURR %1")).toString();
        commandSet_.outputOnCmd =
            SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOnCmd"), QStringLiteral("OUTP ON")).toString();
        commandSet_.outputOffCmd =
            SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOffCmd"), QStringLiteral("OUTP OFF")).toString();
        commandSet_.readVoltageCmd =
            SETTINGS.value(QStringLiteral("VisaPower/ScpiReadVoltageCmd"), QStringLiteral("MEASure:VOLTage:DC?"))
                .toString();
        commandSet_.readCurrentCmd =
            SETTINGS.value(QStringLiteral("VisaPower/ScpiReadCurrentCmd"), QStringLiteral("MEASure:CURRent:DC? 500e-3"))
                .toString();
        return;
    }
    if (profile == VisaDeviceProfile::RxCmwInstrument) {
        ProtocolConfig next;
        next.useVisa = true;
        next.visaAddress = loadAddressFromSettings
                               ? SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed()
                               : preservedAddress;
        next.timeoutMs = SETTINGS.value(QStringLiteral("BlePer/CmwTimeoutMs"), 5000).toInt();
        next.commandProfile = VisaDeviceProfile::RxCmwInstrument;
        if (config_.useVisa != next.useVisa || config_.visaAddress != next.visaAddress) {
            closeConnection();
        }
        config_ = next;

        commandSet_.setVoltageCmd = SETTINGS.value(QStringLiteral("BlePer/CmwCmdStateOffOpc"),
                                                   QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"))
                                        .toString();
        commandSet_.setCurrentCmd = SETTINGS.value(QStringLiteral("BlePer/CmwCmdListOff"),
                                                   QStringLiteral("SOURce:GPRF:GEN:LIST OFF"))
                                        .toString();
        commandSet_.outputOnCmd = SETTINGS.value(QStringLiteral("BlePer/CmwCmdBbModeArb"),
                                                 QStringLiteral("SOURce:GPRF:GEN:BBMode ARB"))
                                      .toString();
        commandSet_.outputOffCmd = SETTINGS.value(QStringLiteral("BlePer/CmwCmdStateOnOpc"),
                                                  QStringLiteral("SOURce:GPRF:GEN:STATe ON;*OPC?"))
                                       .toString();
        commandSet_.readVoltageCmd =
            SETTINGS.value(QStringLiteral("BlePer/CmwCmdSystemError"), QStringLiteral("SYSTem:ERRor?")).toString();
        commandSet_.readCurrentCmd =
            SETTINGS.value(QStringLiteral("BlePer/CmwCmdStateQuery"), QStringLiteral("SOURce:GPRF:GEN:STATe?")).toString();
        return;
    }
    commandSet_.setVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetVoltageCmd"), QStringLiteral("VOLT %1")).toString();
    commandSet_.setCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetCurrentCmd"), QStringLiteral("CURR %1")).toString();
    commandSet_.outputOnCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOnCmd"), QStringLiteral("OUTP ON")).toString();
    commandSet_.outputOffCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOffCmd"), QStringLiteral("OUTP OFF")).toString();
    commandSet_.readVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadVoltageCmd"), QStringLiteral("MEASure:VOLTage:DC?")).toString();
    commandSet_.readCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadCurrentCmd"), QStringLiteral("MEASure:CURRent:DC? 500e-3"))
            .toString();
}

Qvisa::ProtocolConfig Qvisa::protocolConfig() const {
    return config_;
}

bool Qvisa::ensureConnected() {
#ifdef HAVE_NI_VISA
    if (visaInst_ != VI_NULL) {
        return true;
    }
#endif
    if (config_.visaAddress.trimmed().isEmpty()) {
        qDebug() << "VISA地址为空";
        return false;
    }
#ifdef HAVE_NI_VISA
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

void Qvisa::closeConnection() {
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

bool Qvisa::writeLine(const QString& cmd) {
    if (!ensureConnected()) {
        return false;
    }
#ifdef HAVE_NI_VISA
    QByteArray payload = cmd.toLocal8Bit();
    if (!payload.endsWith('\n')) {
        payload.append('\n');
    }
    qDebug().noquote() << "VISA TX:" << QString::fromLatin1(payload.toHex(' ').toUpper());
    ViUInt32 writeCount = 0;
    const ViStatus status =
        viWrite(visaInst_, reinterpret_cast<ViBuf>(payload.data()), static_cast<ViUInt32>(payload.size()), &writeCount);
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

bool Qvisa::configurePower(double voltageV, double currentA) {
    const bool vOk = writeLine(commandSet_.setVoltageCmd.arg(QString::number(voltageV, 'f', 3)));
    const bool cOk = writeLine(commandSet_.setCurrentCmd.arg(QString::number(currentA, 'f', 3)));
    return vOk && cOk;
}

bool Qvisa::readPowerVoltage() {
    lastQueryResponse_.clear();
    if (!ensureConnected()) {
        emit programmablePowerVoltageRead(0.0, false);
        return false;
    }
#ifdef HAVE_NI_VISA
    if (!writeLine(commandSet_.readVoltageCmd)) {
        emit programmablePowerVoltageRead(0.0, false);
        return false;
    }
    char buffer[1024] = {0};
    ViUInt32 readCount = 0;
    const ViStatus status = viRead(visaInst_, reinterpret_cast<ViBuf>(buffer), sizeof(buffer) - 1, &readCount);
    if (status < VI_SUCCESS) {
        emit programmablePowerVoltageRead(0.0, false);
        return false;
    }
    lastQueryResponse_ = QString::fromLocal8Bit(buffer, static_cast<int>(readCount)).trimmed();
    bool parseOk = false;
    const double v = lastQueryResponse_.toDouble(&parseOk);
    emit programmablePowerVoltageRead(v, parseOk);
    return parseOk;
#else
    emit programmablePowerVoltageRead(0.0, false);
    return false;
#endif
}

bool Qvisa::readPowerCurrent() {
    lastQueryResponse_.clear();
    if (!ensureConnected()) {
        emit programmablePowerCurrentRead(0.0, false);
        return false;
    }
#ifdef HAVE_NI_VISA
    if (!writeLine(commandSet_.readCurrentCmd)) {
        emit programmablePowerCurrentRead(0.0, false);
        return false;
    }
    char buffer[1024] = {0};
    ViUInt32 readCount = 0;
    const ViStatus status = viRead(visaInst_, reinterpret_cast<ViBuf>(buffer), sizeof(buffer) - 1, &readCount);
    if (status < VI_SUCCESS) {
        emit programmablePowerCurrentRead(0.0, false);
        return false;
    }
    lastQueryResponse_ = QString::fromLocal8Bit(buffer, static_cast<int>(readCount)).trimmed();
    bool parseOk = false;
    const double a = lastQueryResponse_.toDouble(&parseOk);
    emit programmablePowerCurrentRead(a, parseOk);
    return parseOk;
#else
    emit programmablePowerCurrentRead(0.0, false);
    return false;
#endif
}

bool Qvisa::setPowerOutput(bool enable) {
    return writeLine(enable ? commandSet_.outputOnCmd : commandSet_.outputOffCmd);
}

bool Qvisa::initializePower(double voltageV, double currentA) {
    if (!writeLine(QStringLiteral("*RST"))) {
        return false;
    }
    return configurePower(voltageV, currentA);
}

bool Qvisa::set(VisaCmd cmd, const QVariant& data) {
    const QVariantMap m = data.toMap();
    switch (cmd) {
    case VisaCmd::DeviceProfile:
        loadCommandSet(static_cast<VisaDeviceProfile>(data.toInt()));
        return true;
    case VisaCmd::ConfigurePowerSupply: {
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        const double voltage = m.value(QStringLiteral("voltage"), config_.powerVoltageV).toDouble();
        const double current = m.value(QStringLiteral("current"), config_.powerCurrentA).toDouble();
        const bool ok = configurePower(voltage, current);
        if (!ok) {
            qWarning() << "Qvisa set(ConfigurePowerSupply) 失败";
        }
        return ok;
    }
    case VisaCmd::ReadVoltage:
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        return readPowerVoltage();
    case VisaCmd::ReadCurrent:
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        return readPowerCurrent();
    case VisaCmd::PowerOutputOn:
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        return setPowerOutput(true);
    case VisaCmd::PowerOutputOff:
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        return setPowerOutput(false);
    case VisaCmd::InitializePower: {
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        const double voltage = m.value(QStringLiteral("voltage"), config_.powerVoltageV).toDouble();
        const double current = m.value(QStringLiteral("current"), config_.powerCurrentA).toDouble();
        const bool ok = initializePower(voltage, current);
        if (!ok) {
            qWarning() << "Qvisa set(InitializePower) 失败";
        }
        return ok;
    }
    case VisaCmd::WriteLine: {
        const bool ok = writeLine(data.toString());
        if (!ok) {
            qWarning() << "Qvisa set(WriteLine) 失败 cmd=" << data.toString();
        }
        return ok;
    }
    case VisaCmd::QueryLine:
        qWarning() << "Qvisa set(QueryLine) 请使用 get";
        return false;
    }
    return false;
}

bool Qvisa::get(VisaCmd cmd, const QVariant& param) {
    switch (cmd) {
    case VisaCmd::DeviceProfile:
    case VisaCmd::ConfigurePowerSupply:
    case VisaCmd::PowerOutputOn:
    case VisaCmd::PowerOutputOff:
    case VisaCmd::InitializePower:
    case VisaCmd::WriteLine:
        qWarning() << "Qvisa get 不支持该指令，请使用 set";
        return false;
    case VisaCmd::ReadVoltage:
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        return readPowerVoltage();
    case VisaCmd::ReadCurrent:
        if (config_.commandProfile != VisaDeviceProfile::ProgrammablePower) {
            loadCommandSet(VisaDeviceProfile::ProgrammablePower);
        }
        return readPowerCurrent();
    case VisaCmd::QueryLine: {
        lastQueryResponse_.clear();
        if (!ensureConnected()) {
            return false;
        }
#ifdef HAVE_NI_VISA
        const QString query = param.toString();
        if (!writeLine(query)) {
            return false;
        }
        char buffer[1024] = {0};
        ViUInt32 readCount = 0;
        const ViStatus status = viRead(visaInst_, reinterpret_cast<ViBuf>(buffer), sizeof(buffer) - 1, &readCount);
        if (status < VI_SUCCESS) {
            qWarning() << "Qvisa get(QueryLine) 读取失败 cmd=" << query;
            return false;
        }
        lastQueryResponse_ = QString::fromLocal8Bit(buffer, static_cast<int>(readCount)).trimmed();
        return true;
#else
        Q_UNUSED(param);
        return false;
#endif
    }
    }
    return false;
}

bool Qvisa::sendCustomMessage(const QVariantMap& map) {
    if (map.contains(QStringLiteral("line"))) {
        return set(VisaCmd::WriteLine, map.value(QStringLiteral("line")));
    }
    if (map.contains(QStringLiteral("query"))) {
        return get(VisaCmd::QueryLine, map.value(QStringLiteral("query")));
    }
    if (map.contains(QStringLiteral("config"))) {
        const QVariantMap cm = map.value(QStringLiteral("config")).toMap();
        if (cm.size() == 1 && cm.contains(QStringLiteral("commandProfile"))) {
            return set(VisaCmd::DeviceProfile, cm.value(QStringLiteral("commandProfile")));
        }
        ProtocolConfig cfg = config_;
        if (cm.contains(QStringLiteral("useVisa"))) {
            cfg.useVisa = cm.value(QStringLiteral("useVisa")).toBool();
        }
        if (cm.contains(QStringLiteral("visaAddress"))) {
            cfg.visaAddress = cm.value(QStringLiteral("visaAddress")).toString();
        }
        if (cm.contains(QStringLiteral("commandProfile"))) {
            cfg.commandProfile = static_cast<VisaDeviceProfile>(cm.value(QStringLiteral("commandProfile")).toInt());
        }
        if (cm.contains(QStringLiteral("timeoutMs"))) {
            cfg.timeoutMs = cm.value(QStringLiteral("timeoutMs")).toInt();
        }
        if (cm.contains(QStringLiteral("powerVoltageV"))) {
            cfg.powerVoltageV = cm.value(QStringLiteral("powerVoltageV")).toDouble();
        }
        if (cm.contains(QStringLiteral("powerCurrentA"))) {
            cfg.powerCurrentA = cm.value(QStringLiteral("powerCurrentA")).toDouble();
        }
        loadCommandSet(cfg.commandProfile);
        if (config_.useVisa != cfg.useVisa || config_.visaAddress != cfg.visaAddress) {
            closeConnection();
        }
        config_.useVisa = cfg.useVisa;
        if (!cfg.visaAddress.trimmed().isEmpty()) {
            config_.visaAddress = cfg.visaAddress;
        }
        if (cfg.timeoutMs > 0) {
            config_.timeoutMs = cfg.timeoutMs;
        }
        config_.powerVoltageV = cfg.powerVoltageV;
        config_.powerCurrentA = cfg.powerCurrentA;
        return true;
    }
    const QString action = map.value(QStringLiteral("action")).toString().trimmed().toLower();
    if (action == QStringLiteral("configurepowersupply")) {
        QVariantMap payload;
        payload.insert(QStringLiteral("voltage"), map.value(QStringLiteral("voltage")));
        payload.insert(QStringLiteral("current"), map.value(QStringLiteral("current")));
        return set(VisaCmd::ConfigurePowerSupply, payload);
    }
    if (action == QStringLiteral("poweroutputon")) {
        return set(VisaCmd::PowerOutputOn);
    }
    if (action == QStringLiteral("poweroutputoff")) {
        return set(VisaCmd::PowerOutputOff);
    }
    if (action == QStringLiteral("readvoltage")) {
        return get(VisaCmd::ReadVoltage);
    }
    if (action == QStringLiteral("readcurrent")) {
        return get(VisaCmd::ReadCurrent);
    }
    if (action == QStringLiteral("writeline")) {
        return set(VisaCmd::WriteLine, map.value(QStringLiteral("cmd")));
    }
    if (action == QStringLiteral("queryline")) {
        return get(VisaCmd::QueryLine, map.value(QStringLiteral("cmd")));
    }
    return false;
}
