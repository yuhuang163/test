#include "login_dialog.h"
#include "ui_login_dialog.h"

#include "auth_service.h"
#include "factory_cloud_env.h"

#include "my_set/my_typedef.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QIcon>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

/** 自绘眼睛图标：不依赖系统 emoji 字体（Windows 上 👁 常显示为空白）。 */
QIcon makePasswordEyeIcon(bool visible) {
    const int size = 18;
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor stroke(0x51, 0x48, 0x41); // 与 Ubuntu.qss 主文字色接近
    QPen pen(stroke, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // 眼眶
    QPainterPath eye;
    eye.moveTo(1.5, size / 2.0);
    eye.cubicTo(4.5, 3.5, size - 4.5, 3.5, size - 1.5, size / 2.0);
    eye.cubicTo(size - 4.5, size - 3.5, 4.5, size - 3.5, 1.5, size / 2.0);
    p.drawPath(eye);

    // 瞳孔
    p.setBrush(stroke);
    p.drawEllipse(QPointF(size / 2.0, size / 2.0), 2.4, 2.4);

    if (visible) {
        // 斜杠表示「当前已显示，点击隐藏」
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(3.5, size - 3.5), QPointF(size - 3.5, 3.5));
    }
    p.end();
    return QIcon(pm);
}

} // namespace

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog) {
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    applyWidgetStyleSheet(this);
    setupPasswordVisibilityToggle();

    connect(ui->loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(ui->offlineButton, &QPushButton::clicked, this, &LoginDialog::onOfflineClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(ui->usernameEdit, &QLineEdit::returnPressed, this, [this]() { ui->passwordEdit->setFocus(); });

    initFactoryCloudEnvironment();
    loadSavedCredentials();
    refreshOfflineButton();

    if (auto* screen = QApplication::primaryScreen()) {
        QRect sg = screen->availableGeometry();
        move((sg.width() - width()) / 2, (sg.height() - height()) / 2);
    }
}

LoginDialog::~LoginDialog() {
    delete ui;
}

void LoginDialog::setupPasswordVisibilityToggle() {
    auto* toggle = new QAction(ui->passwordEdit);
    toggle->setCheckable(true);
    toggle->setIcon(makePasswordEyeIcon(false));
    toggle->setToolTip(QStringLiteral("显示密码"));
    ui->passwordEdit->addAction(toggle, QLineEdit::TrailingPosition);
    connect(toggle, &QAction::toggled, this, [this, toggle](bool checked) {
        ui->passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        toggle->setIcon(makePasswordEyeIcon(checked));
        toggle->setToolTip(checked ? QStringLiteral("隐藏密码") : QStringLiteral("显示密码"));
    });
}

void LoginDialog::initFactoryCloudEnvironment() {
    FactoryCloudEnv::populateEnvironmentCombo(ui->environmentCombo);
    connect(ui->environmentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LoginDialog::onEnvironmentChanged);
    FactoryCloudEnv::loadToWidgets(ui->environmentCombo, ui->baseUrlEdit);
}

void LoginDialog::onEnvironmentChanged(int index) {
    Q_UNUSED(index);
    FactoryCloudEnv::applyEnvironmentSelection(ui->environmentCombo, ui->baseUrlEdit);
}

void LoginDialog::saveFactoryCloudEnvironment() {
    FactoryCloudEnv::saveFromWidgets(ui->environmentCombo, ui->baseUrlEdit);
}

void LoginDialog::loadSavedCredentials() {
    const QString user = SETTINGS.value(QStringLiteral("FactoryCloud/AuthUser")).toString();
    const QString password = SETTINGS.value(QStringLiteral("FactoryCloud/AuthPassword")).toString();
    const bool remember = SETTINGS.value(QStringLiteral("FactoryCloud/RememberPassword"), false).toBool();
    if (!user.isEmpty()) {
        ui->usernameEdit->setText(user);
    }
    ui->rememberCheck->setChecked(remember);
    if (remember && !password.isEmpty()) {
        ui->passwordEdit->setText(password);
        ui->passwordEdit->selectAll();
    }
}

void LoginDialog::saveCredentials() {
    SETTINGS.setValue(QStringLiteral("FactoryCloud/AuthUser"), ui->usernameEdit->text().trimmed());
    SETTINGS.setValue(QStringLiteral("FactoryCloud/RememberPassword"), ui->rememberCheck->isChecked());
    if (ui->rememberCheck->isChecked()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/AuthPassword"), ui->passwordEdit->text());
    } else {
        SETTINGS.remove(QStringLiteral("FactoryCloud/AuthPassword"));
    }
    SETTINGS.sync();
}

void LoginDialog::onLoginClicked() {
    const QString user = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();

    if (user.isEmpty() || password.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("请输入用户名和密码"));
        return;
    }

    ui->loginButton->setEnabled(false);
    ui->loginButton->setText(QStringLiteral("登录中…"));
    ui->statusLabel->setText(QString());
    QApplication::processEvents();

    saveFactoryCloudEnvironment();
    const AuthService::LoginResult result = AuthService::login(user, password);

    ui->loginButton->setEnabled(true);
    ui->loginButton->setText(QStringLiteral("登录"));

    if (result.ok) {
        saveCredentials();
        accept();
    } else {
        ui->statusLabel->setText(result.message);
        ui->passwordEdit->selectAll();
        ui->passwordEdit->setFocus();
    }
}

void LoginDialog::refreshOfflineButton() {
    const bool enabled = AuthService::isOfflineBypassEnabled();
    ui->offlineButton->setVisible(enabled);
    if (enabled) {
        const QString user =
            SETTINGS.value(QStringLiteral("FactoryCloud/OfflineBypassUser"), QStringLiteral("offline")).toString();
        ui->offlineButton->setToolTip(
            QStringLiteral("服务器不可用时使用离线账号进入，仅本地测试。默认账号 %1。").arg(user));
    }
}

void LoginDialog::onOfflineClicked() {
    const QString user = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();
    if (user.isEmpty() || password.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("请输入离线测试账号和密码"));
        return;
    }

    ui->offlineButton->setEnabled(false);
    ui->statusLabel->setText(QString());
    QApplication::processEvents();

    saveFactoryCloudEnvironment();
    const AuthService::LoginResult result = AuthService::loginOffline(user, password);
    ui->offlineButton->setEnabled(true);

    if (result.ok) {
        accept();
    } else {
        ui->statusLabel->setText(result.message);
    }
}
