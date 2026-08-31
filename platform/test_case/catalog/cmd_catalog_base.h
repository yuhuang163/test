#ifndef PLATFORM_CMD_CATALOG_BASE_H
#define PLATFORM_CMD_CATALOG_BASE_H

#include "cmd_manifest_common.h"
#include "test_case_types.h"
#include <QHash>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

enum class CmdCatalogUiLabelMode { Picker, Raw };
enum class CmdCatalogCmdToNameFallback { Numeric, Empty };
enum class CmdCatalogParamHintStyle { Full, HintOnly };
enum class CmdCatalogParamIniProfile { None, Standard, WithUIntAndLegacyJson, Custom };

/** 指令目录 UI 文案助手。 */
class CmdCatalogUi {
  public:
    static QString pickerDisplayLabel(QString label);
};

/** 标准 Send/Param ini 读写。 */
class CmdCatalogParamIni {
  public:
    static bool readFromIni(CmdCatalogParamIniProfile profile, const QSettings& settings, DeviceCmdParamKind kind,
                            QVariant& out);
    static void writeToIni(CmdCatalogParamIniProfile profile, QSettings& settings, DeviceCmdParamKind kind,
                           const QVariant& value);
};

/** 从 *CmdManifest 表构建的统一登记数据。 */
struct CmdManifestRegistry {
    struct Row {
        int cmd = 0;
        const char* enumName = nullptr;
        const char* uiLabel = nullptr;
        DeviceCmdParamKind paramKind = DeviceCmdParamKind::None;
        const char* paramHint = nullptr;
        uint8_t sendActions = 0;
    };

    struct Policy {
        bool filterBySendAction = true;
        CmdCatalogUiLabelMode uiLabelMode = CmdCatalogUiLabelMode::Picker;
        CmdCatalogCmdToNameFallback cmdToNameFallback = CmdCatalogCmdToNameFallback::Numeric;
        CmdCatalogParamHintStyle paramHintStyle = CmdCatalogParamHintStyle::Full;
        CmdCatalogParamIniProfile paramIniProfile = CmdCatalogParamIniProfile::Standard;
        QString unknownCmdLabel;
        QString unknownNameParamHint;
        QString unregisteredParamHint;
        TestCaseSendAction missingCmdDefaultAction = TestCaseSendAction::Set;
    };

    Policy policy;
    QVector<Row> rows;

    void finalize();
    const Row* findByCmd(int cmd) const;
    const Row* findByEnumName(const QString& name) const;

  private:
    QHash<int, int> cmdIndex_;
    QHash<QString, int> nameIndex_;
};

/**
 * 指令目录基类：查表、Set/Get 过滤、UI 文案、标准 ini 参数读写。
 * 各通道在 catalogs.cpp 里继承此类，只 override 有差异的 paramFromIniGroup / paramToIniGroup。
 */
class CmdManifestCatalog {
  public:
    explicit CmdManifestCatalog(CmdManifestRegistry registry);
    virtual ~CmdManifestCatalog() = default;

    QStringList allCmdNames(TestCaseSendAction action) const;
    TestCaseSendAction actionFor(int cmd) const;
    bool isCmdForAction(int cmd, TestCaseSendAction action) const;
    QString cmdUiLabel(const QString& enumName) const;
    bool cmdFromName(const QString& name, int& out) const;
    QString cmdToName(int cmd) const;
    bool paramSchemaFor(int cmd, DeviceCmdParamSchema& out) const;
    QString paramUiHint(const QString& enumName) const;

    virtual bool paramFromIniGroup(const QSettings& settings, int cmd, QVariant& out) const;
    virtual void paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const;

  protected:
    const CmdManifestRegistry& registry() const { return registry_; }

  private:
    CmdManifestRegistry registry_;
};

/** 将 catalog 查到的 int 指令转为强类型枚举。 */
template <typename CmdEnum>
bool cmdEnumFromName(const CmdManifestCatalog& catalog, const QString& name, CmdEnum& out) {
    int cmd = 0;
    if (!catalog.cmdFromName(name, cmd))
        return false;
    out = static_cast<CmdEnum>(cmd);
    return true;
}

#endif // PLATFORM_CMD_CATALOG_BASE_H
