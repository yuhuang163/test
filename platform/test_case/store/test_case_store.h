#ifndef PLATFORM_TEST_CASE_STORE_H
#define PLATFORM_TEST_CASE_STORE_H

#include "test_case_types.h"

#include <QString>
#include <QStringList>
#include <QVector>

class TestCaseStore {
  public:
    static bool loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut = nullptr);
    /** 合并 steps 库 + profiles/{stationKey}/steps 覆盖；stationKey 为空则仅库/旧平铺 ini */
    static bool loadCaseForStation(const QString& stationKey, const QString& stepId, TestCaseDefinition& out,
                                   QString* errorOut = nullptr);
    /** 仅写入总步骤库 test_case/steps/（旧平铺 ini 同步一份） */
    static bool saveCase(const TestCaseDefinition& def, QString* errorOut = nullptr);
    /**
     * stationKey 非空：只写入 profiles/{工站}/steps/，不改动总步骤库；
     * stationKey 为空：等同 saveCase，写总步骤库。
     */
    static bool saveCaseForStation(const QString& stationKey, const TestCaseDefinition& def,
                                   QString* errorOut = nullptr);
    /** 将旧平铺 ini 迁入 steps/，并为各工站生成 profiles 目录（幂等） */
    static void ensureFilesystemLayout();
    /** 云端下载/外部覆盖 profiles 后：按目录重扫并登记 FlowStations（不依赖进程内一次性迁移标志） */
    static void reregisterFlowStationsFromProfiles();
    /** 运行时实际参与判定的卡控列表（gates 优先，否则单项 gate） */
    static QVector<TestCaseGate> effectiveGates(const TestCaseDefinition& def);
    /** 运行时参与判定的卡控项（case ini 中 Gate/N/Enabled） */
    static QVector<TestCaseGate> activeGatesForEvaluation(const TestCaseDefinition& def);
    static bool usesMultiFieldGates(const TestCaseDefinition& def);
    static QStringList listCaseIniNames();
    /** MES 分项键（MesTag 或 Name）→ 云端展示名（Meta/DisplayName 优先） */
    static QString cloudDisplayNameForItemKey(const QString& itemKey);
    static void invalidateCloudItemNameCache();
    static bool loadFlowMeta(TestFlowMeta& out);
    /** @deprecated 工站选择请用 saveSelectedFlowStation；此方法不再写入总的测试流程.ini */
    static bool saveFlowMeta(const TestFlowMeta& meta);
    /** 当前选中工站（仅存上位机设置.local.ini / TestOrderMeta） */
    static QString loadSelectedFlowStationKey();
    static QString loadSelectedFlowStationName();
    static void saveSelectedFlowStation(const QString& stationKey, const QString& displayName);
    /** 旧版总的测试流程.ini [Meta] 一次性迁入本地设置并清除 */
    static void migrateLegacyFlowMetaToLocalSettings();
    static QStringList loadStationItems(const QString& stationKey);
    static bool saveStationItems(const QString& stationKey, const QStringList& items);
    static QVector<TestFlowItemEntry> loadStationFlowItems(const QString& stationKey);
    /** 测试失败执行区域步骤（FailItems） */
    static QVector<TestFlowItemEntry> loadStationFailFlowItems(const QString& stationKey);
    /** 工站级：任一步测试失败是否结束整单流程（默认 true）；为 true 时会接着跑 FailItems */
    static bool loadStationStopFlowOnTestFail(const QString& stationKey, bool defaultValue = true);
    /** 工站 flow.ini [SerialUi]：治具/产品/万用表串口显隐与显示名 */
    static TestCaseSerialUiConfig loadStationSerialUiConfig(const QString& stationKey);
    static bool saveStationSerialUiConfig(const QString& stationKey, const TestCaseSerialUiConfig& config);
    /** 工站 flow.ini [DeviceSide]：三元组位置 / device_side_id */
    static TestCaseDeviceSideConfig loadStationDeviceSideConfig(const QString& stationKey);
    static bool saveStationDeviceSideConfig(const QString& stationKey, const TestCaseDeviceSideConfig& config);
    /** profiles/{工站}/profile.ini [Profile/ProfileVersion]；stationKey 空则当前选中工站，缺失返回 0 */
    static int loadStationProfileVersion(const QString& stationKey = QString());
    static bool saveStationFlowItems(const QString& stationKey, const QVector<TestFlowItemEntry>& items,
                                     bool stopFlowOnTestFail = true);
    static bool saveStationFlowItems(const QString& stationKey, const QVector<TestFlowItemEntry>& items,
                                     const QVector<TestFlowItemEntry>& failItems, bool stopFlowOnTestFail);
    static QStringList listStationKeysFromFlow();

    /** 内置工站（与测试流程编排页预设一致，并含 default / FREE_WORK） */
    static QVector<TestFlowStationEntry> defaultFlowStationPresets();
    /** 从 总的测试流程.ini [FlowStations] 读取；无记录时写入预设并返回 */
    static QVector<TestFlowStationEntry> loadFlowStationCatalog();
    /**
     * 工站显示名是否属于指定产品（Mes/Product_Name）。
     * 自由工站/默认工站通用；其余须以完整产品名开头（见 CommonUtils::stationBelongsToProduct）。
     */
    static bool stationBelongsToProduct(const QString& stationDisplayName, const QString& productName);
    /** 按产品过滤工站目录；productName 空则不过滤。 */
    static QVector<TestFlowStationEntry> loadFlowStationCatalogForProduct(const QString& productName);
    static bool saveFlowStationCatalog(const QVector<TestFlowStationEntry>& entries);
    static QString flowStationDisplayName(const QString& stationKey);
    /** 显示名或已有键 → 流程 ini 使用的工站键（预设如 自由工站→FREE_WORK） */
    static QString resolveFlowStationKey(const QString& displayNameOrKey);
    static bool addFlowStation(const QString& displayName, QString* errorOut = nullptr);
    /** 复制工站流程（功能块列表与 StopFlowOnTestFail）到新工站。 */
    static bool copyFlowStation(const QString& sourceStationKey, const QString& newDisplayName,
                                const QVector<TestFlowItemEntry>& items, bool stopFlowOnTestFail,
                                QString* outNewKey = nullptr, QString* errorOut = nullptr);
    static bool renameFlowStation(const QString& stationKey, const QString& newDisplayName,
                                  QString* errorOut = nullptr);
    static bool removeFlowStation(const QString& stationKey, QString* errorOut = nullptr);
};

#endif // PLATFORM_TEST_CASE_STORE_H
