#ifndef QPROTOCOL_H
#define QPROTOCOL_H

#include <QByteArray>
#include <QVariant>

#include "qpb/qpb_types.h"

class qProtocol {
public:
    qProtocol();
    virtual void parseCmd(const QByteArray& byte) = 0;
    // Unified device command interfaces. Concrete protocols can ignore unsupported commands.
    virtual void set(DeviceCmd cmd, const QVariant& data = {}) = 0;
    virtual void get(DeviceCmd cmd, const QVariant& param = {}) = 0;
};

#endif  // QPROTOCOL_H
