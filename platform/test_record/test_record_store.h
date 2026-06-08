#ifndef TEST_RECORD_STORE_H
#define TEST_RECORD_STORE_H

#include "my_set/my_typedef.h"

#include <QString>
#include <QVector>

/**
 * 过站时本地 SQLite：所有log/test_pass_data.db
 * - mes_test_pass：每次过站流水
 * - mes_<工站名>：按 SYSTEM/station 分表，sn 唯一，一行汇总 itemvalue 分项列
 */
class TestRecordStore {
public:
    struct ParsedItem {
        QString name;
        QString value;
        QString maxValue;
        QString minValue;
        QString standardValue;
        QString unit;
        QString result;
    };

    static TestRecordStore& instance();

    bool saveOnTestPass(const MesPacketData& pack);

    static QVector<ParsedItem> parseItemValue(const MesPacketData& pack);

private:
    TestRecordStore();
    TestRecordStore(const TestRecordStore&) = delete;
    TestRecordStore& operator=(const TestRecordStore&) = delete;

    bool ensureOpen();
    bool ensurePassTable();
    bool upsertStationRow(const QString& workStation, const MesPacketData& pack, const QVector<ParsedItem>& items);
    QString databasePath() const;

    bool opened_ = false;
};

#endif  // TEST_RECORD_STORE_H
