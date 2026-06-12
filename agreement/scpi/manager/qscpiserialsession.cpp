#include "qscpiserialsession.h"

#include <QDebug>
#include <QSerialPort>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QScpiSerialSession::QScpiSerialSession(QSerialPort* port, QObject* parent) : QObject(parent), serialPort_(port) {
}

bool QScpiSerialSession::isOpen() const {
    return serialPort_ && serialPort_->isOpen();
}

bool QScpiSerialSession::queryLine(const QString& line, QString* responseOut) {
    Q_UNUSED(line);
    Q_UNUSED(responseOut);
    return false;
}

bool QScpiSerialSession::writeLine(const QString& line) {
    if (!isOpen()) {
        qDebug() << "QScpiSerialSession: 串口未打开";
        return false;
    }
    QString payload = line;
    payload += QStringLiteral("\r\n");
    const QByteArray data = payload.toLocal8Bit();
    qDebug().noquote() << "SCPI TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    serialPort_->write(data);
    return true;
}

void QScpiSerialSession::feedRx(const QByteArray& chunk) {
    if (chunk.isEmpty()) {
        return;
    }
    qDebug().noquote() << "SCPI RX:" << QString::fromLatin1(chunk.toHex(' ').toUpper());
    scpiLineCodec_.feed(chunk, [this](const QString& line) { emit lineReceived(line); });
}
