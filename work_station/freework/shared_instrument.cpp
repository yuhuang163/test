#include "shared_instrument.h"

#include <QRegularExpression>
#include <QStringList>
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

bool sharingExplicitlyDisabled(const QVariantMap& paramMap) {
    if (!paramMap.contains(QStringLiteral("sharedPair"))
        && !paramMap.contains(QStringLiteral("shareInstrument"))) {
        return false;
    }
    return !paramTruthy(paramMap, QStringLiteral("sharedPair"))
           && !paramTruthy(paramMap, QStringLiteral("shareInstrument"));
}

QVector<int> parseChannelListText(const QString& text) {
    QVector<int> out;
    const QString t = text.trimmed();
    if (t.isEmpty())
        return out;
    if (t.contains(QLatin1Char('-')) && !t.contains(QLatin1Char(','))) {
        const QStringList parts = t.split(QLatin1Char('-'));
        if (parts.size() == 2) {
            bool ok1 = false;
            bool ok2 = false;
            const int a = parts[0].trimmed().toInt(&ok1);
            const int b = parts[1].trimmed().toInt(&ok2);
            if (ok1 && ok2 && a >= 1 && b >= a && b <= 64) {
                for (int i = a; i <= b; ++i)
                    out.append(i);
                return out;
            }
        }
    }
    const QStringList parts = t.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        const int ch = p.toInt(&ok);
        if (ok && ch >= 1 && ch <= 64)
            out.append(ch);
    }
    return out;
}

} // namespace

bool isVisaSharingEnabled(const QVariantMap& paramMap) {
    if (sharingExplicitlyDisabled(paramMap))
        return false;
    if (paramTruthy(paramMap, QStringLiteral("sharedPair"))
        || paramTruthy(paramMap, QStringLiteral("shareInstrument")))
        return true;
    const QString singleAddr = paramMap.value(QStringLiteral("visaAddress")).toString().trimmed();
    if (!singleAddr.isEmpty())
        return false;
    if (!visaAddressFromParam(paramMap, 0).isEmpty() || !visaAddressFromParam(paramMap, 1).isEmpty())
        return true;
    return false;
}

bool isEnabledInParam(const QVariantMap& paramMap) {
    if (sharingExplicitlyDisabled(paramMap))
        return false;
    if (paramTruthy(paramMap, QStringLiteral("sharedPair"))
        || paramTruthy(paramMap, QStringLiteral("shareInstrument")))
        return true;
    if (!visaAddressFromParam(paramMap, 0).isEmpty() || !visaAddressFromParam(paramMap, 1).isEmpty())
        return true;
    if (!tempComNameFromParam(paramMap, 0).isEmpty() || !tempComNameFromParam(paramMap, 1).isEmpty())
        return true;
    if (channelsPerStationFromParam(paramMap) > 1)
        return true;
    if (!parseChannelListText(paramMap.value(QStringLiteral("channels")).toString()).isEmpty())
        return true;
    return false;
}

int stationsPerDeviceFromParam(const QVariantMap& paramMap) {
    const int per = paramMap.value(QStringLiteral("stationsPerDevice"), 2).toInt();
    return qBound(1, per, 16);
}

int channelsPerStationFromParam(const QVariantMap& paramMap) {
    const int n = paramMap.value(QStringLiteral("channelsPerStation"), 1).toInt();
    return qBound(1, n, 64);
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

QVector<int> tempChannelListForStation(int stationIndex1Based, const QVariantMap& paramMap) {
    QVector<int> explicitList = parseChannelListText(paramMap.value(QStringLiteral("channels")).toString());
    if (!explicitList.isEmpty())
        return explicitList;

    const int perStation = channelsPerStationFromParam(paramMap);
    const int perDevice = stationsPerDeviceFromParam(paramMap);
    const int localIndex = (qMax(1, stationIndex1Based) - 1) % perDevice;
    const int start = localIndex * perStation + 1;
    QVector<int> list;
    list.reserve(perStation);
    for (int i = 0; i < perStation; ++i)
        list.append(start + i);
    return list;
}

bool applyVisaParamsForStation(int stationIndex1Based, QVariantMap* paramMap, QString* detailOut) {
    if (!paramMap)
        return false;

    const bool enabled = isVisaSharingEnabled(*paramMap);
    const int per = stationsPerDeviceFromParam(*paramMap);
    const Slot autoSlot = slotForStation(stationIndex1Based, per);
    int deviceIndex = autoSlot.deviceIndex;
    int channel = autoSlot.channel;

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
            *detailOut = QStringLiteral("步骤未配置 sharedPair/tempComName0/channelsPerStation，跳过温度仪共享映射");
        return false;
    }

    const int per = stationsPerDeviceFromParam(*paramMap);
    const int perStation = channelsPerStationFromParam(*paramMap);
    const Slot autoSlot = slotForStation(stationIndex1Based, per);
    int deviceIndex = autoSlot.deviceIndex;

    if (paramTruthy(*paramMap, QStringLiteral("tempDeviceIndexLock"))
        && paramMap->contains(QStringLiteral("tempDeviceIndex"))) {
        deviceIndex = qMax(0, paramMap->value(QStringLiteral("tempDeviceIndex")).toInt());
    }

    const QVector<int> channels = tempChannelListForStation(stationIndex1Based, *paramMap);
    if (!paramTruthy(*paramMap, QStringLiteral("channelLock"))) {
        if (!channels.isEmpty())
            paramMap->insert(QStringLiteral("channel"), channels.first());
        else
            paramMap->insert(QStringLiteral("channel"), autoSlot.channel);
    }

    QStringList chText;
    for (int ch : channels)
        chText.append(QString::number(ch));
    if (!chText.isEmpty())
        paramMap->insert(QStringLiteral("channels"), chText.join(QLatin1Char(',')));

    const QString sharedCom = tempComNameFromParam(*paramMap, deviceIndex);
    if (!sharedCom.isEmpty())
        paramMap->insert(QStringLiteral("sharedComName"), sharedCom);

    paramMap->insert(QStringLiteral("sharedPair"), true);
    paramMap->insert(QStringLiteral("stationsPerDevice"), per);
    paramMap->insert(QStringLiteral("channelsPerStation"), perStation);
    paramMap->insert(QStringLiteral("tempDeviceIndex"), deviceIndex);

    if (detailOut) {
        *detailOut = QStringLiteral("工位%1→温度仪设备%2 通道[%3]%4")
                         .arg(stationIndex1Based)
                         .arg(deviceIndex)
                         .arg(chText.join(QLatin1Char(',')))
                         .arg(sharedCom.isEmpty() ? QString() : QStringLiteral(" 串口=%1").arg(sharedCom));
    }
    return true;
}

} // namespace SharedInstrument
