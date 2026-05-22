#include "qproduct.h"

#include <QDebug>
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
    qDebug().noquote() << "PRODUCT TX:" << QString::fromLatin1(frame.toHex(' ').toUpper());
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
    // 仪器侧原始发包：打 hex 便于与 docs/测试.md 对照
    qDebug() << "[Qproduct] writeRaw len=" << frame.size() << "hex=" << bytesToHex(frame);
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

bool Qproduct::responseContainsPrefix(const QByteArray& rx, const QByteArray& prefix) {
    return rx.indexOf(prefix) >= 0;
}

int Qproduct::parseStopReceivePacketCountLe(const QByteArray& rx) {
    const QByteArray pre = prefixStopReceive();
    const int idx = rx.indexOf(pre);
    if (idx < 0)
        return -1;
    const int valPos = idx + pre.size();
    if (rx.size() < valPos + 2)
        return -1;
    const auto b0 = static_cast<quint8>(rx.at(valPos));
    const auto b1 = static_cast<quint8>(rx.at(valPos + 1));
    return static_cast<int>(b0 | (b1 << 8));
}

double Qproduct::computePer(int instrumentSendCount, int receivedCount) {
    if (instrumentSendCount <= 0)
        return 0.0;
    return static_cast<double>(instrumentSendCount - receivedCount) /
           static_cast<double>(instrumentSendCount);
}

void Qproduct::parseCmd(const QByteArray& data) {
    if (!data.isEmpty()) {
        qDebug().noquote() << "PRODUCT RX:" << QString::fromLatin1(data.toHex(' ').toUpper());
        // 仪器侧原始收包：打 hex 便于与 docs/测试.md 对照
        qDebug() << "[Qproduct] parseCmd len=" << data.size() << "hex=" << bytesToHex(data);
        productSerialRxAccum_.append(data);
        scanRxForInstrumentEvents();
    }
}

void Qproduct::clearProductSerialRxAccum() {
    productSerialRxAccum_.clear();
    emittedAckReset_ = false;
    emittedAckStart_ = false;
    emittedStopReceive_ = false;
}

void Qproduct::scanRxForInstrumentEvents() {
    if (!emittedAckReset_ && responseContainsPrefix(productSerialRxAccum_, ackReset())) {
        emittedAckReset_ = true;
        emit instrumentAckResetSeen();
        qDebug() << "instrumentAckResetSeen" ;
    }
    if (!emittedAckStart_ && responseContainsPrefix(productSerialRxAccum_, ackStartReceive())) {
        emittedAckStart_ = true;
        emit instrumentAckStartReceiveSeen();
        qDebug() << "instrumentAckStartReceiveSeen" ;
    }
    if (!emittedStopReceive_) {
        const int n = parseStopReceivePacketCountLe(productSerialRxAccum_);
        if (n >= 0) {
            emittedStopReceive_ = true;
            emit instrumentStopReceiveSeen(n);
            qDebug() << "instrumentStopReceiveSeen" << n;
        }
    }
}
