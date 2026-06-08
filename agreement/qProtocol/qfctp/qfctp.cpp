#include "qfctp.h"
#include "Abini.h"
#include <QDebug>
#include <QVariantMap>
#include <QString>
#include <QStringList>

#include "comm_protocol_builder.h"
#include "comm_protocol_parser.h"

#include <cstring>
#include <vector>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

static uint16_t qfctpReadLe16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

/// 按键电容 4 字节 → uint32；与现场约定一致：大端=0123，小端=2301（高低16位字交换）
static quint32 qfctpParseKeyCapU32(const uint8_t *p)
{
    const QString endian = SETTINGS.value(QStringLiteral("KeyCap/ValueEndian"), QStringLiteral("big")).toString();
    if (endian.compare(QStringLiteral("little"), Qt::CaseInsensitive) == 0) {
        // 小端 2301：原始 12 34 56 78 -> 56 78 12 34
        return (static_cast<quint32>(p[2]) << 24) | (static_cast<quint32>(p[3]) << 16) | (static_cast<quint32>(p[0]) << 8)
               | static_cast<quint32>(p[1]);
    }
    // 大端 0123：p[0] 为最高字节
    return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) | (static_cast<quint32>(p[2]) << 8)
           | static_cast<quint32>(p[3]);
}



static QByteArray qfctpParseValueString(const QString &text, bool *ok)
{
    QString s = text.trimmed();
    if (s.isEmpty()) {
        if (ok != nullptr) {
            *ok = true;
        }
        return {};
    }

    if (s.startsWith("0x", Qt::CaseInsensitive)) {
        s = s.mid(2);
    }

    QString compact = s;
    compact.remove(' ');
    compact.remove(':');

    // 兼容用户输入单个半字节，如 "0" => "00"
    if (compact.size() % 2 != 0) {
        compact.prepend('0');
    }

    if (compact.isEmpty()) {
        if (ok != nullptr) {
            *ok = true;
        }
        return {};
    }

    for (const QChar ch : compact) {
        if (!ch.isDigit() && (ch.toUpper() < QChar('A') || ch.toUpper() > QChar('F'))) {
            if (ok != nullptr) {
                *ok = false;
            }
            return {};
        }
    }

    if (ok != nullptr) {
        *ok = true;
    }
    return QByteArray::fromHex(compact.toLatin1());
}

static constexpr uint8_t kPhyTxHeaderByte = 0xCC;
static constexpr uint8_t kPhyRxHeaderByte = 0xAA;
static constexpr int     kPhyHeaderSize = 8;
// 与 qpb 中通道定义保持一致：INVALID=0, FAC=1, APP=2, MAIN=3
static constexpr uint8_t kPhyChannelFac = 1;
static constexpr uint8_t kPhyChannelApp = 2;
static constexpr uint8_t kPhyChannelMain = 3;
static constexpr uint16_t kTestsService = COMM_PROTOCOL_TESTS_SERVICE;
static constexpr uint16_t kSystemConfigService = COMM_PROTOCOL_SYSTEM_CONFIG_SERVICE;
static constexpr uint16_t kAlgoService = COMM_PROTOCOL_ALGO_SERVICE;
static constexpr uint16_t kAlgoTlvSampleDataNum = 0x0004;
static constexpr uint16_t kAlgoTlvSampleLightSensorData = 0x0006;
static uint32_t qfctpMakeResponseKey(uint16_t serviceId, uint16_t tlvType)
{
    return (static_cast<uint32_t>(serviceId) << 16) | static_cast<uint32_t>(tlvType);
}

// TESTS SERVICE TLV
static constexpr uint16_t kTlvFactoryMode = 0x0001;
static constexpr uint16_t kTlvAgingModeSet = 0x0002;
static constexpr uint16_t kTlvSuctionMode = 0x0003;
static constexpr uint16_t kTlvAgingStatus = 0x0004;
static constexpr uint16_t kTlvKeyStatusReport = 0x0005;
static constexpr uint16_t kTlvEncoderStatusReport = 0x0006;
static constexpr uint16_t kTlvSnWrite = 0x0007;
static constexpr uint16_t kTlvProductIdWrite = 0x0008;
static constexpr uint16_t kTlvDeviceIdWrite = 0x0009;
static constexpr uint16_t kTlvKeyWrite = 0x000A;
static constexpr uint16_t kTlvTupleRead = 0x000B;
static constexpr uint16_t kTlvDeviceException = 0x000C;
static constexpr uint16_t kTlvCompensationSet = 0x000D;
static constexpr uint16_t kTlvSnRead = 0x000E;
static constexpr uint16_t kTlvFactoryDoneWrite = 0x000F;
static constexpr uint16_t kTlvAgingModeExit = 0x0010;
static constexpr uint16_t kTlvBtSignalMode = 0x0011;
static constexpr uint16_t kTlvBtNoSignalMode = 0x0012;
static constexpr uint16_t kTlvBtFreqMode = 0x0013;
static constexpr uint16_t kTlvTrimSet = 0x0014;
static constexpr uint16_t kTlvTrimGet = 0x0015;
static constexpr uint16_t kTlvStandbyMode = 0x0016;
static constexpr uint16_t kTlvSensorState = 0x0017;
static constexpr uint16_t kTlvFactoryDoneRead = 0x0018;
static constexpr uint16_t kTlvRssiRead = 0x0019;
static constexpr uint16_t kTlvBatteryRead = 0x001A;
static constexpr uint16_t kTlvKeySignalRead = 0x001B;
static constexpr uint16_t kTlvLcdBacklight = 0x001C;
static constexpr uint16_t kTlvLightReportCtrl = 0x001D;
static constexpr uint16_t kTlvLightCalibWrite = 0x001E;
static constexpr uint16_t kTlvLightCalibRead = 0x001F;
static constexpr uint16_t kTlvChargeCurrentRead = 0x0020;

// SYSTEM CONFIG SERVICE TLV
static constexpr uint16_t kTlvFwVersionRead = 0x0001;
static constexpr uint16_t kTlvMacRead = 0x0002;
static constexpr uint16_t kTlvMacWrite = 0x0003;
static constexpr uint16_t kTlvPowerOff = 0x0004;
static constexpr uint16_t kTlvLedTest = 0x0005;
static constexpr uint16_t kTlvNightLightSet = 0x0006;
static constexpr uint16_t kTlvFactoryReset = 0x0007;
//C 回调桥接层，作用是把 C 风格回调转回到 Qfctp 对象方法。
void qfctp_on_full_frame(const uint8_t *frame_data, uint16_t frame_len, void *user_data)
{
    auto *self = static_cast<Qfctp *>(user_data);
    self->handleFullFrame(frame_data, frame_len);
}

Qfctp::Qfctp(QSerialPort* parent) : qProtocol(parent)
{
    serialPort = parent;
    registerResponseHandlers();
}

bool Qfctp::sendCustomMessage(const QVariantMap &map)
{
    const QVariant serviceVar = map.value("serviceId");
    const QVariant tlvVar = map.value("tlvType");
    if (!serviceVar.isValid() || !tlvVar.isValid()) {
        qWarning() << "Qfctp 通用发送缺少必填字段 serviceId/tlvType";
        return false;
    }

    bool okService = false;
    bool okTlv = false;
    const int serviceInt = serviceVar.toInt(&okService);
    const int tlvInt = tlvVar.toInt(&okTlv);
    if (!okService || !okTlv || serviceInt < 0 || serviceInt > 0xFFFF || tlvInt < 0 || tlvInt > 0xFFFF) {
        qWarning() << "Qfctp 通用发送参数范围非法"
                   << "serviceId=" << serviceVar
                   << "tlvType=" << tlvVar;
        return false;
    }

    QByteArray value;
    const QVariant valueVar = map.value("value");
    if (valueVar.isValid()) {
        if (valueVar.canConvert<QString>()) {
            bool okHex = false;
            const QString hex = valueVar.toString();
            value = qfctpParseValueString(hex, &okHex);
            if (!okHex) {
                qWarning() << "Qfctp 通用发送 value 不是有效 hex 字符串:" << hex;
                return false;
            }
        } else if (valueVar.canConvert<QByteArray>()) {
            value = valueVar.toByteArray();
        } else {
            qWarning() << "Qfctp 通用发送 value 类型不支持，需 QByteArray 或 hex QString";
            return false;
        }
    }

    const QString action = map.value("actionName").toString().trimmed();
    const QByteArray actionUtf8 = action.isEmpty() ? QByteArray("上层自定义消息") : action.toUtf8();
    return sendRequest(static_cast<uint16_t>(serviceInt),
                       static_cast<uint16_t>(tlvInt),
                       value,
                       actionUtf8.constData());
}

void Qfctp::handleFullFrame(const uint8_t *frameData, uint16_t frameLen)
{
    const QByteArray frameRaw(reinterpret_cast<const char *>(frameData), static_cast<int>(frameLen));
    comm_protocol_frame_node_t frame{};
    std::memset(&frame, 0, sizeof(frame));
    const int rc = comm_protocol_frame_deserialize(frameData, frameLen, &frame);
    if (rc != COMM_PROTOCOL_ERROR_NONE) {
        qWarning() << "FCTP 帧反序列化失败:" << rc;
        return;
    }

    for (uint16_t i = 0; i < frame.payload.service_count && i < COMM_PROTOCOL_MAX_SERVICE_NUM; ++i) {
        const comm_protocol_service_node_t &svc = frame.payload.services[i];
        if (frame.frame_type == COMM_PROTOCOL_FRAME_TYPE_RESPONSE) {
            handleResponseService(frame.seq, svc.svr_id, svc.tlvs, svc.srv_length, frameRaw);
        } else if (frame.frame_type == COMM_PROTOCOL_FRAME_TYPE_NOTIFY) {
            handleNotifyService(svc.svr_id, svc.tlvs, svc.srv_length);
        }
    }
}

void Qfctp::handleResponseService(uint8_t seq, uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen, const QByteArray &frameRaw)
{
    const QByteArray serviceRaw = (tlvs != nullptr && serviceLen > 0)
                                      ? QByteArray(reinterpret_cast<const char *>(tlvs), static_cast<int>(serviceLen))
                                      : QByteArray();
    uint16_t off = 0;
    uint16_t errCode = 0xFFFF;
    bool hasErrCode = false;
    uint16_t mainType = 0;
    const uint8_t *mainValue = nullptr;
    uint16_t mainLen = 0;

    while (tlvs != nullptr && off + COMM_PROTOCOL_TLV_HEADER_SIZE <= serviceLen) {
        const uint8_t *p = tlvs + off;
        const uint16_t type = qfctpReadLe16(p);
        const uint16_t len = qfctpReadLe16(p + 2);
        if (off + COMM_PROTOCOL_TLV_HEADER_SIZE + len > serviceLen) {
            qWarning() << "FCTP 响应TLV长度非法, svc=" << serviceId << "type=" << type;
            break;
        }
        const uint8_t *value = p + COMM_PROTOCOL_TLV_HEADER_SIZE;
        if (type == PROTOCOL_SERVICE_ERROR_CODE_TLV_ID && len >= 2) {
            errCode = qfctpReadLe16(value);
            hasErrCode = true;
        } else {
            mainType = type;
            mainValue = value;
            mainLen = len;
        }
        off = static_cast<uint16_t>(off + COMM_PROTOCOL_TLV_HEADER_SIZE + len);
    }

    if (!m_pendingRequests.contains(seq)) {
        qWarning() << "FCTP 响应无对应 pending, 忽略 seq=" << static_cast<int>(seq) << "service=" << serviceId;
        return;
    }
    PendingRequest req = m_pendingRequests.take(seq);
    if (req.serviceId == 0 || req.tlvType == 0) {
        req.serviceId = serviceId;
        req.tlvType = mainType;
    }
    if (req.actionName.isEmpty()) {
        req.actionName = "未知请求";
    }

    const QByteArray actionUtf8 = req.actionName.toUtf8();
    qDebug() << "FCTP RX DETAIL:" << actionUtf8.constData()
             << "seq=" << static_cast<int>(seq)
             << "service=" << serviceId
             << "tlv_type=" << mainType
             << "inner=" << frameRaw.toHex(' ').toUpper();

    int mainValueAsInt = 0;
    bool hasMainValueAsInt = false;
    if (mainValue != nullptr && mainLen >= 1) {
        if (mainLen == 1) {
            mainValueAsInt = mainValue[0];
            hasMainValueAsInt = true;
        } else if (mainLen >= 2) {
            mainValueAsInt = qfctpReadLe16(mainValue);
            hasMainValueAsInt = true;
        }
    }

    const bool ok = handleRequestResult(seq, req, mainValueAsInt, hasMainValueAsInt, errCode, hasErrCode);
    handleResponseByType(req, mainValue, mainLen);
    emit sendGetProductResponse(ok ? 1 : 0);
}

void Qfctp::registerResponseHandlers()
{
    // 查发送到回包：先看发送处的 service/tlv，再在这里按同一组 key 找 handler。
    // sendRequest(...) 会同步打印 FCTP EXPECT，现场日志也能直接看到预期回包函数。
    registerResponseHandler(kTestsService, kTlvSnRead, QStringLiteral("handleRspSnRead"), &Qfctp::handleRspSnRead);
    registerResponseHandler(kTestsService, kTlvTupleRead, QStringLiteral("handleRspTupleRead"), &Qfctp::handleRspTupleRead);
    registerResponseHandler(kTestsService, kTlvAgingStatus, QStringLiteral("handleRspAgingStatus"), &Qfctp::handleRspAgingStatus);
    registerResponseHandler(kTestsService, kTlvDeviceException, QStringLiteral("handleRspDeviceException"), &Qfctp::handleRspDeviceException);
    registerResponseHandler(kTestsService, kTlvTrimGet, QStringLiteral("handleRspTrimGet"), &Qfctp::handleRspTrimGet);
    registerResponseHandler(kTestsService, kTlvFactoryDoneRead, QStringLiteral("handleRspFactoryDoneRead"), &Qfctp::handleRspFactoryDoneRead);
    registerResponseHandler(kTestsService, kTlvRssiRead, QStringLiteral("handleRspRssiRead"), &Qfctp::handleRspRssiRead);
    registerResponseHandler(kTestsService, kTlvSensorState, QStringLiteral("handleRspSensorState"), &Qfctp::handleRspSensorState);
    registerResponseHandler(kTestsService, kTlvBatteryRead, QStringLiteral("handleRspBatteryRead"), &Qfctp::handleRspBatteryRead);
    registerResponseHandler(kTestsService, kTlvKeySignalRead, QStringLiteral("handleRspKeySignalRead"), &Qfctp::handleRspKeySignalRead);
    registerResponseHandler(kTestsService, kTlvLightCalibRead, QStringLiteral("handleRspLightCalibRead"), &Qfctp::handleRspLightCalibRead);
    registerResponseHandler(kTestsService, kTlvLcdBacklight, QStringLiteral("handleRspLcdBacklight"), &Qfctp::handleRspLcdBacklight);
    registerResponseHandler(kTestsService, kTlvLightReportCtrl, QStringLiteral("handleRspLightReportCtrl"), &Qfctp::handleRspLightReportCtrl);
    registerResponseHandler(kTestsService, kTlvLightCalibWrite, QStringLiteral("handleRspLightCalibWrite"), &Qfctp::handleRspLightCalibWrite);
    registerResponseHandler(kTestsService, kTlvChargeCurrentRead, QStringLiteral("handleRspChargeCurrentRead"), &Qfctp::handleRspChargeCurrentRead);

    registerResponseHandler(kSystemConfigService, kTlvFwVersionRead, QStringLiteral("handleRspFwVersionRead"), &Qfctp::handleRspFwVersionRead);
    registerResponseHandler(kSystemConfigService, kTlvMacRead, QStringLiteral("handleRspMacRead"), &Qfctp::handleRspMacRead);
}

void Qfctp::registerResponseHandler(uint16_t serviceId, uint16_t tlvType, const QString &responseName, ResponseHandler handler)
{
    const uint32_t key = qfctpMakeResponseKey(serviceId, tlvType);
    m_responseHandlers.insert(key, handler);
    m_responseNames.insert(key, responseName);
}

void Qfctp::handleResponseByType(const PendingRequest &req, const uint8_t *mainValue, uint16_t mainLen)
{
    m_currentResponseRequest = req;
    const auto it = m_responseHandlers.constFind(qfctpMakeResponseKey(req.serviceId, req.tlvType));
    if (it != m_responseHandlers.constEnd()) {
        (this->*(it.value()))(mainValue, mainLen);
    }
}

bool Qfctp::isDataResponse(uint16_t serviceId, uint16_t tlvType) const
{
    return m_responseHandlers.contains(qfctpMakeResponseKey(serviceId, tlvType));
}

QString Qfctp::responseNameFor(uint16_t serviceId, uint16_t tlvType) const
{
    return m_responseNames.value(qfctpMakeResponseKey(serviceId, tlvType));
}

void Qfctp::handleRspSnRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen > 0) {
        const QString sn = QString::fromLatin1(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).trimmed();
        qInfo() << "FCTP SN读取:" << sn;
        emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(ProtocolSnData{ProtocolSnType::TailSn, sn}));
    }
}

void Qfctp::handleRspTupleRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen > 0) {
        const QByteArray raw(reinterpret_cast<const char *>(mainValue), mainLen);
        const QString prod = QString::fromLatin1(raw.left(6)).trimmed();//产品ID
        const QString dev = QString::fromLatin1(raw.mid(6, 16)).trimmed();//设备id
        const QString key = QString::fromLatin1(raw.mid(22, 16)).trimmed();//key
        if (mainLen < 38) {
            qWarning() << "FCTP 三元组读取长度不足 len=" << mainLen << "raw=" << raw.toHex(' ').toUpper();
        }
        qInfo() << "FCTP 三元组读取 prod=" << prod << "dev=" << dev << "key=" << key;
        emitReport(QStringLiteral("ProtocolTupleData"), QVariant::fromValue(ProtocolTupleData{prod, dev, key}));
    }
}

void Qfctp::handleRspAgingStatus(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen >= 7) {
        const QByteArray raw(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen));
        const uint8_t status = mainValue[0];
        const uint16_t loops = qfctpReadLe16(mainValue + 1);
        const uint32_t seconds = static_cast<uint32_t>(mainValue[3]) | (static_cast<uint32_t>(mainValue[4]) << 8)
                               | (static_cast<uint32_t>(mainValue[5]) << 16) | (static_cast<uint32_t>(mainValue[6]) << 24);
        qInfo() << "FCTP 老化状态 raw=" << raw.toHex(' ') << "老化结果=" << status << "循环次数=" << loops << "已经老化时间=" << seconds;
        emitReport(QStringLiteral("ProtocolAgingStatusData"), QVariant::fromValue(ProtocolAgingStatusData{static_cast<int>(status), static_cast<int>(loops), seconds}));
    }
}

void Qfctp::handleRspDeviceException(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen >= 1) {
        const uint8_t status = mainValue[0];
        QString statusText = "未知";
        switch (status) {
        case 0x00: statusText = "正常"; break;
        case 0x01: statusText = "电量低"; break;
        case 0x02: statusText = "漏气"; break;
        case 0x03: statusText = "堵转"; break;
        default: break;
        }
        qInfo() << "FCTP 设备异常状态"
                << "status=0x" + QString::number(status, 16).toUpper().rightJustified(2, '0')
                << "(" << statusText << ")"
                << "raw=" << QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        emitReport(QStringLiteral("ProtocolDeviceExceptionData"), QVariant::fromValue(ProtocolDeviceExceptionData{static_cast<int>(status), statusText}));
    }
}

void Qfctp::handleRspTrimGet(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen >= 4) {
        const uint32_t trim = static_cast<uint32_t>(mainValue[0]) | (static_cast<uint32_t>(mainValue[1]) << 8)
                            | (static_cast<uint32_t>(mainValue[2]) << 16) | (static_cast<uint32_t>(mainValue[3]) << 24);
        const QString trimHex = "0x" + QString::number(trim, 16).toUpper().rightJustified(8, '0');
        QByteArray data((char*)mainValue, mainLen);
        QByteArray hex = data.toHex(' ');
        QString rawHex = hex.toUpper();

        qInfo() << "FCTP Trim读取" << "trim=" << trim << "(" << trimHex << ")" << "raw=" << rawHex;
        emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP Trim读取 trim=%1(%2) raw=%3").arg(QString::number(trim), trimHex, rawHex));
        emitReport(QStringLiteral("ProtocolTrimData"), QVariant::fromValue(ProtocolTrimData{trim}));
    }
}

void Qfctp::handleRspFactoryDoneRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen >= 1) {
        const uint8_t done = mainValue[0];
        const QString doneText = (done == 0x01) ? "产测完成" : "产测未完成";
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        qInfo() << "FCTP 产测完成标识"
                << "value=0x" + QString::number(done, 16).toUpper().rightJustified(2, '0')
                << "(" << doneText << ")" << "raw=" << rawHex;
        emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 产测完成标识 value=0x%1(%2) raw=%3")
                              .arg(QString::number(done, 16).toUpper().rightJustified(2, '0'), doneText, rawHex));
        emitReport(QStringLiteral("ProtocolFactoryDoneData"), QVariant::fromValue(ProtocolFactoryDoneData{done == 0x01}));
    }
}

void Qfctp::handleRspRssiRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen >= 4) {
        const int32_t rssi = static_cast<int32_t>(static_cast<uint32_t>(mainValue[0]) | (static_cast<uint32_t>(mainValue[1]) << 8)
                                                  | (static_cast<uint32_t>(mainValue[2]) << 16) | (static_cast<uint32_t>(mainValue[3]) << 24));
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        qInfo() << "FCTP RSSI读取" << "rssi=" << rssi << "dBm" << "raw=" << rawHex;
        emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP RSSI读取 rssi=%1 dBm raw=%2").arg(rssi).arg(rawHex));
        emitReport(QStringLiteral("ProtocolRssiData"), QVariant::fromValue(ProtocolRssiData{static_cast<int>(rssi)}));
    }
}

void Qfctp::handleRspSensorState(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen >= 5) {
        const auto stateText = [](uint8_t v) { return v == 0x01 ? "成功" : "失败"; };
        const uint8_t press0 = mainValue[0];
        const uint8_t press1 = mainValue[1];
        const uint8_t battIc = mainValue[2];
        const uint8_t touchIc = mainValue[3];
        const uint8_t ledIc = mainValue[4];
        const bool hasPdIc = mainLen >= 6;
        const uint8_t pdIc = hasPdIc ? mainValue[5] : 0xFF;
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        qInfo() << "FCTP 外设sensor状态"
                << "press0=" << QString("0x%1(%2)").arg(QString::number(press0, 16).toUpper().rightJustified(2, '0'), stateText(press0))
                << "press1=" << QString("0x%1(%2)").arg(QString::number(press1, 16).toUpper().rightJustified(2, '0'), stateText(press1))
                << "batteryIc=" << QString("0x%1(%2)").arg(QString::number(battIc, 16).toUpper().rightJustified(2, '0'), stateText(battIc))
                << "touchIc=" << QString("0x%1(%2)").arg(QString::number(touchIc, 16).toUpper().rightJustified(2, '0'), stateText(touchIc))
                << "ledIc=" << QString("0x%1(%2)").arg(QString::number(ledIc, 16).toUpper().rightJustified(2, '0'), stateText(ledIc))
                << "pdIc=" << (hasPdIc ? QString("0x%1(%2)").arg(QString::number(pdIc, 16).toUpper().rightJustified(2, '0'), stateText(pdIc)) : QString("未上报"))
                << "raw=" << rawHex;
        emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 外设sensor状态 press0=%1 press1=%2 batteryIc=%3 touchIc=%4 ledIc=%5 pdIc=%6 raw=%7")
                              .arg(stateText(press0)).arg(stateText(press1)).arg(stateText(battIc)).arg(stateText(touchIc))
                              .arg(stateText(ledIc)).arg(hasPdIc ? stateText(pdIc) : "未上报").arg(rawHex));
        ProtocolPeriphStateData periphData;
        // 回什么传什么：协议层不做通过/失败判定。
        periphData.press0_state = static_cast<int>(press0);
        periphData.press1_state = static_cast<int>(press1);
        periphData.battery_ic_state = static_cast<int>(battIc);
        periphData.touch_ic_state = static_cast<int>(touchIc);
        periphData.led_ic_state = static_cast<int>(ledIc);
        periphData.pd_ic_state = hasPdIc ? static_cast<int>(pdIc) : -1;
        emitReport(QStringLiteral("ProtocolPeriphStateData"), QVariant::fromValue(periphData));
      
    }
}

void Qfctp::handleRspFwVersionRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen > 0) {
        const QByteArray raw(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen));
        const QString hex = raw.toHex(' ').toUpper();
        const QString ascii = QString::fromLatin1(raw).trimmed();
        QStringList versionParts;
        versionParts.reserve(raw.size());
        for (const char byte : raw) {
            versionParts << QString::number(static_cast<uint8_t>(byte));
        }
        const QString versionText = versionParts.join(".");
        qInfo() << "FCTP 固件版本" << "raw=" << hex << "ascii=" << ascii << "version=" << versionText;
        emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 固件版本 raw=%1").arg(hex));
        ProtocolBaseInfoData data;
        data.soft_version = versionText;
        emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(data));
     
    }
}

void Qfctp::handleRspMacRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr && mainLen > 0) {
        const QByteArray raw(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen));
        const QString hex = raw.toHex(' ').toUpper();
        QString macText;
        if (mainLen == 6) {
            QStringList parts;
            for (int i = 0; i < raw.size(); ++i) {
                parts << QString::number(static_cast<uint8_t>(raw.at(i)), 16).toUpper().rightJustified(2, '0');
            }
            macText = parts.join(":");
        } else {
            macText = QString::fromLatin1(raw).trimmed();
        }
        qInfo() << "FCTP MAC读取" << "raw=" << hex << "mac=" << macText;
        emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP MAC读取 raw=%1 mac=%2").arg(hex, macText));
        emitReport(QStringLiteral("ProtocolMacData"), QVariant::fromValue(ProtocolMacData{macText}));
    }
}

void Qfctp::handleRspBatteryRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr) {
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        if (mainLen == 1) {
            const int b = static_cast<int>(static_cast<uint8_t>(mainValue[0]));
            qInfo() << "FCTP 电池电量读取(ACK/单字节) value="
                    << b << "%"
                    << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 电池电量读取 value=%1% raw=%2").arg(b).arg(rawHex));
            ProtocolBatteryData batteryData;
            batteryData.percent = b;
            emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(batteryData));
        } else {
            qWarning() << "FCTP 电池电量读取 长度异常 len=" << mainLen << "raw=" << rawHex;
        }
    }
}

void Qfctp::handleRspKeySignalRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr) {
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        int keyId = -1;
        if (!m_currentResponseRequest.requestValue.isEmpty()) {
            keyId = static_cast<int>(static_cast<uint8_t>(m_currentResponseRequest.requestValue.at(0)));
        }
        const QString keyText = keyId >= 0 ? QString::number(keyId) : QStringLiteral("?");
        if (mainLen >= 4) {
            const quint32 cap = qfctpParseKeyCapU32(mainValue);
            const bool cap2301 = SETTINGS.value(QStringLiteral("KeyCap/ValueEndian"), QStringLiteral("big"))
                                     .toString()
                                     .compare(QStringLiteral("little"), Qt::CaseInsensitive)
                                 == 0;
            const QString endianTag = cap2301 ? QStringLiteral("2301") : QStringLiteral("0123");
            qInfo() << "FCTP 按键电容读取 key=" << keyText << "endian=" << endianTag << "capacitance_u32=" << cap
                    << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("FCTP 按键电容读取 key=%1 %2 value=%3 raw=%4")
                                  .arg(keyText, endianTag)
                                  .arg(cap)
                                  .arg(rawHex));
            ProtocolKeyCapData out;
            out.capacitance = cap;
            out.keyId = keyId;
            emitReport(QStringLiteral("ProtocolKeyCapData"), QVariant::fromValue(out));
        } else {
            qWarning() << "FCTP 按键电容读取 key=" << keyText << "长度异常 len=" << mainLen << "raw=" << rawHex;
        }
    }
}

void Qfctp::handleRspLightCalibRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr) {
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        if (mainLen >= 4) {
            const uint32_t cal = static_cast<uint32_t>(mainValue[0]) | (static_cast<uint32_t>(mainValue[1]) << 8)
                               | (static_cast<uint32_t>(mainValue[2]) << 16) | (static_cast<uint32_t>(mainValue[3]) << 24);
            qInfo() << "FCTP 光感校准读取 calib_u32=" << cal << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感校准读取 value=%1 raw=%2").arg(cal).arg(rawHex));
            emitReport(QStringLiteral("ProtocolLightCalibData"), QVariant::fromValue(ProtocolLightCalibData{cal}));
        } else {
            qWarning() << "FCTP 光感校准读取 长度异常 len=" << mainLen << "raw=" << rawHex;
        }
    }
}

void Qfctp::handleRspLcdBacklight(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr) {
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        if (mainLen == 1) {
            const int ack = static_cast<int>(static_cast<uint8_t>(mainValue[0]));
            const QString ackText = (ack == 1) ? "成功" : "失败";
            qInfo() << "FCTP LCD背光控制应答 响应结果=" << ackText << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP LCD背光控制应答 响应结果=%1 raw=%2").arg(ackText).arg(rawHex));
            emitReport(QStringLiteral("ProtocolAckData"), QVariant::fromValue(ProtocolAckData{ack}));
        } else {
            qInfo() << "FCTP LCD背光控制应答 len=" << mainLen << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP LCD背光控制应答 raw=%1").arg(rawHex));
        }
    }
}

void Qfctp::handleRspLightReportCtrl(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr) {
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        if (mainLen == 1) {
            const int ack = static_cast<int>(static_cast<uint8_t>(mainValue[0]));
            const QString ackText = (ack == 1) ? "成功" : "失败";
            qInfo() << "FCTP 光感上报控制应答 响应结果=" << ackText << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感上报控制应答 响应结果=%1 raw=%2").arg(ackText).arg(rawHex));
            emitReport(QStringLiteral("ProtocolAckData"), QVariant::fromValue(ProtocolAckData{ack}));
        } else {
            qInfo() << "FCTP 光感上报控制应答 len=" << mainLen << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感上报控制应答 raw=%1").arg(rawHex));
        }
    }
}

void Qfctp::handleRspLightCalibWrite(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr) {
        const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
        if (mainLen >= 4) {
            const uint32_t v = static_cast<uint32_t>(mainValue[0]) | (static_cast<uint32_t>(mainValue[1]) << 8)
                             | (static_cast<uint32_t>(mainValue[2]) << 16) | (static_cast<uint32_t>(mainValue[3]) << 24);
            qInfo() << "FCTP 光感校准写入应答 value_u32=" << v << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感校准写入应答 value=%1 raw=%2").arg(v).arg(rawHex));
            emitReport(QStringLiteral("ProtocolLightCalibData"), QVariant::fromValue(ProtocolLightCalibData{v}));
        } else {
            qInfo() << "FCTP 光感校准写入应答 len=" << mainLen << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感校准写入应答 raw=%1").arg(rawHex));
        }
    }
}

void Qfctp::handleRspChargeCurrentRead(const uint8_t *mainValue, uint16_t mainLen)
{
    if (mainValue != nullptr) {
        if (mainLen >= 4) {
            const uint32_t chargeCurrentMa = static_cast<uint32_t>(mainValue[0]) | (static_cast<uint32_t>(mainValue[1]) << 8)
                                           | (static_cast<uint32_t>(mainValue[2]) << 16) | (static_cast<uint32_t>(mainValue[3]) << 24);
            qInfo() << "FCTP 充电电流值 current_mA=" << chargeCurrentMa;
            const QString rawHex = QByteArray(reinterpret_cast<const char *>(mainValue), static_cast<int>(mainLen)).toHex(' ').toUpper();
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 充电电流读取 current_mA=%1 raw=%2").arg(chargeCurrentMa).arg(rawHex));
            emitReport(QStringLiteral("ProtocolChargeCurrentData"),
                        QVariant::fromValue(ProtocolChargeCurrentData{chargeCurrentMa}));
        } else {
            qWarning() << "FCTP ChargeCurrentGet 长度异常 len=" << mainLen;
        }
    }
}

void Qfctp::handleNotifyService(uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen)
{
    uint16_t off = 0;
    int algoLightSampleCount = -1;
    while (tlvs != nullptr && off + COMM_PROTOCOL_TLV_HEADER_SIZE <= serviceLen) {
        const uint8_t *p = tlvs + off;
        const uint16_t type = qfctpReadLe16(p);
        const uint16_t len = qfctpReadLe16(p + 2);
        if (off + COMM_PROTOCOL_TLV_HEADER_SIZE + len > serviceLen) {
            break;
        }
        const uint8_t *value = p + COMM_PROTOCOL_TLV_HEADER_SIZE;
        if (serviceId == kTestsService && type == kTlvKeyStatusReport && len >= 2) {
            // KEY_STATUS_REPORT: value[0]=event(1短按/2长按), value[1]=keyId(1-255)
            const uint8_t event = value[0];
            const uint8_t keyId = value[1];
            const QString eventText = (event == 1u) ? "短按" : ((event == 2u) ? "长按" : "未知事件");
            const QByteArray raw(reinterpret_cast<const char *>(value), static_cast<int>(len));
            qInfo() << "FCTP 按键上报"
                    << "event=" << event << "(" << eventText << ")"
                    << "key=" << keyId
                    << "raw=" << raw.toHex(' ');
            ProtocolButtonStateData buttonData;
            buttonData.modeButtonState = static_cast<int>(event);
            buttonData.keyButtonId = static_cast<int>(keyId);
            // if (keyId == 1u) {
            //     buttonData.modeButtonState = static_cast<int>(event);
            //     buttonData.keyButtonId = static_cast<int>(keyId);
            // } else if (keyId == 2u) {
            //     buttonData.powerButtonState = static_cast<int>(event);
            // }
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(buttonData));
        } else if (serviceId == kTestsService && type == kTlvEncoderStatusReport && len >= 2) {
            // ENCODER_STATUS_REPORT: value[0]=dir(1左旋/2右旋), value[1]=encoderId(1-255)
            const uint8_t dir = value[0];
            const uint8_t encoderId = value[1];
            const QString dirText = (dir == 1u) ? "左旋" : ((dir == 2u) ? "右旋" : "未知方向");
            const QByteArray raw(reinterpret_cast<const char *>(value), static_cast<int>(len));
            qInfo() << "FCTP 编码器上报"
                    << "dir=" << dir << "(" << dirText << ")"
                    << "id=" << encoderId
                    << "raw=" << raw.toHex(' ');
            ProtocolButtonStateData buttonData;
            buttonData.modeButtonState = static_cast<int>(dir);
            buttonData.powerButtonState = static_cast<int>(encoderId);
            buttonData.keyButtonId = static_cast<int>(encoderId);
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(buttonData));
        } else if (serviceId == kAlgoService && type == kAlgoTlvSampleDataNum && len == 2) {
            algoLightSampleCount = static_cast<int>(qfctpReadLe16(value));
            const QByteArray raw(reinterpret_cast<const char *>(value), static_cast<int>(len));
            const QString rawHex = raw.toHex(' ').toUpper();
            qInfo() << "FCTP 光感采样个数" << "count=" << algoLightSampleCount << "raw=" << rawHex;
            emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感采样个数 count=%1 raw=%2").arg(algoLightSampleCount).arg(rawHex));
        } else if (serviceId == kAlgoService && type == kAlgoTlvSampleLightSensorData && len > 0) {
            // 协议 8.2.31：光感数据上报（NOTIFY，Algo Service TLV 0x0006）
            const QByteArray raw(reinterpret_cast<const char *>(value), static_cast<int>(len));
            const QString rawHex = raw.toHex(' ').toUpper();
            const int sampleSize = static_cast<int>(len / 2);
            QStringList samples;
            samples.reserve(sampleSize);
            for (int i = 0; i < sampleSize; ++i) {
                samples << QString::number(qfctpReadLe16(value + i * 2));
            }

            if (sampleSize > 0) {
                const int firstSample = qfctpReadLe16(value);
                const QString countText = (algoLightSampleCount >= 0) ? QString::number(algoLightSampleCount) : QStringLiteral("N/A");
                if ((len % 2) != 0) {
                    qWarning() << "FCTP 光感上报长度非偶数" << "len=" << len << "raw=" << rawHex;
                }
                qInfo() << "FCTP 光感上报"
                        << "count=" << countText
                        << "values=" << samples.join(QStringLiteral(","))
                        << "raw=" << rawHex;
                emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感上报 count=%1 values=%2 raw=%3")
                                      .arg(countText)
                                      .arg(samples.join(QStringLiteral(",")))
                                      .arg(rawHex));
                emitReport(QStringLiteral("ProtocolPhotosensitiveData"), QVariant::fromValue(ProtocolPhotosensitiveData{firstSample}));
            } else {
                qInfo() << "FCTP 光感上报(短包) raw=" << rawHex;
                emitReport(QStringLiteral("ProtocolPbDate"), QString("FCTP 光感上报 raw=%1").arg(rawHex));
            }
        } else {
            qDebug() << "FCTP NOTIFY" << "service=" << serviceId << "type=" << type << "len=" << len;
        }
        off = static_cast<uint16_t>(off + COMM_PROTOCOL_TLV_HEADER_SIZE + len);
    }
}

bool Qfctp::handleRequestResult(uint8_t seq, const PendingRequest &req, int mainValue, bool hasMainValue, uint16_t errCode, bool hasErrCode)
{
    // 读类指令（已注册 handleRsp*）：回包主体是数据，不以业务 TLV 数值 1 表示成功
    const bool queryResponse = isDataResponse(req.serviceId, req.tlvType);
    // 服务层错误码 TLV 存在且为 0 才算协议层无错
    const bool errOk = hasErrCode && (errCode == 0u);
    // 写/设类：无业务值或业务值为 1 视为成功；读类：有数据回包即视为业务成功（具体解析在 handleRsp*）
    const bool bizOk = queryResponse ? true : (!hasMainValue || (mainValue == 1));
    const bool ok = errOk && bizOk;
    const QString bizText = hasMainValue ? QString::number(mainValue) : "N/A";
    const QString errText = hasErrCode ? QString::number(errCode) : "N/A";
    const QByteArray actionUtf8 = req.actionName.toUtf8();
    // 现场对照：query=1 为读类；写/设类业务值=0 或 errOk 为假时 result=FAIL
    qInfo() << "FCTP SOLVE:"
            << actionUtf8.constData()
            << "seq=" << static_cast<int>(seq)
            << "query=" << (queryResponse ? 1 : 0)
            << "业务值=" << bizText
            << "错误码=" << errText
            << "result=" << (ok ? "PASS" : "FAIL");
    return ok;
}

QByteArray Qfctp::wrapPhyPacket(const QByteArray &innerPacket) const
{
    // 外层协议: [8*0xCC][len:1][channel:1][payload:len]
    if (innerPacket.isEmpty()) {
        return {};
    }
    if (innerPacket.size() > 0xFF) {
        qWarning() << "FCTP 外层封装失败，内层长度超过 255:" << innerPacket.size();
        return {};
    }

    std::vector<uint8_t> newBuffer;
    newBuffer.reserve(static_cast<size_t>(kPhyHeaderSize + 1 + 1 + innerPacket.size()));
    newBuffer.insert(newBuffer.end(), kPhyHeaderSize, kPhyTxHeaderByte);
    newBuffer.push_back(static_cast<uint8_t>(innerPacket.size()));
    // 默认发送 FAC 通道
    newBuffer.push_back(kPhyChannelFac);
    newBuffer.insert(newBuffer.end(),
                     reinterpret_cast<const uint8_t *>(innerPacket.constData()),
                     reinterpret_cast<const uint8_t *>(innerPacket.constData()) + innerPacket.size());

    return QByteArray(reinterpret_cast<const char *>(newBuffer.data()), static_cast<int>(newBuffer.size()));
}

bool Qfctp::tryUnwrapPhyPacket(const QByteArray &packet, QList<QByteArray> &outPackets)
{
    const auto resetPhyState = [this]() {
        m_phyState = PHY_STATE_IDLE;
        m_phyHeaderHits = 0;
        m_phyExpectedLen = 0;
        m_phyChannel = 0;
        m_phyPayload.clear();
    };

    std::vector<uint8_t> data(packet.begin(), packet.end());
    for (auto x : data) {
        switch (m_phyState) {
        case PHY_STATE_IDLE:
            if (x == kPhyRxHeaderByte) {
                m_phyHeaderHits = 1;
                m_phyState = PHY_STATE_HEADER;
            }
            break;
        case PHY_STATE_HEADER:
            if (x == kPhyRxHeaderByte) {
                ++m_phyHeaderHits;
                if (m_phyHeaderHits == kPhyHeaderSize) {
                    m_phyState = PHY_STATE_CHANNEL;
                }
            } else {
                resetPhyState();
            }
            break;
        case PHY_STATE_CHANNEL:
            m_phyChannel = x;
            m_phyState = PHY_STATE_LEN;
            break;

        case PHY_STATE_LEN:
            m_phyExpectedLen = static_cast<int>(x);
            if (m_phyExpectedLen <= 0) {
                qWarning() << "FCTP 外层包长度非法:" << m_phyExpectedLen;
                resetPhyState();
                break;
            }
            m_phyPayload.clear();
            m_phyPayload.reserve(m_phyExpectedLen);
            m_phyState = PHY_STATE_PAYLOAD;
            break;

        case PHY_STATE_PAYLOAD:
            m_phyPayload.push_back(static_cast<char>(x));
            if (m_phyPayload.size() >= m_phyExpectedLen) {
                if (m_phyChannel == kPhyChannelFac || m_phyChannel == kPhyChannelApp || m_phyChannel == kPhyChannelMain) {
                    outPackets.push_back(m_phyPayload);
                } else {
                    qWarning() << "FCTP 外层包通道异常，channel =" << static_cast<int>(m_phyChannel);
                }
                resetPhyState();
            }
            break;
        default:
            resetPhyState();
            break;
        }
    }
    return !outPackets.isEmpty();
}

bool Qfctp::sendPacket(const QByteArray &innerPacket, QByteArray *outPhyPacket) const
{
    if (!serialPort || !serialPort->isOpen()) {
        qWarning() << "FCTP 串口未打开，未发送数据包";
        return false;
    }
    const QByteArray phyPacket = wrapPhyPacket(innerPacket);
    if (phyPacket.isEmpty()) {
        return false;
    }
    if (outPhyPacket != nullptr) {
        *outPhyPacket = phyPacket;
    }
    const qint64 n = serialPort->write(phyPacket);
    qDebug().noquote() << "FCTP TX:" << QString::fromLatin1(phyPacket.toHex(' ').toUpper());
    if (n != phyPacket.size()) {
        qWarning() << "FCTP 产测模式发送不完整" << n << "/" << phyPacket.size();
        return false;
    }
    return true;
}

bool Qfctp::sendServiceTlv(uint16_t serviceId, uint16_t tlvType, QByteArray value, const char *actionName, uint8_t *outSeq)
{
    comm_protocol_builder_t b{};
    std::memset(&b, 0, sizeof(b));
    const uint8_t seq = static_cast<uint8_t>(++m_fctpSeq);
    if (comm_protocol_builder_init(&b, 64, seq, COMM_PROTOCOL_FRAME_TYPE_REQUEST) != 0) {
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    comm_protocol_service_t *svc = comm_protocol_builder_init_service(&b, serviceId);
    if (svc == nullptr) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    comm_protocol_tlv_node_t node{};
    node.type   = tlvType;
    node.length = static_cast<uint16_t>(value.size());
    node.value  = value.isEmpty() ? nullptr : reinterpret_cast<uint8_t *>(value.data());
    if (comm_protocol_builder_add_tlv(&b, svc, &node) != 0) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    uint16_t outSize = 0;
    uint8_t *raw     = comm_protocol_builder_finalize(&b, &outSize);
    if (raw == nullptr) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    const QByteArray pkt(reinterpret_cast<const char *>(raw), static_cast<int>(outSize));
    comm_protocol_builder_free(&b);
    if (pkt.isEmpty()) {
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    QByteArray phyPacket;
    if (!sendPacket(pkt, &phyPacket)) {
        return false;
    }



    qDebug() << "FCTP TX DETAIL:" << actionName
             << "seq=" << static_cast<int>(seq)
             << "service=" << serviceId
             << "tlv_type=" << tlvType
             << "inner=" << pkt.toHex(' ').toUpper();
             //<< "outer=" << phyPacket.toHex(' ').toUpper();
    if (outSeq != nullptr) {
        *outSeq = seq;
    }
    return true;
}

bool Qfctp::sendTestsServiceTlv(uint16_t tlvType, QByteArray value, const char *actionName)
{
    return sendRequest(kTestsService, tlvType, value, actionName);
}

bool Qfctp::sendRequest(uint16_t serviceId, uint16_t tlvType, const QByteArray &value, const char *actionName)
{
    uint8_t seq = 0;
    if (!sendServiceTlv(serviceId, tlvType, value, actionName, &seq)) {
        return false;
    }
    PendingRequest req{};
    req.serviceId = serviceId;
    req.tlvType = tlvType;
    req.actionName = QString::fromUtf8(actionName);
    req.requestValue = value;
    m_pendingRequests.insert(seq, req);
    const QString responseName = responseNameFor(serviceId, tlvType);
    if (!responseName.isEmpty()) {
        qDebug() << "FCTP EXPECT:" << req.actionName
                 << "service=" << serviceId
                 << "tlv_type=" << tlvType
                 << "handler=" << responseName;
    }
    return true;
}

void Qfctp::sendFactoryTestMode(bool enter)
{
    const uint8_t v = enter ? 1u : 0u;
    const QByteArray value(reinterpret_cast<const char *>(&v), 1);
    sendTestsServiceTlv(kTlvFactoryMode, value, enter ? "产测模式进入" : "产测模式退出");
}

bool Qfctp::setSn(const QVariant &data)
{
    if (data.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = data.value<DeviceSnPayload>();
        switch (payload.which_sn) {
        case FacDevInfoType_SKUID:
            return sendTestsServiceTlv(kTlvProductIdWrite, payload.sn, "写产品ID");
        case FacDevInfoType_SUB_PID:
            return sendTestsServiceTlv(kTlvDeviceIdWrite, payload.sn, "写入设备id");
        case FacDevInfoType_BOARD_SN:
            qWarning() << "Qfctp BOARD_SN 使用SN通道兼容写入";
            return sendTestsServiceTlv(kTlvSnWrite, payload.sn, "写BOARD_SN");
        case FacDevInfoType_TAIL_SN:
        default:
            return sendTestsServiceTlv(kTlvSnWrite, payload.sn, "写SN");
        }
    }

    const QVariantList list = data.toList();
    if (list.size() >= 2) {
        const auto which = static_cast<FacDevInfoType>(list.at(0).toInt());
        const QByteArray sn = list.at(1).toByteArray();
        if (which == FacDevInfoType_SUB_PID) {
            return sendTestsServiceTlv(kTlvProductIdWrite, sn, "写SUB_PID");
        }
        if (which == FacDevInfoType_SKUID) {
            return sendTestsServiceTlv(kTlvDeviceIdWrite, sn, "写SKUID");
        }
        if (which == FacDevInfoType_BOARD_SN) {
            qWarning() << "Qfctp BOARD_SN 使用SN通道兼容写入";
            return sendTestsServiceTlv(kTlvSnWrite, sn, "写BOARD_SN");
        }
        return sendTestsServiceTlv(kTlvSnWrite, sn, "写SN");
    }

    QByteArray value = data.toByteArray();
    if (value.isEmpty()) {
        const QVariantMap map = data.toMap();
        value = map.value("sn").toByteArray();
    }
    if (value.isEmpty()) {
        qWarning() << "Qfctp SN写入参数为空";
        return false;
    }
    return sendTestsServiceTlv(kTlvSnWrite, value, "写SN");
}

bool Qfctp::setCaseAgingMode(const QVariantMap &map)
{
    const int mode = map.value("mode").toInt();
    if (mode <= 0) {
        qWarning() << "Qfctp setCaseAgingMode mode 非法:" << mode;
        return false;
    }
    QByteArray value;
    value.append(static_cast<char>(mode & 0xFF));
    const uint32_t sec = map.value("seconds").toUInt();
    value.append(static_cast<char>(sec & 0xFF));
    value.append(static_cast<char>((sec >> 8) & 0xFF));
    value.append(static_cast<char>((sec >> 16) & 0xFF));
    value.append(static_cast<char>((sec >> 24) & 0xFF));
    return sendTestsServiceTlv(kTlvAgingModeSet, value, "老化模式设置");
}

bool Qfctp::setCaseAgingExit()
{
    return sendTestsServiceTlv(kTlvAgingModeExit, {}, "老化模式退出");
}

bool Qfctp::setCaseSuctionMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvSuctionMode, QByteArray(1, static_cast<char>(v)), "吸力测试模式");
}

bool Qfctp::setCaseBtSignalMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvBtSignalMode, QByteArray(1, static_cast<char>(v)), "蓝牙信令测试模式");
}

bool Qfctp::setCaseBtNoSignalMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvBtNoSignalMode, QByteArray(1, static_cast<char>(v)), "蓝牙非信令测试模式");
}

bool Qfctp::setCaseBtFreqMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvBtFreqMode, QByteArray(1, static_cast<char>(v)), "蓝牙校频测试模式");
}

bool Qfctp::setCaseStandbyMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvStandbyMode, QByteArray(1, static_cast<char>(v)), "待机模式");
}

bool Qfctp::setCaseWriteKey(const QVariantMap &map)
{
    return sendTestsServiceTlv(kTlvKeyWrite, map.value("value").toByteArray(), "写密钥");
}

bool Qfctp::setCaseFactoryDoneWrite(const QVariantMap &map)
{
    const uint8_t v = map.value("done").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvFactoryDoneWrite, QByteArray(1, static_cast<char>(v)), "写产测完成标识");
}

bool Qfctp::setCaseTrimSet(const QVariantMap &map)
{
    const uint32_t trim = map.value("value").toUInt();
    QByteArray value;
    value.append(static_cast<char>(trim & 0xFF));
    value.append(static_cast<char>((trim >> 8) & 0xFF));
    value.append(static_cast<char>((trim >> 16) & 0xFF));
    value.append(static_cast<char>((trim >> 24) & 0xFF));
    return sendTestsServiceTlv(kTlvTrimSet, value, "写trim");
}

bool Qfctp::setCaseMacWrite(const QVariantMap &map)
{
    return sendRequest(kSystemConfigService, kTlvMacWrite, map.value("value").toByteArray(), "写MAC");
}

bool Qfctp::setCaseNightLightSet(const QVariantMap &map)
{
    const uint value = map.value("value").toUInt();
    // 夜灯亮度按 0~100 直接发送单字节数值：0 -> 0x00，100 -> 0x64。
    const uint8_t v = static_cast<uint8_t>(value > 100u ? 100u : value);
    return sendRequest(kSystemConfigService, kTlvNightLightSet, QByteArray(1, static_cast<char>(v)), "夜灯亮度设置");
}

bool Qfctp::setCaseLedTest(const QVariantMap &map)
{
    const uint8_t v = map.value("on").toInt() != 0 ? 1u : 0u;
    return sendRequest(kSystemConfigService, kTlvLedTest, QByteArray(1, static_cast<char>(v)), "显示测试");
}

bool Qfctp::setCaseFactoryReset()
{
    return sendRequest(kSystemConfigService, kTlvFactoryReset, {}, "恢复出厂");
}

bool Qfctp::setCasePowerOff()
{
    return sendRequest(kSystemConfigService, kTlvPowerOff, {}, "设备关机");
}

bool Qfctp::setCaseLcdBacklight(const QVariantMap &map)
{
    const uint8_t v = map.value("on").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvLcdBacklight, QByteArray(1, static_cast<char>(v)), "LCD背光控制");
}

bool Qfctp::setCaseLightReportControl(const QVariantMap &map)
{
    const uint8_t v = map.value("start").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvLightReportCtrl, QByteArray(1, static_cast<char>(v)), "光感上报控制");
}

bool Qfctp::setCaseLightCalibWrite(const QVariantMap &map)
{
    QByteArray value;
    const uint8_t idx = static_cast<uint8_t>(map.value("index").toUInt() & 0xFF);
    const int32_t calib = map.value("value").toInt();
    value.append(static_cast<char>(idx));
    value.append(static_cast<char>(calib & 0xFF));
    value.append(static_cast<char>((calib >> 8) & 0xFF));
    value.append(static_cast<char>((calib >> 16) & 0xFF));
    value.append(static_cast<char>((calib >> 24) & 0xFF));
    return sendTestsServiceTlv(kTlvLightCalibWrite, value, "光感校准写入");
}

bool Qfctp::getCaseTupleRead()
{
    return sendTestsServiceTlv(kTlvTupleRead, {}, "读取三元组");
}

bool Qfctp::getCaseTrimRead()
{
    return sendTestsServiceTlv(kTlvTrimGet, {}, "读取trim");
}

bool Qfctp::getCaseFwVersionRead()
{
    return sendRequest(kSystemConfigService, kTlvFwVersionRead, {}, "读取固件版本");
}

bool Qfctp::getCaseMacRead()
{
    return sendRequest(kSystemConfigService, kTlvMacRead, {}, "读取MAC");
}

bool Qfctp::setCaseCompensationSet(const QVariantMap &map)
{
    const uint8_t v = map.value("enable").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvCompensationSet, QByteArray(1, static_cast<char>(v)), "吸力补偿开关");
}

bool Qfctp::getCaseRssiRead(const QVariantMap &map)
{
    const uint8_t v = static_cast<uint8_t>(map.value("mode").toUInt() & 0xFF);
    return sendTestsServiceTlv(kTlvRssiRead, QByteArray(1, static_cast<char>(v)), "读取RSSI");
}

bool Qfctp::getCaseKeySignalRead(const QVariantMap &map)
{
    const uint8_t keyId = static_cast<uint8_t>(map.value("key").toUInt() & 0xFF);
    const QString action = QStringLiteral("读取按键电容值 key=%1").arg(keyId);
    return sendRequest(kTestsService, kTlvKeySignalRead, QByteArray(1, static_cast<char>(keyId)), action.toUtf8().constData());
}

bool Qfctp::getCaseLightCalibRead(const QVariantMap &map)
{
    const uint8_t idx = static_cast<uint8_t>(map.value("index").toUInt() & 0xFF);
    return sendTestsServiceTlv(kTlvLightCalibRead, QByteArray(1, static_cast<char>(idx)), "读取光感校准值");
}

bool Qfctp::getCaseChargeCurrentRead()
{
    return sendTestsServiceTlv(kTlvChargeCurrentRead, {}, "读取充电电流");
}

bool Qfctp::getCaseAgingStatusRead(const QVariantMap &map)
{
    const uint8_t mode = static_cast<uint8_t>(map.value("mode").toUInt() & 0xFF);
     qWarning() << "读取老化状态" << mode;
    return sendTestsServiceTlv(kTlvAgingStatus, QByteArray(1, static_cast<char>(mode)), "读取老化状态");
}

bool Qfctp::getCaseFactoryDoneRead()
{
    return sendTestsServiceTlv(kTlvFactoryDoneRead, {}, "读取产测完成标识");
}

bool Qfctp::getCaseDeviceExceptionRead()
{
    return sendTestsServiceTlv(kTlvDeviceException, {}, "读取设备异常状态");
}

bool Qfctp::getCasePeriphStateRead()
{
    return sendTestsServiceTlv(kTlvSensorState, {}, "读取外设sensor状态");
}

bool Qfctp::getCaseBatteryRead()
{
    return sendTestsServiceTlv(kTlvBatteryRead, {}, "读取电池电量");
}

void Qfctp::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty()) {
        return;
    }
    QList<QByteArray> innerPackets;
    if (!tryUnwrapPhyPacket(byte, innerPackets)) {
        return;
    }
    qDebug().noquote() << "FCTP RX:" << QString::fromLatin1(byte.toHex(' ').toUpper());
    // qDebug() << "FCTP 收到外层包"
    //          << "outer=" << byte.toHex(' ')
    //          << "inner_count=" << innerPackets.size();
    const QList<QByteArray> &packets = innerPackets;

    for (const auto &inner : packets) {
        // qDebug() << "inner:" << inner.toHex(' ').toUpper();
        const auto *p = reinterpret_cast<const uint8_t *>(inner.constData());
        const int  rc = comm_protocol_assemble_packet(p, static_cast<uint16_t>(inner.size()), qfctp_on_full_frame, this);
        if (rc < 0) {
            qWarning() << "comm_protocol_assemble_packet 错误:" << rc;
        }
    }
}

void Qfctp::set(DeviceCmd cmd, const QVariant& data) {
    const auto toBurningMap = [&](const QVariant &in) {
        QVariantMap map = in.toMap();
        if (map.isEmpty() && in.canConvert<QVariantList>()) {
            const QVariantList list = in.toList();
            if (!list.isEmpty()) {
                map.insert("mode", list.at(0).toInt());
            }
            if (list.size() >= 2) {
                map.insert("switch", list.at(1).toInt());
            }
        }
        if (!map.contains("seconds")) {
            map.insert("seconds", 3600u);
        }
        return map;
    };

    switch (cmd) {
    case DeviceCmd::FacMode:
        sendFactoryTestMode(data.toInt() != 0);
        return;
    case DeviceCmd::BurningMode: {
        const QVariantMap map = toBurningMap(data);
        // 退出：显式 enter==0 或 switch==0（与 Qpb 侧“关闭老化”语义对齐）
        const QVariant vEnter = map.value("enter");
        const QVariant vSwitch = map.value("switch");
        const bool wantExit =
            (vEnter.isValid() && vEnter.toInt() == 0) || (vSwitch.isValid() && vSwitch.toInt() == 0);
        if (wantExit) {
            if (!setCaseAgingExit()) {
                qWarning() << "Qfctp BurningMode 退出老化失败（发送 TLV 失败），当前参数:" << map;
            }
            return;
        }
        // 进入：必须带 mode>0；seconds 缺省由 toBurningMap 补 3600
        if (setCaseAgingMode(map)) {
            return;
        }
        qWarning() << "Qfctp BurningMode 进入老化失败，已解析参数:" << map
                   << "；规则: QVariantMap{mode(必填>0), seconds(可选)}，或 QVariantList{mode,switch?}，"
                      "退出用 enter:0 / switch:0";
        break;
    }
    case DeviceCmd::SuctionMode:
        if (setCaseSuctionMode(data.toMap())) return;
        break;
    case DeviceCmd::BtSignalMode:
        if (setCaseBtSignalMode(data.toMap())) return;
        break;
    case DeviceCmd::BtNoSignalMode:
        if (setCaseBtNoSignalMode(data.toMap())) return;
        break;
    case DeviceCmd::BtFreqMode:
        if (setCaseBtFreqMode(data.toMap())) return;
        break;
    case DeviceCmd::Sleep:
        if (setCaseStandbyMode(data.toMap())) return;
        break;
    case DeviceCmd::WriteKey:
        if (setCaseWriteKey(data.toMap())) return;
        break;
    case DeviceCmd::Sn:
        if (setSn(data)) {
            return;
        }
        break;
    case DeviceCmd::FacResult: { // FacResult 与 FactoryDoneWrite 已合并，兼容 int/map 两种传参
        QVariantMap map = data.toMap();
        if (map.isEmpty()) {
            map.insert("done", data.toInt());
        }
        if (setCaseFactoryDoneWrite(map)) return;
        break;
    }
    case DeviceCmd::TrimSet:
        if (setCaseTrimSet(data.toMap())) return;
        break;
    case DeviceCmd::MacWrite:
        if (setCaseMacWrite(data.toMap())) return;
        break;
    case DeviceCmd::NightLightSet:
        if (setCaseNightLightSet(data.toMap())) return;
        break;
    case DeviceCmd::LedTest:
        if (setCaseLedTest(data.toMap())) return;
        break;
    case DeviceCmd::FactoryReset:
        if (setCaseFactoryReset()) return;
        break;
    case DeviceCmd::ShipMode:
        if (setCasePowerOff()) return;
        break;
    case DeviceCmd::LcdBacklight:
        if (setCaseLcdBacklight(data.toMap())) return;
        break;
    case DeviceCmd::LightReportControl:
        if (setCaseLightReportControl(data.toMap())) return;
        break;
    case DeviceCmd::LightCalibWrite:
        if (setCaseLightCalibWrite(data.toMap())) return;
        break;
    case DeviceCmd::CompensationSet:
        if (setCaseCompensationSet(data.toMap())) return;
        break;
    case DeviceCmd::BaseInfo:
    case DeviceCmd::SoftVersionRead:
        qWarning() << "Qfctp 请改用主入口命令，不再使用 BaseInfo/SoftVersionRead set";
        break;
    case DeviceCmd::DeviceInfo:
        qWarning() << "Qfctp DeviceInfo(set)请改用细分DeviceCmd";
        break;
    default:
        emitReport(QStringLiteral("ProtocolPbDate"), QString("Qfctp 暂未实现/参数非法 set 命令，cmd=%1 data=%2")
                              .arg(static_cast<int>(cmd))
                              .arg(data.toString()));
        break;
    }

}

void Qfctp::get(DeviceCmd cmd, const QVariant& param) {
    switch (cmd) {
    case DeviceCmd::Sn: {
        const auto which = static_cast<FacDevInfoType>(param.toInt());
        if (which == FacDevInfoType_SUB_PID || which == FacDevInfoType_SKUID) {
            sendTestsServiceTlv(kTlvTupleRead, {}, "读三元组(兼容GetSn参数)");
        } else {
            if (which == FacDevInfoType_BOARD_SN) {
                qWarning() << "Qfctp BOARD_SN 使用SN读取通道兼容";
            }
            sendTestsServiceTlv(kTlvSnRead, {}, "读SN");
        }
        return;
    }
    case DeviceCmd::TrimRead:
        if (getCaseTrimRead()) return;
        break;
    case DeviceCmd::MacRead:
        if (getCaseMacRead()) return;
        break;
    case DeviceCmd::BaseInfo:
    case DeviceCmd::SoftVersionRead:
        if (getCaseFwVersionRead()) return;
        break;
    case DeviceCmd::TupleRead:
        if (getCaseTupleRead()) return;
        break;
    case DeviceCmd::LightSensorInfo:
        if (getCaseChargeCurrentRead()) return;
        break;
    case DeviceCmd::RssiRead:
        if (getCaseRssiRead(param.toMap())) return;
        break;
    case DeviceCmd::KeySignalRead:
        if (getCaseKeySignalRead(param.toMap())) return;
        break;
    case DeviceCmd::LightCalibRead:
        if (getCaseLightCalibRead(param.toMap())) return;
        break;
    case DeviceCmd::ChargeCurrentRead:
        if (getCaseChargeCurrentRead()) return;
        break;
    case DeviceCmd::AgingStatusRead:
        if (getCaseAgingStatusRead(param.toMap())) return;
        break;
    case DeviceCmd::FactoryDoneRead:
        if (getCaseFactoryDoneRead()) return;
        break;
    case DeviceCmd::DeviceExceptionRead:
        if (getCaseDeviceExceptionRead()) return;
        break;
    case DeviceCmd::DeviceInfo:
        qWarning() << "Qfctp DeviceInfo(get) 不再映射异常读取，请改用 DeviceExceptionRead";
        break;
    case DeviceCmd::PeriphState:
        if (getCasePeriphStateRead()) return;
        break;
    case DeviceCmd::GetBattery:
        if (getCaseBatteryRead()) return;
        break;
    default:
        emitReport(QStringLiteral("ProtocolPbDate"), QString("Qfctp 暂未实现/参数非法 get 命令，cmd=%1 param=%2")
                              .arg(static_cast<int>(cmd))
                              .arg(param.toString()));
        break;
    }

}
