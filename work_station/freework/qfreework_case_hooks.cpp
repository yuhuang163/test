#include "label_print_service.h"

#include "qfreework.h"

#include "test_case.h"

#include "qprotocol_types.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

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
        fw->runJigAmmeterCurrentSampleAnyMatch();
        return;
    }
    if (hookId == QStringLiteral("DONGLE_SUCTION_ENABLE")) {
        fw->setDongleSuctionReadEnabled(true);
        fw->markActiveTestCaseStepDone(true, QStringLiteral("ON"), QStringLiteral("通过"));
        return;
    }
    if (hookId == QStringLiteral("DONGLE_SUCTION_DISABLE")) {
        fw->setDongleSuctionReadEnabled(false);
        fw->markActiveTestCaseStepDone(true, QStringLiteral("OFF"), QStringLiteral("通过"));
        return;
    }
    if (hookId == QStringLiteral("DONGLE_SUCTION_SAMPLE")) {
        fw->runDongleSuctionSampleStep();
        return;
    }
    if (hookId == QStringLiteral("DONGLE_SUCTION_SAMPLE_SINGLE")) {
        fw->runDongleSuctionSampleSingleStep();
        return;
    }
    if (hookId == QStringLiteral("SN_WRITE_TAIL")) {
        const QByteArray tailSn = fw->resolvedTailSnToWrite();
        if (tailSn.isEmpty()) {
            fw->stepRuntime_.done = true;
            fw->stepRuntime_.pass = false;
            fw->stepRuntime_.testData = QStringLiteral("整机SN为空");
            fw->TestResult = fw->failValue;
            fw->showlog(QStringLiteral("写入整机SN失败：界面SN为空，请先获取三元组或扫入整机SN"));
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
    if (hookId == QStringLiteral("PRINT_WHOLE_MACHINE_SN")) {
        QString sn = fw->resolvedWholeMachineSnText();
        if (sn.isEmpty())
            sn = fw->resolvedPcbaSnText();
        if (sn.isEmpty()) {
            fw->showlog(QStringLiteral("打印整机SN失败：界面SN为空，请先获取三元组或扫入整机SN"));
            fw->markActiveTestCaseStepDone(false, QStringLiteral("整机SN为空"), QStringLiteral("失败"));
            return;
        }
        QString err;
        if (!LabelPrintService::printQrText(sn, &err)) {
            fw->showlog(QStringLiteral("打印整机SN失败：%1").arg(err));
            fw->markActiveTestCaseStepDone(false, err, QStringLiteral("失败"));
            return;
        }
        fw->showlog(QStringLiteral("已打印整机SN：%1").arg(sn));
        fw->markActiveTestCaseStepDone(true, sn, QStringLiteral("通过"));
        return;
    }
    if (hookId == QStringLiteral("QR_SN_CONSISTENCY_CHECK")) {
        const QString expectedSn = fw->resolvedPcbaSnText().trimmed();
        if (expectedSn.isEmpty()) {
            fw->markActiveTestCaseStepDone(false, QStringLiteral("开局PCBA SN为空"), QStringLiteral("失败"));
            fw->showlog(QStringLiteral("二维码一致性校验失败：开局PCBA SN为空，请先扫入PCBA SN再测试"));
            return;
        }
        QDialog dlg(fw);
        dlg.setWindowTitle(QStringLiteral("二维码一致性校验"));
        dlg.setModal(true);
        auto* layout = new QVBoxLayout(&dlg);
        layout->addWidget(new QLabel(QStringLiteral("请扫入/填入待校验二维码（将与开局SN比对）："), &dlg));
        auto* edit = new QLineEdit(&dlg);
        layout->addWidget(edit);
        // 仅保留确定，扫码枪回车也会触发 accept
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(edit, &QLineEdit::returnPressed, &dlg, &QDialog::accept);
        edit->setFocus();
        dlg.exec();
        const QString scanned = edit->text().trimmed();
        if (scanned.isEmpty()) {
            fw->markActiveTestCaseStepDone(false, QStringLiteral("输入为空"), expectedSn);
            fw->showlog(QStringLiteral("二维码一致性校验失败：输入为空，期望SN=%1").arg(expectedSn));
            return;
        }
        const bool pass = fw->compareVersions(expectedSn, scanned);
        fw->markActiveTestCaseStepDone(pass, scanned, expectedSn);
        fw->showlog(pass ? QStringLiteral("二维码一致性校验通过：扫描=%1，开局SN=%2").arg(scanned, expectedSn)
                         : QStringLiteral("二维码一致性校验失败：扫描=%1，开局SN=%2").arg(scanned, expectedSn));
        return;
    }

    if (hookId == QStringLiteral("MAC_WRITE_ROOT")) {
         fw->showlog(QStringLiteral("MAC_WRITE_ROOT"));
        const QString snText = fw->resolvedPcbaSnText();
        const QString macText = fw->parseMacFromSn(snText);
        if (macText.isEmpty()) {
            fw->stepRuntime_.done = true;
            fw->stepRuntime_.pass = false;
            fw->stepRuntime_.testData = QStringLiteral("从SN解析MAC失败");
            fw->TestResult = fw->failValue;
            fw->showlog(QStringLiteral("写入MAC码失败：无法从SN解析MAC，请检查SN格式"));
            return;
        }
        fw->stepRuntime_.testData = macText;
        fw->setCommandWaitSource(CommandWaitSource::ProductProtocol);
        fw->sendCommandWithRetry([fw, macText]() {
            fw->protocolManager.set(DeviceCmd::MacWrite, macText);
            fw->showlog(QStringLiteral("已发送写MAC（自动流程）: %1").arg(macText));
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
    // 纯按键等待已改为步骤 Gate（ProtocolButtonStateData + Expected），不再走 KEY_* / KEY_M8_* Hook
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
    registerHook(QStringLiteral("DONGLE_SUCTION_ENABLE"));
    registerHook(QStringLiteral("DONGLE_SUCTION_DISABLE"));
    registerHook(QStringLiteral("DONGLE_SUCTION_SAMPLE"));
    registerHook(QStringLiteral("DONGLE_SUCTION_SAMPLE_SINGLE"));
    registerHook(QStringLiteral("SN_WRITE_TAIL"));
    registerHook(QStringLiteral("PRINT_WHOLE_MACHINE_SN"));
    registerHook(QStringLiteral("QR_SN_CONSISTENCY_CHECK"));
    registerHook(QStringLiteral("MAC_WRITE_ROOT"));
    registerHook(QStringLiteral("BLE_CONNECT_BY_NAME"));
    registerHook(QStringLiteral("PLC_MODBUS_CONN"));
    registerHook(QStringLiteral("PLC_V3_SWITCH_RIGHT_WHOLE"));
    registerHook(QStringLiteral("PLC_V3_SWITCH_DONE_RESET_M"));
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
