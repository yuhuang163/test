#include "qcmw.h"

#include "Abini.h"
#include "qvisa.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Qcmw::Qcmw(QObject* parent) : QObject(parent) {
}

void Qcmw::bindVisa(Qvisa* visa) {
    visa_ = visa;
}

QString Qcmw::lastQueryResponse() const {
    return visa_ ? visa_->lastQueryResponse() : QString();
}

bool Qcmw::writeLine(const QString& cmd) {
    if (!visa_) {
        return false;
    }
    visa_->set(VisaCmd::DeviceProfile, static_cast<int>(VisaDeviceProfile::RxCmwInstrument));
    const bool ok = visa_->set(VisaCmd::WriteLine, cmd);
    if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaTrace"), true).toBool()) {
        qDebug() << "[CMW-VISA] >>" << cmd << "(ok=" << ok << ")";
    }
    return ok;
}

bool Qcmw::queryLine(const QString& cmd) {
    if (!visa_) {
        return false;
    }
    visa_->set(VisaCmd::DeviceProfile, static_cast<int>(VisaDeviceProfile::RxCmwInstrument));
    const bool ok = visa_->get(VisaCmd::QueryLine, cmd);
    if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaTrace"), true).toBool()) {
        qDebug() << "[CMW-VISA] >>" << cmd << "<<" << visa_->lastQueryResponse();
    }
    return ok;
}

QString Qcmw::arbFileCommand(const QString& rawPath) {
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

bool Qcmw::set(CmwGprfCmd cmd, const QVariant& data) {
    switch (cmd) {
    case CmwGprfCmd::ClearStatus:
        return writeLine(QStringLiteral("*CLS"));
    case CmwGprfCmd::GenOff:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"));
    case CmwGprfCmd::GenOn:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:STATe ON;*OPC?"));
    case CmwGprfCmd::ListOff:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:LIST OFF"));
    case CmwGprfCmd::BbModeArb:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:BBMode ARB"));
    case CmwGprfCmd::ArbFile:
        return writeLine(arbFileCommand(data.toString()));
    case CmwGprfCmd::ArbRepetition:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(data.toString()));
    case CmwGprfCmd::ArbCycles:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(data.toInt()));
    case CmwGprfCmd::TxLevelDbm:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:RFSettings:LEVel %1").arg(data.toDouble(), 0, 'f', 1));
    case CmwGprfCmd::FrequencyMhz:
        return writeLine(QStringLiteral("SOURce:GPRF:GEN:RFSettings:FREQuency %1MHz").arg(data.toInt()));
    case CmwGprfCmd::ManualArbTrigger:
        return writeLine(QStringLiteral("TRIGger:GPRF:GEN:ARB:MANual:EXECute"));
    case CmwGprfCmd::WriteLine:
        return writeLine(data.toString());
    default:
        return false;
    }
}

bool Qcmw::get(CmwGprfCmd cmd, const QVariant& param) {
    switch (cmd) {
    case CmwGprfCmd::Identity:
        return queryLine(QStringLiteral("*IDN?"));
    case CmwGprfCmd::ArbFilePath:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"));
    case CmwGprfCmd::ArbScount:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:ARB:SCOunt?"));
    case CmwGprfCmd::GenState:
        return queryLine(QStringLiteral("SOURce:GPRF:GEN:STATe?"));
    case CmwGprfCmd::SystemError:
        return queryLine(QStringLiteral("SYSTem:ERRor?"));
    case CmwGprfCmd::QueryLine:
        return queryLine(param.toString());
    default:
        return false;
    }
}
