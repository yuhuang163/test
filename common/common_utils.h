#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QtGlobal>

class QComboBox;

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

    /** RC4 流密码（加密与解密同算法），就地处理 data；与设备端 rc4(key,keyLen,data,dataLen) 一致。 */
    static void rc4Crypt(const QByteArray& key, QByteArray& data);
    /** 规范化为 16 字节 EncryptionKey（超长截断，不足补 0）。 */
    static QByteArray normalizeRootEncryptionKey(const QByteArray& key);
    /**
     * qroot 0xF7 KeyTail 解密：密文 8B 为「密钥后 8 字节」经 RC4(完整 16B key) 的结果。
     * 返回解密后的 8 字节明文；cipher8 长度不对时返回空。
     */
    static QByteArray decryptRootTupleKeyTail(const QByteArray& encryptionKey, const QByteArray& cipher8);
    /** 校验：RC4 解密 cipher8 后是否等于 encryptionKey[8..15]。 */
    static bool matchRootTupleKeyTail(const QByteArray& encryptionKey, const QByteArray& cipher8);
    /**
     * 三元组 deviceSecret 比对：keyCipherHex 非空时按 qroot RC4 KeyTail 校验；
     * 否则走明文全串相等（Qpb/FCTP）。
     */
    static bool matchTupleDeviceSecret(const QString& deviceKeyField, const QString& keyCipherHex,
                                       const QString& expectedSecret);

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
    /** 界面计时标签：无效 timer 为 0.0 s，保留 fractionDigits 位小数 */
    static QString formatElapsedSeconds(const QElapsedTimer& timer, int fractionDigits = 1);
    static QString formatElapsedTimer(const QElapsedTimer& timer, bool compact = true);

    // --- 文件 ---
    static bool fileExists(const QString& path);
    static bool ensureDirectory(const QString& dirPath);
    static bool ensureLogDirectory(const QString& relativeDir);
    static QByteArray readAllBytes(const QString& filePath, QString* errorOut = nullptr);
    /** 若文件以 UTF-8 BOM 开头则去掉（项目约定 ini 等为 UTF-8 无 BOM） */
    static void stripUtf8BomFromFile(const QString& filePath);
    static QString joinPath(const QString& dir, const QString& fileName);

    // --- 字符串（版本卡控与 test_base::compareVersions 语义一致）---
    static bool compareVersions(const QString& versionList, const QString& versionToCompare);
    static QString trimmed(const QString& text);
    static bool equalsIgnoreCase(const QString& left, const QString& right);
    static QString formatList(const QStringList& items, const QString& separator = QStringLiteral(", "));

    /**
     * 产品目录（common_utils.cpp 内 kProductTable 唯一维护）。
     * mesProductNames：设置页；resolveDongleDeviceMapping：Dongle 广播名→产品/协议。
     */
    static QStringList mesProductNames();
    static QStringList dongleBroadcastFilterNames();
    static QString protocolForProduct(const QString& productName);
    /** 工站显示名是否属于指定产品：自由/默认工站通用，其余按工站名前缀匹配产品名。 */
    static bool stationBelongsToProduct(const QString& stationDisplayName, const QString& productName);
    static bool resolveDongleDeviceMapping(const QString& dongleName, QString* productOut, QString* protocolOut);

    /** SN 校验：pattern 为空表示不卡控 */
    static bool isSnPatternEnabled(const QString& pattern);
    static bool matchSnPattern(const QString& sn, const QString& pattern);
    static void initSnPatternComboBox(QComboBox* combo);
    static void selectSnPatternComboBox(QComboBox* combo, const QString& pattern);
    static QString snPatternFromComboBox(const QComboBox* combo);
    static QString snPatternDisplayText(const QString& pattern);
};

#endif // COMMON_UTILS_H
