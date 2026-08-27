#ifndef TEST_CASE_GATE_ACCESSORS_H
#define TEST_CASE_GATE_ACCESSORS_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <functional>

/** 等值比较策略（登记在字段上，evaluate 查表）。 */
enum class GateCompareMode {
    Default,
    MacNormalize,
    Version,
    ScreenColor,
};

struct GateFieldEntry {
    QString id;
    QString displayName;
    QString unit;
    /** 判定详情里「当前值」类前缀；空则默认「当前值」（吸力峰值用「最低峰值」）。 */
    QString valueLabel;
    GateCompareMode compare = GateCompareMode::Default;
    QStringList aliases;
    std::function<bool(const QVariant& payload, double* out)> readNumber;
    std::function<bool(const QVariant& payload, QString* out)> readText;
    /** 纯数值 → 展示文案（如纯色编号→颜色名；期望阈值也走这里）。 */
    std::function<QString(double value)> formatValue;
};

struct GateTypeEntry {
    QString reportType;
    QString displayName;
    QVector<GateFieldEntry> fields;
    std::function<QString(const QVariant& payload)> summary;
};

/** 全局字段登记表（由 GateType<>::commit 填充）。 */
class GateAccessorRegistry {
  public:
    static void ensureInitialized();
    static void addType(GateTypeEntry entry);
    static QVector<GateTypeEntry> allTypes();
    static const GateTypeEntry* findType(const QString& reportType);
    static const GateFieldEntry* findField(const QString& reportType, const QString& field);
    static bool readNumber(const QString& reportType, const QString& field, const QVariant& payload, double* out);
    static bool readText(const QString& reportType, const QString& field, const QVariant& payload, QString* out);
    static QString unitFor(const QString& reportType, const QString& field, const QVariant& payload = QVariant());
};

/**
 * 流畅登记：GateType<D>("ReportType","中文名").number(...).mac(...).commit();
 * 业务只改这一处链式调用。
 */
template <typename D>
class GateType {
  public:
    GateType(const char* reportType, const char* displayName)
        : unpack_(defaultUnpack) {
        entry_.reportType = QString::fromUtf8(reportType);
        entry_.displayName = QString::fromUtf8(displayName);
    }

    using UnpackFn = D (*)(const QVariant& payload, bool* ok);

    GateType& withUnpack(UnpackFn fn) {
        unpack_ = fn ? fn : defaultUnpack;
        return *this;
    }

    template <typename M>
    GateType& number(const char* id, M D::*mem, const char* label, const char* unit) {
        GateFieldEntry f;
        f.id = QString::fromUtf8(id);
        f.displayName = QString::fromUtf8(label);
        f.unit = QString::fromUtf8(unit);
        f.compare = GateCompareMode::Default;
        UnpackFn unpack = unpack_;
        f.readNumber = [unpack, mem](const QVariant& payload, double* out) -> bool {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok || !out)
                return false;
            *out = static_cast<double>(d.*mem);
            return true;
        };
        // 文本默认由数值生成；需要定制格式时再用 textFormat / textFn
        f.readText = [unpack, mem](const QVariant& payload, QString* out) -> bool {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok || !out)
                return false;
            *out = QString::number(static_cast<double>(d.*mem));
            return true;
        };
        entry_.fields.push_back(std::move(f));
        return *this;
    }

    GateType& text(const char* id, QString D::*mem, const char* label) {
        GateFieldEntry f;
        f.id = QString::fromUtf8(id);
        f.displayName = QString::fromUtf8(label);
        f.compare = GateCompareMode::Default;
        UnpackFn unpack = unpack_;
        f.readText = [unpack, mem](const QVariant& payload, QString* out) -> bool {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok || !out)
                return false;
            *out = (d.*mem).trimmed();
            return true;
        };
        entry_.fields.push_back(std::move(f));
        return *this;
    }

    GateType& mac(const char* id, QString D::*mem, const char* label) {
        text(id, mem, label);
        entry_.fields.last().compare = GateCompareMode::MacNormalize;
        return *this;
    }

    GateType& version(const char* id, QString D::*mem, const char* label) {
        text(id, mem, label);
        entry_.fields.last().compare = GateCompareMode::Version;
        return *this;
    }

    /** 自定义数值（数组下标、items.first 等）。 */
    GateType& numberFn(const char* id, const char* label, const char* unit,
                       std::function<bool(const D& d, double* out)> reader) {
        GateFieldEntry f;
        f.id = QString::fromUtf8(id);
        f.displayName = QString::fromUtf8(label);
        f.unit = QString::fromUtf8(unit);
        UnpackFn unpack = unpack_;
        f.readNumber = [unpack, reader](const QVariant& payload, double* out) -> bool {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok)
                return false;
            return reader(d, out);
        };
        f.readText = [unpack, reader](const QVariant& payload, QString* out) -> bool {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok || !out)
                return false;
            double v = 0;
            if (!reader(d, &v))
                return false;
            *out = QString::number(v);
            return true;
        };
        entry_.fields.push_back(std::move(f));
        return *this;
    }

    /** 自定义文本（新建字段）。 */
    GateType& textFn(const char* id, const char* label, GateCompareMode compare,
                     std::function<bool(const D& d, QString* out)> reader) {
        GateFieldEntry f;
        f.id = QString::fromUtf8(id);
        f.displayName = QString::fromUtf8(label);
        f.compare = compare;
        UnpackFn unpack = unpack_;
        f.readText = [unpack, reader](const QVariant& payload, QString* out) -> bool {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok)
                return false;
            return reader(d, out);
        };
        entry_.fields.push_back(std::move(f));
        return *this;
    }

    /** 覆盖上一字段的文本读取（配合 number()+screenColor() 使用）。 */
    GateType& overrideText(std::function<bool(const D& d, QString* out)> reader) {
        if (entry_.fields.isEmpty())
            return *this;
        UnpackFn unpack = unpack_;
        entry_.fields.last().readText = [unpack, reader](const QVariant& payload, QString* out) -> bool {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok)
                return false;
            return reader(d, out);
        };
        return *this;
    }

    /** 上一字段追加旧 ini 别名。 */
    GateType& alias(const char* otherId) {
        if (!entry_.fields.isEmpty())
            entry_.fields.last().aliases.append(QString::fromUtf8(otherId));
        return *this;
    }

    /** 上一数值字段的文本格式（如 'f',2）。 */
    GateType& textFormat(char fmt, int precision) {
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

    /** 上一字段改为 ScreenColor 比较。 */
    GateType& screenColor() {
        if (!entry_.fields.isEmpty())
            entry_.fields.last().compare = GateCompareMode::ScreenColor;
        return *this;
    }

    /** 上一字段：数值展示格式（期望阈值与实测共用）。 */
    GateType& formatValue(std::function<QString(double)> fn) {
        if (!entry_.fields.isEmpty())
            entry_.fields.last().formatValue = std::move(fn);
        return *this;
    }

    /** 上一字段：判定详情数值前缀（默认「当前值」）。 */
    GateType& valueLabel(const char* label) {
        if (!entry_.fields.isEmpty())
            entry_.fields.last().valueLabel = QString::fromUtf8(label);
        return *this;
    }

    GateType& summary(std::function<QString(const D& d)> fn) {
        UnpackFn unpack = unpack_;
        entry_.summary = [unpack, fn](const QVariant& payload) -> QString {
            bool ok = false;
            const D d = unpack(payload, &ok);
            if (!ok)
                return {};
            return fn(d);
        };
        return *this;
    }

    void commit() {
        GateAccessorRegistry::addType(std::move(entry_));
    }

  private:
    static D defaultUnpack(const QVariant& payload, bool* ok) {
        if (ok)
            *ok = payload.canConvert<D>();
        if (ok && !*ok)
            return D{};
        return payload.value<D>();
    }

    GateTypeEntry entry_;
    UnpackFn unpack_;
};

/** 无结构体模板时用（仅 numberFn/textFn 基于 QVariant）。 */
class GateTypeRaw {
  public:
    GateTypeRaw(const char* reportType, const char* displayName);

    GateTypeRaw& numberFn(const char* id, const char* label, const char* unit,
                          std::function<bool(const QVariant& payload, double* out)> reader);
    GateTypeRaw& textFn(const char* id, const char* label, GateCompareMode compare,
                        std::function<bool(const QVariant& payload, QString* out)> reader);
    GateTypeRaw& alias(const char* otherId);
    GateTypeRaw& textFormat(char fmt, int precision);
    GateTypeRaw& summary(std::function<QString(const QVariant& payload)> fn);
    void commit();

  private:
    GateTypeEntry entry_;
};

/** 各协议登记入口（test_case_gate_types.cpp）。 */
void registerAllGateAccessorTypes();

#endif // TEST_CASE_GATE_ACCESSORS_H
