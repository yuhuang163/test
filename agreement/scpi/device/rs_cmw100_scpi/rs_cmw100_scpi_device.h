#ifndef RS_CMW100_SCPI_DEVICE_H
#define RS_CMW100_SCPI_DEVICE_H

#include <QObject>
#include <QVariant>

#include "rs_cmw100_scpi_types.h"

class ScpiTransport;

/** 罗德 CMW100：CmwScpiCmd → GPRF SCPI 行（VISA 同步 query）。 */
class RsCmw100ScpiDevice : public QObject {
    Q_OBJECT

  public:
    explicit RsCmw100ScpiDevice(ScpiTransport* transport, QObject* parent = nullptr);

    void setTransport(ScpiTransport* transport);

    bool set(CmwScpiCmd cmd, const QVariant& data = {});
    bool get(CmwScpiCmd cmd, const QVariant& param = {});

    QString lastQueryResponse() const;

    static QString arbFileCommand(const QString& rawPath);

  private:
    bool writeLine(const QString& cmd);
    bool queryLine(const QString& cmd);

    ScpiTransport* transport_ = nullptr;
    QString lastQueryResponse_;
};

#endif // RS_CMW100_SCPI_DEVICE_H
