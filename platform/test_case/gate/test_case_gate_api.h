#ifndef PLATFORM_TEST_CASE_GATE_API_H
#define PLATFORM_TEST_CASE_GATE_API_H

#include "test_case_types.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

struct GateFieldDescriptor {
    QString field;
    QString displayName;
};

struct GateTypeDescriptor {
    QString reportType;
    QString displayName;
    QVector<GateFieldDescriptor> fields;
};

/** 卡控步 MES/表格展示用的 testData 与 ask。 */
struct GateStepDisplay {
    QString testData;
    QString ask;
};

/** 当前测试指令对应的卡控回包（编辑器按此筛选，不再列出全部类型）。 */
struct GateSendBinding {
    QStringList reportTypes;
    QString defaultField;
};

class GateRegistry {
  public:
    static QStringList reportTypes();
    static QVector<GateTypeDescriptor> allTypeDescriptors();
    /** protocolOrDevice：产品协议 / 治具协议 / Modbus·SCPI 设备键。 */
    static GateSendBinding bindingForSend(TestCaseSendChannel channel, const QString& protocolOrDevice,
                                          const QString& deviceCmd);
    static bool descriptorFor(const QString& reportType, GateTypeDescriptor& out);
    static QStringList fieldsFor(const QString& reportType);
    /** Gate/Field 为 *、all 或空时，对同一回包内全部已登记字段套用相同判定条件。 */
    static bool isAllFieldsGateField(const QString& field);
    static QString fieldDisplayName(const QString& reportType, const QString& field);
    static bool evaluate(const TestCaseGate& gate, const QString& reportType, const QVariant& payload, bool& passOut,
                         QString& detailOut);
    /** 多项卡控须全部通过 */
    static bool evaluateAll(const QVector<TestCaseGate>& gates, const QString& reportType, const QVariant& payload,
                            bool& passOut, QString& detailOut);
    /** 解析 range 卡控上下限（含 LowSettingsKey/HighSettingsKey）。 */
    static void resolveRangeBounds(const TestCaseGate& gate, double& lowOut, double& highOut);
    /**
     * 界面/MES 用单位：优先 ProtocolMeasureData.unit，否则按 reportType+field 默认映射；
     * 无物理单位的文本/状态字段返回空。
     */
    static QString unitFor(const QString& reportType, const QString& field, const QVariant& payload = QVariant());
    /** 单项卡控的期望展示（范围/比较符/等值，末尾带单位）。 */
    static QString formatGateAsk(const TestCaseGate& gate, const QString& reportType,
                                 const QVariant& payload = QVariant());
    /** 多项卡控合并期望展示（各项末尾带单位）。 */
    static QString formatMultiFieldAsk(const QVector<TestCaseGate>& gates, const QString& reportType,
                                       const QVariant& payload = QVariant());
    /** 从回包与主卡控项生成步骤 testData/ask（判定逻辑仍用 evaluate/evaluateAll）。 */
    static GateStepDisplay formatStepDisplay(const TestCaseGate& primaryGate, const QVector<TestCaseGate>& allGates,
                                             const QString& reportType, const QVariant& payload, bool multiFieldMode);
};

#endif // PLATFORM_TEST_CASE_GATE_API_H
