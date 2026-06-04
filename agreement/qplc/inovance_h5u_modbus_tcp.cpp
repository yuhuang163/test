#include "inovance_h5u_modbus_tcp.h"
#include "qmodbus_pdu.h"

#include <QtEndian>
#include <QDebug>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {
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

QString socketStateName(QAbstractSocket::SocketState state) {
    switch (state) {
    case QAbstractSocket::UnconnectedState:
        return QStringLiteral("Unconnected");
    case QAbstractSocket::HostLookupState:
        return QStringLiteral("HostLookup");
    case QAbstractSocket::ConnectingState:
        return QStringLiteral("Connecting");
    case QAbstractSocket::ConnectedState:
        return QStringLiteral("Connected");
    case QAbstractSocket::BoundState:
        return QStringLiteral("Bound");
    case QAbstractSocket::ClosingState:
        return QStringLiteral("Closing");
    case QAbstractSocket::ListeningState:
        return QStringLiteral("Listening");
    }
    return QStringLiteral("UnknownState(%1)").arg(int(state));
}

QString socketErrorName(QAbstractSocket::SocketError error) {
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        return QStringLiteral("ConnectionRefused");
    case QAbstractSocket::RemoteHostClosedError:
        return QStringLiteral("RemoteHostClosed");
    case QAbstractSocket::HostNotFoundError:
        return QStringLiteral("HostNotFound");
    case QAbstractSocket::SocketAccessError:
        return QStringLiteral("SocketAccess");
    case QAbstractSocket::SocketResourceError:
        return QStringLiteral("SocketResource");
    case QAbstractSocket::SocketTimeoutError:
        return QStringLiteral("SocketTimeout");
    case QAbstractSocket::DatagramTooLargeError:
        return QStringLiteral("DatagramTooLarge");
    case QAbstractSocket::NetworkError:
        return QStringLiteral("Network");
    case QAbstractSocket::AddressInUseError:
        return QStringLiteral("AddressInUse");
    case QAbstractSocket::SocketAddressNotAvailableError:
        return QStringLiteral("SocketAddressNotAvailable");
    case QAbstractSocket::UnsupportedSocketOperationError:
        return QStringLiteral("UnsupportedSocketOperation");
    case QAbstractSocket::ProxyAuthenticationRequiredError:
        return QStringLiteral("ProxyAuthenticationRequired");
    case QAbstractSocket::SslHandshakeFailedError:
        return QStringLiteral("SslHandshakeFailed");
    case QAbstractSocket::UnfinishedSocketOperationError:
        return QStringLiteral("UnfinishedSocketOperation");
    case QAbstractSocket::ProxyConnectionRefusedError:
        return QStringLiteral("ProxyConnectionRefused");
    case QAbstractSocket::ProxyConnectionClosedError:
        return QStringLiteral("ProxyConnectionClosed");
    case QAbstractSocket::ProxyConnectionTimeoutError:
        return QStringLiteral("ProxyConnectionTimeout");
    case QAbstractSocket::ProxyNotFoundError:
        return QStringLiteral("ProxyNotFound");
    case QAbstractSocket::ProxyProtocolError:
        return QStringLiteral("ProxyProtocol");
    case QAbstractSocket::OperationError:
        return QStringLiteral("Operation");
    case QAbstractSocket::SslInternalError:
        return QStringLiteral("SslInternal");
    case QAbstractSocket::SslInvalidUserDataError:
        return QStringLiteral("SslInvalidUserData");
    case QAbstractSocket::TemporaryError:
        return QStringLiteral("Temporary");
    case QAbstractSocket::UnknownSocketError:
        return QStringLiteral("UnknownSocket");
    }
    return QStringLiteral("SocketError(%1)").arg(int(error));
}

QString socketSummary(const QTcpSocket& socket) {
    return QStringLiteral("State=%1 Error=%2(%3) ErrorString=%4 BytesAvailable=%5 BytesToWrite=%6")
        .arg(socketStateName(socket.state()))
        .arg(socketErrorName(socket.error()))
        .arg(int(socket.error()))
        .arg(socket.errorString())
        .arg(socket.bytesAvailable())
        .arg(socket.bytesToWrite());
}

QString pduContext(quint8 unitId, int requestTimeoutMs, const QByteArray& pdu) {
    return QStringLiteral("UnitId=%1 RequestTimeoutMs=%2 PduHex=%3")
        .arg(unitId)
        .arg(requestTimeoutMs)
        .arg(pduHexPreview(pdu));
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
    disconnect();
    socket_.connectToHost(host, port);
    const bool ok = socket_.waitForConnected(timeoutMs);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] connect" << host << port << "timeoutMs=" << timeoutMs << "ok=" << ok
                 << "unitId=" << unitId << socketSummary(socket_);
    }
    if (!ok) {
        if (errorMessage) {
            // 连接失败时带上目标、超时和 socket 状态，方便现场直接判断 IP/端口/网络/PLC 服务问题。
            *errorMessage = QStringLiteral("TCP连接失败: IP=%1 Port=%2 UnitId=%3 TimeoutMs=%4 "
                                           "%5")
                                .arg(host)
                                .arg(port)
                                .arg(unitId)
                                .arg(timeoutMs)
                                .arg(socketSummary(socket_));
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
    QModbusPdu::appendUint16Be(adu, transactionId);
    QModbusPdu::appendUint16Be(adu, 0);  // Protocol ID Modbus
    const quint16 following = quint16(1 + pdu.size());
    QModbusPdu::appendUint16Be(adu, following);
    adu.append(char(unitId));
    adu.append(pdu);
    return adu;
}

bool InovanceH5uModbusTcp::parseResponse(const QByteArray& fullAdu, quint16 expectTransactionId,
                                         quint8 expectUnitId, QByteArray* responsePdu, QString* errorMessage) {
    if (fullAdu.size() < 6) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 响应过短: RawBytes=%1 Hex=%2")
                                .arg(fullAdu.size())
                                .arg(pduHexPreview(fullAdu, 64));
        }
        return false;
    }
    const quint16 tid = QModbusPdu::readUint16Be(fullAdu.constData());
    const quint16 len = QModbusPdu::readUint16Be(fullAdu.constData() + 4);
    if (fullAdu.size() < 6 + len) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 响应长度不足: RawBytes=%1 NeedBytes=%2 LenField=%3 Hex=%4")
                                .arg(fullAdu.size())
                                .arg(6 + len)
                                .arg(len)
                                .arg(pduHexPreview(fullAdu, 64));
        }
        return false;
    }
    if (tid != expectTransactionId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 事务号不匹配: ExpectTid=%1 ActualTid=%2 Hex=%3")
                                .arg(expectTransactionId)
                                .arg(tid)
                                .arg(pduHexPreview(fullAdu, 64));
        }
        return false;
    }
    const quint8 uid = quint8(fullAdu[6]);
    if (uid != expectUnitId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 从站号不匹配: ExpectUnitId=%1 ActualUnitId=%2 Hex=%3")
                                .arg(expectUnitId)
                                .arg(uid)
                                .arg(pduHexPreview(fullAdu, 64));
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
            *errorMessage = QStringLiteral("PLC 未连接: %1 %2")
                                .arg(pduContext(unitId, requestTimeoutMs, pdu))
                                .arg(socketSummary(socket_));
        }
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] transact skip: not connected" << pduContext(unitId, requestTimeoutMs, pdu)
                     << socketSummary(socket_);
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
    qDebug().noquote() << "PLC TX:" << QString::fromLatin1(adu.toHex(' ').toUpper());
    if (!socket_.waitForBytesWritten(requestTimeoutMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus 发送超时: %1 %2")
                                .arg(pduContext(unitId, requestTimeoutMs, pdu))
                                .arg(socketSummary(socket_));
        }
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] waitForBytesWritten fail" << pduContext(unitId, requestTimeoutMs, pdu)
                     << socketSummary(socket_);
        }
        return false;
    }

    QByteArray buffer;
    while (buffer.size() < 6) {
        if (!socket_.waitForReadyRead(requestTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Modbus 读响应头超时: %1 ReceivedBytes=%2 ReceivedHex=%3 %4")
                                    .arg(pduContext(unitId, requestTimeoutMs, pdu))
                                    .arg(buffer.size())
                                    .arg(pduHexPreview(buffer, 64))
                                    .arg(socketSummary(socket_));
            }
            if (traceEnabled_) {
                qDebug() << "[H5U-Modbus] read header timeout bufferBytes=" << buffer.size()
                         << pduContext(unitId, requestTimeoutMs, pdu) << socketSummary(socket_);
            }
            return false;
        }
        buffer.append(socket_.readAll());
    }
    const quint16 remaining = QModbusPdu::readUint16Be(buffer.constData() + 4);
    const int total = 6 + remaining;
    while (buffer.size() < total) {
        if (!socket_.waitForReadyRead(requestTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Modbus 读响应体超时: %1 ReceivedBytes=%2 NeedBytes=%3 ReceivedHex=%4 %5")
                                    .arg(pduContext(unitId, requestTimeoutMs, pdu))
                                    .arg(buffer.size())
                                    .arg(total)
                                    .arg(pduHexPreview(buffer, 64))
                                    .arg(socketSummary(socket_));
            }
            if (traceEnabled_) {
                qDebug() << "[H5U-Modbus] read body timeout have=" << buffer.size() << "need=" << total
                         << pduContext(unitId, requestTimeoutMs, pdu) << socketSummary(socket_);
            }
            return false;
        }
        buffer.append(socket_.readAll());
    }

    const QByteArray frame = buffer.left(total);
    qDebug().noquote() << "PLC RX:" << QString::fromLatin1(frame.toHex(' ').toUpper());
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] RX rawBytes=" << frame.size() << "hex=" << pduHexPreview(frame, 64);
    }
    const bool parsed = parseResponse(frame, tid, unitId, responsePdu, errorMessage);
    if (!parsed && errorMessage) {
        *errorMessage = QStringLiteral("%1；%2").arg(*errorMessage).arg(pduContext(unitId, requestTimeoutMs, pdu));
    }
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
            *errorMessage = QStringLiteral("M线圈地址越界: M=%1 Offset=%2 Addr=%3 UnitId=%4 Value=%5")
                                .arg(mNumber)
                                .arg(mCoilAddressOffset)
                                .arg(addr64)
                                .arg(unitId)
                                .arg(value ? 1 : 0);
        }
        return false;
    }
    const quint16 addr = quint16(addr64);
    const QString ctx = QStringLiteral("M=%1 Offset=%2 Addr=%3 UnitId=%4 Value=%5 RequestTimeoutMs=%6")
                            .arg(mNumber)
                            .arg(mCoilAddressOffset)
                            .arg(addr)
                            .arg(unitId)
                            .arg(value ? 1 : 0)
                            .arg(requestTimeoutMs);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] writeMCoil M" << mNumber << "+offset" << mCoilAddressOffset << "=>addr" << addr
                 << "value" << (value ? 1 : 0) << "unitId" << unitId;
    }
    const QByteArray pdu = QModbusPdu::buildWriteSingleCoilRequestPdu(addr, value);

    QByteArray resp;
    if (!transact(pdu, &resp, errorMessage, requestTimeoutMs, unitId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写线圈失败: %1；%2").arg(ctx).arg(*errorMessage);
        }
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] writeMCoil transact failed:" << (errorMessage ? *errorMessage : QString());
        }
        return false;
    }
    if (resp.size() < 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写线圈响应为空: %1 RespHex=%2").arg(ctx).arg(pduHexPreview(resp));
        }
        return false;
    }
    if (QModbusPdu::isExceptionResponse(resp)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写线圈异常: %1 %2 RespHex=%3")
                                .arg(ctx)
                                .arg(QModbusPdu::formatExceptionMessage(resp))
                                .arg(pduHexPreview(resp));
        }
        return false;
    }
    if (resp != pdu) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写线圈响应与请求不一致: %1 ReqHex=%2 RespHex=%3")
                                .arg(ctx)
                                .arg(pduHexPreview(pdu))
                                .arg(pduHexPreview(resp));
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
            *errorMessage = QStringLiteral("读线圈数量非法: MStart=%1 Quantity=%2 UnitId=%3")
                                .arg(mStartNumber)
                                .arg(quantity)
                                .arg(unitId);
        }
        return false;
    }
    const qint64 addr64 = qint64(mStartNumber) + qint64(mCoilAddressOffset);
    if (addr64 < 0 || addr64 > 0xFFFF) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("M线圈起始地址越界: MStart=%1 Offset=%2 Addr=%3 Quantity=%4 UnitId=%5")
                                .arg(mStartNumber)
                                .arg(mCoilAddressOffset)
                                .arg(addr64)
                                .arg(quantity)
                                .arg(unitId);
        }
        return false;
    }
    const quint16 addr = quint16(addr64);
    const QString ctx = QStringLiteral("MStart=%1 Quantity=%2 Offset=%3 Addr=%4 UnitId=%5 RequestTimeoutMs=%6")
                            .arg(mStartNumber)
                            .arg(quantity)
                            .arg(mCoilAddressOffset)
                            .arg(addr)
                            .arg(unitId)
                            .arg(requestTimeoutMs);
    if (traceEnabled_) {
        qDebug() << "[H5U-Modbus] readMCoils M" << mStartNumber << "+offset" << mCoilAddressOffset << "=>addr" << addr
                 << "qty" << quantity << "unitId" << unitId;
    }
    const QByteArray pdu = QModbusPdu::buildReadCoilsRequestPdu(addr, quint16(quantity));

    QByteArray resp;
    if (!transact(pdu, &resp, errorMessage, requestTimeoutMs, unitId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈失败: %1；%2").arg(ctx).arg(*errorMessage);
        }
        if (traceEnabled_) {
            qDebug() << "[H5U-Modbus] readMCoils transact failed:" << (errorMessage ? *errorMessage : QString());
        }
        return false;
    }
    if (resp.size() < 2) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈响应过短: %1 RespBytes=%2 RespHex=%3")
                                .arg(ctx)
                                .arg(resp.size())
                                .arg(pduHexPreview(resp));
        }
        return false;
    }

    if (QModbusPdu::isExceptionResponse(resp)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈异常: %1 %2 RespHex=%3")
                                .arg(ctx)
                                .arg(QModbusPdu::formatExceptionMessage(resp))
                                .arg(pduHexPreview(resp));
        }
        return false;
    }
    QString parseError;
    if (!QModbusPdu::parseReadCoilsPdu(resp, quantity, out, &parseError)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈解析失败: %1 %2 RespHex=%3")
                                .arg(ctx)
                                .arg(parseError)
                                .arg(pduHexPreview(resp));
        }
        return false;
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
