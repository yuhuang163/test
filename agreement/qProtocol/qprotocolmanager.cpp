#include "qprotocolmanager.h"

#include <algorithm>
#include <cctype>

#include "qpb/qpb.h"
#include "qfctp/qfctp.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

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
    return ProtocolType::Unknown;
}

std::string QProtocolManager::protocolTypeToString(QProtocolManager::ProtocolType type) {
    switch (type) {
        case ProtocolType::Qpb: return "qpb";
        case ProtocolType::Qfctp: return "qfctp";
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

bool QProtocolManager::isQpbProtocolActive() const {
    return currentType_ == ProtocolType::Qpb;
}

bool QProtocolManager::isQfctpProtocolActive() const {
    return currentType_ == ProtocolType::Qfctp;
}

void QProtocolManager::syncActivePointer() {
    if (qpb_ != nullptr) {
        disconnect(qpb_, &Qpb::send_pb_date, this, &QProtocolManager::send_pb_date);
        disconnect(qpb_, &Qpb::sendGetProductResponse, this, &QProtocolManager::sendGetProductResponse);
        disconnect(qpb_, &Qpb::send_sn_data, this, &QProtocolManager::send_sn_data);
        disconnect(qpb_, &Qpb::send_battary, this, &QProtocolManager::send_battary);
        disconnect(qpb_, &Qpb::send_button_state, this, &QProtocolManager::send_button_state);
        disconnect(qpb_, &Qpb::send_periph_data, this, &QProtocolManager::send_periph_data);
        disconnect(qpb_, &Qpb::send_photosensitive_info, this, &QProtocolManager::send_photosensitive_info);
    }
    if (qfctp_ != nullptr) {
        disconnect(qfctp_, &Qfctp::send_pb_date, this, &QProtocolManager::send_pb_date);
        disconnect(qfctp_, &Qfctp::sendGetProductResponse, this, &QProtocolManager::sendGetProductResponse);
        disconnect(qfctp_, &Qfctp::send_sn_data, this, &QProtocolManager::send_sn_data);
        disconnect(qfctp_, &Qfctp::send_battary, this, &QProtocolManager::send_battary);
        disconnect(qfctp_, &Qfctp::send_button_state, this, &QProtocolManager::send_button_state);
        disconnect(qfctp_, &Qfctp::send_periph_data, this, &QProtocolManager::send_periph_data);
        disconnect(qfctp_, &Qfctp::send_photosensitive_info, this, &QProtocolManager::send_photosensitive_info);
    }

    switch (currentType_) {
        case ProtocolType::Qpb:
            active_ = qpb_ ? static_cast<qProtocol*>(qpb_) : nullptr;
            if (qpb_ != nullptr) {
                connect(qpb_, &Qpb::send_pb_date, this, &QProtocolManager::send_pb_date);
                connect(qpb_, &Qpb::sendGetProductResponse, this, &QProtocolManager::sendGetProductResponse);
                connect(qpb_, &Qpb::send_sn_data, this, &QProtocolManager::send_sn_data);
                connect(qpb_, &Qpb::send_battary, this, &QProtocolManager::send_battary);
                connect(qpb_, &Qpb::send_button_state, this, &QProtocolManager::send_button_state);
                connect(qpb_, &Qpb::send_periph_data, this, &QProtocolManager::send_periph_data);
                connect(qpb_, &Qpb::send_photosensitive_info, this, &QProtocolManager::send_photosensitive_info);
            }
            break;
        case ProtocolType::Qfctp:
            active_ = qfctp_ ? static_cast<qProtocol*>(qfctp_) : nullptr;
            if (qfctp_ != nullptr) {
                connect(qfctp_, &Qfctp::send_pb_date, this, &QProtocolManager::send_pb_date);
                connect(qfctp_, &Qfctp::sendGetProductResponse, this, &QProtocolManager::sendGetProductResponse);
                connect(qfctp_, &Qfctp::send_sn_data, this, &QProtocolManager::send_sn_data);
                connect(qfctp_, &Qfctp::send_battary, this, &QProtocolManager::send_battary);
                connect(qfctp_, &Qfctp::send_button_state, this, &QProtocolManager::send_button_state);
                connect(qfctp_, &Qfctp::send_periph_data, this, &QProtocolManager::send_periph_data);
                connect(qfctp_, &Qfctp::send_photosensitive_info, this, &QProtocolManager::send_photosensitive_info);
            }
            break;
        default:
            active_ = nullptr;
            break;
    }
}
