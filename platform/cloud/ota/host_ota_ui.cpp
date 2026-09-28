#include "host_ota_ui.h"

#include "auth_service.h"
#include "factory_cloud_client.h"
#include "host_ota_apply.h"
#include "host_ota_service.h"
#include "my_set/my_typedef.h"
#include "ui_host_ota_ui.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QUrlQuery>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static QString promptUploadReleaseNotes(QWidget* parent) {
    for (;;) {
        bool ok = false;
        const QString notes = QInputDialog::getMultiLineText(
            parent, QStringLiteral("上传上位机"), QStringLiteral("请填写本次修改内容："), QString(), &ok);
        if (!ok) {
            return QString();
        }
        const QString trimmed = notes.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("上传上位机"), QStringLiteral("修改内容不能为空，请填写后再上传。"));
        }
    }
}

static bool confirmDownloadWithReleaseNotes(QWidget* parent, const HostOtaService::CheckResult& info) {
    if (!parent) {
        return true;
    }
    const QString notes = info.releaseNotes.trimmed();
    const QString notesText =
        notes.isEmpty() ? QStringLiteral("（上传时未填写修改说明）") : notes;
    const QString text = QStringLiteral("版本：%1\nbuildId：%2\n\n修改内容：\n%3\n\n是否下载并安装？")
                             .arg(info.appVersion, info.buildId, notesText);
    return QMessageBox::question(parent, QStringLiteral("下载上位机"), text, QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

void HostOtaUi::showRestartCountdown(QWidget* parent) {
    if (!parent) {
        return;
    }
    QMessageBox* msgBox = new QMessageBox(parent);
    msgBox->setWindowTitle(QStringLiteral("软件更新"));
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setDefaultButton(QMessageBox::Ok);
    // 点确定立即继续；未点则倒计时结束后自动关闭
    int remainSec = 1;
    auto refreshText = [msgBox, &remainSec]() {
        msgBox->setText(QStringLiteral("下载完成，即将重启并安装新版本。\n\n"
                                       "点击「确定」立即重启；%1 秒后自动关闭。")
                            .arg(remainSec));
    };
    refreshText();
    QTimer* countdown = new QTimer(msgBox);
    countdown->setInterval(1000);
    QObject::connect(countdown, &QTimer::timeout, msgBox, [msgBox, countdown, &remainSec, refreshText]() {
        --remainSec;
        if (remainSec <= 0) {
            countdown->stop();
            msgBox->accept();
            return;
        }
        refreshText();
    });
    countdown->start();
    msgBox->exec();
    countdown->stop();
    msgBox->deleteLater();
}

bool HostOtaUi::uploadCurrentExe(QString* message, const QString& releaseNotes) {
    if (AuthService::isOfflineSession()) {
        if (message) {
            *message = QStringLiteral("离线测试模式，无法上传 exe");
        }
        return false;
    }
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            if (message) {
                *message = login.message;
            }
            qDebug() << "[OTA] 上传失败: 登录失败" << (message ? *message : "");
            return false;
        }
    }
    // 与服务端 POST /host-app/upload 一致：仅 admin 可上传当前上位机版本
    if (!AuthService::isAdmin()) {
        if (message) {
            *message = QStringLiteral("仅管理员可上传当前上位机版本");
        }
        qDebug() << "[OTA] 上传失败: 非 admin";
        return false;
    }

    const QString exePath = QCoreApplication::applicationFilePath();
    if (!QFile::exists(exePath)) {
        if (message) {
            *message = QStringLiteral("当前 exe 不存在");
        }
        qDebug() << "[OTA] 上传失败: exe 不存在" << exePath;
        return false;
    }

    const QString sha256 = HostOtaApply::sha256File(exePath);
    const QString appVer = FactoryCloudClient::appVersion();
    const QString bid = FactoryCloudClient::buildId();
    const QString pkg = FactoryCloudClient::packageName();
    qDebug() << "[OTA] 上传 exe:" << exePath << "appVersion=" << appVer << "buildId=" << bid << "packageName=" << pkg
             << "releaseNotes=" << releaseNotes.trimmed();

    QList<QPair<QString, QString>> fields;
    fields.append({QStringLiteral("appVersion"), appVer});
    fields.append({QStringLiteral("buildId"), bid});
    fields.append({QStringLiteral("packageName"), pkg});
    fields.append({QStringLiteral("releaseNotes"), releaseNotes.trimmed()});
    if (!sha256.isEmpty()) {
        fields.append({QStringLiteral("sha256"), sha256});
    }

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::uploadExe(exePath, fields);
    if (!api.ok) {
        if (message) {
            *message = api.message;
        }
        qDebug() << "[OTA] 上传失败:" << (message ? *message : "");
        return false;
    }
    if (message) {
        *message = QStringLiteral("上位机版本已上报（%1 buildId=%2）").arg(appVer, bid);
    }
    // 本机当前版本即刚上传的包，同步落盘更新说明供测试结果上报
    HostOtaService::saveLocalReleaseNotes(appVer, releaseNotes);
    qDebug() << "[OTA] 上传成功:" << appVer << bid;
    return true;
}

bool HostOtaUi::showVersionPicker(QWidget* parent,
                                  const std::function<void(const QString&)>& logFn) {
    auto log = [&](const QString& text) {
        if (logFn) {
            logFn(text);
        }
    };

    if (AuthService::isOfflineSession()) {
        const QString msg = QStringLiteral("离线测试模式，无法检查更新");
        if (parent) {
            QMessageBox::information(parent, QStringLiteral("检查更新"), msg);
        }
        log(msg);
        return false;
    }
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            if (parent) {
                QMessageBox::warning(parent, QStringLiteral("检查更新"), login.message);
            }
            log(login.message);
            return false;
        }
    }

    QUrlQuery versionsQuery;
    versionsQuery.addQueryItem(QStringLiteral("packageName"), FactoryCloudClient::packageName());
    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::get(QStringLiteral("/host-app/versions"), versionsQuery);
    if (!api.ok) {
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("检查更新"), api.message);
        }
        log(api.message);
        return false;
    }

    const QJsonArray items = api.data.value(QStringLiteral("items")).toArray();
    if (items.isEmpty()) {
        if (parent) {
            QMessageBox::information(parent, QStringLiteral("检查更新"),
                                     QStringLiteral("服务器上暂无可用版本"));
        }
        return true;
    }

    QDialog dialog(parent);
    Ui::HostOtaVersionDialog ui;
    ui.setupUi(&dialog);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    applyWidgetStyleSheet(&dialog);
    const QString localPkg = FactoryCloudClient::packageName();
    int visibleCount = 0;
    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject v = items.at(i).toObject();
        const QString itemPkg = v.value(QStringLiteral("packageName")).toString().trimmed();
        if (!itemPkg.isEmpty() && itemPkg.compare(localPkg, Qt::CaseInsensitive) != 0) {
            continue;
        }
        const QString ver = v.value(QStringLiteral("appVersion")).toString();
        const QString bid = v.value(QStringLiteral("buildId")).toString();
        const QString time = v.value(QStringLiteral("uploadedAt")).toString();
        const QString notes = v.value(QStringLiteral("releaseNotes")).toString().trimmed();
        QString label = QStringLiteral("%1 (buildId=%2)").arg(ver, bid);
        if (!time.isEmpty()) {
            label += QStringLiteral("  %1").arg(time);
        }
        auto* item = new QListWidgetItem(label, ui.versionList);
        item->setToolTip(notes.isEmpty() ? QStringLiteral("（上传时未填写修改说明）") : notes);
        item->setData(Qt::UserRole, i);
        ++visibleCount;
    }
    if (visibleCount <= 0) {
        if (parent) {
            QMessageBox::information(parent, QStringLiteral("检查更新"),
                                     QStringLiteral("服务器上没有与当前 packageName 匹配的版本"));
        }
        return true;
    }
    ui.versionList->setCurrentRow(0);
    QObject::connect(ui.buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(ui.buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(ui.versionList, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) {
        return true;
    }
    const int row = ui.versionList->currentRow();
    if (row < 0 || row >= ui.versionList->count()) {
        return true;
    }
    const int idx = ui.versionList->item(row)->data(Qt::UserRole).toInt();
    const QJsonObject chosen = items.at(idx).toObject();
    HostOtaService::CheckResult info;
    info.hasUpdate = true;
    info.appVersion = chosen.value(QStringLiteral("appVersion")).toString();
    info.buildId = chosen.value(QStringLiteral("buildId")).toString();
    info.sha256 = chosen.value(QStringLiteral("sha256")).toString();
    info.forceUpgrade = chosen.value(QStringLiteral("forceUpgrade")).toBool();
    info.releaseNotes = chosen.value(QStringLiteral("releaseNotes")).toString();
    info.packageName = chosen.value(QStringLiteral("packageName")).toString();
    info.packageKind = chosen.value(QStringLiteral("packageKind")).toString();
    info.fileCount = chosen.value(QStringLiteral("fileCount")).toInt();
    info.uploadedAt = chosen.value(QStringLiteral("uploadedAt")).toString();

    if (!confirmDownloadWithReleaseNotes(parent, info)) {
        log(QStringLiteral("[OTA] 用户取消下载"));
        return true;
    }

    QString message;
    if (!HostOtaApply::downloadAndApply(info, parent, &message)) {
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("软件更新"),
                                 message.isEmpty() ? QStringLiteral("升级失败") : message);
        }
        if (!message.isEmpty()) {
            log(message);
        }
        return true;
    }
    if (!message.isEmpty()) {
        log(message);
    }
    return true;
}

bool HostOtaUi::tryInteractiveUpdate(QWidget* parent,
                                     const std::function<void(const QString&)>& logFn) {
    const QString baseUrl = FactoryCloudClient::baseUrl();
    qDebug() << "[OTA] tryInteractiveUpdate baseUrl=" << baseUrl;
    if (baseUrl.isEmpty()) {
        qDebug() << "[OTA] OTA 未配置，跳过";
        return false;
    }

    auto log = [&](const QString& text) {
        if (logFn) {
            logFn(text);
        }
    };

    log(QStringLiteral("[OTA] 正在检查更新…"));
    const HostOtaService::CheckResult check = HostOtaService::checkUpdate();
    if (!check.ok) {
        log(QStringLiteral("[OTA] 检查失败: ") + check.message);
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("检查更新"), check.message);
        }
        return false;
    }

    if (check.hasUpdate) {
        if (check.forceUpgrade) {
            log(QStringLiteral("[OTA] 服务器要求强制升级到 %1 (buildId=%2)")
                    .arg(check.appVersion, check.buildId));
            if (parent) {
                const QString notes = check.releaseNotes.trimmed();
                QMessageBox::information(
                    parent, QStringLiteral("软件更新"),
                    QStringLiteral("服务器要求强制升级到 %1（buildId=%2）。\n\n修改内容：\n%3")
                        .arg(check.appVersion, check.buildId,
                             notes.isEmpty() ? QStringLiteral("（上传时未填写修改说明）") : notes));
            }
            QString message;
            if (!HostOtaApply::downloadAndApply(check, parent, &message)) {
                if (parent) {
                    QMessageBox::warning(parent, QStringLiteral("软件更新"),
                                         message.isEmpty() ? QStringLiteral("强制升级失败") : message);
                }
                if (!message.isEmpty()) {
                    log(message);
                }
                return false;
            }
            if (!message.isEmpty()) {
                log(message);
            }
            return true;
        }
        log(QStringLiteral("[OTA] 发现服务器有新版本，弹出版本选择列表"));
        return showVersionPicker(parent, logFn);
    }

    // 已是最新或本地更新：admin 可上传当前包；均可从服务器选择任意历史版本下载（含降级）
    const QString localVer = FactoryCloudClient::appVersion();
    const QString localBid = FactoryCloudClient::buildId();
    const QString statusText =
        check.hostNewer
            ? QStringLiteral("本地版本（%1，buildId=%2）比服务器新。").arg(localVer, localBid)
            : QStringLiteral("当前已是最新版本（%1，buildId=%2）。").arg(localVer, localBid);
    log(QStringLiteral("[OTA] ") + check.message);

    if (!parent) {
        log(QStringLiteral("[OTA] 无界面，跳过"));
        return true;
    }

    const bool canUpload = AuthService::isAdmin();
    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("检查更新"));
    box.setText(statusText + QStringLiteral("\n\n请选择操作："));
    QPushButton* uploadBtn =
        canUpload ? box.addButton(QStringLiteral("上传当前版本"), QMessageBox::ActionRole) : nullptr;
    QPushButton* downloadBtn = box.addButton(QStringLiteral("下载其他版本"), QMessageBox::ActionRole);
    QPushButton* cancelBtn = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(cancelBtn);
    box.exec();

    if (box.clickedButton() == cancelBtn || box.clickedButton() == nullptr) {
        log(QStringLiteral("[OTA] 用户取消"));
        return true;
    }
    if (box.clickedButton() == downloadBtn) {
        log(QStringLiteral("[OTA] 用户选择下载其他版本"));
        return showVersionPicker(parent, logFn);
    }
    if (!canUpload || box.clickedButton() != uploadBtn) {
        log(QStringLiteral("[OTA] 未选择上传（非管理员无上传入口）"));
        return true;
    }

    const QString releaseNotes = promptUploadReleaseNotes(parent);
    if (releaseNotes.isEmpty()) {
        log(QStringLiteral("[OTA] 用户取消上传或未填写修改内容"));
        return true;
    }

    QString uploadMsg;
    const bool uploaded = uploadCurrentExe(&uploadMsg, releaseNotes);
    if (uploaded) {
        QMessageBox::information(parent, QStringLiteral("检查更新"),
                                 QStringLiteral("已将目前版本上传至服务器：\n") + uploadMsg);
    } else {
        QMessageBox::warning(parent, QStringLiteral("检查更新"),
                             QStringLiteral("上传失败：") + uploadMsg);
    }
    log(QStringLiteral("[OTA] 上传结果: ") + uploadMsg);
    return true;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
