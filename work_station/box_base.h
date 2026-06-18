#ifndef BOX_BASE_H
#define BOX_BASE_H

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "qmesmanager.h"
#include "qapplication.h"
#include "qobject.h"
#include "test_base.h"

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

        QVBoxLayout* vlayout = new QVBoxLayout(parent->centralWidget());
        for (int i = 0; i < formRow; i++) {
            QHBoxLayout* hlayout = new QHBoxLayout(parent);
            for (int j = 0; j < formColumn; j++) {
                const int index = i * formColumn + j;
                WidgetType* currentWidget = new WidgetType(index + 1, parent);
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

    QList<test_base*> testList;
    MesPacketData pack;
    std::vector<int> FixTureStates;

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

  protected:
    void closeEvent(QCloseEvent* event) override;

  signals:
    void go_screen_next(int);
    void sendBoxLog(QString);
};

#endif // BOX_BASE_H
