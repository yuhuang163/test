#ifndef QPROTOCOL_H
#define QPROTOCOL_H

#include <QByteArray>

class qProtocol {
public:
  
    qProtocol();
    virtual void parseCmd(const QByteArray& byte) = 0;
};

#endif  // QPROTOCOL_H
