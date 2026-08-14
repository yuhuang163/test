#include "test_case_ini_param.h"
#include "test_case.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QVariant readJsonMap(const QSettings& s, const QString& prefix) {
    QVariantMap map;
    const QStringList keys = s.allKeys();
    const QString p = prefix + QLatin1Char('/');
    for (const QString& key : keys) {
        if (!key.startsWith(p))
            continue;
        const QString sub = key.mid(p.size());
        if (sub.contains(QLatin1Char('/')))
            continue;
        map.insert(sub, s.value(key));
    }
    if (map.isEmpty()) {
        const QString json = s.value(prefix + QStringLiteral("/json")).toString();
        if (!json.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
            if (doc.isObject())
                return doc.object().toVariantMap();
        }
        return {};
    }
    return map;
}

void removeKeysWithPrefix(QSettings& s, const QString& prefix) {
    const QString p = prefix + QLatin1Char('/');
    for (const QString& key : s.allKeys()) {
        if (key == prefix || key.startsWith(p))
            s.remove(key);
    }
}

void writeJsonMap(QSettings& s, const QString& prefix, const QVariant& value) {
    if (prefix == sendParamIniPrefix()) {
        writeSendParamMap(s, value.toMap());
        return;
    }
    removeKeysWithPrefix(s, prefix);
    const QVariantMap map = value.toMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        s.setValue(prefix + QLatin1Char('/') + it.key(), it.value());
}

QString sendParamIniPrefix() {
    return QStringLiteral("Send/Param");
}

QString sendParamIniKey(const QString& leafKey) {
    return sendParamIniPrefix() + QLatin1Char('_') + leafKey;
}

void removeSendParamKeys(QSettings& s) {
    const QString underscorePrefix = sendParamIniPrefix() + QLatin1Char('_');
    const QString slashPrefix = sendParamIniPrefix() + QLatin1Char('/');
    for (const QString& key : s.allKeys()) {
        if (key.startsWith(underscorePrefix) || key.startsWith(slashPrefix))
            s.remove(key);
    }
    removeKeysWithPrefix(s, QStringLiteral("Param"));
}

void writeSendParamMap(QSettings& s, const QVariantMap& map) {
    removeSendParamKeys(s);
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        s.setValue(sendParamIniKey(it.key()), it.value());
}

void writeSendParamLeaf(QSettings& s, const QString& leafKey, const QVariant& value) {
    removeSendParamKeys(s);
    s.setValue(sendParamIniKey(leafKey), value);
}

QVariant normalizeScpiModbusParamFromMap(const QVariantMap& map) {
    if (map.isEmpty())
        return QVariant();
    // 只有 int/uint/value/string 这类叶子键才还原成标量；具名参数（如 comPort）即使只有一个
    // 也必须保持 map，否则单参数步骤会被折叠成裸值，UI 回显与执行按键名取参都拿不到
    if (map.size() == 1) {
        const QString leaf = map.constBegin().key();
        if (leaf == QLatin1String("int") || leaf == QLatin1String("uint") || leaf == QLatin1String("value")
            || leaf == QLatin1String("string"))
            return map.constBegin().value();
    }
    return map;
}

void writeScpiModbusParamToIni(QSettings& ini, const QVariant& param) {
    removeSendParamKeys(ini);
    if (!param.isValid())
        return;
    const QString prefix = sendParamIniPrefix();
    if (param.canConvert<QVariantMap>()) {
        writeJsonMap(ini, prefix, param);
        return;
    }
    if (param.userType() == QMetaType::QString)
        writeSendParamLeaf(ini, QStringLiteral("string"), param.toString());
    else
        writeSendParamLeaf(ini, QStringLiteral("int"), param.toInt());
}

QVariant readSendScopedParam(const QSettings& settings, const QString& leafKey, const QVariant& defaultValue) {
    const QString sendKey = sendParamIniKey(leafKey);
    if (settings.contains(sendKey))
        return settings.value(sendKey);
    return defaultValue;
}

QVariantMap readSendParamMap(const QSettings& settings) {
    QVariantMap map;
    const QString keyPrefix = sendParamIniPrefix() + QLatin1Char('_');
    for (const QString& key : settings.allKeys()) {
        if (!key.startsWith(keyPrefix))
            continue;
        const QString leaf = key.mid(keyPrefix.size());
        if (leaf.isEmpty() || leaf == QStringLiteral("json"))
            continue;
        map.insert(leaf, settings.value(key));
    }
    if (!map.isEmpty())
        return map;

    const QString json = settings.value(sendParamIniKey(QStringLiteral("json"))).toString();
    if (!json.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (doc.isObject())
            return doc.object().toVariantMap();
    }
    return map;
}

void mergeSendParamMapInto(QVariant& param, const QVariantMap& extra) {
    if (extra.isEmpty())
        return;
    QVariantMap merged;
    if (param.canConvert<QVariantMap>())
        merged = param.toMap();
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        merged.insert(it.key(), it.value());
    param = merged;
}

bool hookUsesGenericSendParamMap(const TestCaseDefinition& def) {
    if (!def.hook.enabled)
        return false;
    return def.hook.hookId.trimmed() == QLatin1String("COUNTDOWN_WAIT");
}

void writeGenericHookSendParamMap(QSettings& ini, const TestCaseDefinition& def) {
    if (!def.send.param.canConvert<QVariantMap>())
        return;
    const QVariantMap map = def.send.param.toMap();
    if (map.isEmpty())
        return;
    if (hookUsesGenericSendParamMap(def)) {
        writeSendParamMap(ini, map);
        return;
    }
    if (def.send.channel == TestCaseSendChannel::Product) {
        DeviceCmd cmd;
        if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd))
            writeSendParamMap(ini, map);
    }
}

int jsonMapIntValue(const QVariantMap& map, int defaultValue) {
    if (map.contains(QStringLiteral("int")))
        return map.value(QStringLiteral("int")).toInt();
    if (map.contains(QStringLiteral("value")))
        return map.value(QStringLiteral("value")).toInt();
    if (map.size() == 1)
        return map.constBegin().value().toInt();
    return defaultValue;
}

QVariantMap jsonMapWithLegacyInt(const QSettings& settings) {
    QVariantMap map = readSendParamMap(settings);
    if (!map.isEmpty())
        return map;
    const QVariant legacyInt = readSendScopedParam(settings, QStringLiteral("int"), QVariant());
    if (legacyInt.isValid())
        map.insert(QStringLiteral("value"), legacyInt);
    const QString legacyStr = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
    if (!legacyStr.isEmpty())
        map.insert(QStringLiteral("value"), legacyStr);
    return map;
}

bool overlayHasSendParamKeys(const QSettings& overlay) {
    if (!readSendParamMap(overlay).isEmpty())
        return true;
    if (overlay.contains(QStringLiteral("Send/Param")))
        return true;
    const QString prefix = sendParamIniPrefix() + QLatin1Char('_');
    for (const QString& key : overlay.allKeys()) {
        if (key.startsWith(prefix))
            return true;
    }
    return false;
}
