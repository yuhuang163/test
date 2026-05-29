#include "test_step_descriptor.h"

#include <QFile>
#include <QJsonDocument>
#include <QDebug>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

// ---- ActionType 字符串映射 ----

QString TestStepDescriptor::actionTypeToString(ActionType type) {
    switch (type) {
        case ProtocolSet:    return QStringLiteral("ProtocolSet");
        case ProtocolGet:    return QStringLiteral("ProtocolGet");
        case AtCommand:      return QStringLiteral("AtCommand");
        case UsbCommand:     return QStringLiteral("UsbCommand");
        case CustomFunction: return QStringLiteral("CustomFunction");
    }
    return QStringLiteral("ProtocolSet");
}

TestStepDescriptor::ActionType TestStepDescriptor::actionTypeFromString(const QString& str) {
    if (str == QStringLiteral("ProtocolGet"))    return ProtocolGet;
    if (str == QStringLiteral("AtCommand"))      return AtCommand;
    if (str == QStringLiteral("UsbCommand"))     return UsbCommand;
    if (str == QStringLiteral("CustomFunction")) return CustomFunction;
    return ProtocolSet;
}

// ---- JSON 序列化 ----

QJsonObject TestStepDescriptor::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["category"] = category;
    obj["needCaseDone"] = needCaseDone;
    obj["mesTag"] = mesTag;
    obj["actionType"] = actionTypeToString(actionType);
    obj["deviceCmd"] = deviceCmd;
    obj["params"] = QJsonObject::fromVariantMap(params);
    obj["retryTimeoutMs"] = retryTimeoutMs;
    if (!customFunctionName.isEmpty()) {
        obj["customFunctionName"] = customFunctionName;
    }
    return obj;
}

TestStepDescriptor TestStepDescriptor::fromJson(const QJsonObject& obj) {
    TestStepDescriptor d;
    d.id = obj["id"].toInt(-1);
    d.name = obj["name"].toString();
    d.category = obj["category"].toString(QStringLiteral("product"));
    d.needCaseDone = obj["needCaseDone"].toBool(false);
    d.mesTag = obj["mesTag"].toString();
    d.actionType = actionTypeFromString(obj["actionType"].toString());
    d.deviceCmd = obj["deviceCmd"].toString();
    d.params = obj["params"].toObject().toVariantMap();
    d.retryTimeoutMs = obj["retryTimeoutMs"].toInt(300);
    d.customFunctionName = obj["customFunctionName"].toString();
    return d;
}

// ---- 整套配置序列化 ----

QJsonArray TestStepConfig::toJsonArray(const QVector<TestStepDescriptor>& steps) {
    QJsonArray arr;
    for (const auto& s : steps) {
        arr.append(s.toJson());
    }
    return arr;
}

QVector<TestStepDescriptor> TestStepConfig::fromJsonArray(const QJsonArray& arr) {
    QVector<TestStepDescriptor> steps;
    steps.reserve(arr.size());
    for (const auto& val : arr) {
        steps.append(TestStepDescriptor::fromJson(val.toObject()));
    }
    return steps;
}

bool TestStepConfig::saveToFile(const QString& filePath, const QVector<TestStepDescriptor>& steps) {
    QJsonObject root;
    root["version"] = 1;
    root["steps"] = toJsonArray(steps);

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[TestStepConfig] 无法写入文件:" << filePath << file.errorString();
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    qDebug() << "[TestStepConfig] 已保存" << steps.size() << "个步骤到" << filePath;
    return true;
}

QVector<TestStepDescriptor> TestStepConfig::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[TestStepConfig] 无法读取文件:" << filePath << file.errorString();
        return {};
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[TestStepConfig] JSON 解析错误:" << err.errorString();
        return {};
    }

    const QJsonObject root = doc.object();
    const QJsonArray arr = root["steps"].toArray();
    auto steps = fromJsonArray(arr);
    qDebug() << "[TestStepConfig] 从" << filePath << "加载了" << steps.size() << "个步骤";
    return steps;
}
