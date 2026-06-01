#include "qfreework.h"

#include "test_case.h"

#include "qusb.h"
#include "qprotocol_types.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

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
    if (hookId == QStringLiteral("BT_DIRECT_DCON")) {
        const QString mac = fw->macAddress;
        fw->sendCommandWithRetry([fw, mac]() { fw->at->sendDcon(mac); }, 6 * 1000);
        return;
    }
    if (hookId == QStringLiteral("BT_SCAN_MAC")) {
        const QString mac = fw->macAddress;
        fw->sendCommandWithRetry([fw, mac]() { fw->at->sendMac(mac); }, 6 * 1000);
        return;
    }
    if (hookId == QStringLiteral("CLOUD_TUPLE_FETCH")) {
        fw->applyTupleByMac();
        return;
    }
    if (hookId == QStringLiteral("WRITE_PRODUCT_KEY")) {
        if (fw->failTupleWriteIfNoValidField(QStringLiteral("写入productKey"), !fw->tupleData_.productKey.isEmpty(),
                                             QStringLiteral("productKey为空")))
            return;
        fw->stepRuntime_.testData = fw->tupleData_.productKey;
        const QByteArray pkUtf8 = fw->tupleData_.productKey.toUtf8();
        fw->sendCommandWithRetry([fw, pkUtf8]() {
            fw->protocolManager.set(DeviceCmd::Sn,
                                    QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SKUID, pkUtf8}));
        });
        return;
    }
    if (hookId == QStringLiteral("WRITE_DEVICE_NAME")) {
        if (fw->failTupleWriteIfNoValidField(QStringLiteral("写入deviceName"), !fw->tupleData_.deviceName.isEmpty(),
                                             QStringLiteral("deviceName为空")))
            return;
        fw->stepRuntime_.testData = fw->tupleData_.deviceName;
        const QByteArray nameUtf8 = fw->tupleData_.deviceName.toUtf8();
        fw->sendCommandWithRetry([fw, nameUtf8]() {
            fw->protocolManager.set(DeviceCmd::Sn,
                                    QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SUB_PID, nameUtf8}));
        });
        return;
    }
    if (hookId == QStringLiteral("WRITE_DEVICE_SECRET")) {
        if (fw->failTupleWriteIfNoValidField(QStringLiteral("写入deviceSecret"),
                                             !fw->tupleData_.deviceSecret.isEmpty(), QStringLiteral("deviceSecret为空")))
            return;
        fw->stepRuntime_.testData = fw->tupleData_.deviceSecret;
        const QByteArray secretUtf8 = fw->tupleData_.deviceSecret.toUtf8();
        fw->sendCommandWithRetry([fw, secretUtf8]() {
            QVariantMap map;
            map[QStringLiteral("value")] = secretUtf8;
            fw->protocolManager.set(DeviceCmd::WriteKey, map);
        });
        return;
    }
    if (hookId == QStringLiteral("TUPLE_WRITE_REPORT")) {
        fw->reportTupleWriteRecord();
        return;
    }
    if (hookId == QStringLiteral("SN_WRITE_TAIL")) {
        const QByteArray tailSn = fw->expectedTailSnFromMes;
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
    if (hookId == QStringLiteral("PROD_INST_RESET_ACK_1")) {
        fw->startProductInstrumentResetAndWaitAck(QStringLiteral("产品串口仪器复位应答1"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_RESET_ACK_2")) {
        fw->startProductInstrumentResetAndWaitAck(QStringLiteral("产品串口仪器复位应答2"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_RESET_ACK_3")) {
        fw->startProductInstrumentResetAndWaitAck(QStringLiteral("产品串口仪器复位应答3"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_RESET_ACK_4")) {
        fw->startProductInstrumentResetAndWaitAck(QStringLiteral("产品串口仪器复位应答4"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_RESET_ACK_5")) {
        fw->startProductInstrumentResetAndWaitAck(QStringLiteral("产品串口仪器复位应答5"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_RESET_ACK_6")) {
        fw->startProductInstrumentResetAndWaitAck(QStringLiteral("产品串口仪器复位应答6"));
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
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P0")) {
        QString detail;
        const bool ok = fw->runFreeInstrumentBleCmwBurstForBrushProfile(&detail, 0);
        fw->markActiveTestCaseStepDone(ok, detail, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
        if (!ok)
            fw->TestResult = fw->failValue;
        return;
    }
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P1")) {
        QString detail;
        const bool ok = fw->runFreeInstrumentBleCmwBurstForBrushProfile(&detail, 1);
        fw->markActiveTestCaseStepDone(ok, detail, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
        if (!ok)
            fw->TestResult = fw->failValue;
        return;
    }
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P2")) {
        QString detail;
        const bool ok = fw->runFreeInstrumentBleCmwBurstForBrushProfile(&detail, 2);
        fw->markActiveTestCaseStepDone(ok, detail, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
        if (!ok)
            fw->TestResult = fw->failValue;
        return;
    }
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P3")) {
        QString detail;
        const bool ok = fw->runFreeInstrumentBleCmwBurstForBrushProfile(&detail, 3);
        fw->markActiveTestCaseStepDone(ok, detail, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
        if (!ok)
            fw->TestResult = fw->failValue;
        return;
    }
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P4")) {
        QString detail;
        const bool ok = fw->runFreeInstrumentBleCmwBurstForBrushProfile(&detail, 4);
        fw->markActiveTestCaseStepDone(ok, detail, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
        if (!ok)
            fw->TestResult = fw->failValue;
        return;
    }
    if (hookId == QStringLiteral("FREE_INSTR_CMW_GPRF_P5")) {
        QString detail;
        const bool ok = fw->runFreeInstrumentBleCmwBurstForBrushProfile(&detail, 5);
        fw->markActiveTestCaseStepDone(ok, detail, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
        if (!ok)
            fw->TestResult = fw->failValue;
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_STOP_RX_PER_1")) {
        fw->startProductInstrumentStopReceiveAndPer(QStringLiteral("产品串口停止接收与PER1"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_STOP_RX_PER_2")) {
        fw->startProductInstrumentStopReceiveAndPer(QStringLiteral("产品串口停止接收与PER2"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_STOP_RX_PER_3")) {
        fw->startProductInstrumentStopReceiveAndPer(QStringLiteral("产品串口停止接收与PER3"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_STOP_RX_PER_4")) {
        fw->startProductInstrumentStopReceiveAndPer(QStringLiteral("产品串口停止接收与PER4"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_STOP_RX_PER_5")) {
        fw->startProductInstrumentStopReceiveAndPer(QStringLiteral("产品串口停止接收与PER5"));
        return;
    }
    if (hookId == QStringLiteral("PROD_INST_STOP_RX_PER_6")) {
        fw->startProductInstrumentStopReceiveAndPer(QStringLiteral("产品串口停止接收与PER6"));
        return;
    }
}

void QFreeWorkTestCaseHookRegistrar::registerAll() {
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    registerHook(QStringLiteral("JIG_CURRENT_READ"));
    registerHook(QStringLiteral("BT_DIRECT_DCON"));
    registerHook(QStringLiteral("BT_SCAN_MAC"));
    registerHook(QStringLiteral("CLOUD_TUPLE_FETCH"));
    registerHook(QStringLiteral("WRITE_PRODUCT_KEY"));
    registerHook(QStringLiteral("WRITE_DEVICE_NAME"));
    registerHook(QStringLiteral("WRITE_DEVICE_SECRET"));
    registerHook(QStringLiteral("TUPLE_WRITE_REPORT"));
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
    registerHook(QStringLiteral("PROD_INST_RESET_ACK_1"));
    registerHook(QStringLiteral("PROD_INST_RESET_ACK_2"));
    registerHook(QStringLiteral("PROD_INST_RESET_ACK_3"));
    registerHook(QStringLiteral("PROD_INST_RESET_ACK_4"));
    registerHook(QStringLiteral("PROD_INST_RESET_ACK_5"));
    registerHook(QStringLiteral("PROD_INST_RESET_ACK_6"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2402_1M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2440_1M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2480_1M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2402_2M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2440_2M"));
    registerHook(QStringLiteral("PROD_INST_START_RX_2480_2M"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P0"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P1"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P2"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P3"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P4"));
    registerHook(QStringLiteral("FREE_INSTR_CMW_GPRF_P5"));
    registerHook(QStringLiteral("PROD_INST_STOP_RX_PER_1"));
    registerHook(QStringLiteral("PROD_INST_STOP_RX_PER_2"));
    registerHook(QStringLiteral("PROD_INST_STOP_RX_PER_3"));
    registerHook(QStringLiteral("PROD_INST_STOP_RX_PER_4"));
    registerHook(QStringLiteral("PROD_INST_STOP_RX_PER_5"));
    registerHook(QStringLiteral("PROD_INST_STOP_RX_PER_6"));
}

void registerQFreeWorkCatalogTestCaseHooks() {
    QFreeWorkTestCaseHookRegistrar::registerAll();
}
