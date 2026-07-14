#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <QString>

class QWidget;

/** 工厂云登录与会话 */
class AuthService {
  public:
    struct LoginResult {
        bool ok = false;
        QString message;
        QStringList roles;
        QStringList stationKeys;
    };

    static LoginResult login(const QString& username, const QString& password);
    static LoginResult loginWithSavedCredentials();
    /** 服务器不可用时，凭 FactoryCloud/OfflineBypass*（ini）本地进入；仅本地测试 */
    static LoginResult loginOffline(const QString& username, const QString& password);
    static bool isOfflineBypassEnabled();
    static bool isOfflineSession();
    static bool isLoggedIn();
    /** 本地 token 已过期（仍存有云端 Token，非离线会话） */
    static bool isSessionExpired();
    /** 会话过期时尝试记住密码自动登录，失败则弹窗要求重新登录 */
    static bool promptReLoginIfExpired(QWidget* parent);
    static QStringList currentRoles();
    /** admin / engineer（工艺）且 SYSTEM/setting 开启时可打开功能设置 */
    static bool canOpenSettings();
    static void logout();
};

#endif // AUTH_SERVICE_H
