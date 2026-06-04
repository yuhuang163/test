#ifndef QLOG_H
#define QLOG_H
#include <QByteArray>

#include "Abini.h"
#include "qplaintextedit.h"

struct TestItem {
    QString testItem;
    QString testData;
    QString testResult;
    QString ask;
};

class Qlog {
public:
    Qlog();
    void saveTestCsv(const QString& ver, const QString& sn, const QString& macAddress,
                     const QVector<TestItem>& testItems);
    void save_brush_log(int m_index, QString macAddress, QString data);
    /** 带 UI 的 Fixture_uart 串口原始收发日志（所有log/治具log/治具日志YYYYMMDD.log） */
    void save_fixture_uart_log(int txrx, const QByteArray& data);
    /** test_base 侧 Qjig 通道日志（所有log/治具log/Jig治具日志YYYYMMDD.log） */
    void save_jig_uart_log(int txrx, const QByteArray& data);
    void showlog(QString msg, int mechine, QPlainTextEdit* msgEdit);
    void writeRow(QTextStream& stream, const QStringList& rowData);
    void save_quiescent_current_test_data_to_csv();
};

#endif  // QLOG_H
