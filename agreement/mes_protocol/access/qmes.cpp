#include "qmes.h"

#include "test_record_store.h"

Qmes::Qmes() {
}

QVector<Qmes::TestDataItem> Qmes::parseTestDataItems(const MesPacketData& pack) const {
    QVector<TestDataItem> items;
    // 与自由工站/云端一致：支持 \: 转义与 VALUE 内冒号，避免分项错位
    const QVector<TestRecordStore::ParsedItem> parsed = TestRecordStore::parseItemValue(pack);
    items.reserve(parsed.size());
    for (const TestRecordStore::ParsedItem& p : parsed) {
        TestDataItem item;
        item.name = p.name;
        item.value = p.value;
        item.maxValue = p.maxValue.isEmpty() ? pack.maxValue : p.maxValue;
        item.minValue = p.minValue.isEmpty() ? pack.minValue : p.minValue;
        item.standardValue = p.standardValue.isEmpty() ? pack.standardValue : p.standardValue;
        item.unit = p.unit.isEmpty() ? pack.unit : p.unit;
        item.result = p.result.isEmpty() ? pack.result : p.result;
        if (!item.name.isEmpty())
            items.append(item);
    }
    return items;
}
