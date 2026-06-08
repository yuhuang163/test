#include "qfreework.h"

#include "test_case.h"

#include "qusb.h"
#include "qprotocol_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

/** FREE_INSTR_CMW_GPRF_* HookId → brush profile 0～5；兼容旧 Profile0～5 与新区段命名。 */
int freeInstrCmwGprfHookProfile(const QString& hookId) {
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P0") || hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_2402_1M"))
        return 0;
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P1") || hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_2440_1M"))
        return 1;
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P2") || hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_2480_1M"))
        return 2;
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P3") || hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_2402_2M"))
        return 3;
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P4") || hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_2440_2M"))
        return 4;
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P5") || hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_2480_2M"))
        return 5;
    return -1;
}

} // namespace

/** 友元类静态成员可访问 QFreeWork 私有接口；lambda 不行，故用 dispatch。 */
class QFreeWorkTestCaseHookRegistrar {
  public:
    static void registerAll();
    static void dispatch(QFreeWork* fw, const QString& hookId);

  private:
    static void registerHook(const QString& hookId);
};

void QFreeWorkTestCaseHookRegistrar::registerHook(const QString& hookId) {
    TestCaseHookRegistry::registerHook(hookId, [hookId](QFreeWork* fw) { dispatch(fw, hookId); });
}

void QFreeWorkTestCaseHookRegistrar::dispatch(QFreeWork* fw, const QString& hookId) {
    if (!fw)
        return;

    if (hookId == QStringLiteral("JIG_CURRENT_READ")) {
        fw->sendCommandWithRetry([&]() { fw->usb->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement); });
        return;
    }
    if (hookId == QStringLiteral("SN_WRITE_TAIL")) {
        const QByteArray tailSn = fw->resolvedTailSnToWrite();
        if (tailSn.isEmpty()) {
            fw->stepRuntime_.done = true;
            fw->stepRuntime_.pass = false;
            fw->stepRuntime_.testData = QStringLiteral("整机SN为空");
            fw->TestResult = fw->failValue;
            fw->showlog(QStringLiteral("写入SN码失败：请先在 SN 框扫入整机码(MES 或离线)，再扫 MAC 开始测试"));
            return;
        }
        fw->stepRuntime_.testData = QString::fromUtf8(tailSn);
        fw->setCommandWaitSource(CommandWaitSource::ProductProtocol);
        fw->sendCommandWithRetry([fw, tailSn]() {
            fw->protocolManager.set(DeviceCmd::Sn,
                                    QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, tailSn}));
        });
        return;
    }
    if (hookId == QStringLiteral("PLC_MODBUS_CONN")) {
        fw->runPlcModbusConnectTest();
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_SWITCH_RIGHT_WHOLE")) {
        fw->startPlcSwitchPlcAndWaitRightRotate();
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_SWITCH_DONE_RESET_M")) {
        fw->runPlcSwitchTestDoneResetM();
        return;
    }
    if (hookId == QStringLiteral("KEY_POWER")) {
        fw->startKeyButtonTest(QStringLiteral("电源键测试"), QStringLiteral("请短按下旋钮"),
                               QStringLiteral("ProductInfo/KeyIdPower"), QStringLiteral("ProductInfo/KeyIdPower_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_START_PAUSE")) {
        fw->startKeyButtonTest(QStringLiteral("开始/暂停键测试"), QStringLiteral("请短按下开始/暂停按钮"),
                               QStringLiteral("ProductInfo/KeyIdStartPause"),
                               QStringLiteral("ProductInfo/KeyIdStartPause_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_MODE")) {
        fw->startKeyButtonTest(QStringLiteral("模式键测试"), QStringLiteral("请短按下模式按钮"),
                               QStringLiteral("ProductInfo/KeyIdMode"), QStringLiteral("ProductInfo/KeyIdMode_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_SPEED")) {
        fw->startKeyButtonTest(QStringLiteral("速度键测试"), QStringLiteral("请短按下速度按钮"),
                               QStringLiteral("ProductInfo/KeyIdSpeed"), QStringLiteral("ProductInfo/KeyIdSpeed_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_PROGRAM")) {
        fw->startKeyButtonTest(QStringLiteral("程序键测试"), QStringLiteral("请短按下程序按钮"),
                               QStringLiteral("ProductInfo/KeyIdProgram"),
                               QStringLiteral("ProductInfo/KeyIdProgram_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_LEFT")) {
        fw->startKeyButtonTest(QStringLiteral("左键测试"), QStringLiteral("请短按下左按钮"),
                               QStringLiteral("ProductInfo/KeyIdLeft"), QStringLiteral("ProductInfo/KeyIdLeft_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_RIGHT")) {
        fw->startKeyButtonTest(QStringLiteral("右键测试"), QStringLiteral("请短按下右按钮"),
                               QStringLiteral("ProductInfo/KeyIdRight"), QStringLiteral("ProductInfo/KeyIdRight_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_ROT_LEFT")) {
        fw->startKeyButtonTest(QStringLiteral("左旋键测试"), QStringLiteral("请左旋按钮"),
                               QStringLiteral("ProductInfo/KeyIdLeftRotate"),
                               QStringLiteral("ProductInfo/KeyIdLeftRotate_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("KEY_ROT_RIGHT")) {
        fw->startKeyButtonTest(QStringLiteral("右旋键测试"), QStringLiteral("请右旋按钮"),
                               QStringLiteral("ProductInfo/KeyIdRightRotate"),
                               QStringLiteral("ProductInfo/KeyIdRightRotate_checkBox"));
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_KEY_MODE")) {
        fw->startPlcKeyButtonTest(QStringLiteral("PLC+V3模式键"), QString(), QStringLiteral("ProductInfo/KeyIdMode"),
                                  QStringLiteral("ProductInfo/KeyIdMode_checkBox"), 0, true);
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_KEY_PROGRAM")) {
        fw->startPlcKeyButtonTest(QStringLiteral("PLC+V3程序键"), QString(), QStringLiteral("ProductInfo/KeyIdProgram"),
                                  QStringLiteral("ProductInfo/KeyIdProgram_checkBox"), 1, true);
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_KEY_SPEED")) {
        fw->startPlcKeyButtonTest(QStringLiteral("PLC+V3速度键"), QString(), QStringLiteral("ProductInfo/KeyIdSpeed"),
                                  QStringLiteral("ProductInfo/KeyIdSpeed_checkBox"), 2, true);
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_KEY_RIGHT")) {
        fw->startPlcKeyButtonTest(QStringLiteral("PLC+V3右键"), QString(), QStringLiteral("ProductInfo/KeyIdRight"),
                                  QStringLiteral("ProductInfo/KeyIdRight_checkBox"), 3, true);
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_KEY_START_PAUSE")) {
        fw->startPlcKeyButtonTest(QStringLiteral("PLC+V3开始暂停键"), QString(),
                                  QStringLiteral("ProductInfo/KeyIdStartPause"),
                                  QStringLiteral("ProductInfo/KeyIdStartPause_checkBox"), 4, true);
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_KEY_LEFT")) {
        fw->startPlcKeyButtonTest(QStringLiteral("PLC+V3左键"), QString(), QStringLiteral("ProductInfo/KeyIdLeft"),
                                  QStringLiteral("ProductInfo/KeyIdLeft_checkBox"), 5, true);
        return;
    }
    if (hookId == QStringLiteral("PLC_V3_KEY_POWER")) {
        fw->startPlcKeyButtonTest(QStringLiteral("PLC+V3电源键"), QString(), QStringLiteral("ProductInfo/KeyIdPower"),
                                  QStringLiteral("ProductInfo/KeyIdPower_checkBox"), 6, false);
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_RESET_ACK") || hookId.startsWith(QStringLiteral("PROD_INST_RESET_ACK_"))) {
        fw->startProductInstrumentResetAndWaitAck(QString());
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_START_RX_2402_1M")) {
        fw->startProductInstrumentStartReceiveForCatalog(QStringLiteral("产品串口开始接收2402_BLE1M"), 0);
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_START_RX_2440_1M")) {
        fw->startProductInstrumentStartReceiveForCatalog(QStringLiteral("产品串口开始接收2440_BLE1M"), 1);
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_START_RX_2480_1M")) {
        fw->startProductInstrumentStartReceiveForCatalog(QStringLiteral("产品串口开始接收2480_BLE1M"), 2);
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_START_RX_2402_2M")) {
        fw->startProductInstrumentStartReceiveForCatalog(QStringLiteral("产品串口开始接收2402_BLE2M"), 3);
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_START_RX_2440_2M")) {
        fw->startProductInstrumentStartReceiveForCatalog(QStringLiteral("产品串口开始接收2440_BLE2M"), 4);
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_START_RX_2480_2M")) {
        fw->startProductInstrumentStartReceiveForCatalog(QStringLiteral("产品串口开始接收2480_BLE2M"), 5);
        return;
    }
    {
        const int cmwProfile = freeInstrCmwGprfHookProfile(hookId);
        if (cmwProfile >= 0) {
            QString detail;
            const bool ok = fw->runFreeInstrumentBleCmwBurstForBrushProfile(&detail, cmwProfile);
            fw->markActiveTestCaseStepDone(ok, detail, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
            if (!ok)
                fw->TestResult = fw->failValue;
            return;
        }
    }
    if (hookId == QStringLiteral("PROD_INST_STOP_RX_PER") || hookId.startsWith(QStringLiteral("PROD_INST_STOP_RX_PER_"))) {
        fw->startProductInstrumentStopReceiveAndPer(QString());
        return;
    }
}

void QFreeWorkTestCaseHookRegistrar::registerAll() {
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    registerHook(QStringLiteral("JIG_CURRENT_READ"));
    registerHook(QStringLiteral("SN_WRITE_TAIL"));
    registerHook(QStringLiteral("PLC_MODBUS_CONN"));
    registerHook(QStringLiteral("PLC_V3_SWITCH_RIGHT_WHOLE"));
    registerHook(QStringLiteral("PLC_V3_SWITCH_DONE_RESET_M"));
    registerHook(QStringLiteral("KEY_POWER"));
    registerHook(QStringLiteral("KEY_START_PAUSE"));
    registerHook(QStringLiteral("KEY_MODE"));
    registerHook(QStringLiteral("KEY_SPEED"));
    registerHook(QStringLiteral("KEY_PROGRAM"));
    registerHook(QStringLiteral("KEY_LEFT"));
    registerHook(QStringLiteral("KEY_RIGHT"));
    registerHook(QStringLiteral("KEY_ROT_LEFT"));
    registerHook(QStringLiteral("KEY_ROT_RIGHT"));
    registerHook(QStringLiteral("PLC_V3_KEY_MODE"));
    registerHook(QStringLiteral("PLC_V3_KEY_PROGRAM"));
    registerHook(QStringLiteral("PLC_V3_KEY_SPEED"));
    registerHook(QStringLiteral("PLC_V3_KEY_RIGHT"));
    registerHook(QStringLiteral("PLC_V3_KEY_START_PAUSE"));
    registerHook(QStringLiteral("PLC_V3_KEY_LEFT"));
    registerHook(QStringLiteral("PLC_V3_KEY_POWER"));
    registerHook(QStringLiteral("PROD_INST_RESET_ACK"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2402_1M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2440_1M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2480_1M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2402_2M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2440_2M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2480_2M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_2402_1M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_2440_1M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_2480_1M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_2402_2M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_2440_2M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_2480_2M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P0"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P1"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P2"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P3"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P4"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P5"));
    registerHook(QStringLiteral("PROD_INST_STOP_RX_PER"));
}

void registerQFreeWorkCatalogTestCaseHooks() {
    QFreeWorkTestCaseHookRegistrar::registerAll();
}
