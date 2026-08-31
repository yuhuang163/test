#include "test_case_gate_accessors.h"
#include "qprotocol_types.h"
#include <mutex>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static QVector<GateTypeEntry>& gateTypeStore() {
    static QVector<GateTypeEntry> store;
    return store;
}

static bool gateFieldMatches(const GateFieldEntry& f, const QString& field) {
    if (f.id == field)
        return true;
    return f.aliases.contains(field);
}

void GateAccessorRegistry::addType(GateTypeEntry entry) {
    QVector<GateTypeEntry>& store = gateTypeStore();
    for (GateTypeEntry& existing : store) {
        if (existing.reportType == entry.reportType) {
            existing = std::move(entry);
            return;
        }
    }
    store.push_back(std::move(entry));
}

void GateAccessorRegistry::ensureInitialized() {
    static std::once_flag once;
    std::call_once(once, []() { registerAllGateAccessorTypes(); });
}

QVector<GateTypeEntry> GateAccessorRegistry::allTypes() {
    ensureInitialized();
    return gateTypeStore();
}

const GateTypeEntry* GateAccessorRegistry::findType(const QString& reportType) {
    ensureInitialized();
    for (const GateTypeEntry& t : gateTypeStore()) {
        if (t.reportType == reportType)
            return &t;
    }
    return nullptr;
}

const GateFieldEntry* GateAccessorRegistry::findField(const QString& reportType, const QString& field) {
    const GateTypeEntry* t = findType(reportType);
    if (!t)
        return nullptr;
    for (const GateFieldEntry& f : t->fields) {
        if (gateFieldMatches(f, field))
            return &f;
    }
    return nullptr;
}

bool GateAccessorRegistry::readNumber(const QString& reportType, const QString& field, const QVariant& payload,
                                      double* out) {
    const GateFieldEntry* f = findField(reportType, field);
    if (!f || !f->readNumber)
        return false;
    return f->readNumber(payload, out);
}

bool GateAccessorRegistry::readText(const QString& reportType, const QString& field, const QVariant& payload,
                                    QString* out) {
    const GateFieldEntry* f = findField(reportType, field);
    if (!f)
        return false;
    if (f->readText)
        return f->readText(payload, out);
    if (f->readNumber && out) {
        double v = 0;
        if (!f->readNumber(payload, &v))
            return false;
        *out = QString::number(v);
        return true;
    }
    return false;
}

QString GateAccessorRegistry::unitFor(const QString& reportType, const QString& field, const QVariant& payload) {
    if (reportType == QLatin1String("ProtocolMeasureData") && payload.canConvert<ProtocolMeasureData>()) {
        const QString runtimeUnit = payload.value<ProtocolMeasureData>().unit.trimmed();
        if (!runtimeUnit.isEmpty())
            return runtimeUnit;
    }
    const GateFieldEntry* f = findField(reportType, field);
    if (!f)
        return {};
    return f->unit;
}

GateTypeRaw::GateTypeRaw(const char* reportType, const char* displayName) {
    entry_.reportType = QString::fromUtf8(reportType);
    entry_.displayName = QString::fromUtf8(displayName);
}

GateTypeRaw& GateTypeRaw::numberWithReader(const char* id, const char* label, const char* unit,
                                   std::function<bool(const QVariant&, double*)> reader) {
    GateFieldEntry f;
    f.id = QString::fromUtf8(id);
    f.displayName = QString::fromUtf8(label);
    f.unit = QString::fromUtf8(unit);
    f.readNumber = reader;
    f.readText = [reader](const QVariant& payload, QString* out) -> bool {
        double v = 0;
        if (!reader || !reader(payload, &v) || !out)
            return false;
        *out = QString::number(v);
        return true;
    };
    entry_.fields.push_back(std::move(f));
    return *this;
}

GateTypeRaw& GateTypeRaw::textWithReader(const char* id, const char* label, GateCompareMode compare,
                                 std::function<bool(const QVariant&, QString*)> reader) {
    GateFieldEntry f;
    f.id = QString::fromUtf8(id);
    f.displayName = QString::fromUtf8(label);
    f.compare = compare;
    f.readText = std::move(reader);
    entry_.fields.push_back(std::move(f));
    return *this;
}

GateTypeRaw& GateTypeRaw::alias(const char* otherId) {
    if (!entry_.fields.isEmpty())
        entry_.fields.last().aliases.append(QString::fromUtf8(otherId));
    return *this;
}

GateTypeRaw& GateTypeRaw::textFormat(char fmt, int precision) {
    if (entry_.fields.isEmpty() || !entry_.fields.last().readNumber)
        return *this;
    auto numReader = entry_.fields.last().readNumber;
    entry_.fields.last().readText = [numReader, fmt, precision](const QVariant& payload, QString* out) -> bool {
        double v = 0;
        if (!numReader(payload, &v) || !out)
            return false;
        *out = QString::number(v, fmt, precision);
        return true;
    };
    return *this;
}

GateTypeRaw& GateTypeRaw::summary(std::function<QString(const QVariant&)> fn) {
    entry_.summary = std::move(fn);
    return *this;
}

void GateTypeRaw::commit() {
    GateAccessorRegistry::addType(std::move(entry_));
}
