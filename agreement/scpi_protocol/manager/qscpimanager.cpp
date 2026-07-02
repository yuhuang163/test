#include "qscpimanager.h"

#include "Abini.h"
#include "huiling_wfp60h_scpi_device.h"
#include "qscpiserialsession.h"
#include "qscpivisasession.h"

#include <QDebug>
#include <QSerialPort>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QScpiManager::QScpiManager(QObject* parent)
    : QObject(parent), huilingDevice_(nullptr, this), cmwDevice_(nullptr, this) {
    visaSession_ = new QScpiVisaSession(this);
    connect(&huilingDevice_, &HuilingWfp60hScpiDevice::measureReadingReceived, this, &QScpiManager::measureReadingReceived);
    connect(&huilingDevice_, &HuilingWfp60hScpiDevice::programmablePowerVoltageRead, this,
            &QScpiManager::programmablePowerVoltageRead);
    connect(&huilingDevice_, &HuilingWfp60hScpiDevice::programmablePowerCurrentRead, this,
            &QScpiManager::programmablePowerCurrentRead);
}

QScpiManager::QScpiManager(QSerialPort* port, QObject* parent) : QScpiManager(parent) {
    attachSerialPort(port);
}

void QScpiManager::bindActiveTransport(ScpiTransport* transport, ScpiLinkKind kind) {
    activeTransport_ = transport;
    linkKind_ = kind;
    huilingDevice_.setTransport(activeTransport_);
    cmwDevice_.setTransport(activeTransport_);
}

void QScpiManager::attachSerialPort(QSerialPort* port) {
    if (!serialSession_) {
        serialSession_ = new QScpiSerialSession(port, this);
        connect(serialSession_, &QScpiSerialSession::lineReceived, this, &QScpiManager::onLineReceived);
    }
    bindActiveTransport(serialSession_, ScpiLinkKind::Serial);
}

void QScpiManager::setVisaConfig(const ScpiVisaLinkConfig& config) {
    if (!visaSession_) {
        visaSession_ = new QScpiVisaSession(this);
    }
    visaSession_->setVisaConfig(config);
    bindActiveTransport(visaSession_, ScpiLinkKind::Visa);
}

ScpiVisaLinkConfig QScpiManager::visaConfig() const {
    return visaSession_ ? visaSession_->visaConfig() : ScpiVisaLinkConfig{};
}

ScpiLinkKind QScpiManager::linkKind() const {
    return linkKind_;
}

bool QScpiManager::ensureConnected() {
    if (!activeTransport_) {
        return false;
    }
    if (linkKind_ == ScpiLinkKind::Visa && visaSession_) {
        return visaSession_->ensureConnected();
    }
    return activeTransport_->isOpen();
}

void QScpiManager::closeConnection() {
    if (visaSession_) {
        visaSession_->closeConnection();
    }
}

QSerialPort* QScpiManager::serialPort() const {
    return serialSession_ ? serialSession_->serialPort() : nullptr;
}

QScpiSerialSession* QScpiManager::session() {
    return serialSession_;
}

void QScpiManager::setDeviceRoute(ScpiDeviceRoute route) {
    deviceRoute_ = route;
}

ScpiDeviceRoute QScpiManager::deviceRoute() const {
    return deviceRoute_;
}

bool QScpiManager::isScpiDeviceRoute() const {
    return deviceRoute_ != ScpiDeviceRoute::None;
}

void QScpiManager::setHuilingProfilePatch(const HuilingWfp60hScpiProfile& patch) {
    huilingDevice_.setProfilePatch(patch);
}

void QScpiManager::clearHuilingProfilePatch() {
    huilingDevice_.clearProfilePatch();
}

HuilingWfp60hScpiProfile QScpiManager::huilingActiveProfile() const {
    return huilingDevice_.activeProfile();
}

void QScpiManager::loadHuilingVisaFromSettings() {
    ScpiVisaLinkConfig link;
    link.visaAddress =
        SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QStringLiteral("GPIB0::7::INSTR")).toString().trimmed();
    link.timeoutMs = SETTINGS.value(QStringLiteral("VisaPower/TimeoutMs"), 3000).toInt();
    setVisaConfig(link);
    setDeviceRoute(ScpiDeviceRoute::HuilingWfp60h);
    setHuilingProfilePatch(HuilingWfp60hScpiProfile::fromVisaPowerSettings());
}

bool QScpiManager::loadHuilingVisaFromParamMap(const QVariantMap& map, int timeoutMs) {
    const QString visaAddress = map.value(QStringLiteral("visaAddress")).toString().trimmed();
    if (visaAddress.isEmpty()) {
        return false;
    }
    ScpiVisaLinkConfig link;
    link.visaAddress = visaAddress;
    link.timeoutMs = timeoutMs > 0 ? timeoutMs : 3000;
    setVisaConfig(link);
    setDeviceRoute(ScpiDeviceRoute::HuilingWfp60h);
    setHuilingProfilePatch(HuilingWfp60hScpiProfile::fromParamMap(map));
    return true;
}

void QScpiManager::loadCmwVisaFromSettings() {
    ScpiVisaLinkConfig link;
    link.visaAddress = SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed();
    link.timeoutMs = SETTINGS.value(QStringLiteral("BlePer/CmwTimeoutMs"), 5000).toInt();
    setVisaConfig(link);
    setDeviceRoute(ScpiDeviceRoute::RsCmw100);
    clearHuilingProfilePatch();
}

HuilingWfp60hScpiDevice* QScpiManager::huilingDevice() {
    return &huilingDevice_;
}

RsCmw100ScpiDevice* QScpiManager::cmwDevice() {
    return &cmwDevice_;
}

QString QScpiManager::lastQueryResponse() const {
    if (deviceRoute_ == ScpiDeviceRoute::RsCmw100) {
        return cmwDevice_.lastQueryResponse();
    }
    return QString();
}

IScpiDevice* QScpiManager::activeDevice() {
    switch (deviceRoute_) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return &huilingDevice_;
    case ScpiDeviceRoute::RsCmw100:
        return &cmwDevice_;
    default:
        return nullptr;
    }
}

bool QScpiManager::isTransportReady(QString* errorMessage) const {
    if (!activeTransport_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SCPI 传输未初始化");
        }
        return false;
    }
    if (linkKind_ == ScpiLinkKind::Visa) {
        if (!visaSession_ || !visaSession_->ensureConnected()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("VISA 未连接");
            }
            return false;
        }
        return true;
    }
    if (!activeTransport_->isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SCPI 串口未打开");
        }
        return false;
    }
    return true;
}

bool QScpiManager::feedRx(const QByteArray& chunk) {
    if (!serialSession_ || chunk.isEmpty()) {
        return false;
    }
    serialSession_->feedRx(chunk);
    return true;
}

bool QScpiManager::sendCustomMessage(const QVariantMap& map) {
    if (deviceRoute_ != ScpiDeviceRoute::HuilingWfp60h) {
        return false;
    }
    return huilingDevice_.sendCustomMessage(map);
}

void QScpiManager::onLineReceived(const QString& line) {
    if (deviceRoute_ == ScpiDeviceRoute::HuilingWfp60h) {
        huilingDevice_.handleLineReceived(line);
    }
}
