#include "test_record_store.h"

#include "Abini.h"
#include "common_utils.h"
#include "qlog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <qdebug.h>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kLogRoot = "所有log";
constexpr const char* kDbFileName = "test_pass_data.db";
constexpr const char* kConnectionName = "test_pass_sqlite";

QMutex& storeMutex()
{
    static QMutex m;
    return m;
}

void logRecord(const QString& msg)
{
    Qlog::showlog(QStringLiteral("[TestRecord] ") + msg);
    qDebug() << QStringLiteral("[TestRecord]") << msg;
}

bool isItemResultToken(QString v)
{
    v = v.trimmed();
    if (v.isEmpty()) {
        return false;
    }
    // 兼容常见 MES/工站写法
    const QString u = v.toUpper();
    return u == QStringLiteral("PASS") || u == QStringLiteral("FAIL") || u == QStringLiteral("NG") ||
           v == QStringLiteral("通过") || v == QStringLiteral("失败") || v == QStringLiteral("不通过");
}

QString sanitizeSqlIdent(const QString& raw)
{
    QString out;
    out.reserve(raw.size());
    for (const QChar ch : raw.trimmed()) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_')) {
            out.append(ch);
        } else {
            out.append(QLatin1Char('_'));
        }
    }
    if (out.isEmpty()) {
        return QStringLiteral("unknown");
    }
    if (out.at(0).isDigit()) {
        out.prepend(QLatin1Char('c'));
    }
    return out.left(48);
}

QString stationTableName(const QString& workStation)
{
    const QString ws = sanitizeSqlIdent(workStation);
    return QStringLiteral("mes_%1").arg(ws);
}

QString itemColumnPrefix(const QString& itemName)
{
    return sanitizeSqlIdent(itemName);
}

void ensureSqlitePluginPath()
{
    const QString pluginPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("sqldrivers"));
    QCoreApplication::addLibraryPath(pluginPath);
}

QSet<QString> existingColumns(QSqlDatabase& db, const QString& tableName)
{
    QSet<QString> cols;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(tableName))) {
        return cols;
    }
    while (query.next()) {
        cols.insert(query.value(1).toString());
    }
    return cols;
}

bool addColumnIfMissing(QSqlDatabase& db, const QString& tableName, const QString& columnName)
{
    QSet<QString> cols = existingColumns(db, tableName);
    if (cols.contains(columnName)) {
        return true;
    }
    QSqlQuery query(db);
    const QString sql = QStringLiteral("ALTER TABLE \"%1\" ADD COLUMN \"%2\" TEXT").arg(tableName, columnName);
    if (!query.exec(sql)) {
        logRecord(QStringLiteral("添加列失败：%1 %2").arg(columnName, query.lastError().text()));
        return false;
    }
    return true;
}

bool ensureStationTableBase(QSqlDatabase& db, const QString& tableName)
{
    QSet<QString> required;
    required.insert(QStringLiteral("sn"));
    required.insert(QStringLiteral("item_name"));
    required.insert(QStringLiteral("item_value"));
    required.insert(QStringLiteral("max_value"));
    required.insert(QStringLiteral("min_value"));
    required.insert(QStringLiteral("standard_value"));
    required.insert(QStringLiteral("unit"));
    required.insert(QStringLiteral("item_result"));

    // 如果旧版本是“宽表(每SN一行)”而不是“长表(每项一行)”，这里直接丢弃重建
    const QSet<QString> cols = existingColumns(db, tableName);
    if (!cols.isEmpty() && !cols.contains(QStringLiteral("item_name"))) {
        QSqlQuery dropQuery(db);
        if (!dropQuery.exec(QStringLiteral("DROP TABLE IF EXISTS \"%1\"").arg(tableName))) {
            logRecord(QStringLiteral("删除旧工站表失败：%1 %2").arg(tableName, dropQuery.lastError().text()));
            return false;
        }
    }

    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS \"%1\" ("
        "sn TEXT NOT NULL,"
        "item_name TEXT NOT NULL,"
        "item_value TEXT,"
        "max_value TEXT,"
        "min_value TEXT,"
        "standard_value TEXT,"
        "unit TEXT,"
        "item_result TEXT,"
        "PRIMARY KEY (sn, item_name))")
                            .arg(tableName);

    if (!query.exec(sql)) {
        logRecord(QStringLiteral("建工站表失败：%1 %2").arg(tableName, query.lastError().text()));
        return false;
    }

    // 若表存在但列不完整（比如用户改过代码又没删库），尽量补齐列
    for (const QString& c : required) {
        if (!cols.contains(c)) {
            (void)addColumnIfMissing(db, tableName, c);
        }
    }

    return true;
}

}  // namespace

TestRecordStore& TestRecordStore::instance()
{
    static TestRecordStore self;
    return self;
}

TestRecordStore::TestRecordStore() = default;

QString TestRecordStore::databasePath() const
{
    // 固定相对 exe 目录，避免 Qt Creator 工作目录与 bin 不一致导致库写到别处
    const QString logDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("所有log"));  // 与 qlog.cpp 一致
    if (!CommonUtils::ensureDirectory(logDir)) {
        return {};
    }
    return QDir(logDir).filePath(QString::fromLatin1(kDbFileName));
}

bool TestRecordStore::ensureOpen()
{
    if (opened_) {
        return true;
    }

    ensureSqlitePluginPath();

    const QString dbPath = databasePath();
    if (dbPath.isEmpty()) {
        logRecord(QStringLiteral("无法创建日志目录，跳过数据库打开"));
        return false;
    }

    if (!QSqlDatabase::contains(QLatin1String(kConnectionName))) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kConnectionName));
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            logRecord(QStringLiteral("打开数据库失败：%1 路径=%2").arg(db.lastError().text(), dbPath));
            QSqlDatabase::removeDatabase(QLatin1String(kConnectionName));
            return false;
        }
    } else {
        QSqlDatabase db = QSqlDatabase::database(QLatin1String(kConnectionName));
        if (!db.isOpen() && !db.open()) {
            logRecord(QStringLiteral("重新打开数据库失败：%1").arg(db.lastError().text()));
            return false;
        }
    }

    opened_ = ensurePassTable();
    if (opened_) {
        logRecord(QStringLiteral("数据库已就绪：") + dbPath);
    }
    return opened_;
}

bool TestRecordStore::ensurePassTable()
{
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(kConnectionName));
    QSqlQuery query(db);
    const bool okPass = query.exec(
        QStringLiteral("CREATE TABLE IF NOT EXISTS mes_test_pass ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "created_at TEXT NOT NULL,"
                       "mechines INTEGER,"
                       "sn TEXT,"
                       "factory TEXT,"
                       "work_station TEXT,"
                       "machine_no TEXT,"
                       "product TEXT,"
                       "lot_name TEXT,"
                       "user_no TEXT,"
                       "test_result TEXT,"
                       "itemvalue_raw TEXT)"));
    if (!okPass) {
        logRecord(QStringLiteral("建表 mes_test_pass 失败：") + query.lastError().text());
        return false;
    }
    return true;
}

QVector<TestRecordStore::ParsedItem> TestRecordStore::parseItemValue(const MesPacketData& pack)
{
    QVector<ParsedItem> items;
    QString inner = pack.itemvalue.trimmed();
    if (inner.length() >= 2 && inner.startsWith(QLatin1Char('|')) && inner.endsWith(QLatin1Char('|'))) {
        inner = inner.mid(1, inner.length() - 2);
    }

    const QStringList segments = inner.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString& segment : segments) {
        const QString kv = segment.trimmed();
        if (kv.isEmpty()) {
            continue;
        }

        ParsedItem item;
        // 默认不填 item_result，只有检测到“显式结果字段”才写入
        item.result = QString();
        item.value.clear();
        item.maxValue.clear();
        item.minValue.clear();
        item.standardValue.clear();
        item.unit.clear();
        const QStringList parts = kv.split(QLatin1Char(':'), QString::KeepEmptyParts);
        if (parts.size() >= 6) {
            item.name = parts.at(0).trimmed();
            item.value = parts.at(1).trimmed();
            item.maxValue = parts.at(2).trimmed();
            item.minValue = parts.at(3).trimmed();
            item.standardValue = parts.at(4).trimmed();
            item.unit = parts.at(5).trimmed();
            if (parts.size() >= 7) {
                item.result = parts.at(6).trimmed();
            }
        } else if (parts.size() == 2) {
            item.name = parts.at(0).trimmed();
            const QString v = parts.at(1).trimmed();
            // 形如 "ITEM:PASS/FAIL/NG"：把 item_result 放进去，item_value 留空
            if (isItemResultToken(v)) {
                item.result = v;
            } else {
                item.value = v;
            }
        } else {
            const int colon = kv.indexOf(QLatin1Char(':'));
            if (colon <= 0) {
                continue;
            }
            item.name = kv.left(colon).trimmed();
            const QString v = kv.mid(colon + 1).trimmed();
            if (isItemResultToken(v)) {
                item.result = v;
            } else {
                item.value = v;
            }
        }

        if (!item.name.isEmpty()) {
            items.append(item);
        }
    }
    return items;
}

bool TestRecordStore::upsertStationRow(const QString& workStation, const MesPacketData& pack,
                                       const QVector<ParsedItem>& items)
{
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(kConnectionName));
    const QString tableName = stationTableName(workStation);
    if (!ensureStationTableBase(db, tableName)) {
        return false;
    }
    const QString sn = pack.sn.trimmed();
    if (sn.isEmpty()) {
        logRecord(QStringLiteral("sn 为空，跳过工站表写入"));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO \"%1\" "
                                  "(sn, item_name, item_value, max_value, min_value, standard_value, unit, item_result) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
                       .arg(tableName));

    for (const ParsedItem& item : items) {
        query.addBindValue(sn);
        query.addBindValue(item.name);
        query.addBindValue(item.value);
        query.addBindValue(item.maxValue);
        query.addBindValue(item.minValue);
        query.addBindValue(item.standardValue);
        query.addBindValue(item.unit);
        query.addBindValue(item.result);

        if (!query.exec()) {
            logRecord(QStringLiteral("写入工站项失败：%1 sn=%2 item=%3 %4")
                          .arg(tableName, sn, item.name, query.lastError().text()));
            return false;
        }
    }

    return true;
}

bool TestRecordStore::saveOnTestPass(const MesPacketData& pack)
{
    QMutexLocker locker(&storeMutex());
    if (!ensureOpen()) {
        return false;
    }

    const QString workStation = SETTINGS.value(QStringLiteral("SYSTEM/station")).toString().trimmed();
    const QVector<ParsedItem> items = parseItemValue(pack);
    const QString sn = pack.sn.trimmed();

    QSqlDatabase db = QSqlDatabase::database(QLatin1String(kConnectionName));

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO mes_test_pass (created_at, mechines, sn, factory, work_station, machine_no, "
                       "product, lot_name, user_no, test_result, itemvalue_raw) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")));
    query.addBindValue(pack.mechines);
    query.addBindValue(sn);
    query.addBindValue(pack.factory.trimmed());
    query.addBindValue(workStation);
    query.addBindValue(pack.machineNo.trimmed());
    query.addBindValue(pack.product.trimmed());
    query.addBindValue(pack.lotName.trimmed());
    query.addBindValue(pack.userNo.trimmed());
    query.addBindValue(pack.result.trimmed());
    query.addBindValue(pack.itemvalue);

    if (!query.exec()) {
        logRecord(QStringLiteral("插入过站主记录失败：") + query.lastError().text());
        return false;
    }

    const qint64 passId = query.lastInsertId().toLongLong();

    // 工站表失败不回滚主表，避免整笔丢失
    const bool stationOk = upsertStationRow(workStation, pack, items);
    if (stationOk) {
        logRecord(QStringLiteral("过站已入库 pass_id=%1 table=%2 sn=%3 result=%4 items=%5")
                      .arg(passId)
                      .arg(stationTableName(workStation))
                      .arg(sn)
                      .arg(pack.result.trimmed())
                      .arg(items.size()));
    } else {
        logRecord(QStringLiteral("主表已写入 pass_id=%1，工站表失败 sn=%2 result=%3")
                      .arg(passId)
                      .arg(sn.isEmpty() ? QStringLiteral("(空)") : sn)
                      .arg(pack.result.trimmed()));
    }
    return true;
}
