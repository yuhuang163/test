#include "qproduct.h"

#include <QSerialPort>

Qproduct::Qproduct(QSerialPort* port, QObject* parent) : QObject(parent), port_(port) {}

QByteArray Qproduct::hexToBytes(const QString& hexNoSpaces) {
    QString s;
    s.reserve(hexNoSpaces.size());
    for (const QChar& ch : hexNoSpaces) {
        if (ch.isSpace())
            continue;
        s.append(ch);
    }
    if (s.size() % 2 != 0)
        return {};
    return QByteArray::fromHex(s.toLatin1());
}

QString Qproduct::bytesToHex(const QByteArray& data) {
    return QString::fromLatin1(data.toHex());
}

bool Qproduct::writeRaw(const QByteArray& frame, QString* errorOut) {
    if (!port_) {
        if (errorOut)
            *errorOut = QStringLiteral("product serial null");
        return false;
    }
    if (!port_->isOpen()) {
        if (errorOut)
            *errorOut = QStringLiteral("product serial not open");
        return false;
    }
    const qint64 n = port_->write(frame);
    if (n != frame.size()) {
        if (errorOut)
            *errorOut = QStringLiteral("write incomplete");
        return false;
    }
    if (!port_->waitForBytesWritten(3000)) {
        if (errorOut)
            *errorOut = QStringLiteral("waitForBytesWritten timeout");
        return false;
    }
    return true;
}

bool Qproduct::writeHex(const QString& hexNoSpaces, QString* errorOut) {
    const QByteArray frame = hexToBytes(hexNoSpaces);
    if (frame.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("invalid hex");
        return false;
    }
    return writeRaw(frame, errorOut);
}
