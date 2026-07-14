#include "auth_service.h"

#include "login_dialog.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QSysInfo>
#include <QWidget>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kOfflineToken = "offline-local-session";

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

void persistAuthRoles(const QStringList& roles) {
    if (roles.isEmpty()) {
        SETTINGS.remove(QStringLiteral("FactoryCloud/AuthRoles"));
    } else {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/AuthRoles"), roles.join(QLatin1Char(',')));
    }
}

bool AuthService::isOfflineBypassEnabled() {
    return SETTINGS.value(QStringLiteral("FactoryCloud/OfflineBypassEnabled"), false).toBool();
}

bool AuthService::isOfflineSession() {
    return FactoryCloudClient::accessToken() == QLatin1String(kOfflineToken);
}

AuthService::LoginResult AuthService::loginOffline(const QString& username, const QString& password) {
    LoginResult result;
    if (!isOfflineBypassEnabled()) {
        result.message = QStringLiteral(
            "未启用离线测试入口，请在上位机设置.ini（或 .local.ini）中设置 FactoryCloud/OfflineBypassEnabled=true");
        return result;
    }
    const QString user = username.trimmed();
    const QString expectUser =
        SETTINGS.value(QStringLiteral("FactoryCloud/OfflineBypassUser"), QStringLiteral("offline")).toString().trimmed();
    const QString expectPass =
        SETTINGS.value(QStringLiteral("FactoryCloud/OfflineBypassPassword"), QStringLiteral("offline26"))
            .toString();
    if (user.isEmpty() || password.isEmpty()) {
        result.message = QStringLiteral("请输入离线测试账号和密码");
        return result;
    }
    if (user != expectUser || password != expectPass) {
        result.message = QStringLiteral("离线测试账号或密码错误");
        return result;
    }

    SETTINGS.setValue(QStringLiteral("FactoryCloud/AuthUser"), user);
    SETTINGS.setValue(QStringLiteral("FactoryCloud/Token"), QLatin1String(kOfflineToken));
    SETTINGS.remove(QStringLiteral("FactoryCloud/TokenExpireAt"));
    result.roles = QStringList{QStringLiteral("operator")};
    persistAuthRoles(result.roles);
    SETTINGS.sync();

    result.ok = true;
    result.message = QStringLiteral("已进入离线测试模式（云端上传/同步不可用）");
    return result;
}

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
    result.roles = jsonStringList(api.data, "roles");
    result.stationKeys = jsonStringList(api.data, "stationKeys");
    persistAuthRoles(result.roles);
    SETTINGS.sync();

    result.ok = true;
    result.message = QStringLiteral("登录成功");
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
    if (token == QLatin1String(kOfflineToken)) {
        return isOfflineBypassEnabled();
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

bool AuthService::isSessionExpired() {
    const QString token = FactoryCloudClient::accessToken();
    if (token.isEmpty() || token == QLatin1String(kOfflineToken)) {
        return false;
    }
    const QString expireAt = SETTINGS.value(QStringLiteral("FactoryCloud/TokenExpireAt")).toString().trimmed();
    if (expireAt.isEmpty()) {
        return false;
    }
    const QDateTime exp = QDateTime::fromString(expireAt, Qt::ISODate);
    if (!exp.isValid()) {
        return false;
    }
    return exp <= QDateTime::currentDateTimeUtc();
}

bool AuthService::promptReLoginIfExpired(QWidget* parent) {
    if (isLoggedIn() || !isSessionExpired()) {
        return isLoggedIn();
    }

    static bool prompting = false;
    if (prompting) {
        return false;
    }

    const LoginResult autoResult = loginWithSavedCredentials();
    if (autoResult.ok || isLoggedIn()) {
        return true;
    }

    prompting = true;
    if (parent) {
        QMessageBox::warning(parent, QStringLiteral("登录过期"),
                             QStringLiteral("云平台登录已过期，请重新登录。未登录时测试数据将无法上传云端。"));
        LoginDialog loginDialog(parent);
        loginDialog.setWindowModality(Qt::ApplicationModal);
        loginDialog.exec();
    }
    prompting = false;
    return isLoggedIn();
}

QStringList AuthService::currentRoles() {
    QStringList roles;
    for (const QString& part :
         SETTINGS.value(QStringLiteral("FactoryCloud/AuthRoles")).toString().split(QLatin1Char(','),
                                                                                  QString::SkipEmptyParts)) {
        const QString role = part.trimmed();
        if (!role.isEmpty()) {
            roles << role;
        }
    }
    return roles;
}

bool AuthService::canOpenSettings() {
    if (!SETTINGS.value(QStringLiteral("SYSTEM/setting")).toInt()) {
        return false;
    }
    if (!isLoggedIn()) {
        return false;
    }
    const QStringList roles = currentRoles();
    for (const QString& role : roles) {
        if (role == QLatin1String("admin") || role == QLatin1String("engineer")) {
            return true;
        }
    }
    return false;
}

void AuthService::logout() {
    SETTINGS.remove(QStringLiteral("FactoryCloud/Token"));
    SETTINGS.remove(QStringLiteral("FactoryCloud/TokenExpireAt"));
    SETTINGS.remove(QStringLiteral("FactoryCloud/AuthRoles"));
    SETTINGS.sync();
}
