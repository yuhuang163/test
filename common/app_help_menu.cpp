#include "app_help_menu.h"

#include <QAction>
#include <QFutureWatcher>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QObject>
#include <QPair>
#include <QProgressDialog>
#include <QWidget>
#include <QtConcurrent>

#include "AbIni.h"
#include "login_dialog.h"
#include "log_upload_service.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static void startLogUpload(QWidget* parent) {
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

    auto* progress = new QProgressDialog(parent);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setWindowTitle(QStringLiteral("日志上传"));
    progress->setLabelText(QStringLiteral("正在打包日志，请稍候…"));
    progress->setCancelButton(nullptr);
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setRange(0, 0);
    progress->show();
    progress->raise();

    auto* watcher = new QFutureWatcher<QPair<bool, QString>>(parent);
    QObject::connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, parent, [parent, watcher, progress]() {
        if (progress) {
            progress->close();
        }
        const QPair<bool, QString> result = watcher->result();
        if (result.first) {
            QMessageBox::information(parent, QStringLiteral("日志上传"), result.second);
        } else {
            QMessageBox::warning(parent, QStringLiteral("日志上传"), result.second);
        }
        watcher->deleteLater();
    });

    const auto reportProgress = [progress](const QString& phase, qint64 current, qint64 total) {
        if (!progress) {
            return;
        }
        // 工作线程回调，切回 UI 线程更新进度条
        QMetaObject::invokeMethod(
            progress,
            [progress, phase, current, total]() {
                if (phase == QStringLiteral("pack")) {
                    progress->setRange(0, 0);
                    progress->setLabelText(QStringLiteral("正在打包日志，请稍候…"));
                    return;
                }
                if (total > 0) {
                    int pct = static_cast<int>((current * 100) / total);
                    if (pct < 0) {
                        pct = 0;
                    } else if (pct > 100) {
                        pct = 100;
                    }
                    progress->setRange(0, 100);
                    progress->setValue(pct);
                    progress->setLabelText(QStringLiteral("正在上传日志… %1%").arg(pct));
                } else {
                    progress->setRange(0, 0);
                    progress->setLabelText(QStringLiteral("正在上传日志…"));
                }
            },
            Qt::QueuedConnection);
    };

    watcher->setFuture(QtConcurrent::run([cfg, reportProgress]() {
        QString message;
        const bool ok = LogUploadService::packAndUpload(cfg, &message, reportProgress);
        return qMakePair(ok, message);
    }));
}

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

    const bool otaMenuVisible = SETTINGS.value(QStringLiteral("SYSTEM/ShowUpperComputerOTAFunc")).toBool();

    QAction* rollbackPrevious = helpMenu->addAction(QStringLiteral("回退上一版本..."));
    QObject::connect(rollbackPrevious, &QAction::triggered, dialogParent, [callbacks]() {
        if (callbacks.onRollbackPrevious) {
            callbacks.onRollbackPrevious();
        }
    });
    if (!otaMenuVisible) {
        rollbackPrevious->setVisible(false);
    }

    QAction* checkUpdate = helpMenu->addAction(QStringLiteral("检查更新..."));
    QObject::connect(checkUpdate, &QAction::triggered, dialogParent, [callbacks]() {
        if (callbacks.onCheckUpdate) {
            callbacks.onCheckUpdate();
        }
    });
    if (!otaMenuVisible) {
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
