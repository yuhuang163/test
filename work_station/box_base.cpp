#include "box_base.h"
#include "qcombobox.h"
#include "qstatusbar.h"
#include "test_base.h"
#include <QMessageBox>
#include <QtGlobal>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

box_base::box_base(QWidget *parent) : QMainWindow(parent)
{
    loginMes();
}
void box_base::signalAndslot()
{
    for (int i = 0; i < testList.size(); i++)
    {
        connect(this, SIGNAL(go_screen_next(int)), testList[i], SLOT(can_go_next(int)));

        connect(testList[i], SIGNAL(goNextTest(int)), this, SLOT(checkAllTest(int)));
        connect(testList[i], SIGNAL(endTest(int)), this, SLOT(checkAllover(int)));
        // mes类
        connect(testList[i], SIGNAL(sendTestPass(MesPacketData)), MesManager,
                SLOT(TestPassAll(MesPacketData)));
        connect(testList[i], SIGNAL(sendProcessInspection(MesPacketData)), MesManager,
                SLOT(ProcessInspectionAll(MesPacketData)));
        connect(testList[i], SIGNAL(getMesTestValue(MesPacketData)), MesManager,
                SLOT(GetTestDataAll(MesPacketData)));

        connect(testList[i], SIGNAL(startTest(int)), this, SLOT(reset_vector(int)));

        // connect(MesManager, SIGNAL(MesState(const int)), testList[i],
        //         SLOT(solveMesSucess(const int)));
        connect(MesManager, SIGNAL(MesError(const int, QString)), testList[i],
                SLOT(solveMesData(const int, QString)));
        connect(MesManager, SIGNAL(MesSucess(const int)), testList[i],
                SLOT(solveMesSucess(const int)));
        connect(MesManager, SIGNAL(MesTestvalue(const int, const QString)), testList[i],
                SLOT(getTestValue(const int, const QString)));
    }

    for (int i = 0; i < testList.size() - 1; i++)
    {
        connect(testList[i], SIGNAL(goNextFocus()), testList[i + 1]->getMacLineEdit(),
                SLOT(setFocus()));
    }

    connect(testList[testList.size() - 1], SIGNAL(goNextFocus()), testList[0]->getMacLineEdit(),
            SLOT(setFocus()));

    initData();
}
void box_base::reset_vector(int i)
{
    FixTureStates[i - 1] = 0;
}
void box_base::initData()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    pack.factory = settings.value("Mes/FACTORY", "xwd").toString();
    pack.Employee_ID = settings.value("Mes/mUserno").toString();
    pack.action = settings.value("Mes/Action").toString();
    pack.machineNo = settings.value("Mes/machineNo").toString();
    pack.product = settings.value("Mes/Product_Name").toString();
    pack.line = settings.value("Mes/Line").toString();
    pack.model = settings.value("Mes/model").toString();
    pack.test_station = settings.value("Mes/test_station").toString();
    pack.password = settings.value("Mes/M_PASSWORD").toString();
    pack.userNo = settings.value("Mes/M_USERNO").toString();
    pack.error = "NULL";

    for (int i = 0; i < testList.size(); i++)
    {
        FixTureStates.push_back(0);   // 添加0到vector中
    }
}
// 辅助函数定义
void setComboBoxValue(QSettings &settings, const QString &baseKey, const QString &key,
                      QComboBox *comboBox)
{
    if (comboBox != nullptr && comboBox->currentText() != "")
    {
        settings.setValue(QString("%1/%2").arg(baseKey).arg(key), comboBox->currentText());
        qDebug() << comboBox->currentText();
    }
}
void box_base::saveCustom()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    settings.setValue("Window/Size", this->size());
    for (int i = 0; i < testList.size(); i++)
    {
        qDebug() << "保存的串口号";
        QString baseKey = QString("mechine/%1").arg(i);
        // 保存COM口相关信息
        setComboBoxValue(settings, baseKey, "comName", testList[i]->getComNameCombo());

        setComboBoxValue(settings, baseKey, "usbcomName", testList[i]->getUsbcomNameCombo());
        setComboBoxValue(settings, baseKey, "JigcomName", testList[i]->getJigcomNameCombo());
        setComboBoxValue(settings, baseKey, "ProductcomName",
                         testList[i]->getProductcomNameCombo());

        if (testList[i]->getMotorCaliParam() != nullptr)
            settings.setValue(QString("%1/MotorCaliParam").arg(baseKey),
                              testList[i]->getMotorCaliParam()->text());
    }
}

void box_base::closeEvent(QCloseEvent *)
{
    isTestContinue = 0;
    for (auto x : testList)
    {
        x->close();
    }
    saveCustom();
}
void box_base::startAllReturnPressed()
{
    for (int i = 0; i < testList.size(); i++)
    {
        qDebug() << "全部上位机敲回车";
        testList[i]->macInputLineEdit()->returnPressed();
    }
}

void box_base::TotallyTask()
{
    while (isTestContinue)
    {
        for (int i = 0; i < testList.size(); i++)
        {
            testList[i]->start_task();
        }

        QCoreApplication::processEvents();
    }

    qDebug() << "已经退出主任务";
}
void setComboBoxEditText(QComboBox *comboBox, const QString &text)
{
    if (comboBox != nullptr)
    {
        comboBox->setCurrentText(text);
    }
}

void box_base::recoverCustom()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    for (int i = 0; i < testList.size(); ++i)
    {
        QString baseKey = QString("mechine/%1").arg(i);
        // 从设置中读取串口相关信息
        QString comName = settings.value(QString("%1/comName").arg(baseKey)).toString();
        QString usbComName = settings.value(QString("%1/usbcomName").arg(baseKey)).toString();
        QString jigComomName = settings.value(QString("%1/JigcomName").arg(baseKey)).toString();
        QString ProductComName =
            settings.value(QString("%1/ProductcomName").arg(baseKey)).toString();

        QString MotorCaliParam =
            settings.value(QString("%1/MotorCaliParam").arg(baseKey)).toString();

        setComboBoxEditText(testList[i]->getComNameCombo(), comName);
        setComboBoxEditText(testList[i]->getUsbcomNameCombo(), usbComName);
        setComboBoxEditText(testList[i]->getJigcomNameCombo(), jigComomName);
        setComboBoxEditText(testList[i]->getProductcomNameCombo(), ProductComName);

        if (testList[i]->getMotorCaliParam() != nullptr)
        {
            testList[i]->getMotorCaliParam()->setText(MotorCaliParam);
        }

        // qDebug() << "恢复的串口号" << comName << usbComName << jigComomName << ProductComName;
    }
}

void box_base::ShowData(QMainWindow *parent)
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    if (parent)
    {   // 确保 parent 指针不为空
        if (pack.factory == "xwd")
            parent->statusBar()->addPermanentWidget(new QLabel("欣旺达"));
        else if (pack.factory == "lx")
            parent->statusBar()->addPermanentWidget(new QLabel("立讯"));
        else if (pack.factory == "hq")
            parent->statusBar()->addPermanentWidget(new QLabel("华勤"));
        else if (pack.factory == "jj")
            parent->statusBar()->addPermanentWidget(new QLabel("金进"));
        else
            parent->statusBar()->addPermanentWidget(new QLabel("未知工厂"));   // 处理默认情况
    }
    else
    {
        qWarning() << "ShowData的parent指针为空";
    }
   for (int i = 0; i < testList.size(); ++i)
    testList[i]->msgEdit()->appendPlainText("当前产品为:"+pack.product);

}

void box_base::loginMes()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    pack.factory = settings.value("Mes/FACTORY", "xwd").toString();
    pack.machineNo = settings.value("Mes/M_MACHINENO").toString();
    pack.password = settings.value("Mes/M_PASSWORD").toString();
    pack.userNo = settings.value("Mes/M_USERNO").toString();

    MesManager->loginAll(pack);
}

void box_base::checkAllover(int fixtureNumber)
{
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size())
    {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates))
    {
        for (int i = 0; i < testList.size(); ++i)
        {
            FixTureStates[i] = 0;
        }

        testList[0]->getMacLineEdit()->setFocus();
    }
}
bool box_base::checkStateReady(std::vector<int> States)
{
    for (int i = 0; i < testList.size(); ++i)
    {
        if (States[i] != 1)
        {
            return false;
        }
    }
    return true;
}
void box_base::waitWork(int ms)
{
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}
