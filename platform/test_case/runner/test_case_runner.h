#ifndef PLATFORM_TEST_CASE_RUNNER_H
#define PLATFORM_TEST_CASE_RUNNER_H

#include "test_case_types.h"

#include <QString>
#include <QStringList>

class QFreeWork;

class TestCaseRunner {
  public:
    static QStringList loadFlowForStation(const QString& stationKey);
    static bool loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut = nullptr);
    static bool loadCaseForStation(const QString& stationKey, const QString& stepId, TestCaseDefinition& out,
                                   QString* errorOut = nullptr);
    static void beginStep(QFreeWork* ctx, const TestCaseDefinition& def);
    static QString stepLabel(const TestCaseDefinition& def);
    /** 本步是否必须等异步 markDone 才过步（Gate/连接/采样等）；与 sendCommandWithRetry::allowResend 无关 */
    static bool needAsyncDone(const TestCaseDefinition& def);
    /** Dongle 扫描/直连蓝牙：需等待连接成功，不能发完即过步 */
    static bool isDongleBleConnectStep(const TestCaseDefinition& def);
    /** 本步是否必须通过已连接的产品 BLE 收发协议（仅此类步骤在未连蓝牙时阻塞流程） */
    static bool stepRequiresProductBle(const TestCaseDefinition& def);
    /** 无卡控的提示步：先点「是」再发指令；有卡控时弹窗只提示、立刻发并等上报 */
    static bool stepWaitsForPromptAck(const TestCaseDefinition& def);
    /** 本 case 指令等待/重试间隔(ms) */
    static int commandTimeoutMs(const TestCaseDefinition& def);
};

#endif // PLATFORM_TEST_CASE_RUNNER_H
