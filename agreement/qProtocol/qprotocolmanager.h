#ifndef QPROTOCOLMANAGER_H
#define QPROTOCOLMANAGER_H

#include "qprotocol.h"
#include <string>

class Qpb;
class Qfctp;

class QProtocolManager {
public:
    QProtocolManager();

    enum class ProtocolType {
        Unknown = 0,
        Qpb,
        Qfctp,
    };

    ProtocolType currentProtocolType() const;
    void setCurrentProtocolType(ProtocolType type);

    static ProtocolType protocolTypeFromString(const std::string& value);
    static std::string protocolTypeToString(ProtocolType type);

    void bindQpb(Qpb* pb);
    void bindQfctp(Qfctp* fctp);

    qProtocol* currentProtocol() const;
    Qpb* currentQpb() const;
    Qfctp* currentQfctp() const;

    bool isQpbProtocolActive() const;
    bool isQfctpProtocolActive() const;

private:
    void syncActivePointer();

    ProtocolType currentType_ = ProtocolType::Qpb;
    Qpb* qpb_ = nullptr;
    Qfctp* qfctp_ = nullptr;
    qProtocol* active_ = nullptr;
};

#endif  // QPROTOCOLMANAGER_H
