#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/** 工程公共工具：字节序/十六进制/CRC、时间、文件、字符串 */
class CommonUtils {
  public:
    // --- 字节 ---
    static void appendLe16(QByteArray* buffer, quint16 value);
    static void appendLe32(QByteArray* buffer, quint32 value);
    static quint16 readLe16(const QByteArray& buffer, int offset);
    static quint32 readLe32(const QByteArray& buffer, int offset);

    static QString toHexSpaced(const QByteArray& data, QChar separator = QLatin1Char(' '));
    static QString toHexUpperSpaced(const QByteArray& data);
    static QByteArray fromHexString(const QString& hex, bool* ok = nullptr);

    /** IEEE 802.3 CRC32（OTA 镜像等） */
    static quint32 crc32(const QByteArray& data);
    static quint32 crc32Update(quint32 crc, const char* data, int length);
    static int progressPercent(int sentBytes, int totalBytes);

    // --- 时间 ---
    static QString isoDateTime(const QDateTime& dateTime = QDateTime::currentDateTime());
    static QString dateStampYmd(const QDateTime& dateTime = QDateTime::currentDateTime());
    static QString formatDateIso(const QDateTime& dateTime = QDateTime::currentDateTime());
    static QString dateTimeStamp(const QDateTime& dateTime = QDateTime::currentDateTime());
    /** yyyy-MM-dd HH:mm:ss.zzzzzz（微秒），串口/设备日志常用 */
    static QString formatTimestampMs(const QDateTime& dateTime = QDateTime::currentDateTime());
    static QString formatElapsedMs(qint64 elapsedMs, bool compact = true);
    static QString formatElapsedSeconds(int seconds, bool compact = true);
    static QString formatElapsedTimer(const QElapsedTimer& timer, bool compact = true);

    // --- 文件 ---
    static bool fileExists(const QString& path);
    static bool ensureDirectory(const QString& dirPath);
    static bool ensureLogDirectory(const QString& relativeDir);
    static QByteArray readAllBytes(const QString& filePath, QString* errorOut = nullptr);
    static QString joinPath(const QString& dir, const QString& fileName);

    // --- 字符串（版本卡控与 test_base::compareVersions 语义一致）---
    static bool compareVersions(const QString& versionList, const QString& versionToCompare);
    static QString trimmed(const QString& text);
    static bool equalsIgnoreCase(const QString& left, const QString& right);
    static QString formatList(const QStringList& items, const QString& separator = QStringLiteral(", "));
};

#endif // COMMON_UTILS_H
