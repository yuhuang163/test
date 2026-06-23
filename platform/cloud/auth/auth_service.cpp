#include "auth_service.h"

#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSysInfo>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

QStringList jsonStringList(const QJsonObject& obj, const char* key) {
    QStringList out;
    const QJsonArray arr = obj.value(QString::fromUtf8(key)).toArray();
    for (const QJsonValue& v : arr) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty()) {
            out << s;
        }
    }
    return out;
}

} // namespace

AuthService::LoginResult AuthService::login(const QString& username, const QString& password) {
    LoginResult result;
    const QString user = username.trimmed();
    if (user.isEmpty() || password.isEmpty()) {
        result.message = QStringLiteral("用户名或密码为空");
        return result;
    }

    QJsonObject body;
    body.insert(QStringLiteral("username"), user);
    body.insert(QStringLiteral("password"), password);
    body.insert(QStringLiteral("hostName"), QSysInfo::machineHostName());
    body.insert(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());
    body.insert(QStringLiteral("stationKey"), FactoryCloudClient::stationKey());

    const FactoryCloudClient::ApiResult api = FactoryCloudClient::post(QStringLiteral("/auth/login"), body);
    if (!api.ok) {
        result.message = api.message.isEmpty() ? QStringLiteral("登录失败") : api.message;
        return result;
    }

    const QString token = api.data.value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty()) {
        result.message = QStringLiteral("登录响应缺少 accessToken");
        return result;
    }

    SETTINGS.setValue(QStringLiteral("FactoryCloud/AuthUser"), user);
    SETTINGS.setValue(QStringLiteral("FactoryCloud/Token"), token);
    const QString expireAt = api.data.value(QStringLiteral("expireAt")).toString();
    if (!expireAt.isEmpty()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/TokenExpireAt"), expireAt);
    }
    if (SETTINGS.value(QStringLiteral("FactoryCloud/RememberPassword"), false).toBool()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/AuthPassword"), password);
    } else {
        SETTINGS.remove(QStringLiteral("FactoryCloud/AuthPassword"));
    }
    SETTINGS.sync();

    result.ok = true;
    result.message = QStringLiteral("登录成功");
    result.roles = jsonStringList(api.data, "roles");
    result.stationKeys = jsonStringList(api.data, "stationKeys");
    return result;
}

AuthService::LoginResult AuthService::loginWithSavedCredentials() {
    LoginResult result;
    const QString user = SETTINGS.value(QStringLiteral("FactoryCloud/AuthUser")).toString().trimmed();
    const QString password = SETTINGS.value(QStringLiteral("FactoryCloud/AuthPassword")).toString();
    if (user.isEmpty() || password.isEmpty()) {
        result.message = QStringLiteral("未保存账号密码，请先在设置页登录");
        return result;
    }
    return login(user, password);
}

bool AuthService::isLoggedIn() {
    const QString token = FactoryCloudClient::accessToken();
    if (token.isEmpty()) {
        return false;
    }
    const QString expireAt = SETTINGS.value(QStringLiteral("FactoryCloud/TokenExpireAt")).toString().trimmed();
    if (expireAt.isEmpty()) {
        return true;
    }
    const QDateTime exp = QDateTime::fromString(expireAt, Qt::ISODate);
    if (!exp.isValid()) {
        return true;
    }
    return exp > QDateTime::currentDateTimeUtc();
}

void AuthService::logout() {
    SETTINGS.remove(QStringLiteral("FactoryCloud/Token"));
    SETTINGS.remove(QStringLiteral("FactoryCloud/TokenExpireAt"));
    SETTINGS.sync();
}
