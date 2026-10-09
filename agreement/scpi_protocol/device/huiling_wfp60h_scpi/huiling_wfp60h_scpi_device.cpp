#include "huiling_wfp60h_scpi_device.h"

#include "scpi_transport.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

HuilingWfp60hScpiDevice::HuilingWfp60hScpiDevice(ScpiTransport* transport, QObject* parent)
    : QObject(parent), transport_(transport) {
    registerCommand();
}

void HuilingWfp60hScpiDevice::setTransport(ScpiTransport* transport) {
    transport_ = transport;
}

void HuilingWfp60hScpiDevice::setProfilePatch(const HuilingWfp60hScpiProfile& patch) {
    profilePatch_ = patch;
    lastProgCurrentRangeSent_.clear();
}

void HuilingWfp60hScpiDevice::clearProfilePatch() {
    profilePatch_.reset();
    lastProgCurrentRangeSent_.clear();
}

HuilingWfp60hScpiProfile HuilingWfp60hScpiDevice::activeProfile() const {
    HuilingWfp60hScpiProfile profile = HuilingWfp60hScpiProfile::defaults();
    if (!profilePatch_) {
        return profile;
    }
    return *profilePatch_;
}

bool HuilingWfp60hScpiDevice::ensureTransportOpen() const {
    return transport_ && transport_->isOpen();
}

bool HuilingWfp60hScpiDevice::queryMeasureLine(const QString& queryLine, bool emitAsAmmeterReading) {
    QString response;
    if (!transport_->queryLine(queryLine, &response)) {
        if (emitAsAmmeterReading) {
            emit measureReadingReceived(QString());
        }
        return false;
    }
    if (emitAsAmmeterReading) {
        emit measureReadingReceived(response);
        qDebug() << "SCPI 电流读数(VISA):" << response;
    }
    return true;
}

bool HuilingWfp60hScpiDevice::queryProgrammablePower(const QString& queryLine, ProgrammablePowerReadPending pending) {
    pendingProgPowerRead_ = pending;
    QString response;
    if (!transport_->queryLine(queryLine, &response)) {
        pendingProgPowerRead_ = ProgrammablePowerReadPending::None;
        if (pending == ProgrammablePowerReadPending::Voltage) {
            emit programmablePowerVoltageRead(0.0, false);
        } else if (pending == ProgrammablePowerReadPending::Current) {
            emit programmablePowerCurrentRead(0.0, false);
        }
        return false;
    }
    emitProgrammablePowerReadIfPending(response);
    return true;
}

void HuilingWfp60hScpiDevice::registerCommand() {
    commandList_[QStringLiteral("CURR")] = std::bind(&HuilingWfp60hScpiDevice::CONFigureFUNCtion, this, std::placeholders::_1);
}

void HuilingWfp60hScpiDevice::CONFigureFUNCtion(const QString& p) {
    Q_UNUSED(p);
    qDebug() << "当前是电流模式";
}

bool HuilingWfp60hScpiDevice::writeCurrentRangeIfConfigured(const HuilingWfp60hScpiProfile& profile,
                                                            const QVariant& data) {
    // 与配置步一致：仅步骤显式带 currentRange 才发；勿用 profile 默认量程在每次读电流时刷总线
    if (!data.canConvert<QVariantMap>())
        return true;
    const QVariantMap m = data.toMap();
    if (!m.contains(QStringLiteral("currentRange")))
        return true;
    const QString range = m.value(QStringLiteral("currentRange")).toString().trimmed();
    if (range.isEmpty())
        return true;
    if (range == lastProgCurrentRangeSent_)
        return true;
    const QString line = profile.buildSetCurrentRangeLine(range);
    if (line.isEmpty())
        return true;
    qDebug().noquote() << "[Scpi] TX range:" << line;
    if (!transport_->writeLine(line))
        return false;
    lastProgCurrentRangeSent_ = range;
    return true;
}

bool HuilingWfp60hScpiDevice::writePowerChannelSelectIfConfigured(const HuilingWfp60hScpiProfile& profile) {
    if (profile.powerChannel < 1 || profile.scpiChannelSelectCmd.trimmed().isEmpty())
        return true;
    const QString line = profile.scpiChannelSelectCmd.arg(profile.powerChannel);
    qDebug().noquote() << "[Scpi] TX channel select:" << line;
    return transport_->writeLine(line);
}

bool HuilingWfp60hScpiDevice::set(HuilingScpiCmd cmd, const QVariant& data) {
    if (!transport_ || !transport_->isOpen()) {
        return false;
    }

    const HuilingWfp60hScpiProfile profile = activeProfile();
    const bool visaSync = transport_->linkKind() == ScpiLinkKind::Visa;

    switch (cmd) {
    case HuilingScpiCmd::ConfigureMeasure:
        return transport_->writeLine(profile.buildConfigureMeasureLine());
    case HuilingScpiCmd::ReadMeasureConfiguration:
        if (visaSync) {
            return queryMeasureLine(profile.buildReadMeasureConfigurationLine(), false);
        }
        return transport_->writeLine(profile.buildReadMeasureConfigurationLine());
    case HuilingScpiCmd::InitializeDevice:
        return transport_->writeLine(QStringLiteral("*RST"));
    case HuilingScpiCmd::ConfigureProgrammablePower: {
        const QVariantMap m = data.toMap();
        // 仅步骤显式带 currentRange 时才发量程，避免配置电压电流时多塞 SENS/RANG
        const bool setRange = m.contains(QStringLiteral("currentRange"));
        const double voltageV = m.value(QStringLiteral("voltage"), profile.scpiPowerVoltageV).toDouble();
        const double currentA = m.value(QStringLiteral("current"), profile.scpiPowerCurrentA).toDouble();
        // 模板须含 %1；勿把 *IDN? 等查询填进设压/设流
        const QString voltTmpl = profile.scpiSetVoltageCmd.trimmed();
        const QString currTmpl = profile.scpiSetCurrentCmd.trimmed();
        if (!voltTmpl.isEmpty()
            && (!voltTmpl.contains(QLatin1String("%1")) || voltTmpl.contains(QLatin1Char('?')))) {
            qDebug().noquote() << "[Scpi] 设电压模板非法（须含 %1 且非查询）:" << voltTmpl;
            return false;
        }
        if (!currTmpl.isEmpty()
            && (!currTmpl.contains(QLatin1String("%1")) || currTmpl.contains(QLatin1Char('?')))) {
            qDebug().noquote() << "[Scpi] 设电流模板非法（须含 %1 且非查询）:" << currTmpl;
            return false;
        }
        // Agilent 66319D：主路 VOLT/CURR，辅路 VOLT2/CURR2（手册 [SOURce:]VOLTage2）
        const QRegularExpression agilentBareVolt(QStringLiteral(R"(^VOLT(\s|%|$))"),
                                                 QRegularExpression::CaseInsensitiveOption);
        const bool pairAgilent =
            profile.stationsPerDevice > 1 && !voltTmpl.isEmpty() && agilentBareVolt.match(voltTmpl).hasMatch();
        auto agilentTmplForChannel = [](QString tmpl, int ch) -> QString {
            if (ch <= 1 || tmpl.isEmpty())
                return tmpl;
            static const QRegularExpression re(QStringLiteral(R"(^(\s*)(VOLT|CURR)(\b))"),
                                               QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch m = re.match(tmpl);
            if (!m.hasMatch())
                return tmpl;
            tmpl.insert(m.capturedEnd(2), QString::number(ch));
            return tmpl;
        };
        auto writeVoltCurr = [&](const QString& vTmpl, const QString& cTmpl) -> bool {
            QStringList lines;
            if (!vTmpl.isEmpty())
                lines.append(vTmpl.arg(QString::number(voltageV, 'f', 3)));
            if (!cTmpl.isEmpty())
                lines.append(cTmpl.arg(QString::number(currentA, 'f', 3)));
            if (lines.isEmpty())
                return true;
            if (visaSync && lines.size() == 2) {
                const QString combined = lines.at(0) + QLatin1Char(';') + lines.at(1);
                qDebug().noquote() << "[Scpi] TX volt+curr:" << combined;
                return transport_->writeLine(combined);
            }
            for (const QString& line : lines) {
                qDebug().noquote() << "[Scpi] TX:" << line;
                if (!transport_->writeLine(line))
                    return false;
            }
            return true;
        };
        // 配置后确保本通道输出打开（避免上轮关输出后本轮只设压未开出）
        auto ensureOutputOn = [&](int ch) -> bool {
            QString onCmd = profile.scpiOutputOnCmd.trimmed();
            if (pairAgilent) {
                // Agilent 分路：勿发裸 OUTP ON（默认耦合时会双路一起动）
                onCmd = (ch >= 2) ? QStringLiteral("OUTP2 ON") : QStringLiteral("OUTP1 ON");
            } else if (onCmd.isEmpty() || onCmd.contains(QLatin1Char('?'))) {
                onCmd = QStringLiteral("OUTP ON");
            }
            qDebug().noquote() << "[Scpi] TX output after configure:" << onCmd;
            return transport_->writeLine(onCmd);
        };
        if (pairAgilent) {
            // 解耦只需发一次；本工位只写本通道 V/I
            const int ch = profile.powerChannel > 0 ? profile.powerChannel : 1;
            if (ch <= 1) {
                qDebug().noquote() << "[Scpi] TX uncouple outputs: INST:COUP:OUTP:STAT NONE";
                if (!transport_->writeLine(QStringLiteral("INST:COUP:OUTP:STAT NONE")))
                    return false;
            }
            // 量程指令仅主路有效，但工位1/2步进需一致：任一通道配置步都发同一 currentRange（去重）
            if (setRange && !writeCurrentRangeIfConfigured(profile, data))
                return false;
            if (!writeVoltCurr(agilentTmplForChannel(voltTmpl, ch), agilentTmplForChannel(currTmpl, ch)))
                return false;
            if (!ensureOutputOn(ch))
                return false;
            qDebug().noquote() << QStringLiteral("[Scpi] 已配置通道%1：%2 V / %3 A")
                                      .arg(ch)
                                      .arg(voltageV, 0, 'f', 3)
                                      .arg(currentA, 0, 'f', 3);
            return true;
        }
        if (!writePowerChannelSelectIfConfigured(profile))
            return false;
        if (setRange && !writeCurrentRangeIfConfigured(profile, data))
            return false;
        if (!writeVoltCurr(voltTmpl, currTmpl))
            return false;
        {
            const int ch = profile.powerChannel > 0 ? profile.powerChannel : 1;
            if (!ensureOutputOn(ch))
                return false;
        }
        return true;
    }
    case HuilingScpiCmd::ProgrammablePowerOutput: {
        if (!writePowerChannelSelectIfConfigured(profile))
            return false;
        const QString line =
            (data.toBool() ? profile.scpiOutputOnCmd : profile.scpiOutputOffCmd).trimmed();
        if (line.isEmpty() || line.contains(QLatin1Char('?'))) {
            qDebug().noquote() << "[Scpi] 输出开关指令非法（勿填 *IDN? 等查询）:" << line;
            return false;
        }
        qDebug().noquote() << "[Scpi] TX output:" << line;
        return transport_->writeLine(line);
    }
    case HuilingScpiCmd::ReadProgrammableVoltage:
        if (!writePowerChannelSelectIfConfigured(profile))
            return false;
        if (visaSync) {
            return queryProgrammablePower(profile.scpiReadVoltageCmd, ProgrammablePowerReadPending::Voltage);
        }
        pendingProgPowerRead_ = ProgrammablePowerReadPending::Voltage;
        return transport_->writeLine(profile.scpiReadVoltageCmd);
    case HuilingScpiCmd::ReadProgrammableCurrent:
        if (!writePowerChannelSelectIfConfigured(profile))
            return false;
        // 工位1/2 读电流都按步骤 currentRange 切量程（SENS 仅主路；同台去重，首包一次）
        if (!writeCurrentRangeIfConfigured(profile, data))
            return false;
        if (visaSync) {
            return queryProgrammablePower(profile.scpiReadCurrentCmd, ProgrammablePowerReadPending::Current);
        }
        pendingProgPowerRead_ = ProgrammablePowerReadPending::Current;
        return transport_->writeLine(profile.scpiReadCurrentCmd);
    case HuilingScpiCmd::InitializeProgrammablePower:
        if (!transport_->writeLine(QStringLiteral("*RST"))) {
            return false;
        }
        return set(HuilingScpiCmd::ConfigureProgrammablePower,
                   QVariantMap{{QStringLiteral("voltage"), profile.scpiPowerVoltageV},
                               {QStringLiteral("current"), profile.scpiPowerCurrentA}});
    case HuilingScpiCmd::SendRawLine:
        return transport_->writeLine(data.toString());
    default:
        return false;
    }
}

bool HuilingWfp60hScpiDevice::get(HuilingScpiCmd cmd, const QVariant& param) {
    if (!transport_ || !transport_->isOpen()) {
        return false;
    }
    const HuilingWfp60hScpiProfile profile = activeProfile();
    switch (cmd) {
    case HuilingScpiCmd::ReadMeasureCurrent:
        if (transport_->linkKind() == ScpiLinkKind::Visa) {
            return queryMeasureLine(profile.buildReadMeasureCurrentLine(), true);
        }
        return transport_->writeLine(profile.buildReadMeasureCurrentLine());
    case HuilingScpiCmd::ReadProgrammableVoltage:
        return set(HuilingScpiCmd::ReadProgrammableVoltage);
    case HuilingScpiCmd::ReadProgrammableCurrent:
        return set(HuilingScpiCmd::ReadProgrammableCurrent, param);
    case HuilingScpiCmd::InitializeProgrammablePower:
        return set(HuilingScpiCmd::InitializeProgrammablePower);
    default:
        return false;
    }
}

bool HuilingWfp60hScpiDevice::sendCustomMessage(const QVariantMap& map) {
    const QString action = map.value(QStringLiteral("action")).toString().trimmed().toLower();
    if (action == QStringLiteral("sendscpiline")) {
        return set(HuilingScpiCmd::SendRawLine, map.value(QStringLiteral("line")));
    }
    if (action == QStringLiteral("configureprogrammablepower")) {
        QVariantMap payload;
        payload.insert(QStringLiteral("voltage"), map.value(QStringLiteral("voltage")));
        payload.insert(QStringLiteral("current"), map.value(QStringLiteral("current")));
        return set(HuilingScpiCmd::ConfigureProgrammablePower, payload);
    }
    if (action == QStringLiteral("programmablepoweroutput")) {
        return set(HuilingScpiCmd::ProgrammablePowerOutput, map.value(QStringLiteral("enable")));
    }
    if (action == QStringLiteral("readprogrammablepowervoltage")) {
        return get(HuilingScpiCmd::ReadProgrammableVoltage);
    }
    if (action == QStringLiteral("readprogrammablepowercurrent")) {
        return get(HuilingScpiCmd::ReadProgrammableCurrent);
    }
    if (action == QStringLiteral("initializeprogrammablepower")) {
        return get(HuilingScpiCmd::InitializeProgrammablePower);
    }
    return false;
}

void HuilingWfp60hScpiDevice::emitProgrammablePowerReadIfPending(const QString& scpiLine) {
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

// --- IScpiDevice interface bridge ---

bool HuilingWfp60hScpiDevice::set(int cmd, const QVariant& param) {
    return set(static_cast<HuilingScpiCmd>(cmd), param);
}

bool HuilingWfp60hScpiDevice::get(int cmd, const QVariant& param) {
    return get(static_cast<HuilingScpiCmd>(cmd), param);
}

bool HuilingWfp60hScpiDevice::isQueryCmd(int cmd) const {
    switch (static_cast<HuilingScpiCmd>(cmd)) {
    case HuilingScpiCmd::ReadMeasureCurrent:
    case HuilingScpiCmd::ReadProgrammableVoltage:
    case HuilingScpiCmd::ReadProgrammableCurrent:
    case HuilingScpiCmd::InitializeProgrammablePower:
        return true;
    default:
        return false;
    }
}

void HuilingWfp60hScpiDevice::handleLineReceived(const QString& line) {
    auto it = commandList_.find(line);
    if (it != commandList_.end()) {
        it->second(line);
        return;
    }
    if (pendingProgPowerRead_ != ProgrammablePowerReadPending::None) {
        emitProgrammablePowerReadIfPending(line);
        return;
    }
    emit measureReadingReceived(line);
    qDebug() << "SCPI 电流读数:" << line;
}
