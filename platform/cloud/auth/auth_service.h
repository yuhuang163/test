#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <QString>

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
    static bool isLoggedIn();
    static void logout();
};

#endif // AUTH_SERVICE_H
