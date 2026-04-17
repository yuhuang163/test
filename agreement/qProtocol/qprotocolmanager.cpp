#include "qprotocolmanager.h"

#include <algorithm>
#include <cctype>

#include "qpb.h"
#include "qfctp.h"

QProtocolManager::QProtocolManager() {}

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
    switch (currentType_) {
        case ProtocolType::Qpb:
            active_ = qpb_ ? static_cast<qProtocol*>(qpb_) : nullptr;
            break;
        case ProtocolType::Qfctp:
            active_ = qfctp_ ? static_cast<qProtocol*>(qfctp_) : nullptr;
            break;
        default:
            active_ = nullptr;
            break;
    }
}
