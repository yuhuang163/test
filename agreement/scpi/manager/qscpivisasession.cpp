#include "qscpivisasession.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QScpiVisaSession::QScpiVisaSession(QObject* parent) : QObject(parent), visaChannel_(this) {}

void QScpiVisaSession::setVisaConfig(const ScpiVisaLinkConfig& config) {
    if (config_.visaAddress != config.visaAddress) {
        closeConnection();
    }
    config_ = config;
    VisaChannel::Config channelConfig;
    channelConfig.resourceAddress = config.visaAddress;
    channelConfig.timeoutMs = config.timeoutMs;
    visaChannel_.setConfig(channelConfig);
}

ScpiVisaLinkConfig QScpiVisaSession::visaConfig() const {
    return config_;
}

bool QScpiVisaSession::isOpen() const {
    return visaChannel_.isOpen();
}

bool QScpiVisaSession::ensureConnected() {
    return visaChannel_.ensureConnected();
}

void QScpiVisaSession::closeConnection() {
    visaChannel_.close();
}

bool QScpiVisaSession::writeLine(const QString& line) {
    QByteArray payload = line.toLocal8Bit();
    if (!payload.endsWith('\n')) {
        payload.append('\n');
    }
    return visaChannel_.write(payload);
}

bool QScpiVisaSession::queryLine(const QString& line, QString* responseOut) {
    if (!responseOut) {
        return false;
    }
    responseOut->clear();
    if (!writeLine(line)) {
        return false;
    }
    QByteArray raw;
    if (!visaChannel_.read(&raw)) {
        qDebug() << "QScpiVisaSession: 读取失败 cmd=" << line;
        return false;
    }
    *responseOut = QString::fromLocal8Bit(raw).trimmed();
    return true;
}
