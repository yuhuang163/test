#include "qaiot.h"

#include "Abini.h"
#include "aiot_link_defs.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <cstring>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {
constexpr int kPhyHeaderSize = 8;
constexpr uint8_t kPhyTxHeaderByte = 0xCC; // 上位机→Dongle
constexpr uint8_t kPhyRxHeaderByte = 0xAA; // Dongle→上位机（产测设备数据包）
constexpr uint8_t kPhyChannelFac = 1;

QString hexText(const QByteArray& data) {
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

bool toByteValue(const QVariant& v, quint8* out) {
    bool ok = false;
    const int n = v.toInt(&ok);
    if (!ok || n < 0 || n > 0xFF)
        return false;
    if (out)
        *out = static_cast<quint8>(n);
    return true;
}

QByteArray mapToUtf8(const QVariantMap& map, const QString& key) {
    if (!map.contains(key))
        return {};
    return map.value(key).toString().toUtf8();
}

/** 解析 device_side_id：0 Left / 1 Right / 2 Independent；默认 Independent */
quint8 parseDeviceSideIdText(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return AiotLink::kFctDeviceSideIndependent;
    bool ok = false;
    const uint n = s.toUInt(&ok);
    if (ok && n <= 2u)
        return static_cast<quint8>(n);
    const QString lower = s.toLower();
    if (lower == QLatin1String("left") || lower == QLatin1String("l") || s.contains(QStringLiteral("左")))
        return AiotLink::kFctDeviceSideLeft;
    if (lower == QLatin1String("right") || lower == QLatin1String("r") || s.contains(QStringLiteral("右")))
        return AiotLink::kFctDeviceSideRight;
    if (lower == QLatin1String("independent") || lower == QLatin1String("single") || lower == QLatin1String("s")
        || s.contains(QStringLiteral("单")) || s.contains(QStringLiteral("独立")) || lower == QLatin1String("f")
        || s.contains(QStringLiteral("未指定")))
        return AiotLink::kFctDeviceSideIndependent;
    return AiotLink::kFctDeviceSideIndependent;
}

quint8 resolveDeviceSideId(const QVariantMap& map, int sideIdOverride = -1) {
    if (sideIdOverride >= 0 && sideIdOverride <= 2)
        return static_cast<quint8>(sideIdOverride);
    static const QStringList keys = {QStringLiteral("side"),
                                     QStringLiteral("device_side_id"),
                                     QStringLiteral("deviceSideId"),
                                     QStringLiteral("sideId"),
                                     QStringLiteral("position")};
    for (const QString& key : keys) {
        if (!map.contains(key))
            continue;
        return parseDeviceSideIdText(map.value(key).toString());
    }
    // 步骤未写 Param_side 时，跟主界面「三元组位置」/ SETTINGS Tuple/Position
    const QString uiPos = SETTINGS.value(QStringLiteral("Tuple/Position")).toString().trimmed();
    if (!uiPos.isEmpty())
        return parseDeviceSideIdText(uiPos);
    return AiotLink::kFctDeviceSideIndependent;
}

QString deviceSideIdTip(quint8 side) {
    switch (side) {
    case AiotLink::kFctDeviceSideLeft:
        return QStringLiteral("Left");
    case AiotLink::kFctDeviceSideRight:
        return QStringLiteral("Right");
    case AiotLink::kFctDeviceSideIndependent:
    default:
        return QStringLiteral("Independent");
    }
}

/** 显示 MAC → 线序 6 字节（与 qroot 一致：显示首字节对应线序末字节） */
QByteArray parseMacToWire(const QVariant& data) {
    if (data.canConvert<QByteArray>()) {
        const QByteArray bytes = data.toByteArray();
        if (bytes.size() == 6)
            return bytes;
    }
    QString text = data.toString().trimmed();
    if (text.isEmpty() && data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        text = map.value(QStringLiteral("mac")).toString().trimmed();
        if (text.isEmpty())
            text = map.value(QStringLiteral("value")).toString().trimmed();
        if (text.isEmpty())
            text = map.value(QStringLiteral("string")).toString().trimmed();
    }
    text.remove(QLatin1Char(':'));
    text.remove(QLatin1Char('-'));
    text.remove(QLatin1Char(' '));
    if (text.size() % 2 != 0)
        text.prepend(QLatin1Char('0'));
    const QByteArray hex = QByteArray::fromHex(text.toLatin1());
    if (hex.size() != 6)
        return {};
    QByteArray wire(6, '\0');
    for (int i = 0; i < 6; ++i)
        wire[i] = hex.at(5 - i);
    return wire;
}

QString macWireToDisplay(const QByteArray& wire) {
    if (wire.size() < 6)
        return {};
    QStringList parts;
    parts.reserve(6);
    for (int i = 5; i >= 0; --i) {
        parts.append(QString::number(static_cast<quint8>(wire.at(i)), 16)
                         .rightJustified(2, QLatin1Char('0'))
                         .toUpper());
    }
    return parts.join(QLatin1Char(':'));
}

QByteArray utcTimestampBe4() {
    const quint32 ts = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
    QByteArray out(4, '\0');
    out[0] = static_cast<char>((ts >> 24) & 0xFF);
    out[1] = static_cast<char>((ts >> 16) & 0xFF);
    out[2] = static_cast<char>((ts >> 8) & 0xFF);
    out[3] = static_cast<char>(ts & 0xFF);
    return out;
}

bool parseUtcTimestampBe4(const QByteArray& value, quint32* outTs) {
    if (value.size() < 4)
        return false;
    const quint32 ts = (static_cast<quint8>(value.at(0)) << 24) | (static_cast<quint8>(value.at(1)) << 16)
                       | (static_cast<quint8>(value.at(2)) << 8) | static_cast<quint8>(value.at(3));
    if (outTs)
        *outTs = ts;
    return true;
}

/** 设备数据时间戳：epoch + UTC/本地可读时间 */
QString formatDeviceDataTimestamp(quint32 ts) {
    const QDateTime utc = QDateTime::fromSecsSinceEpoch(ts, Qt::UTC);
    const QDateTime local = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(ts));
    return QStringLiteral("%1 (UTC %2 / 本地 %3)")
        .arg(ts)
        .arg(utc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
        .arg(local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

QByteArray floatToLeBytes(float f) {
    quint32 u = 0;
    static_assert(sizeof(float) == 4, "float must be 32-bit");
    memcpy(&u, &f, sizeof(u));
    QByteArray out(4, '\0');
    out[0] = static_cast<char>(u & 0xFF);
    out[1] = static_cast<char>((u >> 8) & 0xFF);
    out[2] = static_cast<char>((u >> 16) & 0xFF);
    out[3] = static_cast<char>((u >> 24) & 0xFF);
    return out;
}

bool floatFromLeBytes(const QByteArray& raw, int offset, float* out) {
    if (!out || offset < 0 || offset + 4 > raw.size())
        return false;
    const quint32 u = (static_cast<quint8>(raw.at(offset))) | (static_cast<quint8>(raw.at(offset + 1)) << 8)
                      | (static_cast<quint8>(raw.at(offset + 2)) << 16)
                      | (static_cast<quint8>(raw.at(offset + 3)) << 24);
    memcpy(out, &u, sizeof(float));
    return true;
}

bool parseAiotImuCali(const QByteArray& raw, ProtocolAiotImuCaliData* out) {
    if (!out || raw.size() < AiotLink::kFctImuCaliBytes)
        return false;
    float* fields[AiotLink::kFctImuCaliFloatCount] = {&out->kx, &out->ky, &out->kz, &out->syx, &out->szx,
                                                      &out->szy, &out->bx, &out->by, &out->bz};
    for (int i = 0; i < AiotLink::kFctImuCaliFloatCount; ++i) {
        if (!floatFromLeBytes(raw, i * 4, fields[i]))
            return false;
    }
    return true;
}

QByteArray packAiotImuCali(const ProtocolAiotImuCaliData& d) {
    QByteArray out;
    out.reserve(AiotLink::kFctImuCaliBytes);
    const float fields[AiotLink::kFctImuCaliFloatCount] = {d.kx, d.ky, d.kz, d.syx, d.szx, d.szy, d.bx, d.by, d.bz};
    for (float f : fields)
        out.append(floatToLeBytes(f));
    return out;
}

QString formatAiotImuCali(const ProtocolAiotImuCaliData& d) {
    return QStringLiteral("kx=%1 ky=%2 kz=%3 syx=%4 szx=%5 szy=%6 bx=%7 by=%8 bz=%9")
        .arg(d.kx, 0, 'g', 8)
        .arg(d.ky, 0, 'g', 8)
        .arg(d.kz, 0, 'g', 8)
        .arg(d.syx, 0, 'g', 8)
        .arg(d.szx, 0, 'g', 8)
        .arg(d.szy, 0, 'g', 8)
        .arg(d.bx, 0, 'g', 8)
        .arg(d.by, 0, 'g', 8)
        .arg(d.bz, 0, 'g', 8);
}

bool imuCaliFromParamMap(const QVariantMap& map, ProtocolAiotImuCaliData* out) {
    if (!out)
        return false;
    static const QStringList keys = {QStringLiteral("kx"),  QStringLiteral("ky"),  QStringLiteral("kz"),
                                     QStringLiteral("syx"), QStringLiteral("szx"), QStringLiteral("szy"),
                                     QStringLiteral("bx"),  QStringLiteral("by"),  QStringLiteral("bz")};
    bool any = false;
    for (const QString& k : keys) {
        if (map.contains(k)) {
            any = true;
            break;
        }
    }
    if (!any)
        return false;
    out->kx = map.value(QStringLiteral("kx")).toFloat();
    out->ky = map.value(QStringLiteral("ky")).toFloat();
    out->kz = map.value(QStringLiteral("kz")).toFloat();
    out->syx = map.value(QStringLiteral("syx")).toFloat();
    out->szx = map.value(QStringLiteral("szx")).toFloat();
    out->szy = map.value(QStringLiteral("szy")).toFloat();
    out->bx = map.value(QStringLiteral("bx")).toFloat();
    out->by = map.value(QStringLiteral("by")).toFloat();
    out->bz = map.value(QStringLiteral("bz")).toFloat();
    return true;
}

quint8 resolveSensorType(const QVariantMap& map, quint8 defaultType) {
    if (map.contains(QStringLiteral("type")))
        return static_cast<quint8>(map.value(QStringLiteral("type")).toUInt());
    if (map.contains(QStringLiteral("sensorType")))
        return static_cast<quint8>(map.value(QStringLiteral("sensorType")).toUInt());
    const QString name = map.value(QStringLiteral("sensor"), map.value(QStringLiteral("name"))).toString().trimmed().toLower();
    if (name == QLatin1String("imu") || name.contains(QStringLiteral("imu")))
        return AiotLink::kFctSensorTypeImu;
    if (name == QLatin1String("fsensor") || name == QLatin1String("force") || name.contains(QStringLiteral("电容"))
        || name.contains(QStringLiteral("力")) || name.contains(QLatin1String("cap")))
        return AiotLink::kFctSensorTypeCapacitive;
    if (name.contains(QLatin1String("tof")))
        return AiotLink::kFctSensorTypeTof;
    return defaultType;
}

QByteArray buildSensorCalibPayload(quint8 sensorType, const QVariantMap& map, QString* tipExtra) {
    if (sensorType == AiotLink::kFctSensorTypeImu) {
        ProtocolAiotImuCaliData imu;
        if (imuCaliFromParamMap(map, &imu)) {
            if (tipExtra)
                *tipExtra = formatAiotImuCali(imu);
            return packAiotImuCali(imu);
        }
        QByteArray calib = QByteArray::fromHex(
            map.value(QStringLiteral("data"), map.value(QStringLiteral("value"))).toString().remove(QLatin1Char(' ')).toLatin1());
        if (calib.size() == AiotLink::kFctImuCaliBytes) {
            if (tipExtra && parseAiotImuCali(calib, &imu))
                *tipExtra = formatAiotImuCali(imu);
            return calib;
        }
        if (calib.isEmpty()) {
            // 未给参数时写全 0（36B），便于联调
            if (tipExtra)
                *tipExtra = formatAiotImuCali(imu);
            return packAiotImuCali(imu);
        }
        if (tipExtra)
            *tipExtra = QStringLiteral("dataLen=%1(期望36)").arg(calib.size());
        return calib;
    }
    if (sensorType == AiotLink::kFctSensorTypeCapacitive) {
        int flag = -1;
        if (map.contains(QStringLiteral("calibrated")))
            flag = map.value(QStringLiteral("calibrated")).toInt();
        else if (map.contains(QStringLiteral("flag")))
            flag = map.value(QStringLiteral("flag")).toInt();
        else if (map.contains(QStringLiteral("int")))
            flag = map.value(QStringLiteral("int")).toInt();
        if (flag < 0) {
            const QByteArray calib = QByteArray::fromHex(
                map.value(QStringLiteral("data"), map.value(QStringLiteral("value"))).toString().remove(QLatin1Char(' ')).toLatin1());
            if (!calib.isEmpty())
                flag = static_cast<quint8>(calib.at(0));
            else
                flag = 0;
        }
        flag = flag ? 1 : 0;
        if (tipExtra)
            *tipExtra = flag ? QStringLiteral("已校准") : QStringLiteral("未校准");
        return QByteArray(1, static_cast<char>(flag));
    }
    QByteArray calib = QByteArray::fromHex(
        map.value(QStringLiteral("data"), map.value(QStringLiteral("value"))).toString().remove(QLatin1Char(' ')).toLatin1());
    if (calib.isEmpty())
        calib = QByteArray(1, '\0');
    if (tipExtra)
        *tipExtra = hexText(calib);
    return calib;
}

QString fctExceptionTypeName(quint8 type) {
    switch (type) {
    case AiotLink::kFctExTypeBatLowAlarm:
        return QStringLiteral("电池低电告警");
    case AiotLink::kFctExTypeBatLowShutdown:
        return QStringLiteral("电池低电关机");
    case AiotLink::kFctExTypeChargeOvervolt:
        return QStringLiteral("充电过压");
    case AiotLink::kFctExTypeChargeTimeout:
        return QStringLiteral("充电超时");
    case AiotLink::kFctExTypeBatTempAbnormal:
        return QStringLiteral("电池温度异常");
    case AiotLink::kFctExTypeMotorStallOvercurrent:
        return QStringLiteral("电机堵转/过流");
    case AiotLink::kFctExTypeMotorOpenCircuit:
        return QStringLiteral("电机开路");
    case AiotLink::kFctExTypeNegPressureHigh:
        return QStringLiteral("负压过高");
    default:
        return QStringLiteral("异常类型0x%1").arg(type, 2, 16, QChar('0'));
    }
}

QByteArray u16Be(quint16 v) {
    QByteArray out(2, '\0');
    out[0] = static_cast<char>((v >> 8) & 0xFF);
    out[1] = static_cast<char>(v & 0xFF);
    return out;
}

QByteArray u32Be(quint32 v) {
    QByteArray out(4, '\0');
    out[0] = static_cast<char>((v >> 24) & 0xFF);
    out[1] = static_cast<char>((v >> 16) & 0xFF);
    out[2] = static_cast<char>((v >> 8) & 0xFF);
    out[3] = static_cast<char>(v & 0xFF);
    return out;
}

bool parseExceptionThresholdRaw(quint8 type, const QByteArray& raw, ProtocolAiotExceptionThresholdItem* out) {
    if (!out)
        return false;
    out->type = type;
    out->raw = raw;
    out->value = 0;
    out->valueHigh = 0;
    switch (type) {
    case AiotLink::kFctExTypeBatLowAlarm:
    case AiotLink::kFctExTypeBatLowShutdown:
        if (raw.isEmpty())
            return false;
        out->value = static_cast<quint8>(raw.at(0));
        return true;
    case AiotLink::kFctExTypeChargeOvervolt:
    case AiotLink::kFctExTypeMotorStallOvercurrent:
    case AiotLink::kFctExTypeMotorOpenCircuit:
    case AiotLink::kFctExTypeNegPressureHigh:
        if (raw.size() < 2)
            return false;
        out->value = (static_cast<quint8>(raw.at(0)) << 8) | static_cast<quint8>(raw.at(1));
        return true;
    case AiotLink::kFctExTypeChargeTimeout:
        if (raw.size() < 4)
            return false;
        out->value = (static_cast<quint8>(raw.at(0)) << 24) | (static_cast<quint8>(raw.at(1)) << 16)
                     | (static_cast<quint8>(raw.at(2)) << 8) | static_cast<quint8>(raw.at(3));
        return true;
    case AiotLink::kFctExTypeBatTempAbnormal:
        if (raw.size() < 2)
            return false;
        out->value = static_cast<qint8>(static_cast<quint8>(raw.at(0)));
        out->valueHigh = static_cast<qint8>(static_cast<quint8>(raw.at(1)));
        return true;
    default:
        return !raw.isEmpty();
    }
}

QString formatExceptionThresholdItem(const ProtocolAiotExceptionThresholdItem& item) {
    const quint8 type = static_cast<quint8>(item.type);
    switch (type) {
    case AiotLink::kFctExTypeBatLowAlarm:
    case AiotLink::kFctExTypeBatLowShutdown:
        return QStringLiteral("%1=%2%%").arg(fctExceptionTypeName(type)).arg(item.value);
    case AiotLink::kFctExTypeChargeOvervolt:
        return QStringLiteral("%1=%2mV").arg(fctExceptionTypeName(type)).arg(item.value);
    case AiotLink::kFctExTypeChargeTimeout:
        return QStringLiteral("%1=%2s").arg(fctExceptionTypeName(type)).arg(item.value);
    case AiotLink::kFctExTypeBatTempAbnormal:
        return QStringLiteral("%1=%2~%3°C").arg(fctExceptionTypeName(type)).arg(item.value).arg(item.valueHigh);
    case AiotLink::kFctExTypeMotorStallOvercurrent:
    case AiotLink::kFctExTypeMotorOpenCircuit:
        return QStringLiteral("%1=%2mA").arg(fctExceptionTypeName(type)).arg(item.value);
    case AiotLink::kFctExTypeNegPressureHigh:
        return QStringLiteral("%1=%2").arg(fctExceptionTypeName(type)).arg(item.value);
    default:
        return QStringLiteral("%1 raw=%2").arg(fctExceptionTypeName(type), hexText(item.raw));
    }
}

QByteArray packExceptionThresholdValue(quint8 type, const QVariantMap& map, QString* tipOut, QString* errOut) {
    auto fail = [&](const QString& msg) -> QByteArray {
        if (errOut)
            *errOut = msg;
        return {};
    };
    switch (type) {
    case AiotLink::kFctExTypeBatLowAlarm:
    case AiotLink::kFctExTypeBatLowShutdown: {
        const int v = map.value(QStringLiteral("value"),
                                map.value(QStringLiteral("percent"), map.value(QStringLiteral("int"))))
                          .toInt();
        if (v < 0 || v > 100)
            return fail(QStringLiteral("电量阈值须 0~100"));
        if (tipOut)
            *tipOut = QStringLiteral("%1%%").arg(v);
        return QByteArray(1, static_cast<char>(v));
    }
    case AiotLink::kFctExTypeChargeOvervolt: {
        const int v = map.value(QStringLiteral("value"),
                                map.value(QStringLiteral("voltageMv"), map.value(QStringLiteral("mV"))))
                          .toInt();
        if (v < 0 || v > 0xFFFF)
            return fail(QStringLiteral("过压阈值无效"));
        if (tipOut)
            *tipOut = QStringLiteral("%1mV").arg(v);
        return u16Be(static_cast<quint16>(v));
    }
    case AiotLink::kFctExTypeChargeTimeout: {
        const int v = map.value(QStringLiteral("value"), map.value(QStringLiteral("seconds"))).toInt();
        if (v < 0)
            return fail(QStringLiteral("超时秒数无效"));
        if (tipOut)
            *tipOut = QStringLiteral("%1s").arg(v);
        return u32Be(static_cast<quint32>(v));
    }
    case AiotLink::kFctExTypeBatTempAbnormal: {
        const int low = map.value(QStringLiteral("low"),
                                  map.value(QStringLiteral("tempLow"), map.value(QStringLiteral("value"))))
                            .toInt();
        const int high = map.value(QStringLiteral("high"), map.value(QStringLiteral("tempHigh"),
                                                                    map.value(QStringLiteral("valueHigh"))))
                             .toInt();
        if (low < -128 || low > 127 || high < -128 || high > 127)
            return fail(QStringLiteral("温度阈值须 int8"));
        if (tipOut)
            *tipOut = QStringLiteral("%1~%2°C").arg(low).arg(high);
        QByteArray out(2, '\0');
        out[0] = static_cast<char>(static_cast<qint8>(low));
        out[1] = static_cast<char>(static_cast<qint8>(high));
        return out;
    }
    case AiotLink::kFctExTypeMotorStallOvercurrent:
    case AiotLink::kFctExTypeMotorOpenCircuit: {
        const int v = map.value(QStringLiteral("value"),
                                map.value(QStringLiteral("currentMa"), map.value(QStringLiteral("mA"))))
                          .toInt();
        if (v < 0 || v > 0xFFFF)
            return fail(QStringLiteral("电流阈值无效"));
        if (tipOut)
            *tipOut = QStringLiteral("%1mA").arg(v);
        return u16Be(static_cast<quint16>(v));
    }
    case AiotLink::kFctExTypeNegPressureHigh: {
        const int v = map.value(QStringLiteral("value"), map.value(QStringLiteral("pressure"))).toInt();
        if (v < 0 || v > 0xFFFF)
            return fail(QStringLiteral("负压阈值无效"));
        if (tipOut)
            *tipOut = QString::number(v);
        return u16Be(static_cast<quint16>(v));
    }
    default: {
        QByteArray raw = QByteArray::fromHex(
            map.value(QStringLiteral("data"), map.value(QStringLiteral("value"))).toString().remove(QLatin1Char(' ')).toLatin1());
        if (raw.isEmpty())
            return fail(QStringLiteral("未知类型须提供 Param_data(hex)"));
        if (tipOut)
            *tipOut = hexText(raw);
        return raw;
    }
    }
}

quint8 resolveExceptionType(const QVariantMap& map, bool* okOut = nullptr) {
    if (okOut)
        *okOut = true;
    if (map.contains(QStringLiteral("type")))
        return static_cast<quint8>(map.value(QStringLiteral("type")).toUInt());
    if (map.contains(QStringLiteral("exceptionType")))
        return static_cast<quint8>(map.value(QStringLiteral("exceptionType")).toUInt());
    const QString name = map.value(QStringLiteral("name")).toString().trimmed().toLower();
    if (name.contains(QLatin1String("bat_low_alarm")) || name.contains(QStringLiteral("低电告警")))
        return AiotLink::kFctExTypeBatLowAlarm;
    if (name.contains(QLatin1String("bat_low_shutdown")) || name.contains(QStringLiteral("低电关机")))
        return AiotLink::kFctExTypeBatLowShutdown;
    if (name.contains(QLatin1String("charge_overvolt")) || name.contains(QStringLiteral("过压")))
        return AiotLink::kFctExTypeChargeOvervolt;
    if (name.contains(QLatin1String("charge_timeout")) || name.contains(QStringLiteral("超时")))
        return AiotLink::kFctExTypeChargeTimeout;
    if (name.contains(QLatin1String("bat_temp")) || name.contains(QStringLiteral("温度异常")))
        return AiotLink::kFctExTypeBatTempAbnormal;
    if (name.contains(QLatin1String("stall")) || name.contains(QStringLiteral("堵转")) || name.contains(QStringLiteral("过流")))
        return AiotLink::kFctExTypeMotorStallOvercurrent;
    if (name.contains(QLatin1String("open")) || name.contains(QStringLiteral("开路")))
        return AiotLink::kFctExTypeMotorOpenCircuit;
    if (name.contains(QLatin1String("neg_pressure")) || name.contains(QStringLiteral("负压")))
        return AiotLink::kFctExTypeNegPressureHigh;
    if (okOut)
        *okOut = false;
    return 0;
}

/** FCT&ATE CID → 中文名（日志对照） */
QString fctCidName(quint8 cid) {
    switch (cid) {
    case AiotLink::kFctCidGetFactoryStatus:
        return QStringLiteral("获取产测状态");
    case AiotLink::kFctCidSetFactoryStatus:
        return QStringLiteral("设置产测状态");
    case AiotLink::kFctCidGetDeviceData:
        return QStringLiteral("获取通用设备数据");
    case AiotLink::kFctCidSetDeviceData:
        return QStringLiteral("设置通用设备数据");
    case AiotLink::kFctCidSetRfTest:
        return QStringLiteral("设置射频测试");
    case AiotLink::kFctCidGetRfData:
        return QStringLiteral("获取射频数据");
    case AiotLink::kFctCidSetRfData:
        return QStringLiteral("设置射频数据");
    case AiotLink::kFctCidGetSensor:
        return QStringLiteral("获取传感器");
    case AiotLink::kFctCidSetSensor:
        return QStringLiteral("设置传感器");
    case AiotLink::kFctCidGetExceptionThreshold:
        return QStringLiteral("获取设备阈值");
    case AiotLink::kFctCidSetExceptionThreshold:
        return QStringLiteral("设置设备阈值");
    case AiotLink::kFctCidDeviceControl:
        return QStringLiteral("设备控制");
    case AiotLink::kFctCidGetBatteryInfo:
        return QStringLiteral("获取电量信息");
    case AiotLink::kFctCidSetPumpParam:
        return QStringLiteral("设置泵阀运行参数");
    case AiotLink::kFctCidGetPumpParam:
        return QStringLiteral("获取泵阀运行参数");
    case AiotLink::kFctCidSimulateKey:
        return QStringLiteral("模拟按键");
    case AiotLink::kFctCidVirtualBattery:
        return QStringLiteral("电量模拟测试");
    case AiotLink::kFctCidHeatTest:
        return QStringLiteral("自定义加热测试");
    case AiotLink::kFctCidVibrationTest:
        return QStringLiteral("自定义振动测试");
    case AiotLink::kFctCidSetCycleReport:
        return QStringLiteral("设置数据采集上报");
    case AiotLink::kFctCidCycleReportNotify:
        return QStringLiteral("数据采集被动上报");
    case AiotLink::kFctCidDutNotify:
        return QStringLiteral("测试数据主动上报");
    default:
        return QStringLiteral("CID=0x%1").arg(cid, 2, 16, QChar('0'));
    }
}

QString fctModeName(quint8 mode) {
    switch (mode) {
    case AiotLink::kFctModeIdle:
        return QStringLiteral("空闲模式");
    case AiotLink::kFctModeFactoryTest:
        return QStringLiteral("工厂测试模式");
    case AiotLink::kFctModeAging:
        return QStringLiteral("老化测试模式");
    case AiotLink::kFctModeSuction:
        return QStringLiteral("吸力测试模式");
    case AiotLink::kFctModeSuctionCompensate:
        return QStringLiteral("吸力补偿模式");
    case AiotLink::kFctModeAte:
        return QStringLiteral("ATE自动化测试模式");
    default:
        return QStringLiteral("模式0x%1").arg(mode, 2, 16, QChar('0'));
    }
}

QString fctKeyName(quint8 key) {
    switch (key) {
    case 0x01:
        return QStringLiteral("电源按键");
    case 0x02:
        return QStringLiteral("开始按键");
    case 0x03:
        return QStringLiteral("模式按键");
    case 0x04:
        return QStringLiteral("频率按键");
    case 0x05:
        return QStringLiteral("母乳按键");
    case 0x06:
        return QStringLiteral("左控制按键");
    case 0x07:
        return QStringLiteral("右控制按键");
    case 0x08:
        return QStringLiteral("恢复出厂按键");
    case 0x09:
        return QStringLiteral("旅行锁按键");
    case 0x0A:
        return QStringLiteral("旋钮左转");
    case 0x0B:
        return QStringLiteral("旋钮右转");
    default:
        return QStringLiteral("按键0x%1").arg(key, 2, 16, QChar('0'));
    }
}

QString fctSensorName(quint8 sensorType) {
    switch (sensorType) {
    case 0x00:
        return QStringLiteral("IMU");
    case 0x01:
        return QStringLiteral("压力");
    case 0x02:
        return QStringLiteral("气流");
    case 0x03:
        return QStringLiteral("TOF");
    case 0x04:
        return QStringLiteral("电容");
    case 0x05:
        return QStringLiteral("红外");
    case 0x06:
        return QStringLiteral("生物阻抗");
    case 0x07:
        return QStringLiteral("液位");
    case 0x08:
        return QStringLiteral("温度");
    case 0x09:
        return QStringLiteral("湿度");
    case 0x0A:
        return QStringLiteral("接近");
    case 0x0B:
        return QStringLiteral("电流");
    case 0x0C:
        return QStringLiteral("霍尔");
    case 0x0D:
        return QStringLiteral("编码器");
    default:
        return QStringLiteral("传感器0x%1").arg(sensorType, 2, 16, QChar('0'));
    }
}

qint16 beI16At(const QByteArray& raw, int off) {
    return static_cast<qint16>((static_cast<quint8>(raw.at(off)) << 8) | static_cast<quint8>(raw.at(off + 1)));
}

quint16 beU16At(const QByteArray& raw, int off) {
    return static_cast<quint16>((static_cast<quint8>(raw.at(off)) << 8) | static_cast<quint8>(raw.at(off + 1)));
}

qint32 beI32At(const QByteArray& raw, int off) {
    return (static_cast<qint32>(static_cast<quint8>(raw.at(off))) << 24)
           | (static_cast<qint32>(static_cast<quint8>(raw.at(off + 1))) << 16)
           | (static_cast<qint32>(static_cast<quint8>(raw.at(off + 2))) << 8)
           | static_cast<qint32>(static_cast<quint8>(raw.at(off + 3)));
}

quint32 beU32At(const QByteArray& raw, int off) {
    return (static_cast<quint32>(static_cast<quint8>(raw.at(off))) << 24)
           | (static_cast<quint32>(static_cast<quint8>(raw.at(off + 1))) << 16)
           | (static_cast<quint32>(static_cast<quint8>(raw.at(off + 2))) << 8)
           | static_cast<quint32>(static_cast<quint8>(raw.at(off + 3)));
}

bool parseCycleReportSample(quint8 dataType, const QByteArray& raw, ProtocolAiotCycleReportItem* out, QString* tip) {
    if (!out)
        return false;
    out->dataType = static_cast<int>(dataType);
    out->raw = raw;
    switch (dataType) {
    case AiotLink::kFctSensorTypeImu:
        if (raw.size() < 12)
            return false;
        out->accX = beI16At(raw, 0);
        out->accY = beI16At(raw, 2);
        out->accZ = beI16At(raw, 4);
        out->gyroX = beI16At(raw, 6);
        out->gyroY = beI16At(raw, 8);
        out->gyroZ = beI16At(raw, 10);
        if (tip)
            *tip = QStringLiteral("%1 acc=%2/%3/%4 gyro=%5/%6/%7")
                       .arg(fctSensorName(dataType))
                       .arg(out->accX)
                       .arg(out->accY)
                       .arg(out->accZ)
                       .arg(out->gyroX)
                       .arg(out->gyroY)
                       .arg(out->gyroZ);
        return true;
    case AiotLink::kFctSensorTypePressure:
        if (raw.size() < 4)
            return false;
        out->pressureOut = beI16At(raw, 0);
        out->pressureIn = beI16At(raw, 2);
        if (tip)
            *tip = QStringLiteral("%1 p_out=%2(0.1Pa) p_in=%3(0.1Pa)")
                       .arg(fctSensorName(dataType))
                       .arg(out->pressureOut)
                       .arg(out->pressureIn);
        return true;
    case AiotLink::kFctSensorTypeAirflow:
        if (raw.size() < 4)
            return false;
        out->flowRate = beI32At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 flow=%2(0.01L/min)").arg(fctSensorName(dataType)).arg(out->flowRate);
        return true;
    case AiotLink::kFctSensorTypeTof:
        if (raw.size() < 2)
            return false;
        out->distanceMm = beU16At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 dist=%2mm").arg(fctSensorName(dataType)).arg(out->distanceMm);
        return true;
    case AiotLink::kFctSensorTypeCapacitive:
        if (raw.size() < 2)
            return false;
        out->adcRaw = beU16At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 adc=%2").arg(fctSensorName(dataType)).arg(out->adcRaw);
        return true;
    case AiotLink::kFctSensorTypeInfrared:
        if (raw.size() < 2)
            return false;
        out->irLevel = beU16At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 ir=%2").arg(fctSensorName(dataType)).arg(out->irLevel);
        return true;
    case AiotLink::kFctSensorTypeBioimpedance:
        if (raw.size() < 4)
            return false;
        out->impedance = static_cast<int>(beU32At(raw, 0));
        if (tip)
            *tip = QStringLiteral("%1 Z=%2(0.1Ω)").arg(fctSensorName(dataType)).arg(out->impedance);
        return true;
    case AiotLink::kFctSensorTypeLiquidLevel:
        if (raw.size() < 2)
            return false;
        out->levelMm = beU16At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 level=%2mm").arg(fctSensorName(dataType)).arg(out->levelMm);
        return true;
    case AiotLink::kFctSensorTypeTemperature:
        if (raw.isEmpty())
            return false;
        out->temperatureC = static_cast<quint8>(raw.at(0));
        if (tip)
            *tip = QStringLiteral("%1 temp=%2C").arg(fctSensorName(dataType)).arg(out->temperatureC);
        return true;
    case AiotLink::kFctSensorTypeHumidity:
        if (raw.isEmpty())
            return false;
        out->humidity = static_cast<quint8>(raw.at(0));
        if (tip)
            *tip = QStringLiteral("%1 RH=%2%%").arg(fctSensorName(dataType)).arg(out->humidity);
        return true;
    case AiotLink::kFctSensorTypeProximity:
        if (raw.size() < 2)
            return false;
        out->distanceMm = beU16At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 dist=%2").arg(fctSensorName(dataType)).arg(out->distanceMm);
        return true;
    case AiotLink::kFctSensorTypeCurrent:
        if (raw.size() < 2)
            return false;
        out->currentMa = beI16At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 I=%2mA").arg(fctSensorName(dataType)).arg(out->currentMa);
        return true;
    case AiotLink::kFctSensorTypeHall:
        if (raw.isEmpty())
            return false;
        out->hallState = static_cast<quint8>(raw.at(0));
        if (tip)
            *tip = QStringLiteral("%1 state=%2").arg(fctSensorName(dataType)).arg(out->hallState);
        return true;
    case AiotLink::kFctSensorTypeEncoder:
        if (raw.size() < 4)
            return false;
        out->pulseCount = beI32At(raw, 0);
        if (tip)
            *tip = QStringLiteral("%1 pulse=%2").arg(fctSensorName(dataType)).arg(out->pulseCount);
        return true;
    default:
        if (tip)
            *tip = QStringLiteral("%1 hex=%2").arg(fctSensorName(dataType), hexText(raw));
        return !raw.isEmpty();
    }
}

/** 从 Param 解析循环上报配置列表：单条 type+interval，或 types/intervals，或 items JSON */
QVector<ProtocolAiotCycleReportConfigItem> parseCycleReportConfigItems(const QVariantMap& map, QString* err) {
    QVector<ProtocolAiotCycleReportConfigItem> items;
    auto appendOne = [&](quint8 type, quint16 interval) {
        ProtocolAiotCycleReportConfigItem it;
        it.dataType = type;
        it.intervalTime = interval;
        items.append(it);
    };

    if (map.contains(QStringLiteral("items"))) {
        const QVariant v = map.value(QStringLiteral("items"));
        QVariantList list = v.toList();
        if (list.isEmpty() && v.type() == QVariant::String) {
            const QJsonDocument doc = QJsonDocument::fromJson(v.toString().toUtf8());
            if (doc.isArray())
                list = doc.array().toVariantList();
        }
        for (const QVariant& row : list) {
            const QVariantMap m = row.toMap();
            const quint8 type = static_cast<quint8>(
                m.value(QStringLiteral("type"), m.value(QStringLiteral("dataType"))).toUInt());
            const quint16 interval = static_cast<quint16>(
                m.value(QStringLiteral("intervalTime"), m.value(QStringLiteral("interval"))).toUInt());
            appendOne(type, interval);
        }
        if (items.isEmpty() && err)
            *err = QStringLiteral("Param_items 为空或格式错误");
        return items;
    }

    if (map.contains(QStringLiteral("types")) || map.contains(QStringLiteral("dataTypes"))) {
        const QString typesRaw =
            map.value(QStringLiteral("types"), map.value(QStringLiteral("dataTypes"))).toString();
        const QString intervalsRaw =
            map.value(QStringLiteral("intervals"), map.value(QStringLiteral("intervalTimes"))).toString();
        const QStringList typeParts = typesRaw.split(QLatin1Char(','), Qt::SkipEmptyParts);
        const QStringList intervalParts = intervalsRaw.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (int i = 0; i < typeParts.size(); ++i) {
            bool ok = false;
            const uint t = typeParts.at(i).trimmed().toUInt(&ok, 0);
            if (!ok)
                continue;
            quint16 interval = 0;
            if (i < intervalParts.size()) {
                bool iOk = false;
                interval = static_cast<quint16>(intervalParts.at(i).trimmed().toUInt(&iOk, 0));
                if (!iOk)
                    interval = 0;
            }
            appendOne(static_cast<quint8>(t), interval);
        }
        if (items.isEmpty() && err)
            *err = QStringLiteral("Param_types 解析失败");
        return items;
    }

    bool hasType = false;
    quint8 type = 0;
    for (const QString& k : {QStringLiteral("type"), QStringLiteral("dataType"), QStringLiteral("report_data_type"),
                             QStringLiteral("sensorType")}) {
        if (map.contains(k)) {
            type = static_cast<quint8>(map.value(k).toUInt());
            hasType = true;
            break;
        }
    }
    if (!hasType) {
        const QString name =
            map.value(QStringLiteral("sensor"), map.value(QStringLiteral("name"))).toString().trimmed().toLower();
        if (!name.isEmpty()) {
            type = resolveSensorType(map, AiotLink::kFctSensorTypeImu);
            hasType = true;
        }
    }
    if (!hasType)
        return items;

    quint16 interval = 0;
    for (const QString& k : {QStringLiteral("intervalTime"), QStringLiteral("report_interval_time"),
                             QStringLiteral("interval"), QStringLiteral("period")}) {
        if (map.contains(k)) {
            interval = static_cast<quint16>(map.value(k).toUInt());
            break;
        }
    }
    appendOne(type, interval);
    return items;
}

QString aiotErrorCodeText(quint32 code) {
    switch (code) {
    case 100000:
        return QStringLiteral("成功");
    case 100001:
        return QStringLiteral("未知error类型");
    case 100002:
        return QStringLiteral("不支持该Service的请求");
    case 100003:
        return QStringLiteral("不支持该Command的请求");
    case 100004:
        return QStringLiteral("无权限");
    case 100005:
        return QStringLiteral("系统忙");
    case 100006:
        return QStringLiteral("请求格式错误");
    case 100007:
        return QStringLiteral("参数错误");
    case 100009:
        return QStringLiteral("响应超时");
    case 101001:
        return QStringLiteral("入参为空/数据非法");
    case 101002:
        return QStringLiteral("设备电池电量低（禁止恢复出厂设置）");
    case 107001:
        return QStringLiteral("非法查询");
    default:
        return QStringLiteral("未知错误码");
    }
}
} // namespace

Qaiot::Qaiot(QSerialPort* parent) : qProtocol(parent), serialPort(parent) {
}

void Qaiot::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty())
        return;

    QList<QByteArray> inners;
    if (!tryUnwrapPhyPacket(byte, inners)) {
        // 兼容直连：仅无 PHY 头、且本身是 AIOT 链路 SOF 时才当内层
        if (!byte.isEmpty() && static_cast<uint8_t>(byte.at(0)) == AiotLink::kSof)
            inners.append(byte);
    }

    for (const QByteArray& inner : inners) {
        // 与 TX 同层：打 PHY 解出后的完整链路帧，并带上当前待应答指令名
        const QString act = pendingAction_.trimmed().isEmpty() ? QStringLiteral("-") : pendingAction_;
        qDebug().noquote() << QStringLiteral("[QAIOT] RX %1:").arg(act) << hexText(inner);
        QVector<AiotLinkCodec::Frame> frames;
        if (!linkCodec_.feed(inner, &frames))
            continue;
        for (const AiotLinkCodec::Frame& fr : frames)
            handleLinkFrame(fr);
    }
}

void Qaiot::handleLinkFrame(const AiotLinkCodec::Frame& frame) {
    if (frame.control & AiotLink::kCtrlRsp) {
        qDebug() << "QAIOT 链路层响应帧，忽略应用解析 control=" << frame.control;
        return;
    }

    const uint8_t fsnBits = static_cast<uint8_t>(frame.control & AiotLink::kCtrlFsnMask);
    QByteArray pdu;
    if (fsnBits == AiotLink::kCtrlFsnNone) {
        reassembling_ = false;
        reassembly_.clear();
        pdu = frame.payload;
    } else if (fsnBits == AiotLink::kCtrlFsnStart) {
        reassembling_ = true;
        expectFsn_ = static_cast<uint8_t>(frame.fsn + 1);
        reassembly_ = frame.payload;
        return;
    } else if (fsnBits == AiotLink::kCtrlFsnMiddle) {
        if (!reassembling_ || frame.fsn != expectFsn_) {
            reassembling_ = false;
            reassembly_.clear();
            return;
        }
        reassembly_.append(frame.payload);
        ++expectFsn_;
        return;
    } else { // end
        if (!reassembling_ || frame.fsn != expectFsn_) {
            reassembling_ = false;
            reassembly_.clear();
            return;
        }
        reassembly_.append(frame.payload);
        pdu = reassembly_;
        reassembling_ = false;
        reassembly_.clear();
    }

    Message message;
    QString errorMessage;
    if (!parseMessage(pdu, &message, &errorMessage)) {
        qDebug() << "QAIOT 应用层解析失败:" << errorMessage;
        return;
    }
    handleAppMessage(message);
}

void Qaiot::handleAppMessage(const Message& message) {
    // 完整 TLV 摘要只打调试日志，避免刷 UI；带上指令名便于对照步骤
    const QString act = pendingAction_.trimmed().isEmpty() ? QStringLiteral("-") : pendingAction_;
    qDebug().noquote() << QStringLiteral("QAIOT RX [%1] %2").arg(act, describeMessage(message));

    TlvNode err;
    if (findTlvDeep(message.tlvs, AiotLink::kTlvErrorCode, &err) && err.value.size() >= 4) {
        const quint32 code = (static_cast<quint8>(err.value.at(0)) << 24) |
                             (static_cast<quint8>(err.value.at(1)) << 16) |
                             (static_cast<quint8>(err.value.at(2)) << 8) |
                             static_cast<quint8>(err.value.at(3));
        const QString desc = aiotErrorCodeText(code);
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 错误码 %1（%2） action=%3")
                       .arg(code)
                       .arg(desc)
                       .arg(pendingAction_));
        emit sendGetProductResponse(0);
        return;
    }

    if (message.serviceId != AiotLink::kSvcFctAte) {
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 忽略非 FCT 服务 svc=0x%1")
                       .arg(message.serviceId, 2, 16, QChar('0')));
        return;
    }
    handleFctResponse(message);

    // 有应答即视为传输层完成（业务卡控由上层 report 判定）
    if (pendingService_ == message.serviceId && pendingCommand_ == message.commandId)
        emit sendGetProductResponse(1);
}

void Qaiot::handleFctResponse(const Message& message) {
    if (message.commandId == AiotLink::kFctCidGetFactoryStatus ||
        message.commandId == AiotLink::kFctCidSetFactoryStatus) {
        TlvNode n;
        // GET：complete=0x04；SET：complete=0x01；模式 type/status 均为 0x22/0x23
        const quint8 completeType = (message.commandId == AiotLink::kFctCidGetFactoryStatus)
                                        ? AiotLink::kFctGetTlvFactoryComplete
                                        : AiotLink::kFctSetTlvFactoryComplete;
        if (findTlvDeep(message.tlvs, completeType, &n) && !n.value.isEmpty()) {
            const bool done = static_cast<quint8>(n.value.at(0)) == 0x01;
            emitReport(QStringLiteral("ProtocolFactoryDoneData"),
                       QVariant::fromValue(ProtocolFactoryDoneData{done}));
        }

        // GET 顺带回填名称/固件/硬件/资源版本（MAC 改走 CID=0x03 Type=0x05）
        if (message.commandId == AiotLink::kFctCidGetFactoryStatus) {
            ProtocolBaseInfoData info;
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvDeviceName, &n))
                info.product_name = QString::fromUtf8(n.value);
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvFwVersion, &n))
                info.soft_version = QString::fromUtf8(n.value);
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvHwVersion, &n))
                info.hw_version = QString::fromUtf8(n.value);
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvResVersion, &n))
                info.res_version = QString::fromUtf8(n.value);

            if (!info.product_name.isEmpty() || !info.soft_version.isEmpty() || !info.hw_version.isEmpty() ||
                !info.res_version.isEmpty()) {
                emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(info));
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 产测状态 name=%1 soft=%2 hw=%3 res=%4")
                               .arg(info.product_name, info.soft_version, info.hw_version, info.res_version));
            }
        }

        // GET/SET 工厂模式字段 Type 已统一为 0x22/0x23
        quint8 modeType = 0xFF;
        quint8 modeStatus = 0xFF;
        QByteArray modeInfo;
        if (findTlvDeep(message.tlvs, AiotLink::kFctGetTlvModeType, &n) && !n.value.isEmpty())
            modeType = static_cast<quint8>(n.value.at(0));
        if (findTlvDeep(message.tlvs, AiotLink::kFctGetTlvModeStatus, &n) && !n.value.isEmpty()) {
            modeInfo = n.value;
            modeStatus = static_cast<quint8>(n.value.at(0));
        }
        if (modeType != 0xFF || modeStatus != 0xFF) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 工厂模式 type=0x%1 status=0x%2")
                           .arg(modeType, 2, 16, QChar('0'))
                           .arg(modeStatus, 2, 16, QChar('0')));
        }
        // 老化模式 info：status + finished + 双温 + stall_count(2) + 阈值(2) + 电流×5 = 18B
        if (message.commandId == AiotLink::kFctCidGetFactoryStatus
            && modeType == AiotLink::kFctModeAging && modeInfo.size() >= 18) {
            auto be16 = [&](int off) -> int {
                return (static_cast<quint8>(modeInfo.at(off)) << 8)
                       | static_cast<quint8>(modeInfo.at(off + 1));
            };
            ProtocolRootAgingHistoryData hist;
            hist.status = static_cast<quint8>(modeInfo.at(0));
            hist.finishedFlag = static_cast<quint8>(modeInfo.at(1));
            hist.batteryMaxTempC = static_cast<quint8>(modeInfo.at(2));
            hist.flangeMaxTempC = static_cast<quint8>(modeInfo.at(3));
            hist.stallCount = be16(4);
            hist.stallThreshold = be16(6);
            for (int i = 0; i < 5; ++i)
                hist.stallCurrents[i] = be16(8 + i * 2);
            qDebug().noquote() << "[Qaiot] AgingModeInfo enable=" << hist.status
                               << "finished=" << hist.finishedFlag
                               << "batMax=" << hist.batteryMaxTempC << "C"
                               << "flangeMax=" << hist.flangeMaxTempC << "C"
                               << "stallCount=" << hist.stallCount
                               << "stallTh=" << hist.stallThreshold << "I=" << hist.stallCurrents[0]
                               << hist.stallCurrents[1] << hist.stallCurrents[2]
                               << hist.stallCurrents[3] << hist.stallCurrents[4];
            emitReport(QStringLiteral("ProtocolRootAgingHistoryData"), QVariant::fromValue(hist));
        }
    } else if (message.commandId == AiotLink::kFctCidGetDeviceData) {
        // device_data_type：01 SN / 02~04 三元组 / 05 MAC；另有 side(0x01)/时间戳(0x02)
        QString prod, dev, key, sn, macText;
        ProtocolBaseInfoData macInfo;
        QString sideTip;
        QString tsTip;
        TlvNode meta;
        if (findTlvDeep(message.tlvs, AiotLink::kFctDeviceSideId, &meta) && !meta.value.isEmpty())
            sideTip = deviceSideIdTip(static_cast<quint8>(meta.value.at(0)));
        quint32 ts = 0;
        if (findTlvDeep(message.tlvs, AiotLink::kFctDeviceDataTimestamp, &meta)
            && parseUtcTimestampBe4(meta.value, &ts))
            tsTip = formatDeviceDataTimestamp(ts);

        QList<TlvNode> structs;
        for (const TlvNode& t : message.tlvs) {
            if (t.type == 0x04)
                structs.append(t);
            for (const TlvNode& c : t.children) {
                if (c.type == 0x04)
                    structs.append(c);
                for (const TlvNode& cc : c.children) {
                    if (cc.type == 0x04)
                        structs.append(cc);
                }
            }
        }
        for (const TlvNode& st : structs) {
            TlvNode typeNode, dataNode;
            if (!findTlv(st.children, 0x05, &typeNode) || !findTlv(st.children, 0x06, &dataNode))
                continue;
            if (typeNode.value.isEmpty())
                continue;
            const quint8 dt = static_cast<quint8>(typeNode.value.at(0));
            if (dt == AiotLink::kFctDataTypeMac) {
                if (dataNode.value.size() < 6)
                    continue;
                macInfo.ble_mac.size = 6;
                for (int i = 0; i < 6; ++i)
                    macInfo.ble_mac.bytes[i] = static_cast<uint8_t>(dataNode.value.at(i));
                macText = macWireToDisplay(dataNode.value);
                continue;
            }
            const QString text = QString::fromUtf8(dataNode.value);
            if (dt == AiotLink::kFctDataTypeSn)
                sn = text;
            else if (dt == AiotLink::kFctDataTypeProductId)
                prod = text;
            else if (dt == AiotLink::kFctDataTypeDeviceId)
                dev = text;
            else if (dt == AiotLink::kFctDataTypeDeviceSecret)
                key = text;
        }
        if (!sn.isEmpty())
            emitReport(QStringLiteral("ProtocolSnData"),
                       QVariant::fromValue(ProtocolSnData{ProtocolSnType::TailSn, sn}));
        if (!prod.isEmpty() || !dev.isEmpty() || !key.isEmpty())
            emitReport(QStringLiteral("ProtocolTupleData"),
                       QVariant::fromValue(ProtocolTupleData{prod, dev, key}));
        if (!macText.isEmpty()) {
            emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(macInfo));
            emitReport(QStringLiteral("ProtocolMacData"), QVariant::fromValue(ProtocolMacData{macText}));
        }

        QStringList parts;
        if (!sideTip.isEmpty())
            parts << QStringLiteral("side=%1").arg(sideTip);
        if (!tsTip.isEmpty())
            parts << QStringLiteral("时间戳=%1").arg(tsTip);
        if (!sn.isEmpty())
            parts << QStringLiteral("SN=%1").arg(sn);
        if (!prod.isEmpty())
            parts << QStringLiteral("productKey=%1").arg(prod);
        if (!dev.isEmpty())
            parts << QStringLiteral("deviceName=%1").arg(dev);
        if (!key.isEmpty())
            parts << QStringLiteral("deviceSecret=%1").arg(key);
        if (!macText.isEmpty())
            parts << QStringLiteral("MAC=%1").arg(macText);
        if (!parts.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 设备数据 %1").arg(parts.join(QLatin1Char(' '))));
        }
    } else if (message.commandId == AiotLink::kFctCidGetRfData) {
        TlvNode n;
        if (findTlv(message.tlvs, 0x01, &n) && n.value.size() >= 1) {
            int rssi = static_cast<qint8>(static_cast<quint8>(n.value.at(0)));
            if (n.value.size() >= 2)
                rssi = static_cast<qint16>((static_cast<quint8>(n.value.at(0)) << 8) |
                                           static_cast<quint8>(n.value.at(1)));
            emitReport(QStringLiteral("ProtocolRssiData"), QVariant::fromValue(ProtocolRssiData{rssi}));
        }
    } else if (message.commandId == AiotLink::kFctCidGetBatteryInfo) {
        // CID=0x0E：battery_percent(0x01)/voltage(0x02)/current(0x03)/temperature(0x04)
        TlvNode n;
        ProtocolBatteryData batteryData;
        if (findTlvDeep(message.tlvs, AiotLink::kFctBattPercent, &n) && !n.value.isEmpty())
            batteryData.percent = static_cast<int>(static_cast<quint8>(n.value.at(0)));
        if (findTlvDeep(message.tlvs, AiotLink::kFctBattVoltage, &n) && n.value.size() >= 2) {
            batteryData.voltageMv = (static_cast<quint8>(n.value.at(0)) << 8)
                                    | static_cast<quint8>(n.value.at(1));
        }
        if (findTlvDeep(message.tlvs, AiotLink::kFctBattCurrent, &n) && n.value.size() >= 2) {
            batteryData.currentMa = (static_cast<quint8>(n.value.at(0)) << 8)
                                    | static_cast<quint8>(n.value.at(1));
        }
        if (findTlvDeep(message.tlvs, AiotLink::kFctBattTemperature, &n) && !n.value.isEmpty())
            batteryData.temperatureC = static_cast<int>(static_cast<quint8>(n.value.at(0)));
        emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(batteryData));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 电池 percent=%1 voltage=%2mV current=%3mA temp=%4C")
                       .arg(batteryData.percent)
                       .arg(batteryData.voltageMv)
                       .arg(batteryData.currentMa)
                       .arg(batteryData.temperatureC));
    } else if (message.commandId == AiotLink::kFctCidGetSensor ||
               message.commandId == AiotLink::kFctCidSetSensor) {
        // list(0x01)/struct(0x02)/type(0x03)/data(0x04)；兼容设备误用外层 Type=0x03
        QList<TlvNode> structs;
        auto collectStructs = [&](const QList<TlvNode>& nodes) {
            for (const TlvNode& t : nodes) {
                if (t.type == 0x02)
                    structs.append(t);
                for (const TlvNode& c : t.children) {
                    if (c.type == 0x02)
                        structs.append(c);
                    for (const TlvNode& cc : c.children) {
                        if (cc.type == 0x02)
                            structs.append(cc);
                    }
                }
            }
        };
        collectStructs(message.tlvs);
        if (structs.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 传感器回包无 struct"));
        }
        for (const TlvNode& st : structs) {
            TlvNode typeNode, dataNode;
            if (!findTlv(st.children, 0x03, &typeNode) || typeNode.value.isEmpty())
                continue;
            findTlv(st.children, 0x04, &dataNode);
            const quint8 stype = static_cast<quint8>(typeNode.value.at(0));
            const QByteArray raw = dataNode.value;
            if (stype == AiotLink::kFctSensorTypeImu) {
                ProtocolAiotImuCaliData imu;
                if (parseAiotImuCali(raw, &imu)) {
                    emitReport(QStringLiteral("ProtocolAiotImuCaliData"), QVariant::fromValue(imu));
                    emitReport(QStringLiteral("ProtocolPbDate"),
                               QStringLiteral("QAIOT 读IMU校准 %1").arg(formatAiotImuCali(imu)));
                } else {
                    emitReport(QStringLiteral("ProtocolPbDate"),
                               QStringLiteral("QAIOT 读IMU校准失败：期望36B，实际=%1 hex=%2")
                                   .arg(raw.size())
                                   .arg(hexText(raw)));
                }
                continue;
            }
            if (stype == AiotLink::kFctSensorTypeCapacitive) {
                ProtocolAiotFsensorCaliData fs;
                fs.calibrated = (!raw.isEmpty() && static_cast<quint8>(raw.at(0)) != 0) ? 1 : 0;
                emitReport(QStringLiteral("ProtocolAiotFsensorCaliData"), QVariant::fromValue(fs));
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 读电容/力传感校准标志=%1（%2）")
                               .arg(fs.calibrated)
                               .arg(fs.calibrated ? QStringLiteral("已校准") : QStringLiteral("未校准")));
                continue;
            }
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 读%1传感器 data=%2")
                           .arg(fctSensorName(stype), raw.isEmpty() ? QStringLiteral("-") : hexText(raw)));
        }
    } else if (message.commandId == AiotLink::kFctCidGetExceptionThreshold
               || message.commandId == AiotLink::kFctCidSetExceptionThreshold) {
        // list(0x01)/struct(0x02)/type(0x03)/threshold(0x04)
        QList<TlvNode> structs;
        auto collectStructs = [&](const QList<TlvNode>& nodes) {
            for (const TlvNode& t : nodes) {
                if (t.type == 0x02)
                    structs.append(t);
                for (const TlvNode& c : t.children) {
                    if (c.type == 0x02)
                        structs.append(c);
                    for (const TlvNode& cc : c.children) {
                        if (cc.type == 0x02)
                            structs.append(cc);
                    }
                }
            }
        };
        collectStructs(message.tlvs);
        ProtocolAiotExceptionThresholdData data;
        QStringList tips;
        for (const TlvNode& st : structs) {
            TlvNode typeNode, dataNode;
            if (!findTlv(st.children, 0x03, &typeNode) || typeNode.value.isEmpty())
                continue;
            findTlv(st.children, 0x04, &dataNode);
            ProtocolAiotExceptionThresholdItem item;
            const quint8 et = static_cast<quint8>(typeNode.value.at(0));
            if (!parseExceptionThresholdRaw(et, dataNode.value, &item)) {
                tips << QStringLiteral("%1(解析失败 hex=%2)")
                            .arg(fctExceptionTypeName(et), hexText(dataNode.value));
                continue;
            }
            data.items.append(item);
            tips << formatExceptionThresholdItem(item);
        }
        if (!data.items.isEmpty())
            emitReport(QStringLiteral("ProtocolAiotExceptionThresholdData"), QVariant::fromValue(data));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 异常阈值 %1")
                       .arg(tips.isEmpty() ? QStringLiteral("(空)") : tips.join(QStringLiteral("; "))));
    } else if (message.commandId == AiotLink::kFctCidGetPumpParam
               || message.commandId == AiotLink::kFctCidSetPumpParam) {
        // CID=0x0F/0x10：circle(0x01) + param_struct(0x02)/duration~pwm
        ProtocolAiotPumpParamData data;
        auto readU16 = [&](const QList<TlvNode>& nodes, quint8 type, int* out) {
            TlvNode n;
            if (!findTlv(nodes, type, &n) || n.value.size() < 2 || !out)
                return false;
            *out = (static_cast<quint8>(n.value.at(0)) << 8) | static_cast<quint8>(n.value.at(1));
            return true;
        };
        auto readU8 = [&](const QList<TlvNode>& nodes, quint8 type, int* out) {
            TlvNode n;
            if (!findTlv(nodes, type, &n) || n.value.isEmpty() || !out)
                return false;
            *out = static_cast<quint8>(n.value.at(0));
            return true;
        };
        readU16(message.tlvs, AiotLink::kFctPumpCircleNum, &data.circleNum);
        TlvNode st;
        if (findTlvDeep(message.tlvs, AiotLink::kFctPumpParamStruct, &st)) {
            const QList<TlvNode>& ch = st.hasChildren ? st.children : QList<TlvNode>{};
            readU16(ch, AiotLink::kFctPumpDurationTime, &data.durationTime);
            readU16(ch, AiotLink::kFctPumpIntervalTime, &data.intervalTime);
            readU16(ch, AiotLink::kFctValveEnableTime, &data.valveEnableTime);
            readU16(ch, AiotLink::kFctValveDisableTime, &data.valveDisableTime);
            readU8(ch, AiotLink::kFctPumpPwmValue, &data.pumpPwm);
            readU8(ch, AiotLink::kFctValvePwmValue, &data.valvePwm);
        }
        emitReport(QStringLiteral("ProtocolAiotPumpParamData"), QVariant::fromValue(data));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 泵阀 循环=%1 泵=%2/%3 阀=%4/%5 PWM泵=%6%% 阀=%7%%")
                       .arg(data.circleNum)
                       .arg(data.durationTime)
                       .arg(data.intervalTime)
                       .arg(data.valveEnableTime)
                       .arg(data.valveDisableTime)
                       .arg(data.pumpPwm)
                       .arg(data.valvePwm));
    } else if (message.commandId == AiotLink::kFctCidHeatTest) {
        // CID=0x14：struct(0x01)/enable(0x02)/strength(0x03)/duration(0x04 可选)
        // 部分固件仅回 svc+cid 空 ACK，无结果 TLV → 按发送参数回填
        ProtocolAiotHeatTestData data;
        TlvNode st;
        if (findTlvDeep(message.tlvs, AiotLink::kFctHeatStatusStruct, &st)) {
            const QList<TlvNode>& ch = st.hasChildren ? st.children : QList<TlvNode>{};
            TlvNode n;
            if (findTlv(ch, AiotLink::kFctHeatEnable, &n) && !n.value.isEmpty())
                data.enable = static_cast<quint8>(n.value.at(0));
            if (findTlv(ch, AiotLink::kFctHeatDriveStrength, &n) && !n.value.isEmpty())
                data.driveStrength = static_cast<quint8>(n.value.at(0));
            if (findTlv(ch, AiotLink::kFctHeatDurationTime, &n) && n.value.size() >= 2) {
                data.durationTime = (static_cast<quint8>(n.value.at(0)) << 8) | static_cast<quint8>(n.value.at(1));
                data.hasDuration = true;
            }
        } else if (message.tlvs.isEmpty() && hasPendingHeat_) {
            data = pendingHeat_;
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 加热空ACK，按发送参数回填 enable=%1 strength=%2")
                           .arg(data.enable)
                           .arg(data.driveStrength));
        }
        hasPendingHeat_ = false;
        emitReport(QStringLiteral("ProtocolAiotHeatTestData"), QVariant::fromValue(data));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   data.hasDuration
                       ? QStringLiteral("QAIOT 加热 enable=%1 strength=%2 duration=%3")
                             .arg(data.enable)
                             .arg(data.driveStrength)
                             .arg(data.durationTime)
                       : QStringLiteral("QAIOT 加热 enable=%1 strength=%2")
                             .arg(data.enable)
                             .arg(data.driveStrength));
    } else if (message.commandId == AiotLink::kFctCidVibrationTest) {
        // CID=0x15：struct/enable/strength/freq/duration；空 ACK 同加热回填
        ProtocolAiotVibrationTestData data;
        TlvNode st;
        if (findTlvDeep(message.tlvs, AiotLink::kFctVibrationStatusStruct, &st)) {
            const QList<TlvNode>& ch = st.hasChildren ? st.children : QList<TlvNode>{};
            TlvNode n;
            if (findTlv(ch, AiotLink::kFctVibrationEnable, &n) && !n.value.isEmpty())
                data.enable = static_cast<quint8>(n.value.at(0));
            if (findTlv(ch, AiotLink::kFctVibrationDriveStrength, &n) && !n.value.isEmpty())
                data.driveStrength = static_cast<quint8>(n.value.at(0));
            if (findTlv(ch, AiotLink::kFctVibrationFreq, &n) && !n.value.isEmpty())
                data.freq = static_cast<quint8>(n.value.at(0));
            if (findTlv(ch, AiotLink::kFctVibrationDurationTime, &n) && n.value.size() >= 2) {
                data.durationTime = (static_cast<quint8>(n.value.at(0)) << 8) | static_cast<quint8>(n.value.at(1));
            }
        } else if (message.tlvs.isEmpty() && hasPendingVibration_) {
            data = pendingVibration_;
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 振动空ACK，按发送参数回填 enable=%1").arg(data.enable));
        }
        hasPendingVibration_ = false;
        emitReport(QStringLiteral("ProtocolAiotVibrationTestData"), QVariant::fromValue(data));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 振动 enable=%1 strength=%2 freq=%3 duration=%4")
                       .arg(data.enable)
                       .arg(data.driveStrength)
                       .arg(data.freq)
                       .arg(data.durationTime));
    } else if (message.commandId == AiotLink::kFctCidSetCycleReport) {
        // CID=0x18：enable(0x01) + list(0x02)/struct(0x03)/type(0x04)/interval(0x05)
        // 固件常只回空 ACK，但配置已生效（后续会有 0x19）→ 空包按发送参数回填
        ProtocolAiotCycleReportConfigData data;
        TlvNode en;
        const bool hasEnableTlv =
            findTlv(message.tlvs, AiotLink::kFctCycleReportEnable, &en) && !en.value.isEmpty();
        if (hasEnableTlv)
            data.enable = static_cast<quint8>(en.value.at(0));
        QList<TlvNode> structs;
        TlvNode listNode;
        if (findTlv(message.tlvs, AiotLink::kFctCycleReportTypeList, &listNode)) {
            for (const TlvNode& c : listNode.children) {
                if (c.type == AiotLink::kFctCycleReportConfigStruct)
                    structs.append(c);
            }
        }
        if (structs.isEmpty()) {
            for (const TlvNode& t : message.tlvs) {
                if (t.type == AiotLink::kFctCycleReportConfigStruct)
                    structs.append(t);
                for (const TlvNode& c : t.children) {
                    if (c.type == AiotLink::kFctCycleReportConfigStruct)
                        structs.append(c);
                }
            }
        }
        QStringList tips;
        for (const TlvNode& st : structs) {
            ProtocolAiotCycleReportConfigItem item;
            TlvNode n;
            if (findTlv(st.children, AiotLink::kFctCycleReportCfgDataType, &n) && !n.value.isEmpty())
                item.dataType = static_cast<quint8>(n.value.at(0));
            if (findTlv(st.children, AiotLink::kFctCycleReportIntervalTime, &n) && n.value.size() >= 2)
                item.intervalTime =
                    (static_cast<quint8>(n.value.at(0)) << 8) | static_cast<quint8>(n.value.at(1));
            else if (findTlv(st.children, AiotLink::kFctCycleReportIntervalTime, &n) && n.value.size() == 1)
                item.intervalTime = static_cast<quint8>(n.value.at(0)); // 兼容规范 Length=1 笔误
            data.items.append(item);
            tips << QStringLiteral("%1@%2ms").arg(fctSensorName(static_cast<quint8>(item.dataType))).arg(item.intervalTime);
        }
        if (!hasEnableTlv && structs.isEmpty() && hasPendingCycleReport_) {
            data = pendingCycleReport_;
            tips.clear();
            for (const ProtocolAiotCycleReportConfigItem& it : data.items)
                tips << QStringLiteral("%1@%2ms")
                            .arg(fctSensorName(static_cast<quint8>(it.dataType)))
                            .arg(it.intervalTime);
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 循环上报配置空ACK，按发送参数回填 enable=%1")
                           .arg(data.enable));
        }
        hasPendingCycleReport_ = false;
        emitReport(QStringLiteral("ProtocolAiotCycleReportConfigData"), QVariant::fromValue(data));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 循环上报配置 enable=%1 [%2]")
                       .arg(data.enable)
                       .arg(tips.isEmpty() ? QStringLiteral("-") : tips.join(QStringLiteral(", "))));
    } else if (message.commandId == AiotLink::kFctCidCycleReportNotify) {
        // CID=0x19：list(0x01)/struct(0x02)/type(0x03)/data(0x04)
        QList<TlvNode> structs;
        TlvNode listNode;
        if (findTlv(message.tlvs, AiotLink::kFctCycleReportDataList, &listNode)) {
            for (const TlvNode& c : listNode.children) {
                if (c.type == AiotLink::kFctCycleReportDataStruct)
                    structs.append(c);
            }
        }
        if (structs.isEmpty()) {
            for (const TlvNode& t : message.tlvs) {
                if (t.type == AiotLink::kFctCycleReportDataStruct)
                    structs.append(t);
                for (const TlvNode& c : t.children) {
                    if (c.type == AiotLink::kFctCycleReportDataStruct)
                        structs.append(c);
                }
            }
        }
        ProtocolAiotCycleReportData data;
        QStringList tips;
        for (const TlvNode& st : structs) {
            TlvNode typeNode, dataNode;
            if (!findTlv(st.children, AiotLink::kFctCycleReportSampleType, &typeNode) || typeNode.value.isEmpty())
                continue;
            findTlv(st.children, AiotLink::kFctCycleReportSampleData, &dataNode);
            ProtocolAiotCycleReportItem item;
            QString tip;
            const quint8 stype = static_cast<quint8>(typeNode.value.at(0));
            if (!parseCycleReportSample(stype, dataNode.value, &item, &tip)) {
                tips << QStringLiteral("%1(解析失败 len=%2 hex=%3)")
                            .arg(fctSensorName(stype))
                            .arg(dataNode.value.size())
                            .arg(hexText(dataNode.value));
                item.dataType = stype;
                item.raw = dataNode.value;
            } else {
                tips << tip;
            }
            data.items.append(item);
        }
        if (!data.items.isEmpty())
            emitReport(QStringLiteral("ProtocolAiotCycleReportData"), QVariant::fromValue(data));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 循环上报数据 %1")
                       .arg(tips.isEmpty() ? QStringLiteral("(空)") : tips.join(QStringLiteral("; "))));
    } else if (message.commandId == AiotLink::kFctCidDutNotify) {
        // CID=0x1A 主动上报：list(0x01)/struct(0x02)/type(0x03)/value(0x04)；type=0x00 为按键
        QList<TlvNode> structs;
        TlvNode listNode;
        if (findTlv(message.tlvs, AiotLink::kFctDutNotifyList, &listNode)) {
            for (const TlvNode& c : listNode.children) {
                if (c.type == AiotLink::kFctDutNotifyStruct)
                    structs.append(c);
            }
        }
        if (structs.isEmpty()) {
            for (const TlvNode& t : message.tlvs) {
                if (t.type == AiotLink::kFctDutNotifyStruct)
                    structs.append(t);
                for (const TlvNode& c : t.children) {
                    if (c.type == AiotLink::kFctDutNotifyStruct)
                        structs.append(c);
                }
            }
        }
        for (const TlvNode& st : structs) {
            TlvNode typeNode;
            TlvNode valueNode;
            if (!findTlv(st.children, AiotLink::kFctDutNotifyType, &typeNode) || typeNode.value.isEmpty())
                continue;
            if (!findTlv(st.children, AiotLink::kFctDutNotifyValue, &valueNode) || valueNode.value.isEmpty())
                continue;
            const quint8 notifyType = static_cast<quint8>(typeNode.value.at(0));
            if (notifyType != AiotLink::kFctDutNotifyTypeVirtualKey) {
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 主动上报 type=0x%1 value=%2")
                               .arg(notifyType, 2, 16, QChar('0'))
                               .arg(hexText(valueNode.value)));
                continue;
            }
            const quint8 key = static_cast<quint8>(valueNode.value.at(0));
            ProtocolButtonStateData btn;
            btn.keyButtonId = static_cast<int>(key);
            // 与 FCTP 对齐：普通键 mode=短按(1)；旋钮用 mode 表示方向 1左/2右
            if (key == 0x0A)
                btn.modeButtonState = 1;
            else if (key == 0x0B)
                btn.modeButtonState = 2;
            else
                btn.modeButtonState = 1;
            if (key == 0x01)
                btn.powerButtonState = 1;
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 按键上报 %1(0x%2)")
                           .arg(fctKeyName(key))
                           .arg(key, 2, 16, QChar('0')));
        }
    }
}

void Qaiot::set(DeviceCmd cmd, const QVariant& data) {
    const QVariantMap map = data.toMap();
    switch (cmd) {
    case DeviceCmd::FacMode: {
        // SET CID=0x02：list(0x20)/struct(0x21)/type(0x22)/status(0x23)；Param_mode 指定工厂模式类型
        int enable = 1;
        if (!map.isEmpty()) {
            enable = map.value(QStringLiteral("on"),
                               map.value(QStringLiteral("value"), map.value(QStringLiteral("switch"), 1)))
                         .toInt();
        } else if (data.canConvert<int>()) {
            enable = data.toInt();
        }
        const quint8 modeType = static_cast<quint8>(
            map.value(QStringLiteral("mode"), AiotLink::kFctModeFactoryTest).toInt());
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(modeType)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctGetTlvModeList,
                       {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})};
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus, tlvs,
                           QStringLiteral("%1%2").arg(enable ? QStringLiteral("进入") : QStringLiteral("退出"),
                                                     fctModeName(modeType)));
        break;
    }
    case DeviceCmd::BurningMode: {
        const int enable = map.value(QStringLiteral("switch"), map.value(QStringLiteral("enter"), 1)).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(AiotLink::kFctModeAging)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctGetTlvModeList,
                        {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})},
            enable ? QStringLiteral("进入老化测试模式") : QStringLiteral("退出老化测试模式"));
        break;
    }
    case DeviceCmd::SuctionMode: {
        const int enable = map.value(QStringLiteral("on"), map.value(QStringLiteral("switch"), 1)).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(AiotLink::kFctModeSuction)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctGetTlvModeList,
                        {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})},
            enable ? QStringLiteral("进入吸力测试模式") : QStringLiteral("退出吸力测试模式"));
        break;
    }
    case DeviceCmd::CompensationSet: {
        // 吸力补偿模式 Type=0x04
        const int enable = map.value(QStringLiteral("on"), map.value(QStringLiteral("switch"), map.value(QStringLiteral("value"), 1))).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(AiotLink::kFctModeSuctionCompensate)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctGetTlvModeList,
                        {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})},
            enable ? QStringLiteral("进入吸力补偿模式") : QStringLiteral("退出吸力补偿模式"));
        break;
    }
    case DeviceCmd::FacResult: {
        quint8 done = 0x01;
        if (data.canConvert<int>())
            done = data.toInt() ? 0x01 : 0x00;
        else if (map.contains(QStringLiteral("done")))
            done = map.value(QStringLiteral("done")).toInt() ? 0x01 : 0x00;
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
                           {makeLeaf(AiotLink::kFctSetTlvFactoryComplete, u8(done))},
                           QStringLiteral("写产测完成标识"));
        break;
    }
    case DeviceCmd::BtSignalMode:
    case DeviceCmd::BtNoSignalMode:
    case DeviceCmd::BtFreqMode: {
        quint8 rfType = 0x00;
        QString modeName = QStringLiteral("蓝牙信号模式");
        if (cmd == DeviceCmd::BtNoSignalMode) {
            rfType = 0x01;
            modeName = QStringLiteral("蓝牙无信号模式");
        } else if (cmd == DeviceCmd::BtFreqMode) {
            rfType = 0x02;
            modeName = QStringLiteral("蓝牙定频模式");
        }
        const int enable = map.value(QStringLiteral("on"), 1).toInt();
        QList<TlvNode> children;
        children.append(makeLeaf(0x02, u8(rfType)));
        children.append(makeLeaf(0x03, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetRfTest, {makeParent(0x01, children)},
                           QStringLiteral("%1%2")
                               .arg(enable ? QStringLiteral("开启") : QStringLiteral("关闭"), modeName));
        break;
    }
    case DeviceCmd::TrimSet: {
        const quint8 trim = static_cast<quint8>(map.value(QStringLiteral("trim"), map.value(QStringLiteral("value"))).toUInt());
        const quint8 power = static_cast<quint8>(map.value(QStringLiteral("power"), 0).toUInt());
        QList<TlvNode> children;
        children.append(makeLeaf(0x03, u8(power)));
        children.append(makeLeaf(0x04, u8(trim)));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetRfData, {makeParent(0x02, children)},
                           QStringLiteral("写射频 trim"));
        break;
    }
    case DeviceCmd::Sn:
    case DeviceCmd::WriteKey:
    case DeviceCmd::MacWrite: {
        // CID=0x04：device_side_id + UTC 时间戳 + data_type(01 SN / 02~04 三元组 / 05 MAC)
        QList<TlvNode> items;
        int sideOverride = -1;
        auto appendItem = [&](quint8 dataType, const QByteArray& bytes) {
            if (bytes.isEmpty())
                return;
            QList<TlvNode> ch;
            ch.append(makeLeaf(0x05, u8(dataType)));
            ch.append(makeLeaf(0x06, bytes));
            items.append(makeParent(0x04, ch));
        };
        if (cmd == DeviceCmd::MacWrite) {
            const QByteArray mac6 = parseMacToWire(data.isValid() ? data : QVariant(map));
            if (mac6.isEmpty()) {
                emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写 MAC 格式无效"));
                return;
            }
            appendItem(AiotLink::kFctDataTypeMac, mac6);
        } else if (data.canConvert<DeviceSnPayload>()) {
            const DeviceSnPayload payload = data.value<DeviceSnPayload>();
            sideOverride = payload.sideId;
            quint8 dataType = AiotLink::kFctDataTypeSn;
            if (payload.which_sn == FacDevInfoType_SKUID)
                dataType = AiotLink::kFctDataTypeProductId;
            else if (payload.which_sn == FacDevInfoType_SUB_PID)
                dataType = AiotLink::kFctDataTypeDeviceId;
            appendItem(dataType, payload.sn);
        } else {
            appendItem(AiotLink::kFctDataTypeSn, mapToUtf8(map, QStringLiteral("sn")));
            QByteArray productId = mapToUtf8(map, QStringLiteral("productId"));
            if (productId.isEmpty())
                productId = mapToUtf8(map, QStringLiteral("productKey"));
            QByteArray deviceId = mapToUtf8(map, QStringLiteral("deviceId"));
            if (deviceId.isEmpty())
                deviceId = mapToUtf8(map, QStringLiteral("deviceName"));
            QByteArray deviceSecret = mapToUtf8(map, QStringLiteral("deviceSecret"));
            if (deviceSecret.isEmpty())
                deviceSecret = mapToUtf8(map, QStringLiteral("key"));
            if (deviceSecret.isEmpty())
                deviceSecret = mapToUtf8(map, QStringLiteral("value"));
            appendItem(AiotLink::kFctDataTypeProductId, productId);
            appendItem(AiotLink::kFctDataTypeDeviceId, deviceId);
            appendItem(AiotLink::kFctDataTypeDeviceSecret, deviceSecret);
            if (items.isEmpty() && data.type() == QVariant::String)
                appendItem(AiotLink::kFctDataTypeSn, data.toString().toUtf8());
        }
        if (items.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写设备数据缺少字段"));
            return;
        }
        const quint8 side = resolveDeviceSideId(map, sideOverride);
        const QByteArray tsBytes = utcTimestampBe4();
        quint32 ts = 0;
        parseUtcTimestampBe4(tsBytes, &ts);
        QList<TlvNode> tlvs;
        tlvs.append(makeLeaf(AiotLink::kFctDeviceSideId, u8(side)));
        tlvs.append(makeLeaf(AiotLink::kFctDeviceDataTimestamp, tsBytes)); // device_data_timestap（必选）
        tlvs.append(makeParent(0x03, items));
        QString tip = QStringLiteral("写设备数据");
        if (cmd == DeviceCmd::MacWrite)
            tip = QStringLiteral("写MAC");
        else if (cmd == DeviceCmd::WriteKey)
            tip = QStringLiteral("写三元组");
        tip += QStringLiteral(" side=%1 时间戳=%2").arg(deviceSideIdTip(side), formatDeviceDataTimestamp(ts));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetDeviceData, tlvs, tip);
        break;
    }
    case DeviceCmd::ButtonState: {
        // 模拟按键 CID=0x11：Param_int / Param_key = 0x01~0x0B
        quint8 key = 0;
        if (map.contains(QStringLiteral("key")))
            key = static_cast<quint8>(map.value(QStringLiteral("key")).toUInt());
        else if (map.contains(QStringLiteral("int")))
            key = static_cast<quint8>(map.value(QStringLiteral("int")).toUInt());
        else if (data.canConvert<int>())
            key = static_cast<quint8>(data.toUInt());
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSimulateKey, {makeLeaf(0x01, u8(key))},
                           QStringLiteral("模拟%1").arg(fctKeyName(key)));
        break;
    }
    case DeviceCmd::SetBattery: {
        // CID=0x13：可选 percent/voltageMv/currentMa/temperatureC（各通道独立；置 0=该通道真实值）
        const bool hasPercent = map.contains(QStringLiteral("percent"))
                                || map.contains(QStringLiteral("simbatterypercent"));
        const bool hasVoltage = map.contains(QStringLiteral("voltageMv"))
                                || map.contains(QStringLiteral("voltage"))
                                || map.contains(QStringLiteral("mV"))
                                || map.contains(QStringLiteral("simbatteryvoltagemv"));
        const bool hasCurrent = map.contains(QStringLiteral("currentMa"))
                                || map.contains(QStringLiteral("current"))
                                || map.contains(QStringLiteral("simbatterycurrentma"));
        const bool hasTemp = map.contains(QStringLiteral("temperatureC"))
                             || map.contains(QStringLiteral("temperature"))
                             || map.contains(QStringLiteral("temp"))
                             || map.contains(QStringLiteral("simbatterytemperaturec"));

        QList<TlvNode> tlvs;
        QStringList tipParts;
        if (hasPercent) {
            const quint8 pct = static_cast<quint8>(
                map.value(QStringLiteral("percent"), map.value(QStringLiteral("simbatterypercent")))
                    .toUInt());
            tlvs.append(makeLeaf(AiotLink::kFctBattPercent, u8(pct)));
            tipParts << QStringLiteral("%1%%").arg(pct);
        }
        quint16 mv = 0;
        bool setMv = false;
        if (hasVoltage) {
            if (map.contains(QStringLiteral("voltageMv")))
                mv = static_cast<quint16>(map.value(QStringLiteral("voltageMv")).toUInt());
            else if (map.contains(QStringLiteral("simbatteryvoltagemv")))
                mv = static_cast<quint16>(map.value(QStringLiteral("simbatteryvoltagemv")).toUInt());
            else if (map.contains(QStringLiteral("voltage")))
                mv = static_cast<quint16>(map.value(QStringLiteral("voltage")).toUInt());
            else
                mv = static_cast<quint16>(map.value(QStringLiteral("mV")).toUInt());
            setMv = true;
        } else if (map.contains(QStringLiteral("value"))) {
            // 兼容旧 Param_value=mV（现电压 TLV 为 0x02）
            mv = static_cast<quint16>(map.value(QStringLiteral("value")).toUInt());
            setMv = true;
        } else if (!hasPercent && !hasCurrent && !hasTemp && data.canConvert<int>()) {
            mv = static_cast<quint16>(data.toUInt());
            setMv = true;
        }
        if (setMv) {
            tlvs.append(makeLeaf(AiotLink::kFctBattVoltage, u16be(mv)));
            tipParts << QStringLiteral("%1mV").arg(mv);
        }
        if (hasCurrent) {
            const quint16 ma = static_cast<quint16>(
                map.value(QStringLiteral("currentMa"),
                          map.value(QStringLiteral("current"),
                                    map.value(QStringLiteral("simbatterycurrentma"))))
                    .toUInt());
            tlvs.append(makeLeaf(AiotLink::kFctBattCurrent, u16be(ma)));
            tipParts << QStringLiteral("%1mA").arg(ma);
        }
        if (hasTemp) {
            const quint8 tc = static_cast<quint8>(
                map.value(QStringLiteral("temperatureC"),
                          map.value(QStringLiteral("temperature"),
                                    map.value(QStringLiteral("temp"),
                                              map.value(QStringLiteral("simbatterytemperaturec")))))
                    .toUInt());
            tlvs.append(makeLeaf(AiotLink::kFctBattTemperature, u8(tc)));
            tipParts << QStringLiteral("%1°C").arg(tc);
        }
        if (tlvs.isEmpty()) {
            qWarning() << "[QAIOT] SetBattery CID=0x13 缺少 percent/voltageMv/currentMa/temperatureC";
            break;
        }
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidVirtualBattery, tlvs,
                           tipParts.join(QStringLiteral(" ")));
        break;
    }
    case DeviceCmd::LightCalibWrite: {
        // CID=0x09：写传感器校准；IMU=36B float LE；电容/fsensor=1B 标志
        const quint8 sensorType = resolveSensorType(map, AiotLink::kFctSensorTypeInfrared);
        QString tipExtra;
        const QByteArray calib = buildSensorCalibPayload(sensorType, map, &tipExtra);
        if (sensorType == AiotLink::kFctSensorTypeImu && calib.size() != AiotLink::kFctImuCaliBytes) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 写IMU校准失败：data 须 36 字节(9×float)，当前=%1").arg(calib.size()));
            return;
        }
        const QList<TlvNode> tlvs = {
            makeParent(0x01,
                       {makeParent(0x02,
                                   {makeLeaf(0x03, u8(sensorType)), makeLeaf(0x04, calib)})}),
        };
        QString tip = QStringLiteral("写%1传感器校准").arg(fctSensorName(sensorType));
        if (!tipExtra.isEmpty())
            tip += QStringLiteral(" %1").arg(tipExtra);
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetSensor, tlvs, tip);
        break;
    }
    case DeviceCmd::ExceptionThresholdWrite: {
        // CID=0x0B：list/struct/type/threshold；掉电不消失
        bool typeOk = false;
        const quint8 exType = resolveExceptionType(map, &typeOk);
        if (!typeOk || exType == 0) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写异常阈值缺少 Param_type"));
            return;
        }
        QString tipExtra;
        QString err;
        const QByteArray thr = packExceptionThresholdValue(exType, map, &tipExtra, &err);
        if (thr.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 写异常阈值失败：%1").arg(err.isEmpty() ? QStringLiteral("无数据") : err));
            return;
        }
        const QList<TlvNode> tlvs = {
            makeParent(0x01,
                       {makeParent(0x02, {makeLeaf(0x03, u8(exType)), makeLeaf(0x04, thr)})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetExceptionThreshold, tlvs,
                           QStringLiteral("写%1 %2").arg(fctExceptionTypeName(exType), tipExtra));
        break;
    }
    case DeviceCmd::PumpParamWrite:
    case DeviceCmd::ValveParamWrite: {
        // CID=0x0F：泵/阀分命令；只打包对应字段（共用 param_struct）
        const bool isPump = (cmd == DeviceCmd::PumpParamWrite);
        auto pickU16 = [&](const QStringList& keys, bool* found) -> quint16 {
            for (const QString& k : keys) {
                if (map.contains(k)) {
                    if (found)
                        *found = true;
                    return static_cast<quint16>(map.value(k).toUInt());
                }
            }
            if (found)
                *found = false;
            return 0;
        };
        auto pickU8 = [&](const QStringList& keys, bool* found) -> quint8 {
            for (const QString& k : keys) {
                if (map.contains(k)) {
                    if (found)
                        *found = true;
                    return static_cast<quint8>(map.value(k).toUInt());
                }
            }
            if (found)
                *found = false;
            return 0;
        };

        QList<TlvNode> tlvs;
        QStringList tipParts;
        QList<TlvNode> children;

        if (isPump) {
            bool hasCircle = false, hasDur = false, hasInterval = false, hasPpwm = false;
            const quint16 circle = pickU16({QStringLiteral("circleNum"), QStringLiteral("pump_circle_num"),
                                            QStringLiteral("circle"), QStringLiteral("loops")},
                                           &hasCircle);
            const quint16 duration =
                pickU16({QStringLiteral("durationTime"), QStringLiteral("pump_duration_time"),
                         QStringLiteral("duration")},
                        &hasDur);
            const quint16 interval =
                pickU16({QStringLiteral("intervalTime"), QStringLiteral("pump_interval_time"),
                         QStringLiteral("interval")},
                        &hasInterval);
            const quint8 pumpPwm = pickU8(
                {QStringLiteral("pumpPwm"), QStringLiteral("pump_pwm_value"), QStringLiteral("pump_pwm")},
                &hasPpwm);
            if (hasPpwm && pumpPwm > 100) {
                emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写泵：PWM须 0~100"));
                return;
            }
            if (!hasCircle && !hasDur && !hasInterval && !hasPpwm) {
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 写泵缺少参数（circleNum/durationTime/intervalTime/pumpPwm）"));
                return;
            }
            if (hasCircle) {
                tlvs.append(makeLeaf(AiotLink::kFctPumpCircleNum, u16be(circle)));
                tipParts << QStringLiteral("循环=%1").arg(circle);
            }
            if (hasDur)
                children.append(makeLeaf(AiotLink::kFctPumpDurationTime, u16be(duration)));
            if (hasInterval)
                children.append(makeLeaf(AiotLink::kFctPumpIntervalTime, u16be(interval)));
            if (hasPpwm)
                children.append(makeLeaf(AiotLink::kFctPumpPwmValue, u8(pumpPwm)));
            if (!children.isEmpty())
                tipParts << QStringLiteral("泵时长=%1 间隔=%2 PWM=%3")
                                .arg(hasDur ? duration : 0)
                                .arg(hasInterval ? interval : 0)
                                .arg(hasPpwm ? pumpPwm : 0);
        } else {
            bool hasVen = false, hasVdis = false, hasVpwm = false;
            const quint16 valveEn =
                pickU16({QStringLiteral("valveEnableTime"), QStringLiteral("value_enable_time"),
                         QStringLiteral("valve_enable_time"), QStringLiteral("valueEnableTime"),
                         QStringLiteral("enableTime")},
                        &hasVen);
            const quint16 valveDis =
                pickU16({QStringLiteral("valveDisableTime"), QStringLiteral("value_disable_time"),
                         QStringLiteral("valve_disable_time"), QStringLiteral("valueDisableTime"),
                         QStringLiteral("disableTime")},
                        &hasVdis);
            const quint8 valvePwm =
                pickU8({QStringLiteral("valvePwm"), QStringLiteral("value_pwm_value"),
                        QStringLiteral("valve_pwm_value"), QStringLiteral("valve_pwm"),
                        QStringLiteral("valuePwm"), QStringLiteral("pwm")},
                       &hasVpwm);
            if (hasVpwm && valvePwm > 100) {
                emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写阀：PWM须 0~100"));
                return;
            }
            if (!hasVen && !hasVdis && !hasVpwm) {
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 写阀缺少参数（valveEnableTime/valveDisableTime/valvePwm）"));
                return;
            }
            if (hasVen)
                children.append(makeLeaf(AiotLink::kFctValveEnableTime, u16be(valveEn)));
            if (hasVdis)
                children.append(makeLeaf(AiotLink::kFctValveDisableTime, u16be(valveDis)));
            if (hasVpwm)
                children.append(makeLeaf(AiotLink::kFctValvePwmValue, u8(valvePwm)));
            tipParts << QStringLiteral("阀使能=%1 关闭=%2 PWM=%3")
                            .arg(hasVen ? valveEn : 0)
                            .arg(hasVdis ? valveDis : 0)
                            .arg(hasVpwm ? valvePwm : 0);
        }
        if (!children.isEmpty())
            tlvs.append(makeParent(AiotLink::kFctPumpParamStruct, children));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetPumpParam, tlvs,
                           QStringLiteral("%1 %2")
                               .arg(isPump ? QStringLiteral("写泵参数") : QStringLiteral("写阀参数"),
                                    tipParts.join(QLatin1Char(' '))));
        break;
    }
    case DeviceCmd::HeatTestWrite: {
        // CID=0x14：struct/enable/strength；duration 可选
        auto pickU8 = [&](const QStringList& keys, bool* found) -> quint8 {
            for (const QString& k : keys) {
                if (map.contains(k)) {
                    if (found)
                        *found = true;
                    return static_cast<quint8>(map.value(k).toUInt());
                }
            }
            if (found)
                *found = false;
            return 0;
        };
        bool hasEn = false, hasStr = false, hasDur = false;
        const quint8 enable =
            pickU8({QStringLiteral("enable"), QStringLiteral("heat_enable"), QStringLiteral("heatEnable")},
                   &hasEn);
        const quint8 strength = pickU8({QStringLiteral("driveStrength"), QStringLiteral("heat_drive_strength"),
                                        QStringLiteral("strength"), QStringLiteral("pwm")},
                                       &hasStr);
        quint16 duration = 0;
        for (const QString& k : {QStringLiteral("durationTime"), QStringLiteral("heat_duration_time"),
                                 QStringLiteral("duration")}) {
            if (map.contains(k)) {
                duration = static_cast<quint16>(map.value(k).toUInt());
                hasDur = true;
                break;
            }
        }
        if (!hasEn || !hasStr) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 加热测试缺少 Param_enable / Param_driveStrength"));
            return;
        }
        if (enable > 1) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 加热 enable 须为 0 或 1"));
            return;
        }
        QList<TlvNode> children = {
            makeLeaf(AiotLink::kFctHeatEnable, u8(enable)),
            makeLeaf(AiotLink::kFctHeatDriveStrength, u8(strength)),
        };
        if (hasDur)
            children.append(makeLeaf(AiotLink::kFctHeatDurationTime, u16be(duration)));
        const QList<TlvNode> tlvs = {makeParent(AiotLink::kFctHeatStatusStruct, children)};
        pendingHeat_ = {};
        pendingHeat_.enable = enable;
        pendingHeat_.driveStrength = strength;
        pendingHeat_.durationTime = duration;
        pendingHeat_.hasDuration = hasDur;
        hasPendingHeat_ = true;
        QString tip = QStringLiteral("加热%1 strength=%2")
                          .arg(enable ? QStringLiteral("开") : QStringLiteral("关"))
                          .arg(strength);
        if (hasDur)
            tip += QStringLiteral(" duration=%1").arg(duration);
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidHeatTest, tlvs, tip);
        break;
    }
    case DeviceCmd::VibrationTestWrite: {
        // CID=0x15：struct/enable/strength/freq/duration（规范均为 M）
        auto pickU8 = [&](const QStringList& keys, bool* found) -> quint8 {
            for (const QString& k : keys) {
                if (map.contains(k)) {
                    if (found)
                        *found = true;
                    return static_cast<quint8>(map.value(k).toUInt());
                }
            }
            if (found)
                *found = false;
            return 0;
        };
        bool hasEn = false, hasStr = false, hasFreq = false, hasDur = false;
        const quint8 enable = pickU8(
            {QStringLiteral("enable"), QStringLiteral("vibration_enable"), QStringLiteral("vibrationEnable")},
            &hasEn);
        const quint8 strength =
            pickU8({QStringLiteral("driveStrength"), QStringLiteral("vibration_drive_strength"),
                    QStringLiteral("strength"), QStringLiteral("pwm")},
                   &hasStr);
        const quint8 freq =
            pickU8({QStringLiteral("freq"), QStringLiteral("vibration_freq"), QStringLiteral("frequency")},
                   &hasFreq);
        quint16 duration = 0;
        for (const QString& k : {QStringLiteral("durationTime"), QStringLiteral("vibration_duration_time"),
                                 QStringLiteral("duration")}) {
            if (map.contains(k)) {
                duration = static_cast<quint16>(map.value(k).toUInt());
                hasDur = true;
                break;
            }
        }
        if (!hasEn || !hasStr || !hasFreq || !hasDur) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral(
                           "QAIOT 振动测试缺少 Param_enable/driveStrength/freq/durationTime"));
            return;
        }
        if (enable > 1) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 振动 enable 须为 0 或 1"));
            return;
        }
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctVibrationStatusStruct,
                       {makeLeaf(AiotLink::kFctVibrationEnable, u8(enable)),
                        makeLeaf(AiotLink::kFctVibrationDriveStrength, u8(strength)),
                        makeLeaf(AiotLink::kFctVibrationFreq, u8(freq)),
                        makeLeaf(AiotLink::kFctVibrationDurationTime, u16be(duration))}),
        };
        pendingVibration_ = {};
        pendingVibration_.enable = enable;
        pendingVibration_.driveStrength = strength;
        pendingVibration_.freq = freq;
        pendingVibration_.durationTime = duration;
        hasPendingVibration_ = true;
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidVibrationTest, tlvs,
                           QStringLiteral("振动%1 strength=%2 freq=%3 duration=%4")
                               .arg(enable ? QStringLiteral("开") : QStringLiteral("关"))
                               .arg(strength)
                               .arg(freq)
                               .arg(duration));
        break;
    }
    case DeviceCmd::CycleReportWrite: {
        // CID=0x18：enable + list/struct(type,interval)；interval 按 uint16 BE（规范 Length=1 视为笔误）
        bool hasEn = false;
        quint8 enable = 0;
        for (const QString& k : {QStringLiteral("enable"), QStringLiteral("cycle_report_enable"),
                                 QStringLiteral("cycleReportEnable")}) {
            if (map.contains(k)) {
                enable = static_cast<quint8>(map.value(k).toUInt());
                hasEn = true;
                break;
            }
        }
        if (!hasEn) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 循环上报缺少 Param_enable"));
            return;
        }
        if (enable > 1) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 循环上报 enable 须为 0 或 1"));
            return;
        }
        QString err;
        const QVector<ProtocolAiotCycleReportConfigItem> items = parseCycleReportConfigItems(map, &err);
        if (enable == 1 && items.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 开启循环上报缺少类型列表：%1")
                           .arg(err.isEmpty() ? QStringLiteral("Param_type/types/items") : err));
            return;
        }
        QList<TlvNode> structs;
        QStringList tipParts;
        for (const ProtocolAiotCycleReportConfigItem& it : items) {
            if (it.dataType > 0x0D) {
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 循环上报类型越界：0x%1").arg(it.dataType, 2, 16, QChar('0')));
                return;
            }
            structs.append(makeParent(
                AiotLink::kFctCycleReportConfigStruct,
                {makeLeaf(AiotLink::kFctCycleReportCfgDataType, u8(static_cast<quint8>(it.dataType))),
                 makeLeaf(AiotLink::kFctCycleReportIntervalTime, u16be(static_cast<quint16>(it.intervalTime)))}));
            tipParts << QStringLiteral("%1@%2ms")
                            .arg(fctSensorName(static_cast<quint8>(it.dataType)))
                            .arg(it.intervalTime);
        }
        const QList<TlvNode> tlvs = {
            makeLeaf(AiotLink::kFctCycleReportEnable, u8(enable)),
            makeParent(AiotLink::kFctCycleReportTypeList, structs),
        };
        pendingCycleReport_ = {};
        pendingCycleReport_.enable = enable;
        pendingCycleReport_.items = items;
        hasPendingCycleReport_ = true;
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetCycleReport, tlvs,
                           QStringLiteral("循环上报%1 [%2]")
                               .arg(enable ? QStringLiteral("开") : QStringLiteral("关"))
                               .arg(tipParts.isEmpty() ? QStringLiteral("-") : tipParts.join(QStringLiteral(", "))));
        break;
    }
    case DeviceCmd::ShipMode:
    case DeviceCmd::FactoryReset:
    case DeviceCmd::TravelLock:
    case DeviceCmd::DevReset: {
        // CID=0x0C：side(0x01) + control_type(0x02) + control_data(0x03，可空)
        // 0x01 恢复出厂 / 0x02 关机 / 0x03 旅行锁 / 0x04 重启
        quint8 ctrlType = AiotLink::kFctDutCtrlPowerOff;
        QString tip = QStringLiteral("关机");
        if (cmd == DeviceCmd::FactoryReset) {
            ctrlType = AiotLink::kFctDutCtrlFactoryReset;
            tip = QStringLiteral("恢复出厂");
        } else if (cmd == DeviceCmd::TravelLock) {
            ctrlType = AiotLink::kFctDutCtrlTravelLock;
            tip = QStringLiteral("旅行锁");
        } else if (cmd == DeviceCmd::DevReset) {
            ctrlType = AiotLink::kFctDutCtrlReset;
            tip = QStringLiteral("重启");
        }
        // 允许 Param_type / controlType 覆盖
        for (const QString& k : {QStringLiteral("type"), QStringLiteral("controlType"),
                                 QStringLiteral("dut_control_data_type")}) {
            if (map.contains(k)) {
                ctrlType = static_cast<quint8>(map.value(k).toUInt());
                break;
            }
        }
        if (ctrlType < 1 || ctrlType > 4) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 设备控制 type 须为 1恢复出厂/2关机/3旅行锁/4重启"));
            return;
        }
        if (ctrlType == AiotLink::kFctDutCtrlFactoryReset)
            tip = QStringLiteral("恢复出厂");
        else if (ctrlType == AiotLink::kFctDutCtrlPowerOff)
            tip = QStringLiteral("关机");
        else if (ctrlType == AiotLink::kFctDutCtrlTravelLock)
            tip = QStringLiteral("旅行锁");
        else
            tip = QStringLiteral("重启");

        QByteArray extra;
        for (const QString& k : {QStringLiteral("data"), QStringLiteral("dut_control_data"),
                                 QStringLiteral("controlData")}) {
            if (!map.contains(k))
                continue;
            const QVariant v = map.value(k);
            if (v.type() == QVariant::ByteArray)
                extra = v.toByteArray();
            else
                extra = QByteArray::fromHex(v.toString().remove(QLatin1Char(' ')).toLatin1());
            break;
        }
        const quint8 side = resolveDeviceSideId(map);
        const QList<TlvNode> tlvs = {
            makeLeaf(AiotLink::kFctDeviceSideId, u8(side)),
            makeLeaf(AiotLink::kFctDutControlDataType, u8(ctrlType)),
            makeLeaf(AiotLink::kFctDutControlData, extra),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidDeviceControl, tlvs,
                           QStringLiteral("设备控制-%1(side=%2)")
                               .arg(tip)
                               .arg(side));
        break;
    }
    default:
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 暂未映射 set 命令，cmd=%1").arg(static_cast<int>(cmd)));
        break;
    }
}

void Qaiot::get(DeviceCmd cmd, const QVariant& param) {
    const QVariantMap map = param.toMap();
    // GET：请求侧带 Type（Value 可空，Length=0）标明要查询的字段
    auto queryLeaf = [this](quint8 type) { return makeLeaf(type, {}); };

    switch (cmd) {
    case DeviceCmd::SoftVersionRead: {
        // CID=0x01：Param_field=soft_version|hw_version|res_version（默认固件）
        const QString field = map.value(QStringLiteral("field")).toString().trimmed().toLower();
        quint8 tlvType = AiotLink::kFctGetTlvFwVersion;
        QString tip = QStringLiteral("读固件版本");
        if (field == QLatin1String("hw_version") || field == QLatin1String("hw")) {
            tlvType = AiotLink::kFctGetTlvHwVersion;
            tip = QStringLiteral("读硬件版本");
        } else if (field == QLatin1String("res_version") || field == QLatin1String("res")) {
            tlvType = AiotLink::kFctGetTlvResVersion;
            tip = QStringLiteral("读资源版本");
        }
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus, {queryLeaf(tlvType)}, tip);
        break;
    }
    case DeviceCmd::BaseInfo:
    case DeviceCmd::DeviceInfo:
        // CID=0x01：只查设备名称 Type=0x01
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus,
                           {queryLeaf(AiotLink::kFctGetTlvDeviceName)}, QStringLiteral("读设备名称"));
        break;
    case DeviceCmd::MacRead: {
        // CID=0x03：读 device_mac_address（Type=0x05）
        const quint8 side = resolveDeviceSideId(map);
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidGetDeviceData,
            {makeLeaf(AiotLink::kFctDeviceSideId, u8(side)),
             makeParent(0x03, {makeParent(0x04, {makeLeaf(0x05, u8(AiotLink::kFctDataTypeMac))})})},
            QStringLiteral("读MAC side=%1").arg(deviceSideIdTip(side)));
        break;
    }
    case DeviceCmd::GetBattery: {
        // CID=0x0E：Param_field=percent|voltage|current|temperature（默认查四项）
        const QString field = map.value(QStringLiteral("field")).toString().trimmed().toLower();
        QList<TlvNode> tlvs;
        QString tip = QStringLiteral("读电量");
        if (field == QLatin1String("percent") || field == QLatin1String("battery_percent")) {
            tlvs = {queryLeaf(AiotLink::kFctBattPercent)};
            tip = QStringLiteral("读电池百分比");
        } else if (field == QLatin1String("voltage") || field == QLatin1String("voltageMv")
                   || field == QLatin1String("battery_voltage")) {
            tlvs = {queryLeaf(AiotLink::kFctBattVoltage)};
            tip = QStringLiteral("读电池电压");
        } else if (field == QLatin1String("current") || field == QLatin1String("currentMa")
                   || field == QLatin1String("battery_current")) {
            tlvs = {queryLeaf(AiotLink::kFctBattCurrent)};
            tip = QStringLiteral("读电池电流");
        } else if (field == QLatin1String("temperature") || field == QLatin1String("temperatureC")
                   || field == QLatin1String("battery_temperature")) {
            tlvs = {queryLeaf(AiotLink::kFctBattTemperature)};
            tip = QStringLiteral("读电池温度");
        } else {
            tlvs = {queryLeaf(AiotLink::kFctBattPercent), queryLeaf(AiotLink::kFctBattVoltage),
                    queryLeaf(AiotLink::kFctBattCurrent), queryLeaf(AiotLink::kFctBattTemperature)};
        }
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetBatteryInfo, tlvs, tip);
        break;
    }
    case DeviceCmd::FactoryDoneRead:
    case DeviceCmd::FacResult:
        // 只查产测完成标识 Type=0x04，不附带模式列表 0x20
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus,
                           {queryLeaf(AiotLink::kFctGetTlvFactoryComplete)}, QStringLiteral("读产测状态"));
        break;
    case DeviceCmd::TupleRead:
    case DeviceCmd::Sn: {
        // CID=0x03：device_side_id + data_type；TupleRead 可经 Param_dataType/Param_type 只查单项
        QList<TlvNode> items;
        auto appendDataType = [&](quint8 dataType) {
            items.append(makeParent(0x04, {makeLeaf(0x05, u8(dataType))}));
        };
        if (cmd == DeviceCmd::Sn) {
            appendDataType(AiotLink::kFctDataTypeSn);
        } else {
            quint8 onlyType = 0;
            if (map.contains(QStringLiteral("dataType")))
                onlyType = static_cast<quint8>(map.value(QStringLiteral("dataType")).toUInt());
            else if (map.contains(QStringLiteral("type")))
                onlyType = static_cast<quint8>(map.value(QStringLiteral("type")).toUInt());
            if (onlyType == AiotLink::kFctDataTypeProductId || onlyType == AiotLink::kFctDataTypeDeviceId ||
                onlyType == AiotLink::kFctDataTypeDeviceSecret) {
                appendDataType(onlyType);
            } else {
                appendDataType(AiotLink::kFctDataTypeProductId);
                appendDataType(AiotLink::kFctDataTypeDeviceId);
                appendDataType(AiotLink::kFctDataTypeDeviceSecret);
            }
        }
        const quint8 side = resolveDeviceSideId(map);
        const QList<TlvNode> tlvs = {
            makeLeaf(AiotLink::kFctDeviceSideId, u8(side)),
            makeParent(0x03, items),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetDeviceData, tlvs,
                           QStringLiteral("读设备数据/三元组 side=%1").arg(deviceSideIdTip(side)));
        break;
    }
    case DeviceCmd::RssiRead:
    case DeviceCmd::TrimRead: {
        // CID=0x06：rssi=0x01 / trim=0x02
        QList<TlvNode> tlvs;
        if (cmd == DeviceCmd::RssiRead)
            tlvs.append(queryLeaf(0x01));
        if (cmd == DeviceCmd::TrimRead)
            tlvs.append(queryLeaf(0x02));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetRfData, tlvs, QStringLiteral("读射频数据"));
        break;
    }
    case DeviceCmd::PeriphState:
    case DeviceCmd::LightSensorInfo:
    case DeviceCmd::GetImuCaliResult: {
        // CID=0x08：list/struct/sensor_type；Param_type=0x00~0x0D；GetImuCaliResult 默认 IMU
        quint8 sensorType = (cmd == DeviceCmd::GetImuCaliResult) ? AiotLink::kFctSensorTypeImu
                                                                  : AiotLink::kFctSensorTypeInfrared;
        sensorType = resolveSensorType(map, sensorType);
        if (cmd != DeviceCmd::GetImuCaliResult && map.isEmpty() && param.type() != QVariant::Map
            && param.canConvert<int>())
            sensorType = static_cast<quint8>(param.toInt());
        const QList<TlvNode> tlvs = {
            makeParent(0x01, {makeParent(0x02, {makeLeaf(0x03, u8(sensorType))})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetSensor, tlvs,
                           QStringLiteral("读%1传感器").arg(fctSensorName(sensorType)));
        break;
    }
    case DeviceCmd::ExceptionThresholdRead: {
        // CID=0x0A：与其它 GET 一致，struct 内带 type + 空 threshold(0x04) 作为查询字段
        QList<TlvNode> structs;
        bool typeOk = false;
        const quint8 one = resolveExceptionType(map, &typeOk);
        auto makeStruct = [&](quint8 exType) {
            return makeParent(0x02, {makeLeaf(0x03, u8(exType)), queryLeaf(0x04)});
        };
        if (typeOk && one != 0) {
            structs.append(makeStruct(one));
        } else {
            static const quint8 kAll[] = {
                AiotLink::kFctExTypeBatLowAlarm,         AiotLink::kFctExTypeBatLowShutdown,
                AiotLink::kFctExTypeChargeOvervolt,      AiotLink::kFctExTypeChargeTimeout,
                AiotLink::kFctExTypeBatTempAbnormal,     AiotLink::kFctExTypeMotorStallOvercurrent,
                AiotLink::kFctExTypeMotorOpenCircuit,    AiotLink::kFctExTypeNegPressureHigh,
            };
            for (quint8 t : kAll)
                structs.append(makeStruct(t));
        }
        const QList<TlvNode> tlvs = {makeParent(0x01, structs)};
        const QString tip = (typeOk && one != 0)
                                ? QStringLiteral("读%1阈值").arg(fctExceptionTypeName(one))
                                : QStringLiteral("读全部异常阈值");
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetExceptionThreshold, tlvs, tip);
        break;
    }
    case DeviceCmd::PumpParamRead: {
        // CID=0x10：只查泵相关字段
        const QList<TlvNode> tlvs = {
            queryLeaf(AiotLink::kFctPumpCircleNum),
            makeParent(AiotLink::kFctPumpParamStruct,
                       {queryLeaf(AiotLink::kFctPumpDurationTime), queryLeaf(AiotLink::kFctPumpIntervalTime),
                        queryLeaf(AiotLink::kFctPumpPwmValue)}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetPumpParam, tlvs,
                           QStringLiteral("读泵运行参数"));
        break;
    }
    case DeviceCmd::ValveParamRead: {
        // CID=0x10：只查阀相关字段
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctPumpParamStruct,
                       {queryLeaf(AiotLink::kFctValveEnableTime), queryLeaf(AiotLink::kFctValveDisableTime),
                        queryLeaf(AiotLink::kFctValvePwmValue)}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetPumpParam, tlvs,
                           QStringLiteral("读阀运行参数"));
        break;
    }
    case DeviceCmd::AgingStatusRead: {
        // 查询工厂模式使能；Param_mode 指定类型（默认老化）
        const quint8 modeType =
            static_cast<quint8>(map.value(QStringLiteral("mode"), AiotLink::kFctModeAging).toUInt());
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctGetTlvModeList,
                       {makeParent(AiotLink::kFctGetTlvModeStruct,
                                   {makeLeaf(AiotLink::kFctGetTlvModeType, u8(modeType)),
                                    queryLeaf(AiotLink::kFctGetTlvModeStatus)})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus, tlvs,
                           QStringLiteral("读%1状态").arg(fctModeName(modeType)));
        break;
    }
    default:
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 暂未映射 get 命令，cmd=%1").arg(static_cast<int>(cmd)));
        break;
    }
}

bool Qaiot::sendCustomMessage(const QVariantMap& map) {
    quint8 serviceId = AiotLink::kSvcFctAte;
    quint8 commandId = 0;
    if (!toByteValue(map.value(QStringLiteral("commandId")), &commandId)) {
        qWarning() << "QAIOT 通用发送缺少或非法 commandId";
        return false;
    }
    // Qaiot 固定 Service=0x04；若传入其它值则忽略并告警
    quint8 reqService = AiotLink::kSvcFctAte;
    if (toByteValue(map.value(QStringLiteral("serviceId")), &reqService) &&
        reqService != AiotLink::kSvcFctAte) {
        qWarning() << "QAIOT 仅支持 svc=0x04，已忽略 serviceId=" << reqService;
    }
    serviceId = AiotLink::kSvcFctAte;

    QList<TlvNode> tlvs;
    QString errorMessage;
    const QVariant tlvListValue = map.value(QStringLiteral("tlvs"));
    if (tlvListValue.isValid()) {
        const QVariantList list = tlvListValue.toList();
        for (const QVariant& item : list) {
            TlvNode node;
            if (!tlvFromVariant(item, &node, &errorMessage)) {
                qWarning() << "QAIOT TLV 参数非法:" << errorMessage;
                return false;
            }
            tlvs.append(node);
        }
    } else if (map.contains(QStringLiteral("type"))) {
        TlvNode node;
        if (!tlvFromVariant(map, &node, &errorMessage)) {
            qWarning() << "QAIOT TLV 参数非法:" << errorMessage;
            return false;
        }
        tlvs.append(node);
    }

    return sendServiceCommand(serviceId, commandId, tlvs, map.value(QStringLiteral("action")).toString());
}

bool Qaiot::sendServiceCommand(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs,
                               const QString& actionName) {
    Q_UNUSED(serviceId);
    // AIOT 上位机路径只走 FCT&ATE Service 0x04
    const quint8 svc = AiotLink::kSvcFctAte;
    pendingService_ = svc;
    pendingCommand_ = commandId;
    pendingAction_ = actionName.trimmed().isEmpty() ? fctCidName(commandId) : actionName.trimmed();
    const QByteArray app = buildMessage(svc, commandId, tlvs);
    emitReport(QStringLiteral("ProtocolPbDate"),
               QStringLiteral("QAIOT TX [%1] svc=0x%2 cid=0x%3")
                   .arg(pendingAction_)
                   .arg(svc, 2, 16, QChar('0'))
                   .arg(commandId, 2, 16, QChar('0')));
    return sendAppPdu(app);
}

bool Qaiot::sendAppPdu(const QByteArray& appPdu) {
    if (!serialPort || !serialPort->isOpen()) {
        qWarning() << "QAIOT 串口未打开，未发送数据";
        return false;
    }
    const QVector<QByteArray> linkFrames = AiotLinkCodec::buildFramesForPdu(appPdu);
    for (const QByteArray& link : linkFrames) {
        const QByteArray phy = wrapPhyPacket(link);
        if (phy.isEmpty())
            return false;
        const qint64 n = serialPort->write(phy);
        // 与 qroot 一致：完整 PHY（8×CC+Len+Ch+链路帧）不刷屏，只打内层链路帧 + 指令名
        qDebug().noquote() << QStringLiteral("[QAIOT] TX %1:").arg(pendingAction_) << hexText(link);
        if (n != phy.size()) {
            qWarning() << "QAIOT 发送不完整" << n << "/" << phy.size();
        return false;
        }
    }
    return true;
}

QByteArray Qaiot::wrapPhyPacket(const QByteArray& innerPacket) const {
    if (innerPacket.isEmpty() || innerPacket.size() > 0xFF)
        return {};
    QByteArray out;
    out.reserve(kPhyHeaderSize + 2 + innerPacket.size());
    out.append(QByteArray(kPhyHeaderSize, static_cast<char>(kPhyTxHeaderByte)));
    out.append(static_cast<char>(innerPacket.size()));
    out.append(static_cast<char>(kPhyChannelFac));
    out.append(innerPacket);
    return out;
}

bool Qaiot::tryUnwrapPhyPacket(const QByteArray& packet, QList<QByteArray>& outPackets) {
    const auto resetPhy = [this]() {
        phyState_ = PhyIdle;
        phyHeaderHits_ = 0;
        phyExpectedLen_ = 0;
        phyChannel_ = 0;
        phyPayload_.clear();
    };

    for (unsigned char x : packet) {
        switch (phyState_) {
        case PhyIdle:
            if (x == kPhyRxHeaderByte) {
                phyHeaderHits_ = 1;
                phyState_ = PhyHeader;
            }
            break;
        case PhyHeader:
            if (x == kPhyRxHeaderByte) {
                if (++phyHeaderHits_ == kPhyHeaderSize)
                    phyState_ = PhyChannel; // 收包：通道在前（见 dongle协议.md）
            } else {
                resetPhy();
            }
            break;
        case PhyChannel:
            phyChannel_ = x;
            phyState_ = PhyLen;
            break;
        case PhyLen:
            phyExpectedLen_ = static_cast<int>(x);
            if (phyExpectedLen_ <= 0) {
                qWarning() << "[QAIOT] dongle 外层包长度非法:" << phyExpectedLen_;
                resetPhy();
                break;
            }
            phyPayload_.clear();
            phyPayload_.reserve(phyExpectedLen_);
            phyState_ = PhyPayload;
            break;
        case PhyPayload:
            phyPayload_.append(static_cast<char>(x));
            if (phyPayload_.size() >= phyExpectedLen_) {
                if (phyChannel_ == kPhyChannelFac || phyChannel_ == 2 || phyChannel_ == 3)
                    outPackets.append(phyPayload_);
                else
                    qWarning() << "[QAIOT] dongle 通道异常 channel=" << phyChannel_;
                resetPhy();
            }
            break;
        default:
            resetPhy();
            break;
        }
    }
    return !outPackets.isEmpty();
}

Qaiot::TlvNode Qaiot::makeLeaf(quint8 type, const QByteArray& value) const {
    TlvNode n;
    n.type = type & 0x7F;
    n.hasChildren = false;
    n.value = value;
    return n;
}

Qaiot::TlvNode Qaiot::makeParent(quint8 type, const QList<TlvNode>& children) const {
    TlvNode n;
    n.type = type & 0x7F;
    n.hasChildren = true;
    n.children = children;
    return n;
}

QByteArray Qaiot::u8(quint8 v) const {
    return QByteArray(1, static_cast<char>(v));
}

QByteArray Qaiot::u16be(quint16 v) const {
    QByteArray b(2, Qt::Uninitialized);
    b[0] = static_cast<char>((v >> 8) & 0xFF);
    b[1] = static_cast<char>(v & 0xFF);
    return b;
}

QByteArray Qaiot::u32be(quint32 v) const {
    QByteArray b(4, Qt::Uninitialized);
    b[0] = static_cast<char>((v >> 24) & 0xFF);
    b[1] = static_cast<char>((v >> 16) & 0xFF);
    b[2] = static_cast<char>((v >> 8) & 0xFF);
    b[3] = static_cast<char>(v & 0xFF);
    return b;
}

bool Qaiot::findTlv(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const {
    for (const TlvNode& t : tlvs) {
        if (t.type == type) {
            if (out)
                *out = t;
            return true;
        }
    }
        return false;
    }

bool Qaiot::findTlvDeep(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const {
    if (findTlv(tlvs, type, out))
        return true;
    for (const TlvNode& t : tlvs) {
        if (t.hasChildren && findTlvDeep(t.children, type, out))
            return true;
    }
    return false;
}

bool Qaiot::parseMessage(const QByteArray& frame, Message* message, QString* errorMessage) const {
    if (!message)
        return false;
    if (frame.size() < 2) {
        if (errorMessage)
            *errorMessage = QStringLiteral("报文长度不足");
        return false;
    }
    message->serviceId = static_cast<quint8>(frame.at(0));
    message->commandId = static_cast<quint8>(frame.at(1));
    message->tlvs.clear();
    return parseTlvs(frame, 2, frame.size(), &message->tlvs, errorMessage);
}

bool Qaiot::parseTlvs(const QByteArray& data, int start, int end, QList<TlvNode>* out, QString* errorMessage) const {
    if (!out || start < 0 || end > data.size() || start > end) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV 范围非法");
        return false;
    }

    int pos = start;
    while (pos < end) {
        TlvNode node;
        node.rawType = static_cast<quint8>(data.at(pos++));
        node.hasChildren = (node.rawType & 0x80u) != 0;
        node.type = node.rawType & 0x7Fu;

        int length = 0;
        if (!readVarLength(data, &pos, end, &length, errorMessage))
            return false;
        if (pos + length > end) {
            if (errorMessage)
                *errorMessage = QStringLiteral("TLV 长度越界 type=%1 length=%2").arg(node.type).arg(length);
            return false;
        }

        if (node.hasChildren && length > 0) {
            if (!parseTlvs(data, pos, pos + length, &node.children, errorMessage))
                return false;
        } else {
            node.value = data.mid(pos, length);
        }
        pos += length;
        out->append(node);
    }
    return true;
}

bool Qaiot::readVarLength(const QByteArray& data, int* pos, int end, int* length, QString* errorMessage) const {
    if (!pos || !length || *pos >= end) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Length 字段缺失");
        return false;
    }

    const quint8 b0 = static_cast<quint8>(data.at((*pos)++));
    if ((b0 & 0x80u) == 0) {
        *length = b0;
        return true;
    }

    if (*pos >= end) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Length 第二字节缺失");
        return false;
    }
    const quint8 b1 = static_cast<quint8>(data.at((*pos)++));
    if ((b1 & 0x80u) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Length 超过当前支持的 2 字节范围");
        return false;
    }
    *length = ((b0 & 0x7F) << 7) | (b1 & 0x7F);
    return true;
}

QByteArray Qaiot::buildMessage(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs) const {
    QByteArray frame;
    frame.append(static_cast<char>(serviceId));
    frame.append(static_cast<char>(commandId));
    frame.append(buildTlvs(tlvs));
    return frame;
}

QByteArray Qaiot::buildTlvs(const QList<TlvNode>& tlvs) const {
    QByteArray out;
    for (const TlvNode& tlv : tlvs) {
        QByteArray payload = tlv.hasChildren ? buildTlvs(tlv.children) : tlv.value;
        quint8 rawType = tlv.type & 0x7Fu;
        if (tlv.hasChildren && !payload.isEmpty())
            rawType |= 0x80u;
        out.append(static_cast<char>(rawType));
        out.append(encodeVarLength(payload.size()));
        out.append(payload);
    }
    return out;
}

QByteArray Qaiot::encodeVarLength(int length) const {
    QByteArray out;
    if (length < 0 || length > 0x3FFF)
        return out;
    if (length <= 0x7F) {
        out.append(static_cast<char>(length & 0x7F));
        return out;
    }
    out.append(static_cast<char>(0x80u | ((length >> 7) & 0x7F)));
    out.append(static_cast<char>(length & 0x7F));
    return out;
}

bool Qaiot::tlvFromVariant(const QVariant& value, TlvNode* out, QString* errorMessage) const {
    if (!out)
        return false;
    const QVariantMap map = value.toMap();
    if (map.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV 需要 QVariantMap");
        return false;
    }

    quint8 type = 0;
    if (!toByteValue(map.value(QStringLiteral("type")), &type) || type > 0x7F) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV type 非法");
        return false;
    }
    out->type = type;
    out->children.clear();
    out->value.clear();

    if (map.contains(QStringLiteral("children"))) {
        out->hasChildren = true;
        const QVariantList children = map.value(QStringLiteral("children")).toList();
        for (const QVariant& childVar : children) {
            TlvNode child;
            if (!tlvFromVariant(childVar, &child, errorMessage))
                return false;
            out->children.append(child);
        }
        return true;
    }

    out->hasChildren = false;
    return valueFromVariant(map.value(QStringLiteral("value")), &out->value, errorMessage);
}

bool Qaiot::valueFromVariant(const QVariant& value, QByteArray* out, QString* errorMessage) const {
    if (!out)
        return false;
    if (!value.isValid() || value.isNull()) {
        out->clear();
        return true;
    }
    if (value.type() == QVariant::ByteArray) {
        *out = value.toByteArray();
        return true;
    }
    if (value.type() == QVariant::String) {
        const QString s = value.toString().trimmed();
        if (s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) || s.contains(QLatin1Char(' '))) {
            QByteArray hex = QByteArray::fromHex(s.toLatin1());
            *out = hex;
            return true;
        }
        *out = s.toUtf8();
            return true;
        }
    if (value.canConvert<int>()) {
        bool ok = false;
        const int n = value.toInt(&ok);
        if (!ok || n < 0 || n > 0xFF) {
            if (errorMessage)
                *errorMessage = QStringLiteral("value 整数需在 0~255");
            return false;
        }
        *out = QByteArray(1, static_cast<char>(n));
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("不支持的 value 类型");
    return false;
}

QString Qaiot::describeMessage(const Message& message) const {
    QStringList lines;
    lines << QStringLiteral("svc=0x%1 cid=0x%2(%3)")
                 .arg(message.serviceId, 2, 16, QChar('0'))
                 .arg(message.commandId, 2, 16, QChar('0'))
                 .arg(fctCidName(message.commandId));
    for (const TlvNode& tlv : message.tlvs)
        lines << describeTlv(tlv, 0, message.commandId);
    return lines.join(QStringLiteral(" | "));
}

QString Qaiot::describeTlv(const TlvNode& tlv, int depth, quint8 commandId) const {
    const QString pad = QString(depth * 2, QLatin1Char(' '));
    if (tlv.hasChildren) {
        QStringList parts;
        parts << QStringLiteral("%1TL(type=0x%2){").arg(pad).arg(tlv.type, 2, 16, QChar('0'));
        for (const TlvNode& c : tlv.children)
            parts << describeTlv(c, depth + 1, commandId);
        parts << QStringLiteral("%1}").arg(pad);
        return parts.join(QLatin1Char(' '));
    }
    // 仅 CID=0x03/0x04 的 Type=0x02 为 device_data_timestap
    quint32 ts = 0;
    if ((commandId == AiotLink::kFctCidGetDeviceData || commandId == AiotLink::kFctCidSetDeviceData)
        && tlv.type == AiotLink::kFctDeviceDataTimestamp && parseUtcTimestampBe4(tlv.value, &ts)) {
        return QStringLiteral("%1TLV(type=0x%2,len=%3,val=%4 时间戳=%5)")
            .arg(pad)
            .arg(tlv.type, 2, 16, QChar('0'))
            .arg(tlv.value.size())
            .arg(hexText(tlv.value), formatDeviceDataTimestamp(ts));
    }
    return QStringLiteral("%1TLV(type=0x%2,len=%3,val=%4)")
        .arg(pad)
        .arg(tlv.type, 2, 16, QChar('0'))
        .arg(tlv.value.size())
        .arg(hexText(tlv.value));
}
