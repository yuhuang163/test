#include "qmes.h"

#include <QStringList>

Qmes::Qmes() {}

QVector<Qmes::TestDataItem> Qmes::parseTestDataItems(const MesPacketData& pack) const {
    QVector<TestDataItem> items;

    // itemvalue：多段用 | 分隔；整串可带首尾 |，也可直接「项1|项2」。
    QString inner = pack.itemvalue.trimmed();
    if (inner.length() >= 2 && inner.startsWith(QLatin1Char('|')) && inner.endsWith(QLatin1Char('|'))) {
        inner = inner.mid(1, inner.length() - 2);
    }

    const QStringList keyValuePairs = inner.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString& keyValue : keyValuePairs) {
        const QString kv = keyValue.trimmed();
        if (kv.isEmpty()) {
            continue;
        }

        TestDataItem item;
        item.maxValue = pack.maxValue;
        item.minValue = pack.minValue;
        item.standardValue = pack.standardValue;
        item.unit = pack.unit;
        item.result = pack.result;
        // 兼容「NAME:VALUE」和「NAME:VALUE:MAX:MIN:STANDARD:UNIT」两种上传格式。
        const QStringList parts = kv.split(QLatin1Char(':'), QString::KeepEmptyParts);
        if (parts.size() >= 6) {
            item.name = parts.at(0).trimmed();
            item.value = parts.at(1).trimmed();
            item.maxValue = parts.at(2);
            item.minValue = parts.at(3);
            item.standardValue = parts.at(4);
            item.unit = parts.at(5);
            if (parts.size() >= 7) {
                item.result = parts.at(6);
            }
        } else if (parts.size() == 2) {
            item.name = parts.at(0).trimmed();
            item.value = parts.at(1).trimmed();
        } else {
            const int colon = kv.indexOf(QLatin1Char(':'));
            if (colon <= 0) {
                continue;
            }
            item.name = kv.left(colon).trimmed();
            item.value = kv.mid(colon + 1).trimmed();
        }

        if (!item.name.isEmpty()) {
            items.append(item);
        }
    }

    return items;
}
