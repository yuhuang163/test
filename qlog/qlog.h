#ifndef QLOG_H
#define QLOG_H
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
    void showlog(QString msg, int mechine, QPlainTextEdit* msgEdit);
    void writeRow(QTextStream& stream, const QStringList& rowData);
    void save_quiescent_current_test_data_to_csv();
};

#endif  // QLOG_H
