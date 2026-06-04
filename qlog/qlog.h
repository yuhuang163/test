#ifndef QLOG_H
#define QLOG_H

#include <QByteArray>
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

/** 上位机日志统一入口：文件落盘、UI 文本框、Qt 消息与崩溃记录 */
class Qlog {
public:
    Qlog() = default;

    /** main：安装 qInstallMessageHandler → 所有log/上位机log */
    static void installQtMessageHandler();

#ifdef Q_OS_WIN
    /** main：SetUnhandledExceptionFilter，写入 所有log/*.dmp 与 *_闪退堆栈.log */
    static void installWindowsCrashHandler();
#endif

    /** UI + qDebug；machineIndex 仅用于调试前缀，可为工位号 */
    static void showlog(const QString& msg, int machineIndex = 0, QPlainTextEdit* msgEdit = nullptr);

    static void saveDongleUartLog(int machineIndex, const QString& data);
    /** 主窗口 dongle 日志（无工位号，单文件按日） */
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
};

#endif  // QLOG_H
