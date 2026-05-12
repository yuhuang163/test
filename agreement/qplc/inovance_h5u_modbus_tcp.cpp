#include "inovance_h5u_modbus_tcp.h"

#include <QtEndian>
#include <QDebug>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {
constexpr quint8 kFcReadCoils = 0x01;
constexpr quint8 kFcWriteSingleCoil = 0x05;

void appendUint16Be(QByteArray& b, quint16 v) {
    b.append(char(quint8(v >> 8)));
    b.append(char(quint8(v & 0xFF)));
}

quint16 readUint16Be(const char* p) {
    return (quint8(p[0]) << 8) | quint8(p[1]);
}

QString pduHexPreview(const QByteArray& pdu, int maxBytes = 32) {
    QString s;
    const int n = qMin(pdu.size(), maxBytes);
    for (int i = 0; i < n; ++i) {
        s += QStringLiteral("%1 ").arg(quint8(pdu[i]), 2, 16, QChar('0'));
    }
    if (pdu.size() > maxBytes) {
        s += QStringLiteral("...");
    }
    return s.trimmed();
}
}  // namespace

void InovanceH5uModbusTcp::disconnect() {
    socket_.disconnectFromHost();
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.waitForDisconnected(500);
    }
}

bool InovanceH5uModbusTcp::connectPlc(const QString& host, quint16 port, quint8 unitId, int timeoutMs,
                                       QString* errorMessage) {
    Q_UNUSED(unitId);
    disconnect();
    socket_.connectToHost(host, port);
    const bool ok = socket_.waitForConnected(timeoutMs);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] connect" << host << port << "timeoutMs=" << timeoutMs << "ok=" << ok
                 << "err=" << socket_.errorString();
    }
    if (!ok) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TCP连接失败: %1").arg(socket_.errorString());
        }
        return false;
    }
    return true;
}

quint16 InovanceH5uModbusTcp::nextTransactionId() {
    ++transactionId_;
    if (transactionId_ == 0) {
        transactionId_ = 1;
    }
    return transactionId_;
}

QByteArray InovanceH5uModbusTcp::buildAdu(quint16 transactionId, quint8 unitId, const QByteArray& pdu) {
    QByteArray adu;
    appendUint16Be(adu, transactionId);
    appendUint16Be(adu, 0);  // Protocol ID Modbus
    const quint16 following = quint16(1 + pdu.size());
    appendUint16Be(adu, following);
    adu.append(char(unitId));
    adu.append(pdu);
    return adu;
}

bool InovanceH5uModbusTcp::parseResponse(const QByteArray& fullAdu, quint16 expectTransactionId,
                                         quint8 expectUnitId, QByteArray* responsePdu, QString* errorMessage) {
    if (fullAdu.size() < 6) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 响应过短");
        }
        return false;
    }
    const quint16 tid = readUint16Be(fullAdu.constData());
    const quint16 len = readUint16Be(fullAdu.constData() + 4);
    if (fullAdu.size() < 6 + len) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 响应长度不足");
        }
        return false;
    }
    if (tid != expectTransactionId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 事务号不匹配");
        }
        return false;
    }
    const quint8 uid = quint8(fullAdu[6]);
    if (uid != expectUnitId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 从站号不匹配");
        }
        return false;
    }
    *responsePdu = fullAdu.mid(7, len - 1);
    return true;
}

bool InovanceH5uModbusTcp::transact(const QByteArray& pdu, QByteArray* responsePdu, QString* errorMessage,
                                     int requestTimeoutMs, quint8 unitId) {
    if (!isConnected()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PLC 未连接");
        }
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] transact skip: not connected";
        }
        return false;
    }

    const quint16 tid = nextTransactionId();
    const QByteArray adu = buildAdu(tid, unitId, pdu);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] TX tid=" << tid << "unitId=" << unitId << "aduBytes=" << adu.size()
                 << "pduHex=" << pduHexPreview(pdu);
    }
    socket_.write(adu);
    if (!socket_.waitForBytesWritten(requestTimeoutMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 发送超时: %1").arg(socket_.errorString());
        }
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] waitForBytesWritten fail" << socket_.errorString();
        }
        return false;
    }

    QByteArray buffer;
    while (buffer.size() < 6) {
        if (!socket_.waitForReadyRead(requestTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Modbus 读响应头超时");
            }
            if (traceEnabled_) {
                qDebug() << "[H5U-Modbus] read header timeout bufferBytes=" << buffer.size();
            }
            return false;
        }
        buffer.append(socket_.readAll());
    }
    const quint16 remaining = readUint16Be(buffer.constData() + 4);
    const int total = 6 + remaining;
    while (buffer.size() < total) {
        if (!socket_.waitForReadyRead(requestTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Modbus 读响应体超时");
            }
            if (traceEnabled_) {
                qDebug() << "[H5U-Modbus] read body timeout have=" << buffer.size() << "need=" << total;
            }
            return false;
        }
        buffer.append(socket_.readAll());
    }

    const QByteArray frame = buffer.left(total);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] RX rawBytes=" << frame.size() << "hex=" << pduHexPreview(frame, 64);
    }
    const bool parsed = parseResponse(frame, tid, unitId, responsePdu, errorMessage);
    if (!parsed && traceEnabled_) {
        qDebug() << "[H5U-Modbus] parseResponse fail:" << (errorMessage ? *errorMessage : QString());
    }
    if (parsed && traceEnabled_) {
        qDebug() << "[H5U-Modbus] RX pduHex=" << pduHexPreview(*responsePdu);
    }
    return parsed;
}

bool InovanceH5uModbusTcp::writeMCoil(int mNumber, bool value, int mCoilAddressOffset, quint8 unitId,
                                     int requestTimeoutMs, QString* errorMessage) {
    const qint64 addr64 = qint64(mNumber) + qint64(mCoilAddressOffset);
    if (addr64 < 0 || addr64 > 0xFFFF) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("M线圈地址越界");
        }
        return false;
    }
    const quint16 addr = quint16(addr64);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] writeMCoil M" << mNumber << "+offset" << mCoilAddressOffset << "=>addr" << addr
                 << "value" << (value ? 1 : 0) << "unitId" << unitId;
    }
    QByteArray pdu;
    pdu.append(char(kFcWriteSingleCoil));
    appendUint16Be(pdu, addr);
    appendUint16Be(pdu, value ? quint16(0xFF00) : quint16(0x0000));

    QByteArray resp;
    if (!transact(pdu, &resp, errorMessage, requestTimeoutMs, unitId)) {
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] writeMCoil transact failed:" << (errorMessage ? *errorMessage : QString());
        }
        return false;
    }
    if (resp.size() < 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写线圈响应为空");
        }
        return false;
    }
    if ((quint8(resp[0]) & 0x80) != 0) {
        const quint8 ex = resp.size() > 1 ? quint8(resp[1]) : 0;
        if (errorMessage) {
            *errorMessage = QStringLiteral("写线圈异常: 功能码=0x%1 异常码=%2")
                                .arg(quint8(resp[0]), 2, 16, QChar('0'))
                                .arg(ex);
        }
        return false;
    }
    if (resp != pdu) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写线圈响应与请求不一致");
        }
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] writeMCoil echo mismatch req=" << pduHexPreview(pdu) << "resp="
                     << pduHexPreview(resp);
        }
        return false;
    }
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] writeMCoil OK M" << mNumber << "=" << (value ? 1 : 0);
    }
    return true;
}

bool InovanceH5uModbusTcp::readMCoils(int mStartNumber, int quantity, int mCoilAddressOffset, quint8 unitId,
                                     int requestTimeoutMs, QVector<bool>* out, QString* errorMessage) {
    out->clear();
    if (quantity <= 0 || quantity > 2000) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈数量非法");
        }
        return false;
    }
    const qint64 addr64 = qint64(mStartNumber) + qint64(mCoilAddressOffset);
    if (addr64 < 0 || addr64 > 0xFFFF) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("M线圈起始地址越界");
        }
        return false;
    }
    const quint16 addr = quint16(addr64);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] readMCoils M" << mStartNumber << "+offset" << mCoilAddressOffset << "=>addr" << addr
                 << "qty" << quantity << "unitId" << unitId;
    }
    QByteArray pdu;
    pdu.append(char(kFcReadCoils));
    appendUint16Be(pdu, addr);
    appendUint16Be(pdu, quint16(quantity));

    QByteArray resp;
    if (!transact(pdu, &resp, errorMessage, requestTimeoutMs, unitId)) {
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] readMCoils transact failed:" << (errorMessage ? *errorMessage : QString());
        }
        return false;
    }
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈响应过短");
        }
        return false;

    if ((quint8(resp[0]) & 0x80) != 0) {
        const quint8 ex = resp.size() > 1 ? quint8(resp[1]) : 0;
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈异常: 功能码=0x%1 异常码=%2")
                                .arg(quint8(resp[0]), 2, 16, QChar('0'))
                                .arg(ex);
        }
        return false;
    }
    if (quint8(resp[0]) != kFcReadCoils) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈响应功能码错误");
        }
        return false;
    }
    const int byteCount = quint8(resp[1]);
    if (resp.size() < 2 + byteCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈数据长度不足");
        }
        return false;
    }
    for (int i = 0; i < quantity; ++i) {
        const int byteIndex = i / 8;
        const int bitIndex = i % 8;
        const bool bit = (quint8(resp[2 + byteIndex]) & (1 << bitIndex)) != 0;
        out->append(bit);
    }
    if (traceEnabled_) {
        QString bits;
        for (int i = 0; i < qMin(quantity, 16); ++i) {
            bits += out->value(i) ? QLatin1Char('1') : QLatin1Char('0');
        }
        if (quantity > 16) {
            bits += QLatin1String("...");
        }
        qDebug() << "[H5U-Modbus] readMCoils OK M" << mStartNumber << "bits" << bits;
    }
    return true;
}
