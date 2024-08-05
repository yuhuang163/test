#ifndef BOX_BASE_H
#define BOX_BASE_H

#include "mesmanager.h"
#include "qapplication.h"
#include "qobject.h"
#include "test_base.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

class box_base : public QMainWindow
{
    Q_OBJECT
public:
    explicit box_base(QWidget *parent = nullptr);

    template <class WidgetType> void CreatWindow(QMainWindow *parent)
    {
        QSettings settings(SETTING_NAME, QSettings::IniFormat);
        const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
        QVariant windowSize(availableSize / 4 * 3);
        this->resize(settings.value("Window/Size", windowSize).toSize());
        restoreState(settings.value("Window/windowState").toByteArray());
        formColumn = settings.value("User/formColumn", "1").toInt();
        formRow = settings.value("User/formRow", "1").toInt();

        QVBoxLayout *vlayout = new QVBoxLayout(parent->centralWidget());

        // 创建一个列表以存储所有的 WidgetType 对象
        for (int i = 0; i < formRow; i++)
        {
            QHBoxLayout *hlayout = new QHBoxLayout(parent);
            for (int j = 0; j < formColumn; j++)
            {
                int index = i * formColumn + j;   // 计算索引值
                // 使用 WidgetType 类型定义对象类型，并调用构造函数
                WidgetType *currentWidget = new WidgetType(index + 1, parent);
                hlayout->addWidget(currentWidget);
                testList.append(currentWidget);
            }
            vlayout->addLayout(hlayout);
        }
        testList[0]->getMacLineEdit()->setFocus();
        qDebug() << "当前为1拖" << formColumn * formRow;
    }

    bool checkStateReady(std::vector<int> States);
    void ShowData(QMainWindow *parent);
    void TotallyTask();
    QList<test_base *> testList;
    void signalAndslot();
    void recoverCustom();
    void waitWork(int ms);
    MesPacketData pack;
    std::vector<int> FixTureStates;

public slots:
    virtual void checkAllover(int );
    virtual void checkAllTest(int ){};
    virtual void resetall();
    void checkAndUpdateFile();
    void startAllReturnPressed();

private slots:
    void reset_vector(int i);
    void loginMes();
    void initData();
    void saveCustom();
    void provideAuthentication(QNetworkReply* reply, QAuthenticator* authenticator);

private:
    QMesManager *MesManager = new QMesManager();
    QNetworkAccessManager* updatamanager;

    int formRow = 1;      // 记录总共是几行几列的窗口
    int formColumn = 1;   // 记录总共是几行几列的窗口
    bool isTestContinue = true;
protected:
    virtual void closeEvent(QCloseEvent *);
signals:
    void go_screen_next(int);

    void sendBoxLog(QString );

};

#endif   // BOX_BASE_H
