#include "shared_instrument.h"

#include <QRegularExpression>
#include <QVector>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace SharedInstrument {
namespace {

bool looksLikeHuilingChannelCmd(const QString& text) {
    return text.contains(QStringLiteral("SOURce"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("OUTPut"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("MEASure"), Qt::CaseInsensitive);
}

QString rewriteHuilingChannelDigit(QString cmd, int channel) {
    if (cmd.isEmpty() || channel < 1)
        return cmd;
    static const QRegularExpression re(QStringLiteral(R"((SOURce|OUTPut|MEASure|SENS)(\d+))"),
                                       QRegularExpression::CaseInsensitiveOption);
    QString out = cmd;
    QRegularExpressionMatchIterator it = re.globalMatch(cmd);
    QVector<QPair<int, int>> spans;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        spans.append(qMakePair(m.capturedStart(2), m.capturedLength(2)));
    }
    for (int i = spans.size() - 1; i >= 0; --i)
        out.replace(spans[i].first, spans[i].second, QString::number(channel));
    return out;
}

void rewriteScpiCmdsInMap(QVariantMap* map, int channel) {
    if (!map || channel < 1)
        return;
    static const char* kKeys[] = {
        "scpiSetVoltageCmd", "scpiSetCurrentCmd", "scpiOutputOnCmd", "scpiOutputOffCmd",
        "scpiReadVoltageCmd", "scpiReadCurrentCmd", "scpiSetCurrentRangeCmd",
    };
    bool anyHuiling = false;
    for (const char* key : kKeys) {
        const QString k = QString::fromLatin1(key);
        if (!map->contains(k))
            continue;
        const QString raw = map->value(k).toString();
        if (looksLikeHuilingChannelCmd(raw))
            anyHuiling = true;
        map->insert(k, rewriteHuilingChannelDigit(raw, channel));
    }
    if (!anyHuiling) {
        QString selectCmd = map->value(QStringLiteral("scpiChannelSelectCmd")).toString().trimmed();
        if (selectCmd.isEmpty())
            selectCmd = QStringLiteral("INST OUT%1");
        map->insert(QStringLiteral("scpiChannelSelectCmd"), selectCmd);
    } else {
        map->remove(QStringLiteral("scpiChannelSelectCmd"));
    }
}

bool paramTruthy(const QVariantMap& map, const QString& key) {
    if (!map.contains(key))
        return false;
    const QVariant v = map.value(key);
    if (v.userType() == QMetaType::Bool)
        return v.toBool();
    const QString s = v.toString().trimmed().toLower();
    return s == QStringLiteral("1") || s == QStringLiteral("true") || s == QStringLiteral("yes")
           || s == QStringLiteral("on");
}

} // namespace

bool isEnabledInParam(const QVariantMap& paramMap) {
    if (paramTruthy(paramMap, QStringLiteral("sharedPair"))
        || paramTruthy(paramMap, QStringLiteral("shareInstrument")))
        return true;
    // 写了地址表/串口表也视为启用（方便只配 visaAddress0 不写开关）
    if (!visaAddressFromParam(paramMap, 0).isEmpty() || !visaAddressFromParam(paramMap, 1).isEmpty())
        return true;
    if (!tempComNameFromParam(paramMap, 0).isEmpty() || !tempComNameFromParam(paramMap, 1).isEmpty())
        return true;
    return false;
}

int stationsPerDeviceFromParam(const QVariantMap& paramMap) {
    const int per = paramMap.value(QStringLiteral("stationsPerDevice"), 2).toInt();
    return qBound(1, per, 16);
}

Slot slotForStation(int stationIndex1Based, int stationsPerDevice) {
    Slot slot;
    const int idx = qMax(1, stationIndex1Based);
    const int per = qBound(1, stationsPerDevice, 16);
    const int zero = idx - 1;
    slot.deviceIndex = zero / per;
    slot.channel = (zero % per) + 1;
    return slot;
}

QString visaAddressFromParam(const QVariantMap& paramMap, int deviceIndex0Based) {
    const int n = qMax(0, deviceIndex0Based);
    // 优先 visaAddress0 / visaAddress1；兼容 visaAddress_0
    QString addr = paramMap.value(QStringLiteral("visaAddress%1").arg(n)).toString().trimmed();
    if (addr.isEmpty())
        addr = paramMap.value(QStringLiteral("visaAddress_%1").arg(n)).toString().trimmed();
    return addr;
}

QString tempComNameFromParam(const QVariantMap& paramMap, int deviceIndex0Based) {
    const int n = qMax(0, deviceIndex0Based);
    QString com = paramMap.value(QStringLiteral("tempComName%1").arg(n)).toString().trimmed();
    if (com.isEmpty())
        com = paramMap.value(QStringLiteral("tempComName_%1").arg(n)).toString().trimmed();
    if (com.isEmpty())
        com = paramMap.value(QStringLiteral("sharedComName%1").arg(n)).toString().trimmed();
    return com;
}

bool applyVisaParamsForStation(int stationIndex1Based, QVariantMap* paramMap, QString* detailOut) {
    if (!paramMap)
        return false;

    const bool enabled = isEnabledInParam(*paramMap);
    const int per = stationsPerDeviceFromParam(*paramMap);
    const Slot autoSlot = slotForStation(stationIndex1Based, per);
    int deviceIndex = autoSlot.deviceIndex;
    int channel = autoSlot.channel;

    // 显式锁死设备号/通道（调试用）；sharedPair 开启时默认按工位重算，忽略缓存里的旧 powerChannel
    const bool deviceLocked = paramTruthy(*paramMap, QStringLiteral("visaDeviceIndexLock"))
                              && paramMap->contains(QStringLiteral("visaDeviceIndex"));
    const bool channelLocked = paramTruthy(*paramMap, QStringLiteral("powerChannelLock"))
                               && paramMap->contains(QStringLiteral("powerChannel"));
    if (deviceLocked)
        deviceIndex = qMax(0, paramMap->value(QStringLiteral("visaDeviceIndex")).toInt());
    if (channelLocked)
        channel = qMax(1, paramMap->value(QStringLiteral("powerChannel")).toInt());

    if (!enabled && !channelLocked && !paramMap->contains(QStringLiteral("powerChannel"))) {
        if (detailOut)
            *detailOut = QStringLiteral("步骤未配置 sharedPair/visaAddress0，跳过共享映射");
        return false;
    }
    if (!enabled && paramMap->contains(QStringLiteral("powerChannel")) && !channelLocked) {
        // 仅写了 powerChannel、未开共享：按显式通道改写 SCPI（单电源双通道手工指定）
        channel = qMax(1, paramMap->value(QStringLiteral("powerChannel")).toInt());
        paramMap->insert(QStringLiteral("powerChannel"), channel);
        rewriteScpiCmdsInMap(paramMap, channel);
        if (detailOut)
            *detailOut = QStringLiteral("工位%1 显式 powerChannel=%2").arg(stationIndex1Based).arg(channel);
        return true;
    }

    const QString tableAddr = visaAddressFromParam(*paramMap, deviceIndex);
    if (!tableAddr.isEmpty())
        paramMap->insert(QStringLiteral("visaAddress"), tableAddr);
    // 无地址表时保留步骤/缓存里的 Param_visaAddress（两工位共一台电源同一地址）

    paramMap->insert(QStringLiteral("sharedPair"), true);
    paramMap->insert(QStringLiteral("stationsPerDevice"), per);
    paramMap->insert(QStringLiteral("powerChannel"), channel);
    paramMap->insert(QStringLiteral("visaDeviceIndex"), deviceIndex);
    rewriteScpiCmdsInMap(paramMap, channel);

    if (detailOut) {
        const QString addr = paramMap->value(QStringLiteral("visaAddress")).toString().trimmed();
        *detailOut = QStringLiteral("工位%1→VISA设备%2 通道%3%4")
                         .arg(stationIndex1Based)
                         .arg(deviceIndex)
                         .arg(channel)
                         .arg(addr.isEmpty() ? QString() : QStringLiteral(" 地址=%1").arg(addr));
    }
    return true;
}

bool applyTempLoggerParamsForStation(int stationIndex1Based, QVariantMap* paramMap, QString* detailOut) {
    if (!paramMap)
        return false;
    if (!isEnabledInParam(*paramMap)) {
        if (detailOut)
            *detailOut = QStringLiteral("步骤未配置 sharedPair/tempComName0，跳过温度仪共享映射");
        return false;
    }

    const int per = stationsPerDeviceFromParam(*paramMap);
    const Slot autoSlot = slotForStation(stationIndex1Based, per);
    int deviceIndex = autoSlot.deviceIndex;
    int channel = autoSlot.channel;

    if (paramTruthy(*paramMap, QStringLiteral("tempDeviceIndexLock"))
        && paramMap->contains(QStringLiteral("tempDeviceIndex"))) {
        deviceIndex = qMax(0, paramMap->value(QStringLiteral("tempDeviceIndex")).toInt());
    }
    if (paramTruthy(*paramMap, QStringLiteral("channelLock")) && paramMap->contains(QStringLiteral("channel"))) {
        channel = qMax(1, paramMap->value(QStringLiteral("channel")).toInt());
    } else {
        paramMap->insert(QStringLiteral("channel"), channel);
    }

    const QString sharedCom = tempComNameFromParam(*paramMap, deviceIndex);
    if (!sharedCom.isEmpty())
        paramMap->insert(QStringLiteral("sharedComName"), sharedCom);

    paramMap->insert(QStringLiteral("sharedPair"), true);
    paramMap->insert(QStringLiteral("stationsPerDevice"), per);
    paramMap->insert(QStringLiteral("tempDeviceIndex"), deviceIndex);

    if (detailOut) {
        *detailOut = QStringLiteral("工位%1→温度仪设备%2 通道%3%4")
                         .arg(stationIndex1Based)
                         .arg(deviceIndex)
                         .arg(channel)
                         .arg(sharedCom.isEmpty() ? QString() : QStringLiteral(" 串口=%1").arg(sharedCom));
    }
    return true;
}

} // namespace SharedInstrument
