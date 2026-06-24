#include "inovance_h5u_tcp.h"

#include "Abini.h"
#include "qmodbus_pdu.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QtEndian>
#include <QThread>
#include <QVariantMap>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// --- Modbus TCP ---
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
} // namespace

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
    QModbusPdu::appendUint16Be(adu, 0); // Protocol ID Modbus
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
// --- Station config ---
QString plcBoolText(bool on) {
    return on ? QStringLiteral("开启") : QStringLiteral("关闭");
}

PlcStationConfig PlcStationConfig::fromSettings(int stationIndex) {
    PlcStationConfig cfg;
    const int st = qMax(1, stationIndex);
    cfg.stationIndex = st;

    cfg.ipAddress = SETTINGS.value(QStringLiteral("PLC/IpAddress_Station%1").arg(st),
                                   SETTINGS.value(QStringLiteral("PLC/IpAddress"), QStringLiteral("192.168.1.88")))
                        .toString();
    cfg.port = SETTINGS.value(QStringLiteral("PLC/Port_Station%1").arg(st), SETTINGS.value(QStringLiteral("PLC/Port"), 502))
                   .toInt();
    cfg.unitId = quint8(SETTINGS.value(QStringLiteral("PLC/UnitId_Station%1").arg(st),
                                       SETTINGS.value(QStringLiteral("PLC/UnitId"), 1))
                            .toUInt());
    cfg.mCoilAddressOffset =
        SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset_Station%1").arg(st),
                       SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset"), 0))
            .toInt();

    const int perMBase = SETTINGS.value(QStringLiteral("PLC/MBase_Station%1").arg(st), -1).toInt();
    if (perMBase >= 0) {
        cfg.mBase = perMBase;
    } else {
        const int base1 = SETTINGS.value(QStringLiteral("PLC/MBase"), 200).toInt();
        const int step = SETTINGS.value(QStringLiteral("PLC/MBaseStationStep"), 20).toInt();
        cfg.mBase = st <= 1 ? base1 : base1 + step * (st - 1);
    }

    const int perForward = SETTINGS.value(QStringLiteral("PLC/SwitchForwardM_Station%1").arg(st), -1).toInt();
    if (perForward >= 0) {
        cfg.switchForwardM = perForward;
    } else {
        const int globalForward = SETTINGS.value(QStringLiteral("PLC/SwitchForwardM"), -1).toInt();
        cfg.switchForwardM = globalForward >= 0
            ? globalForward
            : cfg.mBase + SETTINGS.value(QStringLiteral("PLC/SwitchForwardOffset"), 12).toInt();
    }

    const int perPress = SETTINGS.value(QStringLiteral("PLC/SwitchPressM_Station%1").arg(st), -1).toInt();
    if (perPress >= 0) {
        cfg.switchPressM = perPress;
    } else {
        const int globalPress = SETTINGS.value(QStringLiteral("PLC/SwitchPressM"), -1).toInt();
        cfg.switchPressM =
            globalPress >= 0 ? globalPress : cfg.mBase + SETTINGS.value(QStringLiteral("PLC/SwitchPressOffset"), 7).toInt();
    }

    const int perVerify = SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM_Station%1").arg(st), -1).toInt();
    cfg.connectVerifyM =
        perVerify >= 0 ? perVerify : SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM"), cfg.mBase).toInt();

    const int perPos = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase_Station%1").arg(st), -1).toInt();
    if (perPos >= 0) {
        cfg.positionReadyBase = perPos;
    } else {
        const int globalPos = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase"), -1).toInt();
        if (globalPos >= 0) {
            cfg.positionReadyBase = globalPos;
        } else {
            const int step = SETTINGS.value(QStringLiteral("PLC/PositionReadyBaseStationStep"), 20).toInt();
            cfg.positionReadyBase = 250 + step * (st - 1);
        }
    }

    const int perStepDone = SETTINGS.value(QStringLiteral("PLC/StepDoneBase_Station%1").arg(st), -1).toInt();
    if (perStepDone >= 0) {
        cfg.stepDoneBase = perStepDone;
    } else {
        const int globalStepDone = SETTINGS.value(QStringLiteral("PLC/StepDoneBase"), -1).toInt();
        if (globalStepDone >= 0) {
            cfg.stepDoneBase = globalStepDone;
        } else {
            const int step = SETTINGS.value(QStringLiteral("PLC/StepDoneBaseStationStep"), 20).toInt();
            cfg.stepDoneBase = 260 + step * (st - 1);
        }
    }

    const int perKeyDone = SETTINGS.value(QStringLiteral("PLC/KeyDoneM_Station%1").arg(st), -1).toInt();
    if (perKeyDone >= 0) {
        cfg.keyDoneM = perKeyDone;
    } else {
        const int globalKeyDone = SETTINGS.value(QStringLiteral("PLC/KeyDoneM"), -1).toInt();
        cfg.keyDoneM = globalKeyDone >= 0
            ? globalKeyDone
            : cfg.mBase + SETTINGS.value(QStringLiteral("PLC/KeyDoneOffsetFromMBase"), 10).toInt();
    }

    const int perReset = SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetM_Station%1").arg(st), -1).toInt();
    cfg.switchTestDoneResetM =
        perReset >= 0 ? perReset : SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetM"), 211).toInt();

    return cfg;
}
// --- Session ---
PlcModbusSession::PlcModbusSession(InovanceH5uModbusTcp* tcp, const PlcStationConfig& cfg, LogFn log, IsContinueFn isContinue)
    : tcp_(tcp), cfg_(cfg), log_(std::move(log)), isContinue_(std::move(isContinue)) {
}

int PlcModbusSession::requestTimeoutMs() const {
    return SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
}

void PlcModbusSession::logLine(const QString& line) const {
    if (log_) {
        log_(line);
    }
}

bool PlcModbusSession::connectPlc(QString* errorMessage) {
    const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    return tcp_ && tcp_->connectPlc(cfg_.ipAddress, quint16(cfg_.port), cfg_.unitId, connMs, errorMessage);
}

void PlcModbusSession::disconnect() {
    if (tcp_) {
        tcp_->disconnect();
    }
}

bool PlcModbusSession::isConnected() const {
    return tcp_ && tcp_->isConnected();
}

bool PlcModbusSession::readCoil(int absoluteM, bool* value, QString* errorMessage) {
    QVector<bool> bits;
    if (!tcp_ || !tcp_->readMCoils(absoluteM, 1, cfg_.mCoilAddressOffset, cfg_.unitId, requestTimeoutMs(), &bits, errorMessage)) {
        return false;
    }
    *value = bits.value(0);
    return true;
}

bool PlcModbusSession::writeCoil(int absoluteM, bool value, QString* errorMessage) {
    return tcp_ && tcp_->writeMCoil(absoluteM, value, cfg_.mCoilAddressOffset, cfg_.unitId, requestTimeoutMs(), errorMessage);
}

bool PlcModbusSession::waitCoilTrue(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage) {
    QElapsedTimer t;
    t.start();
    const int step = qMax(10, pollMs);
    while (t.elapsed() < timeoutMs) {
        if (isContinue_ && !isContinue_()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("测试已停止");
            }
            return false;
        }
        bool v = false;
        if (!readCoil(absoluteM, &v, errorMessage)) {
            return false;
        }
        if (v) {
            return true;
        }
        QThread::msleep(static_cast<unsigned long>(step));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("等待 M%1=1 超时 %2ms").arg(absoluteM).arg(timeoutMs);
    }
    return false;
}

bool PlcModbusSession::waitCoilFalse(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage) {
    QElapsedTimer t;
    t.start();
    const int step = qMax(10, pollMs);
    while (t.elapsed() < timeoutMs) {
        if (isContinue_ && !isContinue_()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("测试已停止");
            }
            return false;
        }
        bool v = false;
        if (!readCoil(absoluteM, &v, errorMessage)) {
            return false;
        }
        if (!v) {
            return true;
        }
        QThread::msleep(static_cast<unsigned long>(step));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("等待 M%1=0 超时 %2ms").arg(absoluteM).arg(timeoutMs);
    }
    return false;
}

bool PlcModbusSession::sendStepDone(QString* errorMessage) {
    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const int pulseMs = SETTINGS.value(QStringLiteral("PLC/StepDonePulseMs"), 0).toInt();
    const int m = cfg_.stepDoneBase;
    const int offset = cfg_.mCoilAddressOffset;
    logLine(QStringLiteral("PLC发送 StepDone: M%1(addr=%2)=1 GapMs=%3 PulseMs=%4")
                .arg(m)
                .arg(m + offset)
                .arg(gapMs)
                .arg(pulseMs));
    if (!writeCoil(m, true, errorMessage)) {
        return false;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    if (pulseMs > 0) {
        QThread::msleep(static_cast<unsigned long>(pulseMs));
        logLine(QStringLiteral("PLC复位 StepDone 脉冲: M%1(addr=%2)=0").arg(m).arg(m + offset));
        if (!writeCoil(m, false, errorMessage)) {
            return false;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }
    return true;
}

void PlcModbusSession::logSessionSummary(const QString& stepTag) const {
    logLine(QStringLiteral("[PLC调试 %1] IP=%2 Port=%3 UnitId=%4 M偏移=%5 MBase=%6 PosReady基=%7 StepDone基=%8 "
                           "KeyDoneM=%9 ConnectVerifyM=%10 ConnectTimeoutMs=%11 RequestTimeoutMs=%12 "
                           "Trace=%13 验证读=%14 握手=%15 等KeyDone=%16 完成后释放=%17 KeyDone预复位=%18 KeyDone复位等待=%19 "
                           "CommandGapMs=%20 PosTimeoutMs=%21 PosPollMs=%22 KeyDoneTimeoutMs=%23 KeyDonePollMs=%24")
                .arg(stepTag)
                .arg(cfg_.ipAddress)
                .arg(cfg_.port)
                .arg(cfg_.unitId)
                .arg(cfg_.mCoilAddressOffset)
                .arg(cfg_.mBase)
                .arg(cfg_.positionReadyBase)
                .arg(cfg_.stepDoneBase)
                .arg(cfg_.keyDoneM)
                .arg(cfg_.connectVerifyM)
                .arg(SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt())
                .arg(plcBoolText(true))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool()))
                .arg(SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                    SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                         .toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                    SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt())
                         .toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt()));
}
