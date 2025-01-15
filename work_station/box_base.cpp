#include "box_base.h"

#include <QMessageBox>
#include <QtGlobal>

#include "qaction.h"
#include "qcombobox.h"
#include "qmenubar.h"
#include "qstatusbar.h"
#include "test_base.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

box_base::box_base(QWidget* parent) : QMainWindow(parent) {
    QString station = SETTINGS.value("SYSTEM/station").toString();  // 工站
    pack.factory = SETTINGS.value("Mes/FACTORY", "xwd").toString();

    if (station == "PCBA_TEST" || pack.factory == "无mes厂") {
    } else {
        loginMes();
    }

    updatamanager = new QNetworkAccessManager(this);
    connect(updatamanager, &QNetworkAccessManager::authenticationRequired, this, &box_base::provideAuthentication);
}
void box_base::provideAuthentication(QNetworkReply* reply, QAuthenticator* authenticator) {
    authenticator->setUser("usmilejig");
    authenticator->setPassword("Starspulse@123");
}

void box_base::checkAndUpdateFile() {
    QString remoteDirectoryUrl = "http://163.177.79.53:16888/versions/";
    QUrl qUrl(remoteDirectoryUrl);
    QNetworkRequest request(qUrl);

    // qDebug() << "远程目录 URL:" << qUrl;

    QNetworkReply* reply = updatamanager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QString response = reply->readAll();
            // qDebug() << "响应内容:" << response;

            // 使用正则表达式从HTML中提取文件名
            QRegularExpression re("<a href=\"([^\"]+\\.exe)\">");
            QRegularExpressionMatchIterator i = re.globalMatch(response);

            QStringList remoteFiles;
            while (i.hasNext()) {
                QRegularExpressionMatch match = i.next();
                remoteFiles << match.captured(1);
            }

            // 查找本地符合条件的文件
            QDir dir(".");
            QFileInfoList localFiles = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
            QString latestLocalFile;
            QDateTime latestLocalDate;

            for (const QFileInfo& fileInfo : localFiles) {
                if (fileInfo.fileName().startsWith("new_production_") && fileInfo.fileName().endsWith(".exe")) {
                    QString dateString = fileInfo.fileName().mid(15, 8);
                    QDateTime fileDate = QDateTime::fromString(dateString, "yyyyMMdd");
                    if (fileDate > latestLocalDate) {
                        latestLocalDate = fileDate;
                        latestLocalFile = fileInfo.fileName();
                    }
                }
            }
            sendBoxLog("本地最新的文件为:" + latestLocalFile);

            QString latestRemoteFile;
            QDateTime latestRemoteDate;

            for (const QString& fileName : remoteFiles) {
                // qDebug() << "文件名:" << fileName;

                if (fileName.startsWith("new_production_") && fileName.endsWith(".exe")) {
                    QString dateString = fileName.mid(15, 8);
                    QDateTime remoteFileDate = QDateTime::fromString(dateString, "yyyyMMdd");
                    if (remoteFileDate > latestRemoteDate) {
                        latestRemoteDate = remoteFileDate;
                        latestRemoteFile = fileName;
                    }
                }
            }
            sendBoxLog("远程最新的文件为:" + latestRemoteFile);
            if (latestRemoteDate > latestLocalDate) {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this, "更新可用",
                                              QString("发现新版本 %1 可用，是否更新？").arg(latestRemoteFile),
                                              QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    QString downloadUrl = "http://163.177.79.53:16888/versions/" + latestRemoteFile;
                    QString savePath = "./" + latestRemoteFile;
                    QNetworkRequest downloadRequest((QUrl(downloadUrl)));

                    QNetworkReply* downloadReply = updatamanager->get(downloadRequest);
                    connect(downloadReply, &QNetworkReply::finished, [this, downloadReply, savePath]() {
                        if (downloadReply->error() == QNetworkReply::NoError) {
                            QFile file(savePath);
                            if (file.open(QIODevice::WriteOnly)) {
                                file.write(downloadReply->readAll());
                                file.close();
                                sendBoxLog("文件升级成功");
                                QProcess::startDetached(savePath);
                                QString batFileName = "./delete_self.bat";
                                QFile batFile(batFileName);
                                if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                    QTextStream out(&batFile);
                                    out << "@echo off\n";
                                    out << "timeout /t 2 /nobreak\n";
                                    out << "del \"" << QCoreApplication::applicationFilePath() << "\"\n";
                                    out << "del \"" << batFileName << "\"\n";
                                    batFile.close();

                                    // 执行批处理文件
                                    QProcess::startDetached(batFileName);
                                }

                                // 强制退出当前应用
                                QTimer::singleShot(1000, []() {
                                    qApp->quit();
                                    QProcess::startDetached(
                                        "cmd.exe", QStringList()
                                                       << "/c"
                                                       << "taskkill /f /pid " +
                                                              QString::number(QCoreApplication::applicationPid()));
                                });
                            } else {
                                qDebug() << "无法打开文件进行写入:" << savePath;
                            }
                        } else {
                            sendBoxLog("下载失败:" + downloadReply->errorString());
                        }
                        downloadReply->deleteLater();
                    });
                }
            } else {
                sendBoxLog("本地文件已经是最新的");
            }
        } else {
            sendBoxLog("获取远程文件列表失败");
            qDebug() << "获取远程文件列表失败:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void box_base::signalAndslot() {
    for (int i = 0; i < testList.size(); i++) {
        connect(this, SIGNAL(sendBoxLog(QString)), testList[i], SLOT(showlog(QString)));
        connect(this, SIGNAL(go_screen_next(int)), testList[i], SLOT(canGoNextMechine(int)));

        connect(testList[i], SIGNAL(send_go_next_test(int)), this, SLOT(checkAllTest(int)));
        connect(testList[i], SIGNAL(send_end_test(int)), this, SLOT(checkAllover(int)));
        // mes类
        connect(testList[i], SIGNAL(send_end_testPass(MesPacketData)), MesManager, SLOT(TestPassAll(MesPacketData)));
        connect(testList[i], SIGNAL(sendProcessInspection(MesPacketData)), MesManager,
                SLOT(ProcessInspectionAll(MesPacketData)));
        connect(testList[i], SIGNAL(getMesTestValue(MesPacketData)), MesManager, SLOT(GetTestDataAll(MesPacketData)));

        connect(testList[i], SIGNAL(send_startTest(int)), this, SLOT(reset_vector(int)));

        // connect(MesManager, SIGNAL(MesState(const int)), testList[i],SLOT(solveMesSucess(const int)));
        connect(MesManager, SIGNAL(MesError(const int, QString)), testList[i], SLOT(solveMesData(const int, QString)));
        connect(MesManager, SIGNAL(MesSucess(const int)), testList[i], SLOT(solveMesSucess(const int)));
        connect(MesManager, SIGNAL(MesTestvalue(const int, const QString)), testList[i],
                SLOT(getTestValue(const int, const QString)));
    }

    for (int i = 0; i < testList.size() - 1; i++) {
        connect(testList[i], SIGNAL(send_go_next_focus()), testList[i + 1]->getMacLineEdit(), SLOT(setFocus()));

        if (pack.factory == "无mes厂")
            connect(testList[i], SIGNAL(send_go_next_focus()), testList[i + 1]->macInputLineEdit(), SLOT(setFocus()));
        else
            connect(testList[i], SIGNAL(send_go_next_focus()), testList[i + 1]->getMacLineEdit(), SLOT(setFocus()));
    }

    if (pack.factory == "无mes厂")
        connect(testList[testList.size() - 1], SIGNAL(send_go_next_focus()), testList[0]->macInputLineEdit(),
                SLOT(setFocus()));
    else
        connect(testList[testList.size() - 1], SIGNAL(send_go_next_focus()), testList[0]->getMacLineEdit(),
                SLOT(setFocus()));

    connect(testList[testList.size() - 1]->getMacLineEdit(), SIGNAL(returnPressed()), this, SLOT(resetall()));

    initData();
}

//第一个开始了会导致清空
// void box_base::resetall() {
//     for (int i = 0; i < testList.size(); ++i) {
//         FixTureStates[i] = 0;
//     }
// }
//每一路开始测试都会清除掉自己的那个变量
void box_base::reset_vector(int i) { FixTureStates[i - 1] = 0; }
void box_base::initData() {
    pack.factory = SETTINGS.value("Mes/FACTORY", "xwd").toString();
    pack.Employee_ID = SETTINGS.value("Mes/mUserno").toString();
    pack.action = SETTINGS.value("Mes/Action").toString();
    pack.machineNo = SETTINGS.value("Mes/machineNo").toString();
    pack.product = SETTINGS.value("Mes/Product_Name").toString();
    pack.line = SETTINGS.value("Mes/Line").toString();
    pack.model = SETTINGS.value("Mes/model").toString();
    pack.test_station = SETTINGS.value("Mes/test_station").toString();
    pack.password = SETTINGS.value("Mes/M_PASSWORD").toString();
    pack.userNo = SETTINGS.value("Mes/M_USERNO").toString();
    pack.lotName = SETTINGS.value("Mes/Work_Order").toString();
    pack.error = "NULL";

    for (int i = 0; i < testList.size(); i++) {
        FixTureStates.push_back(0);  // 添加0到vector中
    }
}
// 辅助函数定义
void setComboBoxValue(const QString& baseKey, const QString& key, QComboBox* comboBox) {
    if (comboBox != nullptr && comboBox->currentText() != "") {
        SETTINGS.setValue(QString("%1/%2").arg(baseKey).arg(key), comboBox->currentText());
        qDebug() << "数值为" << comboBox->currentText();
    }
}
void box_base::saveCustom() {
    SETTINGS.setValue("Window/Size", this->size());
    for (int i = 0; i < testList.size(); i++) {
        qDebug() << "保存的串口号";
        QString baseKey = QString("mechine/%1").arg(i);
        // 保存COM口相关信息
        setComboBoxValue(baseKey, "comName", testList[i]->getComNameCombo());
        setComboBoxValue(baseKey, "usbcomName", testList[i]->getUsbcomNameCombo());
        setComboBoxValue(baseKey, "JigcomName", testList[i]->getJigcomNameCombo());
        setComboBoxValue(baseKey, "ProductcomName", testList[i]->getProductcomNameCombo());

        if (testList[i]->getMotorCaliParam() != nullptr)
            SETTINGS.setValue(QString("%1/MotorCaliParam").arg(baseKey), testList[i]->getMotorCaliParam()->text());
    }
}
box_base::~box_base() {
    // Cleanup if necessary
    qDebug() << "box_base析构";
}
void box_base::closeEvent(QCloseEvent*) {
    qDebug() << "box_base关闭";
    isTestContinue = 0;
    for (auto x : testList) {
        x->close();
    }
    if (qsetting_ui != NULL)
        qsetting_ui->close();
    saveCustom();
}
void box_base::startAllReturnPressed() {
    for (int i = 0; i < testList.size(); i++) {
        qDebug() << "全部上位机敲回车";
        testList[i]->macInputLineEdit()->returnPressed();
    }
}

void box_base::TotallyTask() {
    while (isTestContinue) {
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->startTask();
        }

        QCoreApplication::processEvents();
    }

    qDebug() << "已经退出主任务";
}
// void box_base::TotallyTask() {
//     QList<QThread*> threads;

//     // 创建线程并分配任务
//     for (int i = 0; i < testList.size(); i++) {
//         QThread* thread = QThread::create([this, i]() {
//             while (isTestContinue) {
//                 testList[i]->startTask();
//                 QThread::msleep(100);  // 根据需要调整任务执行间隔
//             }
//         });

//         threads.append(thread);
//         thread->start();
//     }

//     // 主线程继续运行其他任务
//     while (isTestContinue) {
//         QCoreApplication::processEvents();
//     }

//     // 停止所有线程并清理
//     for (QThread* thread : threads) {
//         thread->quit();
//         thread->wait();  // 等待线程安全退出
//         delete thread;   // 删除线程对象，避免内存泄漏
//     }

//     qDebug() << "已经退出主任务";
// }
void addComboBoxEditText(QComboBox* comboBox, const QString& text) {
    if (comboBox != nullptr) {
        comboBox->addItem(text);
        //  qDebug() << "添加完毕" << text;
    }
}

void setComboBoxEditText(QComboBox* comboBox, const QString& text) {
    if (comboBox != nullptr) {
        comboBox->setCurrentText(text);
        // qDebug() << "设置完毕" << text;
    }
}

void box_base::recoverCustom() {
    for (int i = 0; i < testList.size(); ++i) {
        QString baseKey = QString("mechine/%1").arg(i);
        // 从设置中读取串口相关信息
        QString comName = SETTINGS.value(QString("%1/comName").arg(baseKey)).toString();
        QString usbComName = SETTINGS.value(QString("%1/usbcomName").arg(baseKey)).toString();
        QString jigComomName = SETTINGS.value(QString("%1/JigcomName").arg(baseKey)).toString();
        QString nfcComName = SETTINGS.value(QString("%1/nfcComName").arg(baseKey)).toString();
        QString ProductComName = SETTINGS.value(QString("%1/ProductcomName").arg(baseKey)).toString();
        QString MotorCaliParam = SETTINGS.value(QString("%1/MotorCaliParam").arg(baseKey)).toString();

        setComboBoxEditText(testList[i]->getComNameCombo(), comName);
        setComboBoxEditText(testList[i]->getUsbcomNameCombo(), usbComName);
        setComboBoxEditText(testList[i]->getJigcomNameCombo(), jigComomName);
        setComboBoxEditText(testList[i]->getProductcomNameCombo(), ProductComName);
        addComboBoxEditText(testList[i]->getNfcComboBox(), nfcComName);
        setComboBoxEditText(testList[i]->getNfcComboBox(), nfcComName);

        if (testList[i]->getMotorCaliParam() != nullptr) {
            testList[i]->getMotorCaliParam()->setText(MotorCaliParam);
        }
        // qDebug() << "恢复的串口号" << comName << usbComName << jigComomName << ProductComName;
    }
}

void box_base::ShowData(QMainWindow* parent) {
    if (parent) {  // 确保 parent 指针不为空
        if (pack.factory == "xwd")
            parent->statusBar()->addPermanentWidget(new QLabel("欣旺达"));
        else if (pack.factory == "lx")
            parent->statusBar()->addPermanentWidget(new QLabel("立讯"));
        else if (pack.factory == "hq")
            parent->statusBar()->addPermanentWidget(new QLabel("华勤"));
        else if (pack.factory == "jj")
            parent->statusBar()->addPermanentWidget(new QLabel("金进"));
        else if (pack.factory == "wks")
            parent->statusBar()->addPermanentWidget(new QLabel("伟克森"));
        else if (pack.factory == "ydm")
            parent->statusBar()->addPermanentWidget(new QLabel("亚达明"));
        else
            parent->statusBar()->addPermanentWidget(new QLabel("未知工厂"));  // 处理默认情况
    } else {
        qWarning() << "ShowData的parent指针为空";
    }
    for (int i = 0; i < testList.size(); ++i)
        testList[i]->msgEdit()->appendPlainText("当前产品为:" + pack.product);

    QAction* updata = parent->menuBar()->addAction("软件更新");
    connect(updata, &QAction::triggered, [=]() { checkAndUpdateFile(); });
    if (!SETTINGS.value("SYSTEM/ShowUpperComputerOTAFunc").toInt()) {
        updata->setVisible(false);
    }

    QAction* setting = parent->menuBar()->addAction("功能设置");
    connect(setting, &QAction::triggered, [=]() { setting_ui(); });
    if (!SETTINGS.value("SYSTEM/setting").toInt()) {
        setting->setVisible(false);
    }
}

void box_base::setting_ui() {
    if (qsetting_ui == NULL) {
        qsetting_ui = new qsetting;
    }
    qsetting_ui->raise();
    qsetting_ui->show();
    qsetting_ui->activateWindow();
}

void box_base::loginMes() {
    pack.factory = SETTINGS.value("Mes/FACTORY", "xwd").toString();
    pack.machineNo = SETTINGS.value("Mes/M_MACHINENO").toString();
    pack.password = SETTINGS.value("Mes/M_PASSWORD").toString();
    pack.userNo = SETTINGS.value("Mes/M_USERNO").toString();

    MesManager->loginAll(pack);
}

void box_base::checkAllover(int fixtureNumber) {
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size()) {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates)) {
        for (int i = 0; i < testList.size(); ++i) {
            FixTureStates[i] = 0;
        }

        testList[0]->getMacLineEdit()->setFocus();

        if (pack.factory == "无mes厂")
            testList[0]->macInputLineEdit()->setFocus();
        else
            testList[0]->getMacLineEdit()->setFocus();
    }
}
bool box_base::checkStateReady(std::vector<int> States) {
    for (int i = 0; i < testList.size(); ++i) {
        if (States[i] != 1) {
            return false;
        }
    }
    return true;
}
void box_base::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}
