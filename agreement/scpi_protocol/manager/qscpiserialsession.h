#ifndef QSCPISERIALSESSION_H
#define QSCPISERIALSESSION_H

#include <QByteArray>
#include <QObject>
#include <QString>

#include "scpi_line_codec.h"
#include "scpi_transport.h"

class QSerialPort;

/** 串口 SCPI 传输：写一行、收行切分；不含设备命令语义。 */
class QScpiSerialSession : public QObject, public ScpiTransport {
    Q_OBJECT

  public:
    explicit QScpiSerialSession(QSerialPort* port, QObject* parent = nullptr);

    QSerialPort* serialPort() const {
        return serialPort_;
    }

    ScpiLinkKind linkKind() const override {
        return ScpiLinkKind::Serial;
    }
    bool isOpen() const override;
    bool writeLine(const QString& line) override;
    bool queryLine(const QString& line, QString* responseOut) override;

    void feedRx(const QByteArray& chunk);

  signals:
    void lineReceived(const QString& line);

  private:
    QSerialPort* serialPort_ = nullptr;
    ScpiLineCodec scpiLineCodec_;
};

#endif // QSCPISERIALSESSION_H
