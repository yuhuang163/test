#include "qlog.h"

#ifdef Q_OS_WIN
#include "qlog_win.h"
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTextStream>

#include <cstdio>

#include "common_utils.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kLogRootName = "所有log";

struct SessionState {
    bool active = false;
    int slot = 0;
    QString sn;
    QString mac;
    QString station;
    QString traceCode;
    QDateTime startedAt;
    QDateTime endedAt;
    QString pendingAbsolutePath;
    QString absolutePath;
    QString relativePath;
    QString dongleDailyAbsolutePath;
    QString dongleDailyRelativePath;
    qint64 dongleOffsetStart = 0;
    qint64 dongleOffsetEnd = 0;
    QString processBackgroundDailyAbsolutePath;
    QString processBackgroundDailyRelativePath;
    qint64 processBackgroundOffsetStart = 0;
    qint64 processBackgroundOffsetEnd = 0;
    QString result;
};

QMutex g_sessionMutex;
QHash<int, SessionState> g_activeSessions;
QHash<int, SessionState> g_lastEndedSessions;

bool sessionLogEnabled() {
    return SETTINGS.value(QStringLiteral("FactoryCloud/Log/SessionLogEnabled"), true).toBool();
}

bool qtDebugToProcessLog() {
    return SETTINGS.value(QStringLiteral("FactoryCloud/Log/QtDebugToProcessLog"), true).toBool();
}

QString logRootRelative() {
    return QString::fromUtf8(kLogRootName);
}

QString slotFolderName(int slot) {
    if (slot == Qlog::kMainWindowLogSlot) {
        return QStringLiteral("主窗口");
    }
    return QStringLiteral("工位%1").arg(slot);
}

QString dongleSlotLabel(int slot) {
    if (slot == Qlog::kMainWindowLogSlot) {
        return QStringLiteral("主窗口");
    }
    return QString::number(slot);
}

QString sanitizeTraceCode(QString text) {
    text = text.trimmed();
    static const QRegularExpression bad(QStringLiteral(R"([\\/:*?"<>|\s])"));
    text.replace(bad, QStringLiteral("_"));
    text.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    text = text.trimmed();
    if (text.length() > 64) {
        text = text.left(64);
    }
    return text.isEmpty() ? QStringLiteral("NO_TRACE") : text;
}

QString pickTraceCode(const QString& sn, const QString& mac) {
    if (!sn.trimmed().isEmpty()) {
        return sanitizeTraceCode(sn);
    }
    if (!mac.trimmed().isEmpty()) {
        return sanitizeTraceCode(mac);
    }
    return QStringLiteral("NO_TRACE");
}

QString resolveUniqueFilePath(const QString& dirPath, const QString& fileName) {
    const QString first = CommonUtils::joinPath(dirPath, fileName);
    if (!QFile::exists(first)) {
        return first;
    }
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot > 0 ? fileName.left(dot) : fileName;
    const QString ext = dot > 0 ? fileName.mid(dot) : QString();
    for (int n = 1; n < 1000; ++n) {
        const QString candidateName = stem + QLatin1Char('_') + QString::number(n) + ext;
        const QString candidate = CommonUtils::joinPath(dirPath, candidateName);
        if (!QFile::exists(candidate)) {
            return candidate;
        }
    }
    return first;
}

QString hostLogDirRelative(int slot) {
    return logRootRelative() + QStringLiteral("/上位机log/") + slotFolderName(slot);
}

QString dongleDailyFileName(int slot) {
    return QStringLiteral("dongle日志_%1_%2.log")
        .arg(dongleSlotLabel(slot), CommonUtils::dateStampYmd());
}

QString dongleDailyRelativePath(int slot) {
    return logRootRelative() + QStringLiteral("/dongle的log/") + dongleDailyFileName(slot);
}

QString processBackgroundDailyFileName() {
    return QSysInfo::machineHostName() + QStringLiteral("_进程后台_") + CommonUtils::formatDateIso() +
           QStringLiteral(".txt");
}

QString processBackgroundDailyRelativePath() {
    return logRootRelative() + QStringLiteral("/上位机log/进程后台/") + processBackgroundDailyFileName();
}

bool appendLineToFile(const QString& absolutePath, const QString& line, bool writeBomIfNew) {
    QFile file(absolutePath);
    const bool isNew = !file.exists() || file.size() == 0;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(writeBomIfNew && isNew);
    out << line << QStringLiteral("\r\n");
    return true;
}

void appendProcessBackgroundLog(const QString& line) {
    if (!qtDebugToProcessLog()) {
        return;
    }
    const QString relDir = logRootRelative() + QStringLiteral("/上位机log/进程后台");
    if (!CommonUtils::ensureLogDirectory(relDir)) {
        return;
    }
    const QString fileName =
        QSysInfo::machineHostName() + QStringLiteral("_进程后台_") + CommonUtils::formatDateIso() + QStringLiteral(".txt");
    appendLineToFile(CommonUtils::joinPath(relDir, fileName), line, true);
}

QString exportDailyLogSessionSlice(const QString& dailyAbsolutePath, const QString& outputRelPath,
                                   const QlogSessionInfo& info, qint64 offsetStart, qint64 offsetEnd,
                                   QString* error) {
    if (!info.valid || dailyAbsolutePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("无日誌文件");
        }
        return {};
    }
    QFile inFile(dailyAbsolutePath);
    if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("无法读取日志：") + dailyAbsolutePath;
        }
        return {};
    }

    static const QRegularExpression tsRe(
        QStringLiteral(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}))"));
    const QByteArray all = inFile.readAll();
    inFile.close();

    const QStringList lines = QString::fromUtf8(all).split(QRegularExpression(QStringLiteral("\r?\n")),
                                                         Qt::KeepEmptyParts);
    QStringList kept;
    qint64 pos = 0;
    for (const QString& line : lines) {
        const qint64 lineBytes = line.toUtf8().size() + 1;
        const QRegularExpressionMatch m = tsRe.match(line);
        if (m.hasMatch()) {
            const QDateTime ts = QDateTime::fromString(m.captured(1), QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
            if (ts.isValid() && ts >= info.startedAt && ts <= info.endedAt) {
                kept.append(line);
            }
        } else if (pos >= offsetStart && pos < offsetEnd) {
            kept.append(line);
        }
        pos += lineBytes;
    }
    if (kept.isEmpty() && offsetEnd > offsetStart) {
        const QByteArray slice =
            all.mid(static_cast<int>(offsetStart), static_cast<int>(offsetEnd - offsetStart));
        kept = QString::fromUtf8(slice).split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
    }

    const QString outAbs = QDir(QCoreApplication::applicationDirPath()).filePath(outputRelPath);
    const QString outDirRel = QFileInfo(outputRelPath).path();
    if (!outDirRel.isEmpty() && outDirRel != QLatin1String(".")) {
        if (!CommonUtils::ensureLogDirectory(outDirRel)) {
            if (error) {
                *error = QStringLiteral("无法创建会话切片目录");
            }
            return {};
        }
    }
    QFile outFile(outAbs);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("无法写入会话切片");
        }
        return {};
    }
    QTextStream out(&outFile);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(true);
    for (const QString& line : kept) {
        out << line << QStringLiteral("\r\n");
    }
    outFile.close();
    QString rel = outputRelPath;
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return rel;
}

void writeSessionLine(SessionState& state, const QString& category, const QString& msg) {
    if (!state.active || state.pendingAbsolutePath.isEmpty()) {
        return;
    }
    const QString line = QStringLiteral("[%1] %2 %3")
                               .arg(category, CommonUtils::formatTimestampMs(), msg);
    appendLineToFile(state.pendingAbsolutePath, line, false);
}

SessionState* activeSession(int slot) {
    if (!g_activeSessions.contains(slot)) {
        return nullptr;
    }
    SessionState* state = &g_activeSessions[slot];
    return state->active ? state : nullptr;
}

QlogSessionInfo toPublicInfo(const SessionState& state) {
    QlogSessionInfo info;
    info.slot = state.slot;
    info.sn = state.sn;
    info.mac = state.mac;
    info.traceCode = state.traceCode;
    info.result = state.result;
    info.station = state.station;
    info.startedAt = state.startedAt;
    info.endedAt = state.endedAt;
    info.sessionAbsolutePath = state.absolutePath;
    info.sessionRelativePath = state.relativePath;
    info.dongleDailyAbsolutePath = state.dongleDailyAbsolutePath;
    info.dongleDailyRelativePath = state.dongleDailyRelativePath;
    info.dongleOffsetStart = state.dongleOffsetStart;
    info.dongleOffsetEnd = state.dongleOffsetEnd;
    info.processBackgroundDailyAbsolutePath = state.processBackgroundDailyAbsolutePath;
    info.processBackgroundDailyRelativePath = state.processBackgroundDailyRelativePath;
    info.processBackgroundOffsetStart = state.processBackgroundOffsetStart;
    info.processBackgroundOffsetEnd = state.processBackgroundOffsetEnd;
    info.valid = !state.absolutePath.isEmpty();
    return info;
}

bool appendTextLog(const QString& relativeDir, const QString& fileName, const QString& body,
                   bool prefixTimestampLine = false) {
    if (!CommonUtils::ensureLogDirectory(relativeDir)) {
        qDebug() << "无法创建目录:" << relativeDir;
        return false;
    }
    const QString filePath = CommonUtils::joinPath(relativeDir, fileName);
    QFile logFile(filePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开日志文件：" << fileName;
        return false;
    }
    QTextStream out(&logFile);
    out.setCodec("UTF-8");
    if (prefixTimestampLine) {
        out << CommonUtils::formatTimestampMs() << "\n";
    }
    out << body << "\n";
    return true;
}

void saveUartRawLogImpl(int txrx, const QByteArray& data, const QString& fileNamePrefix) {
    const QString folderName = logRootRelative() + QStringLiteral("/治具log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        qDebug() << "无法创建目录:" << folderName;
        return;
    }

    const QString fileName = fileNamePrefix + CommonUtils::dateStampYmd() + QStringLiteral(".log");
    const QString filePath = CommonUtils::joinPath(folderName, fileName);

    QFile logFile(filePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开治具日志文件：" << fileName;
        return;
    }

    QTextStream out(&logFile);
    out.setCodec("UTF-8");
    const QString detailedTimestamp = CommonUtils::formatTimestampMs();
    const QString hexData = CommonUtils::toHexUpperSpaced(data);

    if (txrx) {
        out << detailedTimestamp << QStringLiteral("- tx发送的原始数据为：") << data << "\n";
        out << detailedTimestamp << QStringLiteral("- tx发送的16进制数据：") << hexData << "\n";
    } else {
        out << detailedTimestamp << QStringLiteral("- rx接收的原始数据为：") << data << "\n";
        out << detailedTimestamp << QStringLiteral("- rx接收的16进制数据：") << hexData << "\n";
    }
    logFile.close();
}

QString csvEscapeCell(QString value) {
    value.replace(QLatin1String("\r\n"), QString());
    value.replace(QLatin1Char('\n'), QString());
    value.replace(QLatin1Char('\r'), QString());
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"')) && !value.contains(QLatin1Char('\n'))) {
        return value;
    }
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}

qint64 fileSizeOrZero(const QString& path) {
    if (path.isEmpty() || !QFile::exists(path)) {
        return 0;
    }
    return QFileInfo(path).size();
}

/** 与会话主文件同名主干：yyyyMMdd_HHmmss_追溯码_结果[_n] */
QString sessionFileStem(const QlogSessionInfo& info) {
    if (!info.sessionAbsolutePath.isEmpty()) {
        const QString base = QFileInfo(info.sessionAbsolutePath).completeBaseName();
        if (!base.isEmpty()) {
            return base;
        }
    }
    return info.startedAt.toString(QStringLiteral("yyyyMMdd_HHmmss")) + QLatin1Char('_') + info.traceCode +
           QLatin1Char('_') + info.result;
}

} // namespace

void qlogQtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    Qlog::handleQtMessage(type, context, msg);
}

void Qlog::installQtMessageHandler() {
    qInstallMessageHandler(qlogQtMessageHandler);
}

QString Qlog::logRootAbsolute() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(logRootRelative());
}

void Qlog::beginSession(int slot, const QString& sn, const QString& mac, const QString& station) {
    if (!sessionLogEnabled()) {
        return;
    }
    QMutexLocker lock(&g_sessionMutex);
    SessionState state;
    state.active = true;
    state.slot = slot;
    state.sn = sn.trimmed();
    state.mac = mac.trimmed();
    state.station = station.trimmed();
    state.traceCode = pickTraceCode(state.sn, state.mac);
    state.startedAt = QDateTime::currentDateTime();
    state.result = QStringLiteral("PENDING");

    const QString relDir = hostLogDirRelative(slot);
    if (!CommonUtils::ensureLogDirectory(relDir)) {
        qDebug() << QStringLiteral("无法创建会话日志目录:") << relDir;
        return;
    }
    const QString absDir = QDir(QCoreApplication::applicationDirPath()).filePath(relDir);

    const QString pendingName = state.startedAt.toString(QStringLiteral("yyyyMMdd_HHmmss")) + QLatin1Char('_') +
                                state.traceCode + QStringLiteral("_PENDING.txt");
    state.pendingAbsolutePath = resolveUniqueFilePath(absDir, pendingName);

    state.dongleDailyRelativePath = dongleDailyRelativePath(slot);
    if (!CommonUtils::ensureLogDirectory(logRootRelative() + QStringLiteral("/dongle的log"))) {
        qDebug() << QStringLiteral("无法创建 dongle 日志目录");
    }
    state.dongleDailyAbsolutePath =
        QDir(QCoreApplication::applicationDirPath()).filePath(state.dongleDailyRelativePath);
    state.dongleOffsetStart = fileSizeOrZero(state.dongleDailyAbsolutePath);

    if (!CommonUtils::ensureLogDirectory(logRootRelative() + QStringLiteral("/上位机log/进程后台"))) {
        qDebug() << QStringLiteral("无法创建进程后台日志目录");
    }
    state.processBackgroundDailyRelativePath = processBackgroundDailyRelativePath();
    state.processBackgroundDailyAbsolutePath =
        QDir(QCoreApplication::applicationDirPath()).filePath(state.processBackgroundDailyRelativePath);
    state.processBackgroundOffsetStart = fileSizeOrZero(state.processBackgroundDailyAbsolutePath);

    const QString header = QStringLiteral("===== SESSION BEGIN =====\r\n"
                                          "slot=%1\r\n"
                                          "trace=%2\r\n"
                                          "sn=%3\r\n"
                                          "mac=%4\r\n"
                                          "station=%5\r\n"
                                          "host=%6\r\n"
                                          "clientVersion=%7\r\n"
                                          "startedAt=%8\r\n"
                                          "=========================")
                               .arg(slot)
                               .arg(state.traceCode)
                               .arg(state.sn)
                               .arg(state.mac)
                               .arg(state.station)
                                          .arg(QSysInfo::machineHostName())
                                          .arg(QFileInfo(QCoreApplication::applicationFilePath()).fileName())
                                          .arg(CommonUtils::formatTimestampMs(state.startedAt));
    appendLineToFile(state.pendingAbsolutePath, header, true);

    g_activeSessions[slot] = state;
}

void Qlog::endSession(int slot, const QString& result) {
    if (!sessionLogEnabled()) {
        return;
    }
    QMutexLocker lock(&g_sessionMutex);
    if (!g_activeSessions.contains(slot) || !g_activeSessions[slot].active) {
        return;
    }
    SessionState state = g_activeSessions[slot];
    state.active = false;
    state.endedAt = QDateTime::currentDateTime();
    state.result = result.trimmed().isEmpty() ? QStringLiteral("NG") : result.trimmed();
    state.dongleOffsetEnd = fileSizeOrZero(state.dongleDailyAbsolutePath);
    state.processBackgroundOffsetEnd = fileSizeOrZero(state.processBackgroundDailyAbsolutePath);

    const QString footer =
        QStringLiteral("===== SESSION END =====\r\n"
                       "result=%1\r\n"
                       "endedAt=%2\r\n"
                       "durationSec=%3\r\n"
                       "=======================")
            .arg(state.result)
            .arg(CommonUtils::formatTimestampMs(state.endedAt))
            .arg(state.startedAt.secsTo(state.endedAt));
    appendLineToFile(state.pendingAbsolutePath, footer, false);

    const QString finalName = state.startedAt.toString(QStringLiteral("yyyyMMdd_HHmmss")) + QLatin1Char('_') +
                              state.traceCode + QLatin1Char('_') + state.result + QStringLiteral(".txt");
    const QString relDir = hostLogDirRelative(slot);
    const QString absDir = QDir(QCoreApplication::applicationDirPath()).filePath(relDir);
    const QString finalAbs = resolveUniqueFilePath(absDir, finalName);
    if (QFile::exists(finalAbs)) {
        QFile::remove(finalAbs);
    }
    if (!QFile::rename(state.pendingAbsolutePath, finalAbs)) {
        state.absolutePath = state.pendingAbsolutePath;
    } else {
        state.absolutePath = finalAbs;
    }
    const QDir appDir(QCoreApplication::applicationDirPath());
    state.relativePath = appDir.relativeFilePath(state.absolutePath).replace(QLatin1Char('\\'), QLatin1Char('/'));

    g_lastEndedSessions[slot] = state;
    g_activeSessions.remove(slot);
}

bool Qlog::hasActiveSession(int slot) {
    QMutexLocker lock(&g_sessionMutex);
    return g_activeSessions.contains(slot) && g_activeSessions[slot].active;
}

QString Qlog::sessionLogPath(int slot) {
    QMutexLocker lock(&g_sessionMutex);
    if (g_lastEndedSessions.contains(slot)) {
        return g_lastEndedSessions[slot].absolutePath;
    }
    return {};
}

QlogSessionInfo Qlog::lastSessionInfo(int slot) {
    QMutexLocker lock(&g_sessionMutex);
    if (!g_lastEndedSessions.contains(slot)) {
        return {};
    }
    return toPublicInfo(g_lastEndedSessions[slot]);
}

void Qlog::logUi(int slot, const QString& msg) {
    if (msg.isEmpty()) {
        return;
    }
    QMutexLocker lock(&g_sessionMutex);
    if (SessionState* state = activeSession(slot)) {
        writeSessionLine(*state, QStringLiteral("UI"), msg);
    }
}

void Qlog::logBackend(int slot, const QString& msg, const char* category) {
    if (msg.isEmpty()) {
        return;
    }
    QMutexLocker lock(&g_sessionMutex);
    if (SessionState* state = activeSession(slot)) {
        writeSessionLine(*state, QString::fromUtf8(category), msg);
    }
}

void Qlog::showlog(const QString& msg, int machineIndex, QPlainTextEdit* msgEdit) {
    if (msg.isEmpty()) {
        return;
    }
    logUi(machineIndex, msg);
    if (msgEdit) {
        msgEdit->appendPlainText(msg);
    }
}

void Qlog::handleQtMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QString levelTag;
    switch (type) {
    case QtInfoMsg:
        levelTag = QStringLiteral("INF");
        break;
    case QtDebugMsg:
        levelTag = QStringLiteral("DBG");
        break;
    case QtWarningMsg:
        levelTag = QStringLiteral("WRN");
        break;
    case QtCriticalMsg:
        levelTag = QStringLiteral("CRT");
        break;
    case QtFatalMsg:
        levelTag = QStringLiteral("FTL");
        break;
    }

    QString message = QStringLiteral("[%1] %2 %3").arg(levelTag, CommonUtils::formatTimestampMs(), msg);
    // Release 下 qDebug 多数不传 __FILE__/__LINE__，line 恒为 0 无排查价值，仅在有有效源码位置时附加
    if (context.file && context.file[0] != '\0' && context.line > 0) {
        const QString fileName = QString::fromUtf8(context.file).split(QLatin1Char('\\')).last();
        message += QStringLiteral(" (%1:%2)").arg(fileName).arg(context.line);
    }

    printf("%s  %s\r\n", CommonUtils::formatTimestampMs().toLocal8Bit().constData(),
           msg.toLocal8Bit().constData());
    fflush(stdout);

    if (sessionLogEnabled()) {
        appendProcessBackgroundLog(message);
        return;
    }

    const bool keepLegacy = SETTINGS.value(QStringLiteral("FactoryCloud/Log/KeepLegacyDailyLog"), false).toBool();
    if (!keepLegacy) {
        return;
    }

    const QString folderName = logRootRelative() + QStringLiteral("/上位机log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        return;
    }
    QString fileNumber;
    const QRegularExpression re(QStringLiteral("^\\d+"));
    const QRegularExpressionMatch match = re.match(msg.trimmed());
    if (match.hasMatch()) {
        fileNumber = match.captured(0);
    } else {
        fileNumber = QStringLiteral("default");
    }
    const QString hostName = QSysInfo::machineHostName();
    const QString fileName = hostName + QStringLiteral("_上位机日志_") + fileNumber + QLatin1Char('_') +
                             CommonUtils::formatDateIso() + QStringLiteral(".txt");
    appendLineToFile(CommonUtils::joinPath(folderName, fileName), message, true);
}

void Qlog::saveDongleUartLog(int machineIndex, const QString& data) {
    const QString folderName = logRootRelative() + QStringLiteral("/dongle的log");
    const QString fileName = dongleDailyFileName(machineIndex);
    const QString line = CommonUtils::formatTimestampMs() + QLatin1Char(' ') + data;
    // dongle 原始串口仅落盘到日文件；会话主文件只保留 [UI]，上传时另按时间窗导出 dongle 切片
    appendTextLog(folderName, fileName, line, false);
}

void Qlog::saveDongleUartLogMain(const QString& data) {
    saveDongleUartLog(kMainWindowLogSlot, data);
}

void Qlog::saveBlackboxLog(const QByteArray& data) {
    const QString folderName = logRootRelative() + QStringLiteral("/设备黑盒的log");
    const QString fileName = QStringLiteral("黑盒日志") + CommonUtils::dateStampYmd() + QStringLiteral(".log");
    const QString body = CommonUtils::formatTimestampMs() + QStringLiteral("\n") + QString::fromUtf8(data);
    appendTextLog(folderName, fileName, body, false);
}

void Qlog::saveOtaStressLog(const QString& msg) {
    const QString folderName = logRootRelative() + QStringLiteral("/ota升级压测/");
    appendTextLog(folderName, QStringLiteral("ota升级log.txt"), msg, false);
}

QString Qlog::exportDongleSessionSlice(const QlogSessionInfo& info, QString* error) {
    if (!info.valid || info.dongleDailyAbsolutePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("无 dongle 日誌文件");
        }
        return {};
    }
    // 与工位会话主文件同主干，仅后缀区分类型
    const QString outName = sessionFileStem(info) + QStringLiteral("_dongle.log");
    const QString outRel = logRootRelative() + QStringLiteral("/dongle的log/") + outName;
    return exportDailyLogSessionSlice(info.dongleDailyAbsolutePath, outRel, info, info.dongleOffsetStart,
                                        info.dongleOffsetEnd, error);
}

QString Qlog::exportProcessBackgroundSessionSlice(const QlogSessionInfo& info, QString* error) {
    if (!info.valid || info.processBackgroundDailyAbsolutePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("无进程后台日志文件");
        }
        return {};
    }
    const QString outName = sessionFileStem(info) + QStringLiteral("_backend.txt");
    const QString outRel = logRootRelative() + QStringLiteral("/上位机log/进程后台/") + outName;
    return exportDailyLogSessionSlice(info.processBackgroundDailyAbsolutePath, outRel, info,
                                      info.processBackgroundOffsetStart, info.processBackgroundOffsetEnd, error);
}

void Qlog::saveTestCsv(const QString& ver, const QString& sn, const QString& macAddress,
                       const QVector<TestItem>& testItems) {
    const QString folderPath = QStringLiteral("D:/测试结果");
    CommonUtils::ensureDirectory(folderPath);

    const QString fileName = CommonUtils::formatDateIso() + QStringLiteral("_%1报告.csv").arg(ver);
    const QString filePath = CommonUtils::joinPath(folderPath, fileName);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);
        const QStringList headers = {QStringLiteral("sn"), QStringLiteral("上位机版本"),
                                     QStringLiteral("mac地址"), QStringLiteral("时间戳"),
                                     QStringLiteral("测试项"), QStringLiteral("测试数据"),
                                     QStringLiteral("测试结果"), QStringLiteral("测试要求")};
        const QString timestamp = CommonUtils::dateTimeStamp();

        if (file.size() == 0) {
            stream << headers.join(QStringLiteral(",")) << "\n";
        }

        for (const TestItem& item : testItems) {
            const QStringList row = {sn, ver, macAddress, timestamp, item.testItem, item.testData,
                                     item.testResult, item.ask};
            QStringList escaped;
            escaped.reserve(row.size());
            for (const QString& cell : row) {
                escaped.append(csvEscapeCell(cell));
            }
            stream << escaped.join(QStringLiteral(",")) << "\n";
        }
        file.close();
    }
}

void Qlog::save_brush_log(int m_index, const QString& macAddress, const QString& data) {
    Q_UNUSED(macAddress);
    const QString folderName = logRootRelative() + QStringLiteral("/设备log");
    const QString fileName = QString::number(m_index) + QStringLiteral(".log");
    const QString body = CommonUtils::formatTimestampMs() + QStringLiteral("\n") + data;
    appendTextLog(folderName, fileName, body, false);
}

void Qlog::save_fixture_uart_log(int txrx, const QByteArray& data) {
    saveUartRawLogImpl(txrx, data, QStringLiteral("治具日志"));
}

void Qlog::save_jig_uart_log(int txrx, const QByteArray& data) {
    saveUartRawLogImpl(txrx, data, QStringLiteral("Jig治具日志"));
}

void Qlog::writeRow(QTextStream& stream, const QStringList& rowData) {
    stream << rowData.join(QStringLiteral(",")) << "\n";
}

#ifdef Q_OS_WIN
void Qlog::setCrashReportExtraInfo(const QString& info) {
    qlogWinSetCrashReportExtraInfoUtf8(info.toUtf8().constData());
}
#endif

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
