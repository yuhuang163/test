#include "log_upload_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSslError>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kLogRootName = "所有log";
constexpr const char* kDefaultUploadUrl = "https://fctp.luteos.com/api/factory-tool/logs/upload";

QString escapePowerShellSingleQuoted(const QString& text) {
    return QString(text).replace(QLatin1Char('\''), QLatin1String("''"));
}

bool runPowerShell(const QString& script, QString* error, int timeoutMs = 120000) {
    QProcess process;
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                          QStringLiteral("-Command"), script});
    process.start();
    if (!process.waitForStarted(5000)) {
        if (error) {
            *error = QStringLiteral("无法启动 PowerShell：") + process.errorString();
        }
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        if (error) {
            *error = QStringLiteral("压缩日志超时");
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QStringLiteral("压缩日志失败：")
                     + QString::fromUtf8(process.readAllStandardError()).trimmed();
        }
        return false;
    }
    return true;
}

bool uploadByCurl(const QString& zipPath, const LogUploadService::UploadConfig& cfg, QByteArray* response,
                  QString* error, int* httpStatus) {
    const QString zipForCurl = QDir::toNativeSeparators(zipPath);
    QStringList args;
    args << QStringLiteral("-k") << QStringLiteral("-sS") << QStringLiteral("--connect-timeout")
         << QStringLiteral("15") << QStringLiteral("--max-time") << QStringLiteral("300")
         << QStringLiteral("-F") << QStringLiteral("deviceId=") + cfg.deviceId.trimmed()
         << QStringLiteral("-F") << QStringLiteral("station=") + cfg.station.trimmed()
         << QStringLiteral("-F") << QStringLiteral("file=@") + zipForCurl << cfg.uploadUrl.trimmed()
         << QStringLiteral("-w") << QStringLiteral("\n__HTTP_STATUS__:%{http_code}");

    QProcess curl;
    curl.start(QStringLiteral("curl.exe"), args);
    if (!curl.waitForStarted(3000)) {
        if (error) {
            *error = QStringLiteral("curl.exe 启动失败：") + curl.errorString();
        }
        return false;
    }
    if (!curl.waitForFinished(310000)) {
        curl.kill();
        curl.waitForFinished(2000);
        if (error) {
            *error = QStringLiteral("curl 上传超时");
        }
        return false;
    }

    QByteArray output = curl.readAllStandardOutput();
    const QByteArray stderrText = curl.readAllStandardError();
    const QByteArray marker = "\n__HTTP_STATUS__:";
    int status = 0;
    const int markerIndex = output.lastIndexOf(marker);
    if (markerIndex >= 0) {
        status = output.mid(markerIndex + marker.size()).trimmed().toInt();
        output = output.left(markerIndex);
    }
    if (httpStatus) {
        *httpStatus = status;
    }
    if (response) {
        *response = output;
    }

    const bool ok = curl.exitStatus() == QProcess::NormalExit && curl.exitCode() == 0 && status >= 200 && status < 300;
    if (!ok && error) {
        *error = QStringLiteral("curl exit=%1 httpStatus=%2 %3 %4")
                     .arg(curl.exitCode())
                     .arg(status)
                     .arg(QString::fromUtf8(stderrText).trimmed())
                     .arg(QString::fromUtf8(output).trimmed())
                     .trimmed();
    }
    return ok;
}

bool parseUploadResponse(const QByteArray& body, QString* message) {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        if (message) {
            *message = QStringLiteral("上传完成，但响应不是 JSON：") + QString::fromUtf8(body.left(256));
        }
        return true;
    }
    const QJsonObject obj = doc.object();
    const int code = obj.value(QStringLiteral("code")).toInt(-1);
    const QString apiMessage = obj.value(QStringLiteral("message")).toString();
    if (code == 0) {
        if (message) {
            *message = QStringLiteral("日志上传成功");
            if (!apiMessage.isEmpty()) {
                *message += QStringLiteral("：") + apiMessage;
            }
        }
        return true;
    }
    if (message) {
        *message = QStringLiteral("上传失败：") + (apiMessage.isEmpty() ? QString::fromUtf8(body) : apiMessage);
    }
    return false;
}

}  // namespace

QString LogUploadService::defaultLogRootPath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromUtf8(kLogRootName));
}

QString LogUploadService::defaultUploadUrl() {
    return QString::fromUtf8(kDefaultUploadUrl);
}

QString LogUploadService::defaultDeviceId() {
    return QSysInfo::machineHostName();
}

QString LogUploadService::compressLogDirectory(const QString& logRoot, QString* error) {
    const QDir rootDir(logRoot);
    if (!rootDir.exists()) {
        if (error) {
            *error = QStringLiteral("日志目录不存在：") + logRoot;
        }
        return {};
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString zipName =
        defaultDeviceId() + QStringLiteral("_所有log_") + timestamp + QStringLiteral(".zip");
    const QString zipPath = QDir(QDir::tempPath()).filePath(zipName);
    if (QFile::exists(zipPath) && !QFile::remove(zipPath)) {
        if (error) {
            *error = QStringLiteral("无法覆盖临时压缩包：") + zipPath;
        }
        return {};
    }

    const QString script =
        QStringLiteral("Compress-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
            .arg(escapePowerShellSingleQuoted(QDir::toNativeSeparators(logRoot)),
                 escapePowerShellSingleQuoted(QDir::toNativeSeparators(zipPath)));
    if (!runPowerShell(script, error)) {
        return {};
    }
    if (!QFile::exists(zipPath)) {
        if (error) {
            *error = QStringLiteral("压缩完成但未找到文件：") + zipPath;
        }
        return {};
    }
    return zipPath;
}

bool LogUploadService::uploadLogArchive(const QString& zipPath, const UploadConfig& cfg, QString* message,
                                        QString* responseBody) {
    if (cfg.uploadUrl.trimmed().isEmpty()) {
        if (message) {
            *message = QStringLiteral("上传地址为空");
        }
        return false;
    }
    if (cfg.deviceId.trimmed().isEmpty()) {
        if (message) {
            *message = QStringLiteral("设备 ID 为空");
        }
        return false;
    }
    if (!QFile::exists(zipPath)) {
        if (message) {
            *message = QStringLiteral("压缩包不存在：") + zipPath;
        }
        return false;
    }

    QFile* file = new QFile(zipPath);
    if (!file->open(QIODevice::ReadOnly)) {
        if (message) {
            *message = QStringLiteral("无法打开压缩包：") + zipPath;
        }
        delete file;
        return false;
    }

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto appendTextPart = [&](const QString& name, const QString& value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"%1\"").arg(name)));
        part.setBody(value.toUtf8());
        multiPart->append(part);
    };
    appendTextPart(QStringLiteral("deviceId"), cfg.deviceId.trimmed());
    appendTextPart(QStringLiteral("station"), cfg.station.trimmed());

    QHttpPart filePart;
    const QString fileName = QFileInfo(zipPath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(fileName)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/zip")));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(cfg.uploadUrl.trimmed())));
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(300000);
#endif

    QNetworkReply* reply = manager.post(request, multiPart);
    multiPart->setParent(reply);
    QObject::connect(reply, QOverload<const QList<QSslError>&>::of(&QNetworkReply::sslErrors), reply,
                     QOverload<>::of(&QNetworkReply::ignoreSslErrors));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(300000);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();

    const QByteArray body = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString qtErrorText = reply->errorString();
    const bool qtOk = reply->error() == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300;
    reply->deleteLater();

    if (qtOk) {
        if (responseBody) {
            *responseBody = body;
        }
        return parseUploadResponse(body, message);
    }

    QByteArray curlBody;
    QString curlError;
    int curlStatus = 0;
    if (uploadByCurl(zipPath, cfg, &curlBody, &curlError, &curlStatus)) {
        if (responseBody) {
            *responseBody = curlBody;
        }
        return parseUploadResponse(curlBody, message);
    }

    if (message) {
        *message = QStringLiteral("上传失败（HTTP %1）：%2 | curl备选：%3")
                       .arg(httpStatus)
                       .arg(qtErrorText)
                       .arg(curlError);
    }
    if (responseBody) {
        *responseBody = body;
    }
    return false;
}

bool LogUploadService::packAndUpload(const UploadConfig& cfg, QString* message) {
    QString zipError;
    const QString logRoot = defaultLogRootPath();
    const QString zipPath = compressLogDirectory(logRoot, &zipError);
    if (zipPath.isEmpty()) {
        if (message) {
            *message = zipError;
        }
        return false;
    }

    const QFileInfo zipInfo(zipPath);
    const QString sizeText = QString::number(zipInfo.size() / 1024.0, 'f', 1) + QStringLiteral(" KB");
    QString uploadMessage;
    const bool ok = uploadLogArchive(zipPath, cfg, &uploadMessage);
    QFile::remove(zipPath);

    if (message) {
        *message = (ok ? QStringLiteral("打包成功（%1），").arg(sizeText) : QString()) + uploadMessage;
    }
    return ok;
}
