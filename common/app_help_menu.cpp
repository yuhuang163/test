#include "app_help_menu.h"

#include <QAction>
#include <QFutureWatcher>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QPair>
#include <QWidget>
#include <QtConcurrent>

#include "AbIni.h"
#include "login_dialog.h"
#include "log_upload_service.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

void startLogUpload(QWidget* parent) {
    if (!LogUploadService::isUploadEnabled()) {
        QMessageBox::information(parent, QStringLiteral("日志上传"), QStringLiteral("当前不可上传日志"));
        return;
    }

    QString configError;
    LogUploadService::UploadConfig cfg;
    if (!LogUploadService::configFromSettings(&cfg, &configError)) {
        QMessageBox::warning(parent, QStringLiteral("日志上传"), configError);
        return;
    }

    auto* watcher = new QFutureWatcher<QPair<bool, QString>>(parent);
    QObject::connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, parent, [parent, watcher]() {
        const QPair<bool, QString> result = watcher->result();
        if (result.first) {
            QMessageBox::information(parent, QStringLiteral("日志上传"), result.second);
        } else {
            QMessageBox::warning(parent, QStringLiteral("日志上传"), result.second);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([cfg]() {
        QString message;
        const bool ok = LogUploadService::packAndUpload(cfg, &message);
        return qMakePair(ok, message);
    }));
}

} // namespace

QMenu* AppHelpMenu::install(QMenuBar* menuBar, QWidget* dialogParent, const HostCallbacks& callbacks) {
    if (!menuBar || !dialogParent) {
        return nullptr;
    }

    QMenu* helpMenu = menuBar->addMenu(QStringLiteral("帮助"));

    QAction* switchAccount = helpMenu->addAction(QStringLiteral("切换账号"));
    QObject::connect(switchAccount, &QAction::triggered, dialogParent, [dialogParent, callbacks]() {
        LoginDialog loginDialog(dialogParent);
        loginDialog.setWindowModality(Qt::ApplicationModal);
        // 取消或叉掉对话框时保持原登录态，勿先 logout
        if (loginDialog.exec() == QDialog::Accepted && callbacks.onAccountSwitched) {
            callbacks.onAccountSwitched();
        }
    });

    helpMenu->addSeparator();

    QAction* uploadLog = helpMenu->addAction(QStringLiteral("上传日志"));
    QObject::connect(uploadLog, &QAction::triggered, dialogParent,
                     [dialogParent]() { startLogUpload(dialogParent); });

    helpMenu->addSeparator();

    QAction* checkUpdate = helpMenu->addAction(QStringLiteral("检查更新..."));
    QObject::connect(checkUpdate, &QAction::triggered, dialogParent, [callbacks]() {
        if (callbacks.onCheckUpdate) {
            callbacks.onCheckUpdate();
        }
    });
    if (!SETTINGS.value(QStringLiteral("SYSTEM/ShowUpperComputerOTAFunc")).toBool()) {
        checkUpdate->setVisible(false);
    }

    return helpMenu;
}

void AppHelpMenu::ensureRightmost(QMenuBar* menuBar, QMenu* helpMenu) {
    if (!menuBar || !helpMenu) {
        return;
    }
    QAction* helpAction = helpMenu->menuAction();
    menuBar->removeAction(helpAction);
    menuBar->addAction(helpAction);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
