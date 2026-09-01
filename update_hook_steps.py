import re

file_path = 'e:/C/test/platform/test_case/hooks/qfreework_hook_steps.cpp'
with open(file_path, 'r', encoding='utf-8') as f:
    text = f.read()

if '#include "hikvision_scanner.h"' not in text:
    text = text.replace('#include "qfreework.h"', '#include "qfreework.h"\n#include "hikvision_scanner.h"')

impl = """
void QFreeWork::runHikvisionScannerReadStep() {
    const TestCaseDefinition& def = activeTestCase();
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>()) {
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    }
    
    QString ip = map.value(QStringLiteral("ip")).toString().trimmed();
    int port = map.value(QStringLiteral("port"), 2001).toInt();
    int timeout = map.value(QStringLiteral("timeout"), 1000).toInt();

    if (ip.isEmpty()) {
        markActiveTestCaseStepDone(false, QStringLiteral("未配置 IP 地址"), QStringLiteral("失败"));
        showlog(QStringLiteral("扫码枪错误：步骤参数未配置 ip"));
        return;
    }

    showlog(QStringLiteral("正在触发扫码枪（IP: %1, Port: %2, Timeout: %3ms）").arg(ip).arg(port).arg(timeout));
    
    QString barcode;
    QString error;
    bool ok = HikvisionScanner::scan(ip, port, timeout, &barcode, &error);
    
    if (ok) {
        showlog(QStringLiteral("扫码成功: %1").arg(barcode));
        // 用户要求：扫码结果回填入原本扫码输入的sn填窗中
        if (ui && ui->macInput) {
            ui->macInput->setText(barcode);
            macAddress = barcode;
        }
        markActiveTestCaseStepDone(true, barcode, QStringLiteral("通过"));
    } else {
        showlog(QStringLiteral("扫码失败: %1").arg(error));
        markActiveTestCaseStepDone(false, error, QStringLiteral("失败"));
    }
}
"""

text += impl

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(text)
