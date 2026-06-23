#include "log_upload_service.h"

#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

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
#include <QRegularExpression>
#include <QSslError>
#include <QSysInfo>
#include <QTextCodec>
#include <QTimer>
#include <QUrl>
#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kLogRootName = "所有log";
constexpr const char* kDefaultUploadUrl = "https://fctp.luteos.com/api/factory-tool/logs/upload";
QString escapePowerShellSingleQuoted(const QString& text) {
    return QString(text).replace(QLatin1Char('\''), QLatin1String("''"));
}

QString decodeHttpBodyText(const QByteArray& body) {
    if (body.isEmpty()) {
        return {};
    }
    const QByteArray lower = body.toLower();
    if (lower.contains("charset=gb2312") || lower.contains("charset=gbk") || lower.contains("charset=gb18030")) {
        if (QTextCodec* codec = QTextCodec::codecForName("GB18030")) {
            return codec->toUnicode(body);
        }
    }
    const QString utf8 = QString::fromUtf8(body);
    if (utf8.count(QChar(0xFFFD)) > 2) {
        if (QTextCodec* codec = QTextCodec::codecForLocale()) {
            return codec->toUnicode(body);
        }
    }
    return utf8;
}

QString extractHtmlTitle(const QString& html) {
    static const QRegularExpression titleRe(
        QStringLiteral("<title[^>]*>([^<]*)</title>"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = titleRe.match(html);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString summarizeHttpBody(int httpStatus, const QByteArray& body, const QString& uploadUrl) {
    if (httpStatus == 404) {
        return QStringLiteral("接口不存在（404）。请确认后端 API 已启动且上传地址正确：%1").arg(uploadUrl);
    }
    if (httpStatus == 401) {
        return QStringLiteral("未登录或 Token 无效（401），请先在云平台登录");
    }
    if (httpStatus == 403) {
        return QStringLiteral("无权限（403）");
    }
    if (httpStatus >= 500) {
        return QStringLiteral("服务器内部错误（HTTP %1）").arg(httpStatus);
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QString apiMessage = doc.object().value(QStringLiteral("message")).toString().trimmed();
        if (!apiMessage.isEmpty()) {
            return apiMessage;
        }
    }

    const QString text = decodeHttpBodyText(body).trimmed();
    if (text.startsWith(QLatin1Char('<')) || text.contains(QStringLiteral("<html"), Qt::CaseInsensitive)) {
        const QString title = extractHtmlTitle(text);
        if (!title.isEmpty()) {
            return QStringLiteral("HTTP %1：%2").arg(httpStatus).arg(title);
        }
        return QStringLiteral("HTTP %1：服务器返回 HTML 错误页（可能未部署工厂云 API 或地址写错）").arg(httpStatus);
    }

    if (text.isEmpty()) {
        return QStringLiteral("HTTP %1").arg(httpStatus);
    }
    return text.length() > 160 ? text.left(160) + QStringLiteral("…") : text;
}

QString buildUploadFailureMessage(int qtHttpStatus, const QString& qtError, int curlStatus,
                                  const QByteArray& curlBody, const QString& curlStderr,
                                  const QString& uploadUrl) {
    const int status = curlStatus > 0 ? curlStatus : qtHttpStatus;
    QString detail = summarizeHttpBody(status, curlBody, uploadUrl);
    if (detail.isEmpty() && !curlStderr.trimmed().isEmpty()) {
        detail = curlStderr.trimmed();
    }
    if (detail.isEmpty() && !qtError.trimmed().isEmpty()) {
        detail = qtError.trimmed();
    }
    if (qtHttpStatus == 0 && curlStatus > 0) {
        return QStringLiteral("上传失败：%1（Qt 网络：%2）").arg(detail, qtError);
    }
    return QStringLiteral("上传失败（HTTP %1）：%2").arg(status).arg(detail);
}

bool runPowerShell(const QString& script, QString* error, QString* warning = nullptr, int timeoutMs = 120000) {
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
    const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QStringLiteral("压缩日志失败：") + stderrText;
        }
        return false;
    }
    if (warning && !stderrText.isEmpty()) {
        *warning = stderrText;
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
         << QStringLiteral("-F") << QStringLiteral("factoryName=") + cfg.factoryName.trimmed();
    const QString hostName = cfg.hostName.trimmed();
    if (!hostName.isEmpty()) {
        args << QStringLiteral("-F") << QStringLiteral("hostName=") + hostName;
    }
    const QString sn = cfg.sn.trimmed();
    if (!sn.isEmpty()) {
        args << QStringLiteral("-F") << QStringLiteral("sn=") + sn;
    }
    const QString testResult = cfg.testResult.trimmed();
    if (!testResult.isEmpty()) {
        args << QStringLiteral("-F") << QStringLiteral("testResult=") + testResult;
    }
    const QString clientVersion = cfg.clientVersion.trimmed();
    if (!clientVersion.isEmpty()) {
        args << QStringLiteral("-F") << QStringLiteral("clientVersion=") + clientVersion;
    }
    const QString token = SETTINGS.value(QStringLiteral("FactoryCloud/Token")).toString().trimmed();
    if (!token.isEmpty()) {
        args << QStringLiteral("-H") << QStringLiteral("Authorization: Bearer ") + token;
    }
    args << QStringLiteral("-F") << QStringLiteral("file=@") + zipForCurl << cfg.uploadUrl.trimmed()
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
        *error = summarizeHttpBody(status, output, cfg.uploadUrl.trimmed());
        const QString stderrMsg = QString::fromUtf8(stderrText).trimmed();
        if (!stderrMsg.isEmpty() && error->isEmpty()) {
            *error = stderrMsg;
        }
    }
    return ok;
}

bool parseUploadResponse(const QByteArray& body, const QString& uploadUrl, QString* message) {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        if (message) {
            *message = summarizeHttpBody(0, body, uploadUrl);
            if (message->isEmpty()) {
                *message = QStringLiteral("上传完成，但响应不是 JSON");
            }
        }
        qDebug() << "parseUploadResponse: non-JSON response from" << uploadUrl << ":" << decodeHttpBodyText(body).left(500);
        return true;
    }
    const QJsonObject obj = doc.object();
    const int code = obj.value(QStringLiteral("code")).toInt(-1);
    const QString apiMessage = obj.value(QStringLiteral("message")).toString();
    if (code == 0) {
        if (message) {
            const QJsonObject data = obj.value(QStringLiteral("data")).toObject();
            const int logId = data.value(QStringLiteral("logId")).toInt(0);
            *message = logId > 0 ? QStringLiteral("日志上传成功（logId=%1）").arg(logId)
                                 : QStringLiteral("日志上传成功");
            if (!apiMessage.isEmpty()) {
                *message += QStringLiteral("：") + apiMessage;
            }
        }
        return true;
    }
    if (message) {
        *message = QStringLiteral("上传失败：")
                   + (apiMessage.isEmpty() ? summarizeHttpBody(0, body, uploadUrl) : apiMessage);
    }
    qDebug() << "parseUploadResponse: api returned error code=" << code << "message=" << apiMessage
             << "body=" << decodeHttpBodyText(body).left(500);
    return false;
}

} // namespace

QString LogUploadService::defaultLogRootPath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromUtf8(kLogRootName));
}

QString LogUploadService::defaultUploadUrl() {
    return QString::fromUtf8(kDefaultUploadUrl);
}

QString LogUploadService::defaultDeviceId() {
    return QSysInfo::machineHostName();
}

bool LogUploadService::isUploadEnabled() {
    return SETTINGS.value(QStringLiteral("FactoryCloud/Feature/LogUpload"), true).toBool();
}

bool LogUploadService::configFromSettings(UploadConfig* cfg, QString* error) {
    if (!cfg) {
        if (error) {
            *error = QStringLiteral("内部错误：配置为空");
        }
        return false;
    }

    cfg->uploadUrl = FactoryCloudClient::defaultLogUploadUrl();

    const QString factoryName = SETTINGS.value(QStringLiteral("Mes/FACTORY")).toString().trimmed();
    if (factoryName.isEmpty()) {
        if (error) {
            *error = QStringLiteral("请先在 MES 设置中选择工厂（Mes/FACTORY）");
        }
        return false;
    }

    QString deviceId = FactoryCloudClient::deviceId().trimmed();
    if (deviceId.isEmpty()) {
        deviceId = defaultDeviceId();
    }

    QString station = FactoryCloudClient::stationKey().trimmed();
    if (station.isEmpty()) {
        station = SETTINGS.value(QStringLiteral("SYSTEM/station")).toString().trimmed();
    }
    if (station.isEmpty()) {
        station = QStringLiteral("DEFAULT");
    }

    cfg->deviceId = deviceId;
    cfg->station = station;
    cfg->factoryName = factoryName;
    cfg->hostName = QSysInfo::machineHostName();
    cfg->sn = SETTINGS.value(QStringLiteral("RemoteLog/LastSn")).toString().trimmed();
    cfg->testResult = SETTINGS.value(QStringLiteral("RemoteLog/LastTestResult")).toString().trimmed();
    cfg->clientVersion = FactoryCloudClient::appVersion();
    return true;
}

QString LogUploadService::compressLogDirectory(const QString& logRoot, QString* error, QString* warning) {
    const QDir rootDir(logRoot);
    if (!rootDir.exists()) {
        if (error) {
            *error = QStringLiteral("日志目录不存在：") + logRoot;
        }
        return {};
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    // 临时 zip 仅用 ASCII 文件名，避免 PowerShell 与 Qt 路径编码不一致导致「压缩成功但找不到文件」
    const QString zipName =
        defaultDeviceId() + QStringLiteral("_alllog_") + timestamp + QStringLiteral(".zip");
    const QString zipPath = QDir(QDir::tempPath()).filePath(zipName);
    if (QFile::exists(zipPath) && !QFile::remove(zipPath)) {
        if (error) {
            *error = QStringLiteral("无法覆盖临时压缩包：") + zipPath;
        }
        return {};
    }

    const QString logRootNative = QDir::toNativeSeparators(logRoot);
    const QString zipPathNative = QDir::toNativeSeparators(zipPath);
    // 逐文件写入 zip，共享读方式打开，跳过被上位机占用的 db 等文件
    const QString script =
        QStringLiteral("$ErrorActionPreference='Stop';"
                       "Add-Type -AssemblyName System.IO.Compression;"
                       "Add-Type -AssemblyName System.IO.Compression.FileSystem;"
                       "if (Test-Path -LiteralPath '%2') { Remove-Item -LiteralPath '%2' -Force };"
                       "$src='%1';$dst='%2';"
                       "$fs=[IO.File]::Open($dst,[IO.FileMode]::CreateNew);"
                       "$zip=New-Object IO.Compression.ZipArchive($fs,[IO.Compression.ZipArchiveMode]::Create);"
                       "$added=0;$skipped=@();"
                       "Get-ChildItem -LiteralPath $src -Recurse -File | ForEach-Object {"
                       "  $rel=$_.FullName.Substring($src.Length).TrimStart([char]92,[char]47).Replace([char]92,[char]47);"
                       "  try {"
                       "    $in=[IO.File]::Open($_.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite);"
                       "    $ent=$zip.CreateEntry($rel,[IO.Compression.CompressionLevel]::Optimal);"
                       "    $out=$ent.Open();$in.CopyTo($out);$out.Dispose();$in.Dispose();$added++"
                       "  } catch { $skipped += $rel }"
                       "};"
                       "$zip.Dispose();$fs.Dispose();"
                       "if ($added -eq 0) { throw ('无可读日志文件' + $(if($skipped.Count){'，已跳过: '+($skipped -join '; ')})) };"
                       "if ($skipped.Count -gt 0) { [Console]::Error.WriteLine(('skipped:'+($skipped -join ';'))) }")
            .arg(escapePowerShellSingleQuoted(logRootNative), escapePowerShellSingleQuoted(zipPathNative));
    QString compressWarning;
    if (!runPowerShell(script, error, &compressWarning)) {
        return {};
    }
    if (warning && !compressWarning.isEmpty()) {
        *warning = compressWarning;
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
        qDebug() << "uploadLogArchive: uploadUrl is empty";
        return false;
    }
    if (cfg.deviceId.trimmed().isEmpty()) {
        if (message) {
            *message = QStringLiteral("设备 ID 为空");
        }
        qDebug() << "uploadLogArchive: deviceId is empty";
        return false;
    }
    if (cfg.factoryName.trimmed().isEmpty()) {
        if (message) {
            *message = QStringLiteral("工厂名为空");
        }
        qDebug() << "uploadLogArchive: factoryName is empty";
        return false;
    }
    if (!QFile::exists(zipPath)) {
        if (message) {
            *message = QStringLiteral("压缩包不存在：") + zipPath;
        }
        qDebug() << "uploadLogArchive: zip file not found:" << zipPath;
        return false;
    }

    QFile* file = new QFile(zipPath);
    if (!file->open(QIODevice::ReadOnly)) {
        if (message) {
            *message = QStringLiteral("无法打开压缩包：") + zipPath;
        }
        qDebug() << "uploadLogArchive: cannot open zip file:" << zipPath << "error:" << file->errorString();
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
    appendTextPart(QStringLiteral("factoryName"), cfg.factoryName.trimmed());
    const QString hostName = cfg.hostName.trimmed();
    if (!hostName.isEmpty()) {
        appendTextPart(QStringLiteral("hostName"), hostName);
    }
    const QString sn = cfg.sn.trimmed();
    if (!sn.isEmpty()) {
        appendTextPart(QStringLiteral("sn"), sn);
    }
    const QString testResult = cfg.testResult.trimmed();
    if (!testResult.isEmpty()) {
        appendTextPart(QStringLiteral("testResult"), testResult);
    }
    const QString clientVersion = cfg.clientVersion.trimmed();
    if (!clientVersion.isEmpty()) {
        appendTextPart(QStringLiteral("clientVersion"), clientVersion);
    }

    QHttpPart filePart;
    const QString fileName = QFileInfo(zipPath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(fileName)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/zip")));
    // filePart.setBodyDevice(file);
    // file->setParent(multiPart);
    QByteArray data = file->readAll();   // ✔ 正确（指针）
    filePart.setBody(data);
    multiPart->append(filePart);

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(cfg.uploadUrl.trimmed())));
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(300000);
#endif

    const QString token = SETTINGS.value(QStringLiteral("FactoryCloud/Token")).toString().trimmed();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    }

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
        return parseUploadResponse(body, cfg.uploadUrl.trimmed(), message);
    }

    qDebug() << "uploadLogArchive: Qt upload failed, httpStatus=" << httpStatus << "qtError=" << qtErrorText
             << "body=" << decodeHttpBodyText(body).left(500) << "uploadUrl=" << cfg.uploadUrl.trimmed();

    QByteArray curlBody;
    QString curlError;
    int curlStatus = 0;
    if (uploadByCurl(zipPath, cfg, &curlBody, &curlError, &curlStatus)) {
        if (responseBody) {
            *responseBody = curlBody;
        }
        return parseUploadResponse(curlBody, cfg.uploadUrl.trimmed(), message);
    }

    if (message) {
        *message = buildUploadFailureMessage(httpStatus, qtErrorText, curlStatus, curlBody, curlError,
                                             cfg.uploadUrl.trimmed());
    }
    if (responseBody) {
        *responseBody = body;
    }
    qDebug() << "uploadLogArchive: final failure:" << (message ? *message : QString())
             << "curlStatus=" << curlStatus << "curlError=" << curlError << "curlBody=" << decodeHttpBodyText(curlBody).left(500);
    return false;
}

bool LogUploadService::packAndUpload(const UploadConfig& cfg, QString* message) {
    QString zipError;
    QString zipWarning;
    const QString logRoot = defaultLogRootPath();
    const QString zipPath = compressLogDirectory(logRoot, &zipError, &zipWarning);
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
qDebug() << "zip size:" << QFileInfo(zipPath).size()<<zipPath;
    if (message) {
        *message = (ok ? QStringLiteral("打包成功（%1），").arg(sizeText) : QString()) + uploadMessage;
        if (ok && zipWarning.startsWith(QStringLiteral("skipped:"))) {
            *message += QStringLiteral("；部分文件被占用已跳过：")
                         + zipWarning.mid(QStringLiteral("skipped:").size());
        }
    }
    return ok;
}
