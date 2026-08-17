#ifndef QLOG_H
#define QLOG_H

#include <QByteArray>
#include <QDateTime>
#include <QPlainTextEdit>
#include <QString>
#include <QTextStream>
#include <QVector>

#include <QMessageLogContext>
#include <QtGlobal>

struct TestItem {
    QString testItem;
    QString testData;
    QString testResult;
    QString ask;
};

/** 测试结束后供上传模块读取的会话信息 */
struct QlogSessionInfo {
    int slot = 0;
    QString sn;
    QString mac;
    QString traceCode;
    QString result;
    QString station;
    QDateTime startedAt;
    QDateTime endedAt;
    QString sessionAbsolutePath;
    QString sessionRelativePath;
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
    bool valid = false;
};

/** 上位机日志统一入口：会话落盘、UI 文本框、Qt 消息与崩溃记录 */
class Qlog {
  public:
    static constexpr int kMainWindowLogSlot = -1;

    Qlog() = default;

    static void installQtMessageHandler();

#ifdef Q_OS_WIN
    static void installWindowsCrashHandler();
    static void setCrashReportExtraInfo(const QString& info);
#endif

    static void beginSession(int slot, const QString& sn, const QString& mac, const QString& station = {});
    static void endSession(int slot, const QString& result);
    static bool hasActiveSession(int slot);
    static QString sessionLogPath(int slot);
    static QlogSessionInfo lastSessionInfo(int slot);

    static void logUi(int slot, const QString& msg);
    static void logBackend(int slot, const QString& msg, const char* category = "DBG");

    static void showlog(const QString& msg, int machineIndex = 0, QPlainTextEdit* msgEdit = nullptr);

    static void saveDongleUartLog(int machineIndex, const QString& data);
    static void saveDongleUartLogMain(const QString& data);
    /**
     * 把高频日志（dongle 原始帧、qDebug 后台日志）的内存缓冲立即写盘。
     * 取会话 offset、退出与崩溃前必须调用，否则切片会缺尾部内容。
     */
    static void flushLogBuffers();
    /**
     * 吸力采样等高频段：日志只进内存，关闭时再一次性落盘。
     * 缓冲超过约 1MB 仍会中途写盘，避免异常退出时内存无上限。
     */
    static void setBufferedLogFlushDeferred(bool deferred);
    static void saveBlackboxLog(const QByteArray& data);
    static void saveOtaStressLog(const QString& msg);
    /**
     * 常驻后台监控日志（心跳/串口扫描/主任务环等），单独日文件，不走 qDebug/UI。
     * 路径：所有log/常驻监控/<主机名>_常驻监控_<日期>.log
     */
    static void saveResidentLog(const QString& tag, const QString& msg);

    void saveTestCsv(const QString& ver, const QString& sn, const QString& macAddress,
                     const QVector<TestItem>& testItems);
    void save_brush_log(int m_index, const QString& macAddress, const QString& data);
    void save_fixture_uart_log(int txrx, const QByteArray& data);
    void save_jig_uart_log(int txrx, const QByteArray& data);

    static void writeRow(QTextStream& stream, const QStringList& rowData);

    static void handleQtMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    static QString logRootAbsolute();
    static QString exportDongleSessionSlice(const QlogSessionInfo& info, QString* error);
    static QString exportProcessBackgroundSessionSlice(const QlogSessionInfo& info, QString* error);
    /** 本轮测试时间窗内的常驻监控切片（心跳/串口扫描等），供测完上传使用 */
    static QString exportResidentSessionSlice(const QlogSessionInfo& info, QString* error);

    /** 吸力采样序列按工位暂存；测中落盘拿不到 PASS/NG，故留到测完导出 */
    static void setSuctionSamples(int slot, const QVector<double>& timeSec, const QVector<double>& ch1,
                                  const QVector<double>& ch2, const QVector<double>& ch3);
    /** 导出本轮吸力采样 CSV（列同主窗口吸力页），无采样数据时返回空且不置 error */
    static QString exportSuctionSamplesCsv(const QlogSessionInfo& info, QString* error);
};

#endif // QLOG_H
