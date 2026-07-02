#ifndef LOGIN_DIALOG_H
#define LOGIN_DIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog {
    Q_OBJECT
  public:
    explicit LoginDialog(QWidget* parent = nullptr);
    ~LoginDialog();

  private slots:
    void onLoginClicked();
    void onOfflineClicked();
    void onEnvironmentChanged(int index);

  private:
    void loadSavedCredentials();
    void saveCredentials();
    void saveFactoryCloudEnvironment();
    void initFactoryCloudEnvironment();
    void refreshOfflineButton();
    Ui::LoginDialog* ui;
};

#endif // LOGIN_DIALOG_H
