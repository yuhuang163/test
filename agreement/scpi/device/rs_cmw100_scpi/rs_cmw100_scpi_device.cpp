#include "rs_cmw100_scpi_device.h"

#include "Abini.h"
#include "scpi_transport.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

RsCmw100ScpiDevice::RsCmw100ScpiDevice(ScpiTransport* transport, QObject* parent)
    : QObject(parent), transport_(transport) {
}

void RsCmw100ScpiDevice::setTransport(ScpiTransport* transport) {
    transport_ = transport;
}

QString RsCmw100ScpiDevice::lastQueryResponse() const {
    return lastQueryResponse_;
}

QString RsCmw100ScpiDevice::arbFileCommand(const QString& rawPath) {
    const QString p = rawPath.trimmed();
    if (p.startsWith(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE"), Qt::CaseInsensitive)) {
        return p;
    }
    if (p.size() >= 2 && p.front() == QLatin1Char('\'') && p.back() == QLatin1Char('\'')) {
        return QStringLiteral("SOURce:GPRF:GEN:ARB:FILE %1").arg(p);
    }
    QString inner = p;
    if (inner.size() >= 2 && inner.front() == QLatin1Char('"') && inner.back() == QLatin1Char('"')) {
        inner = inner.mid(1, inner.size() - 2).trimmed();
    }
    return QStringLiteral("SOURce:GPRF:GEN:ARB:FILE '%1'").arg(inner);
}

bool RsCmw100ScpiDevice::writeLine(const QString& cmd) {
    if (!transport_ || !transport_->isOpen()) {
        return false;
    }
    const bool ok = transport_->writeLine(cmd);
    if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaTrace"), true).toBool()) {
        qDebug() << "[CMW-VISA] >>" << cmd << "(ok=" << ok << ")";
    }
    return ok;
}

bool RsCmw100ScpiDevice::queryLine(const QString& cmd) {
    if (!transport_ || !transport_->isOpen()) {
        return false;
    }
    lastQueryResponse_.clear();
    const bool ok = transport_->queryLine(cmd, &lastQueryResponse_);
    if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaTrace"), true).toBool()) {
        qDebug() << "[CMW-VISA] >>" << cmd << "<<" << lastQueryResponse_;
    }
    return ok;
}

bool RsCmw100ScpiDevice::set(CmwScpiCmd cmd, const QVariant& data) {
    switch (cmd) {
    case CmwScpiCmd::ClearStatus:
        return writeLine(QStringLiteral("*CLS"));
    case CmwScpiCmd::GenOff:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"));
    case CmwScpiCmd::GenOn:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:STATe ON;*OPC?"));
    case CmwScpiCmd::ListOff:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:LIST OFF"));
    case CmwScpiCmd::BbModeArb:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:BBMode ARB"));
    case CmwScpiCmd::ArbFile:
        return writeLine(arbFileCommand(data.toString()));
    case CmwScpiCmd::ArbRepetition:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(data.toString()));
    case CmwScpiCmd::ArbCycles:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(data.toInt()));
    case CmwScpiCmd::TxLevelDbm:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:RFSettings:LEVel %1").arg(data.toDouble(), 0, 'f', 1));
    case CmwScpiCmd::FrequencyMhz:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:RFSettings:FREQuency %1MHz").arg(data.toInt()));
    case CmwScpiCmd::ManualArbTrigger:
        return writeLine(QStringLiteral("TRIGger:GPRF:GEN:ARB:MANual:EXECute"));
    case CmwScpiCmd::WriteLine:
        return writeLine(data.toString());
    default:
        return false;
    }
}

bool RsCmw100ScpiDevice::get(CmwScpiCmd cmd, const QVariant& param) {
    switch (cmd) {
    case CmwScpiCmd::Identity:
        return queryLine(QStringLiteral("*IDN?"));
    case CmwScpiCmd::ArbFilePath:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"));
    case CmwScpiCmd::ArbScount:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:ARB:SCOunt?"));
    case CmwScpiCmd::GenState:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:STATe?"));
    case CmwScpiCmd::SystemError:
        return queryLine(QStringLiteral("SYSTem:ERRor?"));
    case CmwScpiCmd::QueryLine:
        return queryLine(param.toString());
    default:
        return false;
    }
}

// --- IScpiDevice interface bridge ---

bool RsCmw100ScpiDevice::set(int cmd, const QVariant& param) {
    return set(static_cast<CmwScpiCmd>(cmd), param);
}

bool RsCmw100ScpiDevice::get(int cmd, const QVariant& param) {
    return get(static_cast<CmwScpiCmd>(cmd), param);
}

bool RsCmw100ScpiDevice::isQueryCmd(int cmd) const {
    switch (static_cast<CmwScpiCmd>(cmd)) {
    case CmwScpiCmd::Identity:
    case CmwScpiCmd::ArbFilePath:
    case CmwScpiCmd::ArbScount:
    case CmwScpiCmd::GenState:
    case CmwScpiCmd::SystemError:
    case CmwScpiCmd::QueryLine:
    case CmwScpiCmd::GenOff:
    case CmwScpiCmd::GenOn:
        return true;
    default:
        return false;
    }
}
