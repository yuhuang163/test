#include "qlog.h"

#ifdef Q_OS_WIN
#include "qlog_win.h"
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSysInfo>
#include <QStringList>
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
    QString residentDailyAbsolutePath;
    QString residentDailyRelativePath;
    qint64 residentOffsetStart = 0;
    qint64 residentOffsetEnd = 0;
    QString productDailyAbsolutePath;
    QString productDailyRelativePath;
    qint64 productOffsetStart = 0;
    qint64 productOffsetEnd = 0;
    QString fixtureDailyAbsolutePath;
    QString fixtureDailyRelativePath;
    qint64 fixtureOffsetStart = 0;
    qint64 fixtureOffsetEnd = 0;
    QString jigFixtureDailyAbsolutePath;
    QString jigFixtureDailyRelativePath;
    qint64 jigFixtureOffsetStart = 0;
    qint64 jigFixtureOffsetEnd = 0;
    QString result;
};

/** 一轮测试的吸力采样序列，四路等长且同索引 */
struct SuctionSampleBuffer {
    QVector<double> timeSec;
    QVector<double> ch1;
    QVector<double> ch2;
    QVector<double> ch3;
};

QMutex g_sessionMutex;
QHash<int, SessionState> g_activeSessions;
QHash<int, SessionState> g_lastEndedSessions;
QHash<int, SuctionSampleBuffer> g_suctionSamples;
QHash<int, QStringList> g_screenInspectFiles;

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
           QStringLiteral(".log");
}

QString processBackgroundDailyRelativePath() {
    return logRootRelative() + QStringLiteral("/上位机log/进程后台/") + processBackgroundDailyFileName();
}

QString residentDailyFileName() {
    return QSysInfo::machineHostName() + QStringLiteral("_常驻监控_") + CommonUtils::formatDateIso() +
           QStringLiteral(".log");
}

QString residentDailyRelativePath() {
    return logRootRelative() + QStringLiteral("/常驻监控/") + residentDailyFileName();
}

QString productDailyFileName(int slot) {
    return QStringLiteral("产品日志_%1_%2.log")
        .arg(dongleSlotLabel(slot), CommonUtils::dateStampYmd());
}

QString productDailyRelativePath(int slot) {
    return logRootRelative() + QStringLiteral("/产品log/") + productDailyFileName(slot);
}

QString fixtureUartDailyFileName(const QString& fileNamePrefix) {
    return fileNamePrefix + CommonUtils::dateStampYmd() + QStringLiteral(".log");
}

QString fixtureUartDailyRelativePath(const QString& fileNamePrefix) {
    return logRootRelative() + QStringLiteral("/治具log/") + fixtureUartDailyFileName(fileNamePrefix);
}

bool appendLineToFile(const QString& absolutePath, const QString& line, bool writeBomIfNew) {
    QFile file(absolutePath);
    const bool isNew = !file.exists() || file.size() == 0;
    // 行尾已显式写 CRLF，不能再开 Text 模式，否则 \n 被二次翻译成 \r\r\n
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(writeBomIfNew && isNew);
    out << line << QStringLiteral("\r\n");
    return true;
}

/**
 * 高频日志（dongle 原始帧、qDebug 后台日志）先进内存再批量落盘。
 * 吸力上报 50Hz 时逐行 open/append/close 会把主线程压在磁盘 IO 上（现场偶发卡顿主因）。
 * 常规：满 kLogFlushBytes 或距上次落盘超 kLogFlushIntervalMs 才写。
 * 采样推迟模式：只攒内存，关闭推迟或缓冲超过 kLogFlushBytesDeferred 再写。
 */
constexpr int kLogFlushIntervalMs = 200;
constexpr int kLogFlushBytes = 32 * 1024;
constexpr int kLogFlushBytesDeferred = 1024 * 1024;

struct BufferedLogFile {
    QByteArray pending;
    bool writeBomIfNew = false;
};

QMutex g_bufferedLogMutex;
QHash<QString, BufferedLogFile> g_bufferedLogs;
QElapsedTimer g_bufferedLogClock;
qint64 g_bufferedLogLastFlushMs = 0;
bool g_bufferedLogFlushDeferred = false;

void writeBufferedLogToDisk(const QString& absolutePath, BufferedLogFile& buf) {
    if (buf.pending.isEmpty()) {
        return;
    }
    QFile file(absolutePath);
    const bool isNew = !file.exists() || file.size() == 0;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        // 打不开就丢弃本批，避免缓冲无上限膨胀
        buf.pending.clear();
        return;
    }
    if (isNew && buf.writeBomIfNew) {
        file.write("\xEF\xBB\xBF");
    }
    file.write(buf.pending);
    file.close();
    buf.pending.clear();
}

void flushBufferedLogsLocked() {
    for (auto it = g_bufferedLogs.begin(); it != g_bufferedLogs.end(); ++it) {
        writeBufferedLogToDisk(it.key(), it.value());
    }
    if (g_bufferedLogClock.isValid()) {
        g_bufferedLogLastFlushMs = g_bufferedLogClock.elapsed();
    }
}

void appendLineBuffered(const QString& absolutePath, const QString& line, bool writeBomIfNew) {
    QMutexLocker lock(&g_bufferedLogMutex);
    if (!g_bufferedLogClock.isValid()) {
        g_bufferedLogClock.start();
    }
    auto it = g_bufferedLogs.find(absolutePath);
    if (it == g_bufferedLogs.end()) {
        // 只在首次写该文件时建目录，避免每行都 mkpath
        QDir().mkpath(QFileInfo(absolutePath).path());
        it = g_bufferedLogs.insert(absolutePath, BufferedLogFile{});
        it->writeBomIfNew = writeBomIfNew;
    }
    it->pending += line.toUtf8();
    it->pending += "\r\n";
    const qint64 nowMs = g_bufferedLogClock.elapsed();
    const int flushBytes = g_bufferedLogFlushDeferred ? kLogFlushBytesDeferred : kLogFlushBytes;
    const bool timeDue =
        !g_bufferedLogFlushDeferred && nowMs - g_bufferedLogLastFlushMs >= kLogFlushIntervalMs;
    if (it->pending.size() >= flushBytes || timeDue) {
        flushBufferedLogsLocked();
    }
}

void appendProcessBackgroundLog(const QString& line) {
    if (!qtDebugToProcessLog()) {
        return;
    }
    const QString relDir = logRootRelative() + QStringLiteral("/上位机log/进程后台");
    appendLineBuffered(
        QDir(QCoreApplication::applicationDirPath())
            .filePath(CommonUtils::joinPath(relDir, processBackgroundDailyFileName())),
        line, true);
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
    // 必须按原始字节读：落盘是 CRLF，offset 也是磁盘字节；用 Text 模式会丢 \r 导致偏移错乱
    QFile inFile(dailyAbsolutePath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法读取日志：") + dailyAbsolutePath;
        }
        return {};
    }
    const QByteArray all = inFile.readAll();
    inFile.close();

    // dongle 一条记录常为「时间戳行 + 后续正文行」，正文行本身没有时间戳
    static const QRegularExpression tsRe(
        QStringLiteral(R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}))"));

    QStringList kept;
    bool inTimeWindow = false;
    qint64 pos = 0;
    if (all.size() >= 3 && static_cast<unsigned char>(all[0]) == 0xEF
        && static_cast<unsigned char>(all[1]) == 0xBB && static_cast<unsigned char>(all[2]) == 0xBF) {
        pos = 3; // 跳过 UTF-8 BOM，偏移仍按文件绝对位置
    }

    while (pos < all.size()) {
        const qint64 lineStart = pos;
        qint64 i = pos;
        while (i < all.size() && all.at(i) != '\n' && all.at(i) != '\r') {
            ++i;
        }
        const QByteArray lineBytes = all.mid(static_cast<int>(lineStart), static_cast<int>(i - lineStart));
        qint64 next = i;
        if (next < all.size() && all.at(next) == '\r') {
            ++next;
            if (next < all.size() && all.at(next) == '\n') {
                ++next;
            }
        } else if (next < all.size() && all.at(next) == '\n') {
            ++next;
        }

        const QString line = QString::fromUtf8(lineBytes);
        const QRegularExpressionMatch m = tsRe.match(line);
        bool keep = false;
        if (m.hasMatch()) {
            const QDateTime ts =
                QDateTime::fromString(m.captured(1), QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
            inTimeWindow = ts.isValid() && ts >= info.startedAt && ts <= info.endedAt;
            keep = inTimeWindow;
        } else {
            // 无时间戳：归属上一条时间戳头；偏移窗兜底（兼容无时间戳的旧行）
            const bool inOffset = (offsetEnd > offsetStart) && (next > offsetStart) && (lineStart < offsetEnd);
            keep = inTimeWindow || inOffset;
        }
        if (keep) {
            kept.append(line);
        }
        pos = next;
    }

    if (kept.isEmpty() && offsetEnd > offsetStart) {
        const int start = static_cast<int>(qMax(qint64(0), offsetStart));
        const int len = static_cast<int>(qMax(qint64(0), offsetEnd - offsetStart));
        const QByteArray slice = all.mid(start, len);
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
    // 同 appendLineToFile：显式 CRLF，勿开 Text 模式
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
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
    info.residentDailyAbsolutePath = state.residentDailyAbsolutePath;
    info.residentDailyRelativePath = state.residentDailyRelativePath;
    info.residentOffsetStart = state.residentOffsetStart;
    info.residentOffsetEnd = state.residentOffsetEnd;
    info.productDailyAbsolutePath = state.productDailyAbsolutePath;
    info.productDailyRelativePath = state.productDailyRelativePath;
    info.productOffsetStart = state.productOffsetStart;
    info.productOffsetEnd = state.productOffsetEnd;
    info.fixtureDailyAbsolutePath = state.fixtureDailyAbsolutePath;
    info.fixtureDailyRelativePath = state.fixtureDailyRelativePath;
    info.fixtureOffsetStart = state.fixtureOffsetStart;
    info.fixtureOffsetEnd = state.fixtureOffsetEnd;
    info.jigFixtureDailyAbsolutePath = state.jigFixtureDailyAbsolutePath;
    info.jigFixtureDailyRelativePath = state.jigFixtureDailyRelativePath;
    info.jigFixtureOffsetStart = state.jigFixtureOffsetStart;
    info.jigFixtureOffsetEnd = state.jigFixtureOffsetEnd;
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

void appendUartTxRxLines(const QString& absolutePath, int txrx, const QByteArray& data) {
    const QString detailedTimestamp = CommonUtils::formatTimestampMs();
    const QString hexData = CommonUtils::toHexUpperSpaced(data);
    const QString raw = QString::fromLatin1(data);
    if (txrx) {
        appendLineToFile(absolutePath,
                         detailedTimestamp + QStringLiteral("- tx发送的原始数据为：") + raw, true);
        appendLineToFile(absolutePath,
                         detailedTimestamp + QStringLiteral("- tx发送的16进制数据：") + hexData, false);
    } else {
        appendLineToFile(absolutePath,
                         detailedTimestamp + QStringLiteral("- rx接收的原始数据为：") + raw, true);
        appendLineToFile(absolutePath,
                         detailedTimestamp + QStringLiteral("- rx接收的16进制数据：") + hexData, false);
    }
}

void saveUartRawLogImpl(int txrx, const QByteArray& data, const QString& fileNamePrefix) {
    const QString folderName = logRootRelative() + QStringLiteral("/治具log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        return;
    }
    const QString filePath = CommonUtils::joinPath(folderName, fixtureUartDailyFileName(fileNamePrefix));
    appendUartTxRxLines(filePath, txrx, data);
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
    // 缓冲日志最多滞留 kLogFlushIntervalMs，退出前补一次落盘
    if (QCoreApplication* app = QCoreApplication::instance()) {
        QObject::connect(app, &QCoreApplication::aboutToQuit, app, []() { Qlog::flushLogBuffers(); });
    }
}

QString Qlog::logRootAbsolute() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(logRootRelative());
}

void Qlog::beginSession(int slot, const QString& sn, const QString& mac, const QString& station) {
    if (!sessionLogEnabled()) {
        return;
    }
    QMutexLocker lock(&g_sessionMutex);
    // offset 取的是磁盘字节数，先把缓冲写完再量
    flushLogBuffers();
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
                                state.traceCode + QStringLiteral("_PENDING.log");
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

    if (!CommonUtils::ensureLogDirectory(logRootRelative() + QStringLiteral("/常驻监控"))) {
        qDebug() << QStringLiteral("无法创建常驻监控日志目录");
    }
    state.residentDailyRelativePath = residentDailyRelativePath();
    state.residentDailyAbsolutePath =
        QDir(QCoreApplication::applicationDirPath()).filePath(state.residentDailyRelativePath);
    state.residentOffsetStart = fileSizeOrZero(state.residentDailyAbsolutePath);

    if (!CommonUtils::ensureLogDirectory(logRootRelative() + QStringLiteral("/产品log"))) {
        qDebug() << QStringLiteral("无法创建产品日志目录");
    }
    state.productDailyRelativePath = productDailyRelativePath(slot);
    state.productDailyAbsolutePath =
        QDir(QCoreApplication::applicationDirPath()).filePath(state.productDailyRelativePath);
    state.productOffsetStart = fileSizeOrZero(state.productDailyAbsolutePath);

    if (!CommonUtils::ensureLogDirectory(logRootRelative() + QStringLiteral("/治具log"))) {
        qDebug() << QStringLiteral("无法创建治具日志目录");
    }
    state.fixtureDailyRelativePath = fixtureUartDailyRelativePath(QStringLiteral("治具日志"));
    state.fixtureDailyAbsolutePath =
        QDir(QCoreApplication::applicationDirPath()).filePath(state.fixtureDailyRelativePath);
    state.fixtureOffsetStart = fileSizeOrZero(state.fixtureDailyAbsolutePath);
    state.jigFixtureDailyRelativePath = fixtureUartDailyRelativePath(QStringLiteral("Jig治具日志"));
    state.jigFixtureDailyAbsolutePath =
        QDir(QCoreApplication::applicationDirPath()).filePath(state.jigFixtureDailyRelativePath);
    state.jigFixtureOffsetStart = fileSizeOrZero(state.jigFixtureDailyAbsolutePath);

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
    // offset 取的是磁盘字节数，先把缓冲写完再量
    flushLogBuffers();
    state.dongleOffsetEnd = fileSizeOrZero(state.dongleDailyAbsolutePath);
    state.processBackgroundOffsetEnd = fileSizeOrZero(state.processBackgroundDailyAbsolutePath);
    state.residentOffsetEnd = fileSizeOrZero(state.residentDailyAbsolutePath);
    state.productOffsetEnd = fileSizeOrZero(state.productDailyAbsolutePath);
    state.fixtureOffsetEnd = fileSizeOrZero(state.fixtureDailyAbsolutePath);
    state.jigFixtureOffsetEnd = fileSizeOrZero(state.jigFixtureDailyAbsolutePath);

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
                              state.traceCode + QLatin1Char('_') + state.result + QStringLiteral(".log");
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
        // 只有 DBG 走缓冲；警告及以上是异常路径且频率低，立刻落盘以防崩溃丢掉最后几条
        if (type != QtDebugMsg) {
            flushLogBuffers();
        }
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
                             CommonUtils::formatDateIso() + QStringLiteral(".log");
    appendLineToFile(CommonUtils::joinPath(folderName, fileName), message, true);
}

void Qlog::saveDongleUartLog(int machineIndex, const QString& data) {
    const QString folderName = logRootRelative() + QStringLiteral("/dongle的log");
    const QString fileName = dongleDailyFileName(machineIndex);
    const QString line = CommonUtils::formatTimestampMs() + QLatin1Char(' ') + data;
    // dongle 原始串口仅落盘到日文件；会话主文件只保留 [UI]，上传时另按时间窗导出 dongle 切片
    // 吸力上报期间这里是 50Hz 级调用，必须走缓冲批量落盘
    appendLineBuffered(QDir(QCoreApplication::applicationDirPath())
                           .filePath(CommonUtils::joinPath(folderName, fileName)),
                       line, false);
}

void Qlog::flushLogBuffers() {
    QMutexLocker lock(&g_bufferedLogMutex);
    flushBufferedLogsLocked();
}

void Qlog::setBufferedLogFlushDeferred(bool deferred) {
    QMutexLocker lock(&g_bufferedLogMutex);
    g_bufferedLogFlushDeferred = deferred;
    if (!deferred) {
        flushBufferedLogsLocked();
    }
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
    appendTextLog(folderName, QStringLiteral("ota升级log.log"), msg, false);
}

void Qlog::saveResidentLog(const QString& tag, const QString& msg) {
    // 心跳/轮询在工作线程写盘，需互斥避免行交错
    static QMutex residentMutex;
    QMutexLocker locker(&residentMutex);

    const QString folderName = logRootRelative() + QStringLiteral("/常驻监控");
    if (!CommonUtils::ensureLogDirectory(folderName))
        return;
    const QString absolutePath = CommonUtils::joinPath(folderName, residentDailyFileName());
    const QString line = CommonUtils::formatTimestampMs() + QStringLiteral(" [")
                         + (tag.isEmpty() ? QStringLiteral("-") : tag) + QStringLiteral("] ") + msg;
    appendLineToFile(absolutePath, line, true);
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
    const QString outName = sessionFileStem(info) + QStringLiteral("_backend.log");
    const QString outRel = logRootRelative() + QStringLiteral("/上位机log/进程后台/") + outName;
    return exportDailyLogSessionSlice(info.processBackgroundDailyAbsolutePath, outRel, info,
                                      info.processBackgroundOffsetStart, info.processBackgroundOffsetEnd, error);
}

QString Qlog::exportResidentSessionSlice(const QlogSessionInfo& info, QString* error) {
    if (!info.valid || info.residentDailyAbsolutePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("无常驻监控日志文件");
        }
        return {};
    }
    const QString outName = sessionFileStem(info) + QStringLiteral("_resident.log");
    const QString outRel = logRootRelative() + QStringLiteral("/常驻监控/") + outName;
    return exportDailyLogSessionSlice(info.residentDailyAbsolutePath, outRel, info, info.residentOffsetStart,
                                      info.residentOffsetEnd, error);
}

QString Qlog::exportProductSessionSlice(const QlogSessionInfo& info, QString* error) {
    if (!info.valid || info.productDailyAbsolutePath.isEmpty()) {
        return {};
    }
    if (!QFile::exists(info.productDailyAbsolutePath)) {
        return {};
    }
    const QString outName = sessionFileStem(info) + QStringLiteral("_product.log");
    const QString outRel = logRootRelative() + QStringLiteral("/产品log/") + outName;
    return exportDailyLogSessionSlice(info.productDailyAbsolutePath, outRel, info, info.productOffsetStart,
                                      info.productOffsetEnd, error);
}

QString Qlog::exportFixtureSessionSlice(const QlogSessionInfo& info, QString* error) {
    if (!info.valid || info.fixtureDailyAbsolutePath.isEmpty()) {
        return {};
    }
    if (!QFile::exists(info.fixtureDailyAbsolutePath)) {
        return {};
    }
    const QString outName = sessionFileStem(info) + QStringLiteral("_fixture.log");
    const QString outRel = logRootRelative() + QStringLiteral("/治具log/") + outName;
    return exportDailyLogSessionSlice(info.fixtureDailyAbsolutePath, outRel, info, info.fixtureOffsetStart,
                                      info.fixtureOffsetEnd, error);
}

QString Qlog::exportJigFixtureSessionSlice(const QlogSessionInfo& info, QString* error) {
    if (!info.valid || info.jigFixtureDailyAbsolutePath.isEmpty()) {
        return {};
    }
    if (!QFile::exists(info.jigFixtureDailyAbsolutePath)) {
        return {};
    }
    const QString outName = sessionFileStem(info) + QStringLiteral("_jig_fixture.log");
    const QString outRel = logRootRelative() + QStringLiteral("/治具log/") + outName;
    return exportDailyLogSessionSlice(info.jigFixtureDailyAbsolutePath, outRel, info, info.jigFixtureOffsetStart,
                                      info.jigFixtureOffsetEnd, error);
}

void Qlog::saveProductUartLog(int machineIndex, int txrx, const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }
    const QString folderName = logRootRelative() + QStringLiteral("/产品log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        return;
    }
    const QString filePath = CommonUtils::joinPath(folderName, productDailyFileName(machineIndex));
    appendUartTxRxLines(filePath, txrx, data);
}

void Qlog::setSuctionSamples(int slot, const QVector<double>& timeSec, const QVector<double>& ch1,
                             const QVector<double>& ch2, const QVector<double>& ch3) {
    QMutexLocker lock(&g_sessionMutex);
    if (timeSec.isEmpty()) {
        g_suctionSamples.remove(slot);
        return;
    }
    SuctionSampleBuffer& buf = g_suctionSamples[slot];
    buf.timeSec = timeSec;
    buf.ch1 = ch1;
    buf.ch2 = ch2;
    buf.ch3 = ch3;
}

QString Qlog::exportSuctionSamplesCsv(const QlogSessionInfo& info, QString* error) {
    if (!info.valid) {
        return {};
    }
    SuctionSampleBuffer buf;
    {
        QMutexLocker lock(&g_sessionMutex);
        if (!g_suctionSamples.contains(info.slot)) {
            // 非吸力工站属常态，不置 error 以免每轮上传都带无用告警
            return {};
        }
        // take：本轮取走，避免下一轮没采样时又把上一轮数据传一遍
        buf = g_suctionSamples.take(info.slot);
    }
    if (buf.timeSec.isEmpty()) {
        return {};
    }

    const QString outRel =
        logRootRelative() + QStringLiteral("/吸力CSV/") + sessionFileStem(info) + QStringLiteral("_suction.csv");
    const QString outDirRel = QFileInfo(outRel).path();
    if (!outDirRel.isEmpty() && outDirRel != QLatin1String(".")) {
        if (!CommonUtils::ensureLogDirectory(outDirRel)) {
            if (error) {
                *error = QStringLiteral("无法创建吸力CSV目录");
            }
            return {};
        }
    }
    const QString outAbs = QDir(QCoreApplication::applicationDirPath()).filePath(outRel);
    QFile outFile(outAbs);
    // 同会话切片：显式 CRLF，勿开 Text 模式
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("无法写入吸力CSV");
        }
        return {};
    }
    QTextStream out(&outFile);
    out.setCodec("UTF-8");
    // 表头与主窗口吸力页一致，两处 CSV 可共用解析脚本
    out << QStringLiteral("time_s,ch1_kpa,ch2_kpa,ch3_kpa\r\n");
    const int count = buf.timeSec.size();
    auto valueAt = [](const QVector<double>& v, int i) { return i < v.size() ? v.at(i) : 0.0; };
    for (int i = 0; i < count; ++i) {
        out << QString::number(buf.timeSec.at(i), 'f', 3) << QLatin1Char(',')
            << QString::number(valueAt(buf.ch1, i), 'f', 3) << QLatin1Char(',')
            << QString::number(valueAt(buf.ch2, i), 'f', 3) << QLatin1Char(',')
            << QString::number(valueAt(buf.ch3, i), 'f', 3) << QStringLiteral("\r\n");
    }
    outFile.close();

    QString rel = outRel;
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return rel;
}

void Qlog::addScreenInspectImageFiles(int slot, const QStringList& absolutePaths) {
    QMutexLocker lock(&g_sessionMutex);
    if (absolutePaths.isEmpty()) {
        g_screenInspectFiles.remove(slot);
        return;
    }
    QStringList& list = g_screenInspectFiles[slot];
    for (const QString& path : absolutePaths) {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty() || !QFile::exists(trimmed) || list.contains(trimmed)) {
            continue;
        }
        list.append(trimmed);
    }
}

QStringList Qlog::exportScreenInspectImageFiles(const QlogSessionInfo& info, QString* error) {
    if (!info.valid) {
        return {};
    }
    QStringList srcPaths;
    {
        QMutexLocker lock(&g_sessionMutex);
        if (!g_screenInspectFiles.contains(info.slot)) {
            return {};
        }
        // take：本轮取走，避免下一轮无拍摄时又把上一轮图传一遍
        srcPaths = g_screenInspectFiles.take(info.slot);
    }
    if (srcPaths.isEmpty()) {
        return {};
    }

    const QString outDirRel = logRootRelative() + QStringLiteral("/屏幕检测");
    if (!CommonUtils::ensureLogDirectory(outDirRel)) {
        if (error) {
            *error = QStringLiteral("无法创建屏幕检测图片目录");
        }
        return {};
    }
    const QString outDirAbs = QDir(QCoreApplication::applicationDirPath()).filePath(outDirRel);
    const QString stem = sessionFileStem(info);
    QStringList relOut;
    int index = 0;
    for (const QString& srcAbs : srcPaths) {
        if (!QFile::exists(srcAbs)) {
            continue;
        }
        ++index;
        const QString baseName = QFileInfo(srcAbs).fileName();
        const QString outName = stem + QLatin1Char('_') + QString::number(index) + QLatin1Char('_') + baseName;
        const QString outAbs = resolveUniqueFilePath(outDirAbs, outName);
        if (QFile::exists(outAbs)) {
            QFile::remove(outAbs);
        }
        if (!QFile::copy(srcAbs, outAbs)) {
            if (error) {
                *error = QStringLiteral("无法复制屏幕检测图片：") + baseName;
            }
            continue;
        }
        QString rel = outDirRel + QLatin1Char('/') + QFileInfo(outAbs).fileName();
        rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        relOut << rel;
    }
    return relOut;
}

void Qlog::saveTestCsv(const QString& ver, const QString& sn, const QString& macAddress,
                       const QVector<TestItem>& testItems) {
    const QString folderPath = QStringLiteral("D:/测试结果");
    CommonUtils::ensureDirectory(folderPath);

    const QString fileName = CommonUtils::formatDateIso() + QStringLiteral("_%1报告.csv").arg(ver);
    const QString filePath = CommonUtils::joinPath(folderPath, fileName);

    const auto escapeJoin = [](const QStringList& row) {
        QStringList escaped;
        escaped.reserve(row.size());
        for (const QString& cell : row)
            escaped.append(csvEscapeCell(cell));
        return escaped.join(QStringLiteral(","));
    };

    const auto parseCsvLine = [](const QString& line) {
        QStringList fields;
        QString cur;
        bool inQuotes = false;
        for (int i = 0; i < line.size(); ++i) {
            const QChar ch = line.at(i);
            if (inQuotes) {
                if (ch == QLatin1Char('"')) {
                    if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                        cur.append(QLatin1Char('"'));
                        ++i;
                    } else {
                        inQuotes = false;
                    }
                } else {
                    cur.append(ch);
                }
            } else if (ch == QLatin1Char('"')) {
                inQuotes = true;
            } else if (ch == QLatin1Char(',')) {
                fields.append(cur);
                cur.clear();
            } else {
                cur.append(ch);
            }
        }
        fields.append(cur);
        return fields;
    };

    const QString timestamp = CommonUtils::dateTimeStamp();
    // 默认一次测试一行；取消勾选则每个测试项单独一行（长表）
    const bool oneRowPerTest = SETTINGS.value(QStringLiteral("SYSTEM/TestCsvOneRowPerTest"), true).toBool();

    if (!oneRowPerTest) {
    QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
            return;
        QTextStream stream(&file);
        const QStringList headers = {QStringLiteral("sn"), QStringLiteral("上位机版本"),
                                     QStringLiteral("mac地址"), QStringLiteral("时间戳"),
                                     QStringLiteral("测试项"), QStringLiteral("测试数据"),
                                     QStringLiteral("测试结果"), QStringLiteral("测试要求")};
        if (file.size() == 0)
            stream << escapeJoin(headers) << "\n";
        for (const TestItem& item : testItems) {
            stream << escapeJoin({sn, ver, macAddress, timestamp, item.testItem, item.testData, item.testResult,
                                  item.ask})
                   << "\n";
        }
        return;
    }

    // 一次测试一行、无表头：sn/版本/mac/时间 后按「数据名,数据,卡控,结果」成组排列
    QStringList row = {sn, ver, macAddress, timestamp};
    for (const TestItem& item : testItems) {
        QString name = item.testItem.trimmed();
        if (name.isEmpty())
            name = QStringLiteral("未命名");
        row << name << item.testData << item.ask << item.testResult;
    }

    // 若仍是旧版带表头文件，丢掉旧内容，避免空列/错位
    bool truncate = false;
    if (QFile::exists(filePath) && QFileInfo(filePath).size() > 0) {
        QFile in(filePath);
        if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&in);
            const QString first = ts.readLine();
            const QStringList fields = parseCsvLine(first);
            if (!fields.isEmpty() &&
                (fields.first() == QStringLiteral("sn") || fields.contains(QStringLiteral("测试项")) ||
                 fields.contains(QStringLiteral("上位机版本")))) {
                truncate = true;
            }
        }
    }

    QFile out(filePath);
    const QIODevice::OpenMode mode =
        truncate ? (QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)
                 : (QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    if (!out.open(mode))
        return;
    QTextStream stream(&out);
    stream << escapeJoin(row) << "\n";
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
