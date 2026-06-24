#ifndef SCPI_TRANSPORT_H
#define SCPI_TRANSPORT_H

#include "scpi_types.h"

#include <QString>

/** SCPI 协议传输抽象：串口/VISA 会话在 manager 实现（组行、query）；平台仅提供 serial/visa 字节 I/O。 */
class ScpiTransport {
  public:
    virtual ~ScpiTransport() = default;

    virtual ScpiLinkKind linkKind() const = 0;
    virtual bool isOpen() const = 0;
    virtual bool writeLine(const QString& line) = 0;
    /** 同步写问句并读一行应答；串口异步场景可返回 false，由 feedRx + 回调处理。 */
    virtual bool queryLine(const QString& line, QString* responseOut) = 0;
};

#endif // SCPI_TRANSPORT_H
