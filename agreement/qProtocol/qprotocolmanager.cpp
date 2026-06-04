#include "qprotocolmanager.h"

#include <algorithm>
#include <cctype>

#include "qpb/qpb.h"
#include "qfctp/qfctp.h"
#include "qaiot/qaiot.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {
void syncManagerSignals(qProtocol* protocol, QProtocolManager* manager, bool shouldConnect) {
    if (protocol == nullptr || manager == nullptr) {
        return;
    }

    if (shouldConnect) {
        QObject::connect(protocol, &qProtocol::send_pb_date, manager, &QProtocolManager::send_pb_date);
        QObject::connect(protocol, &qProtocol::sendGetProductResponse, manager, &QProtocolManager::sendGetProductResponse);
        QObject::connect(protocol, &qProtocol::send_base_data, manager, &QProtocolManager::send_base_data);
        QObject::connect(protocol, &qProtocol::send_sn_data, manager, &QProtocolManager::send_sn_data);
        QObject::connect(protocol, &qProtocol::send_battary, manager, &QProtocolManager::send_battary);
        QObject::connect(protocol, &qProtocol::send_button_state, manager, &QProtocolManager::send_button_state);
        QObject::connect(protocol, &qProtocol::send_periph_data, manager, &QProtocolManager::send_periph_data);
        QObject::connect(protocol, &qProtocol::send_photosensitive_info, manager, &QProtocolManager::send_photosensitive_info);
        return;
    }

    QObject::disconnect(protocol, &qProtocol::send_pb_date, manager, &QProtocolManager::send_pb_date);
    QObject::disconnect(protocol, &qProtocol::sendGetProductResponse, manager, &QProtocolManager::sendGetProductResponse);
    QObject::disconnect(protocol, &qProtocol::send_base_data, manager, &QProtocolManager::send_base_data);
    QObject::disconnect(protocol, &qProtocol::send_sn_data, manager, &QProtocolManager::send_sn_data);
    QObject::disconnect(protocol, &qProtocol::send_battary, manager, &QProtocolManager::send_battary);
    QObject::disconnect(protocol, &qProtocol::send_button_state, manager, &QProtocolManager::send_button_state);
    QObject::disconnect(protocol, &qProtocol::send_periph_data, manager, &QProtocolManager::send_periph_data);
    QObject::disconnect(protocol, &qProtocol::send_photosensitive_info, manager, &QProtocolManager::send_photosensitive_info);
}
}  // namespace

QProtocolManager::QProtocolManager(QObject* parent) : QObject(parent) {}

QProtocolManager::ProtocolType QProtocolManager::currentProtocolType() const {
    return currentType_;
}

void QProtocolManager::setCurrentProtocolType(QProtocolManager::ProtocolType type) {
    currentType_ = type;
    syncActivePointer();
}

QProtocolManager::ProtocolType QProtocolManager::protocolTypeFromString(const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });

    if (lower == "qpb") {
        return ProtocolType::Qpb;
    }
    if (lower == "qfctp") {
        return ProtocolType::Qfctp;
    }
    if (lower == "qaiot") {
        return ProtocolType::Qaiot;
    }
    return ProtocolType::Unknown;
}

std::string QProtocolManager::protocolTypeToString(QProtocolManager::ProtocolType type) {
    switch (type) {
        case ProtocolType::Qpb: return "qpb";
        case ProtocolType::Qfctp: return "qfctp";
        case ProtocolType::Qaiot: return "qaiot";
        default: return "unknown";
    }
}

void QProtocolManager::bindQpb(Qpb* pb) {
    qpb_ = pb;
    syncActivePointer();
}

void QProtocolManager::bindQfctp(Qfctp* fctp) {
    qfctp_ = fctp;
    syncActivePointer();
}

void QProtocolManager::bindQaiot(Qaiot* aiot) {
    qaiot_ = aiot;
    syncActivePointer();
}


qProtocol* QProtocolManager::currentProtocol() const {
    return active_;
}

bool QProtocolManager::hasActiveProtocol() const {
    return active_ != nullptr;
}

bool QProtocolManager::parseCmd(const QByteArray& byte) const {
    if (!active_) {
        return false;
    }
    active_->parseCmd(byte);
    return true;
}

bool QProtocolManager::set(DeviceCmd cmd, const QVariant& data) const {
    if (!active_) {
        return false;
    }
    active_->set(cmd, data);
    return true;
}

bool QProtocolManager::get(DeviceCmd cmd, const QVariant& param) const {
    if (!active_) {
        return false;
    }
    active_->get(cmd, param);
    return true;
}

bool QProtocolManager::sendCustomMessage(const QVariantMap& map) const {
    if (!active_) {
        return false;
    }
    return active_->sendCustomMessage(map);
}

bool QProtocolManager::setPbMode(int p) const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return false;
    }
    pb->setPbMode(p);
    return true;
}

bool QProtocolManager::setNeedAes(bool needAes) const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return false;
    }
    pb->setNeedAes(needAes);
    return true;
}

bool QProtocolManager::setAppVersion(const QString& version) const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return false;
    }
    pb->setAppVersion(version);
    return true;
}

QString QProtocolManager::getAppVersion() const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return QString();
    }
    return pb->getAppVersion();
}

QByteArray QProtocolManager::aes256Decrypt(const QByteArray& encrypted) const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return QByteArray();
    }
    return pb->aes256Decrypt(encrypted);
}

QByteArray QProtocolManager::aes256Encrypt(const QByteArray& input) const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return QByteArray();
    }
    return pb->aes256Encrypt(input);
}

int QProtocolManager::getState(int stateType, int defaultValue) const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return defaultValue;
    }
    if (stateType < 0) {
        return defaultValue;
    }
    return pb->getState(static_cast<Qpb::PbStateType>(stateType));
}

bool QProtocolManager::setState(int stateType, int value) const {
    Qpb* pb = currentQpb();
    if (!pb || stateType < 0) {
        return false;
    }
    pb->setState(static_cast<Qpb::PbStateType>(stateType), value);
    return true;
}

bool QProtocolManager::resetAllPb() const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return false;
    }
    pb->reset_all_pb();
    return true;
}

bool QProtocolManager::setShipCount(int count) const {
    Qpb* pb = currentQpb();
    if (!pb) {
        return false;
    }
    pb->setShipCount(count);
    return true;
}

Qpb* QProtocolManager::currentQpb() const {
    if (currentType_ == ProtocolType::Qpb) {
        return qpb_;
    }
    return nullptr;
}

Qfctp* QProtocolManager::currentQfctp() const {
    if (currentType_ == ProtocolType::Qfctp) {
        return qfctp_;
    }
    return nullptr;
}

Qaiot* QProtocolManager::currentQaiot() const {
    if (currentType_ == ProtocolType::Qaiot) {
        return qaiot_;
    }
    return nullptr;
}

bool QProtocolManager::isQpbProtocolActive() const {
    return currentType_ == ProtocolType::Qpb;
}

bool QProtocolManager::isQfctpProtocolActive() const {
    return currentType_ == ProtocolType::Qfctp;
}

bool QProtocolManager::isQaiotProtocolActive() const {
    return currentType_ == ProtocolType::Qaiot;
}

void QProtocolManager::syncActivePointer() {
    syncManagerSignals(qpb_, this, false);
    syncManagerSignals(qfctp_, this, false);
    syncManagerSignals(qaiot_, this, false);

    switch (currentType_) {
        case ProtocolType::Qpb:
            active_ = qpb_ ? static_cast<qProtocol*>(qpb_) : nullptr;
            syncManagerSignals(active_, this, true);
            break;
        case ProtocolType::Qfctp:
            active_ = qfctp_ ? static_cast<qProtocol*>(qfctp_) : nullptr;
            syncManagerSignals(active_, this, true);
            break;
        case ProtocolType::Qaiot:
            active_ = qaiot_ ? static_cast<qProtocol*>(qaiot_) : nullptr;
            syncManagerSignals(active_, this, true);
            break;
        default:
            active_ = nullptr;
            break;
    }
}
