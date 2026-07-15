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
    static void saveBlackboxLog(const QByteArray& data);
    static void saveOtaStressLog(const QString& msg);

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
};

#endif // QLOG_H
