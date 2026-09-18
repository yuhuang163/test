#ifndef BOX_BASE_H
#define BOX_BASE_H

#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkReply>
#include <QSettings>
#include <QVBoxLayout>

#include "qmesmanager.h"
#include "qapplication.h"
#include "qobject.h"
#include "qsetting.h"
#include "test_base.h"

class QAction;
class QAuthenticator;

class box_base : public QMainWindow {
    Q_OBJECT

  public:
    explicit box_base(QWidget* parent = nullptr);
    ~box_base();

    template <class WidgetType>
    void CreatWindow(QMainWindow* parent) {
        const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
        QVariant windowSize(availableSize / 4 * 3);
        this->resize(SETTINGS.value("Window/Size", windowSize).toSize());
        restoreState(SETTINGS.value("Window/windowState").toByteArray());
        formColumn = SETTINGS.value("User/formColumn", "1").toInt();
        formRow = SETTINGS.value("User/formRow", "1").toInt();

        QWidget* const central = parent->centralWidget();
        if (QLayout* oldCentralLayout = central->layout())
            delete oldCentralLayout;
        QVBoxLayout* vlayout = new QVBoxLayout(central);
        for (int i = 0; i < formRow; i++) {
            // 勿把 QHBoxLayout 直接挂到 QMainWindow，否则与 centralWidget 布局冲突并可能闪退
            QHBoxLayout* hlayout = new QHBoxLayout();
            for (int j = 0; j < formColumn; j++) {
                const int index = i * formColumn + j;
                WidgetType* currentWidget = new WidgetType(index + 1, central);
                hlayout->addWidget(currentWidget);
                testList.append(currentWidget);
            }
            vlayout->addLayout(hlayout);
        }
        testList[0]->getMacLineEdit()->setFocus();
        qDebug() << "当前为1拖" << formColumn * formRow;
        qDebug() << "testList.size" << testList.size();
    }

    bool checkStateReady(std::vector<int> States);
    void ShowData(QMainWindow* parent);
    void TotallyTask();
    void signalAndslot();
    void recoverCustom();
    void waitWork(int ms);
    /** 在已打开的自由工站窗口上单步运行功能块；找不到窗口时返回 false */
    static bool tryRunSingleTestCaseStepOnAnyFreeWork(const QString& stationKey, const QString& caseName,
                                                      QString* errorOut = nullptr);
    /** 设置页切换/保存工站后：刷新已打开自由工站的流程 Tab 与串口显隐 */
    static void refreshFlowUiOnAllFreeWork();

    QList<test_base*> testList;
    MesPacketData pack;
    std::vector<int> FixTureStates;
    QLabel* cloudLoginLabel = nullptr;
    void refreshCloudLoginState();
    void refreshSettingsMenuVisibility();

  public slots:
    virtual void checkAllover(int);
    virtual void checkAllTest(int) {
    }
    virtual void resetall() {
    }
    void checkAndUpdateFile();
    void startAllReturnPressed();
    void setting_ui();

  private slots:
    void reset_vector(int i);
    void loginMes();
    void initData();
    void saveCustom();
    void provideAuthentication(QNetworkReply* reply, QAuthenticator* authenticator);

  private:
    QMesManager* MesManager = new QMesManager();
    QNetworkAccessManager* updatamanager = nullptr;
    int formRow = 1;
    int formColumn = 1;
    bool isTestContinue = true;
    qsetting* qsetting_ui = nullptr;
    QAction* settingMenuAction = nullptr;

  protected:
    void closeEvent(QCloseEvent* event) override;
    /** 关窗时打断治具/串口等阻塞等待；子类按需 override。 */
    virtual void abortBlockingWaits(){};

  signals:
    void go_screen_next(int);
    void sendBoxLog(QString);
};

#endif // BOX_BASE_H
