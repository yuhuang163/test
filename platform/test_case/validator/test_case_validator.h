#ifndef PLATFORM_TEST_CASE_VALIDATOR_H
#define PLATFORM_TEST_CASE_VALIDATOR_H

#include "test_case_types.h"

#include <QString>
#include <QStringList>
#include <QVector>

class TestCaseValidator {
  public:
    static bool validateCase(const TestCaseDefinition& def, QStringList& errors);
    /**
     * 校验当前流程内各步「上报MES的字段」必填且互不重复；错误信息带第几步与步骤名。
     * @param overrideOriginalCaseName 正在编辑的步骤原配置名（可空，新步骤为空）
     * @param overrideDef 正在编辑、尚未落盘的定义（可空）；用于编辑对话框即时校验
     */
    static bool validateFlowMesTags(const QString& stationKey, const QVector<TestFlowItemEntry>& entries,
                                    QStringList& errors, const QString& overrideOriginalCaseName = QString(),
                                    const TestCaseDefinition* overrideDef = nullptr);
};

#endif // PLATFORM_TEST_CASE_VALIDATOR_H
