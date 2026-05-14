#ifndef FACTORY_ANALYZER_H
#define FACTORY_ANALYZER_H

#include "common_class.h"
#include "qadb.h"
#include "qbulk/qbulk.h"
#include "qcustomplot.h"
#include "qshell.h"
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMainWindow>
#include <QUrl>
#include <qlog.h>

#include "draggablecheckbox.h"
#include "ui_factory_analyzer.h"
#include <QMessageBox>
// #include "myopenglwidget.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardItemModel>
#include <QSerialPort>

class Qproduct;

QT_BEGIN_NAMESPACE
namespace Ui {
class factory_analyzer;
}
QT_END_NAMESPACE
#define ROW_SPACING 10
#define COLUMN_SPACING 10
#define MARGIN 10
class factory_analyzer : public QMainWindow {
    Q_OBJECT

public:

    factory_analyzer(QWidget *parent = nullptr);
    ~factory_analyzer();
    QBulk *bulk = nullptr;
    QThread *bulkreadThread;
    QTimer *reconnectTimer;
    void setupUSB();
    void tryOpenUSB();
    void showlog(const QString &msg);
    QStringList processOutputLines;
    std::vector<QCustomPlot *> graph_value_vector;
    void graph_reset(uint8_t argument);
    void updateMainStyle(QString style);
   Qlog* log;
    // 作为类成员
    QTableWidget *table;
    QLabel *adbStatusLabel = nullptr;
    QLabel *usbStatusLabel = nullptr;
    QLabel *bulkStatusLabel = nullptr;
    QLabel *uartStatusLabel = nullptr;

    QString adbLastOutput;
    bool ddr_press = false;
    void runCmd(const QString &cmd);
    Qadb *adb;
    Qshell *shell;

    Qshell *shellMonitor;
    QStandardItemModel *treeModel;
    QStandardItemModel *fileModel;

    QQueue<QString> commandQueue; // 命令队列
    bool isProcessing = false;    // 是否正在执行命令

    QStringList commandHistory;
    int historyIndex = -1;
    // 类成员
    bool treeExpanded = true;  // 当前状态，false=收起，true=展开
        bool json_treeExpanded = false;  // 当前状态，false=收起，true=展开
    QStringList items = {

        "test_rtc_good.sh",
        "test_button_good.sh power_key",
        "test_reboot_with_usb.sh",
        "test_device_power.sh poweroff",
        "test_device_power.sh sleep",
        "dji_sn_ops.sh device RD",
        "test_sd_unset.sh",
        "eagle4_state_pro.sh",
        "test_mp_stage.sh erase",
        "test_mp_stage.sh aging_test 3600",
        "test_ufs_value.sh write 1",
        "test_ufs_value.sh read 1",
        "test_bootmode_check.sh 0",
        "test_bat_info.sh temp",
        "test_rgb_leds.sh F red",
        "test_rgb_leds.sh F green",
        "test_ntc.sh",

    };
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
private slots:
    void updateForwardTable();
    void parseAndAddLine(const QString &line);
    void on_pushButton_clicked();
    QString execAdbBlocking(const QString &args, int timeout);
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void pushFileToGaoTongDevice(const QString &localFile);
    void pushFileToZiYanDevice(const QString &localFile);
    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_6_clicked();
    void runExeWithReport(const QString &exeName);

    void runProcess(const QString &program, const QStringList &arguments,
    const QString &workDir,
    std::function<void(int, QProcess::ExitStatus)> onFinish);

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    void on_pushButton_9_clicked();
    void updateAdbStatus();

    void on_pushButton_10_clicked();

    void on_pushButton_11_clicked();

    void on_pushButton_12_clicked();

    void loadRoot();
    void loadFolder(const QString &path);

    void parseFiles(QString path, QString data);

    void on_treeView_clicked(const QModelIndex &index);

    void on_pushButton_13_clicked();
    void onTableViewContextMenu(const QPoint &pos);
    void onTreeViewContextMenu(const QPoint &pos);
    void on_pushButton_14_clicked();

    void on_pushButton_15_clicked();
    void displayCmdline(QTableWidget *table, const QString &cmdline);
    void on_pushButton_16_clicked();
    void updateQualcommComStatus();
    void on_tableView_doubleClicked(const QModelIndex &index);

    void on_pushButton_17_clicked();
    void updateBatteryLevel();

    void on_pushButton_18_clicked();

    void on_lineEdit_returnPressed();

    void on_pushButton_19_clicked();

    void on_pushButton_20_clicked();

    void on_pushButton_21_clicked();

    void on_pushButton_22_clicked();

    void on_pushButton_23_clicked();

    void on_pushButton_24_clicked();

    void on_pushButton_25_clicked();

    void on_pushButton_26_clicked();
    void refreshbulkData(QString data);

    void solveGetDjiResponse(int data,int errocode);

    void on_pushButton_27_clicked();

    void on_pushButton_28_clicked();
    void on_pushButton_29_clicked();
    void on_pushButton_30_clicked();
    void on_comboBox_activated(int index);



    void on_pushButton_32_clicked();

    void on_comboBox_2_activated(int index);

    void on_pushButton_33_clicked();

    void on_pushButton_34_clicked();

    void on_pushButton_35_clicked();

    void on_pushButton_36_clicked();

    void on_pushButton_37_clicked();

    void on_pushButton_38_clicked();
    void on_pushButton_39_clicked();

    void on_pushButton_40_clicked();

    void on_pushButton_41_clicked();

    void on_save_config_clicked();


    void on_pushButton_42_clicked();

    void on_pushButton_43_clicked();

    void on_pushButton_44_clicked();
    void refreshColor1();
    void on_R1_valueChanged(int value);
    void on_G1_valueChanged(int value);
    void on_B1_valueChanged(int value);
    void on_pushButton_45_clicked();

    void on_pushButton_47_clicked();

    void on_pushButton_49_clicked();

    void on_pushButton_50_clicked();

    void on_pushButton_51_clicked();

    void on_pushButton_52_clicked();

    void refresh_send_bulk_Data(int value);

    void refresh_download_bulk_Data(int value);

    void on_pushButton_46_clicked();

    void on_pushButton_48_clicked();

    void on_pushButton_53_clicked();

    void on_pushButton_54_clicked();

    void on_pushButton_55_clicked();

    void on_pushButton_56_clicked();

    void on_pushButton_57_clicked();

    void on_pushButton_58_clicked();

    void on_pushButton_60_clicked();

    void on_pushButton_59_clicked();

    void on_pushButton_61_clicked();

    void on_pushButton_62_clicked();

    void on_pushButton_63_clicked();

    void on_pushButton_64_clicked();

    void on_pushButton_65_clicked();

    void on_tabWidget_currentChanged(int index);

    void on_pushButton_66_clicked();

    void on_pushButton_31_clicked();
    void readProductSerialPortData(void);
    void handleProductSerialPortError(QSerialPort::SerialPortError error);
    void refreshProductUartState(int state);
    void openProductSerialPort(void);
    void closeProductSerialPort(void);

    void on_productConnectButton_clicked();

    void on_productDisconnectButton_clicked();

    void on_pushButton_67_clicked();
    void scanSerialPorts();
    void on_pushButton_68_clicked();

    void on_pushButton_69_clicked();

    void on_pushButton_70_clicked();

    void on_pushButton_71_clicked();

    void on_pushButton_72_clicked();

    void on_pushButton_73_clicked();


    void json_onTreeClicked(const QModelIndex &index);

    void on_pushButton_75_clicked();

    void on_pushButton_76_clicked();

    void on_pushButton_74_clicked();

    void on_pushButton_77_clicked();

public slots:
      void qmlstartTest();

private:
    void highlightAll(QPlainTextEdit *edit, const QString &text);
    void adbPull(const QString &remotePath);
    void adbDelete(const QString &remotePath);
    void refreshTreeAfterDelete(const QString &remotePath);
    void loadRemoteDirectory(const QString &path);
    void
    execAdb(const QString &args,
            std::function<void(const QString &output, qint64 elapsed)> callback,
            int timeout = 3000);
    bool isTestContinue = false; // 测试是否继续
    QLineEdit *searchEdit;
    QElapsedTimer TestTime;
    bool getRespone = 0;
    bool sendRetryOver = false;
    bool sendCommand_test_result = true;
    bool adb_status = false;

    int m_dirProgressIndex = 0;
    int m_dirTotal = 0;
    int m_dirStep = 0;

    int mf_dirProgressIndex = 0;
    int mf_dirTotal = 0;
    int mf_dirStep = 0;

    QStandardItemModel *json_treeModel;
    QStandardItemModel *json_tableModel;

    QJsonObject json_rootObject;
   void json_loadFile_mechine();
    void json_loadFile(const QString &path);
    void json_buildTree(const QJsonObject &obj, QStandardItem *parent);
    void json_showObject(const QJsonObject &obj);
    void json_showArray(const QJsonArray &arr);
void json_showRouteTable(const QJsonObject &obj);




    QTimer *adb_check_timer = new QTimer(this);
    QTimer* scanSerialPortsTimer = new QTimer(this);
    QSerialPort* productSerialPort;  // 设备硬件层
    Qproduct* product;               // 设备协议层
    QTimer* productSerialPortTimer = new QTimer(this);
    QByteArray productSerialPortBuf = 0;
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    // 放下事件
    void dropEvent(QDropEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::factory_analyzer *ui;
    double m_currentScale = 3; // 初始 0.25
    void startTask() ;
  int teststate = -1;
    QVBoxLayout* conFiglayout;
    QGridLayout* canUselayout;
    static constexpr double SCALE_MIN = 0.05;
    static constexpr double SCALE_MAX = 20.0;
// protected:
//     void wheelEvent(QWheelEvent *event) override;
public:
    struct TimelineEvent {
        QString timeStr;
        QString title;
        QString detail;
        QColor color;
        QDateTime dt;
    };

    QVector<TimelineEvent> m_events; // 类成员
        QVector<TimelineEvent> ufei_m_events; // 类成员

        QVector<TestItem> testItems;
    void executeFunctionByName(const QString functionName);
            void testResultTableUpdate(QVector<TestItem>& testItems);
    QTableWidget* testResultTable()  { return ui->testResultTable; };  // 测试结果表格输入口
    std::vector<NamedFunction> testFunctions;
        void createTestFunctions();
        int sendCommandWithRetry(std::function<void()> commandFunc);
        size_t canUserRow;
        int canUserCol;
        int singleCheckBoxHeight;
        int singleCheckBoxWidth;
            QVector<DraggableCheckBox*> checkBoxes;
        void testResultTableInit();
    void reorderCheckBoxes();
          QVector<int> loadIndexesFromConfig();
    DraggableCheckBox* getCheckBoxByIndex(int index);
    DraggableCheckBox* getCanUseCheckBoxByIndex(int index);
    int getIndexAt(const QPoint& pos);
    void showTestIndexes();
    void saveIndexesToConfig(const QVector<int>& indexes);
    bool canGoNext = false;
   QString TestResult = "";
    QString passValue = "通过";
    QString failValue = "失败";
    typedef enum {
        STATE_IDLE = 0,  // 休眠状态
        STATE_WATI_CONNECT,
        STATE_DISABLE_SLEEP_1,  // 进入禁止休眠
        STATE_WATI_BASE_INFO,
        STATE_WATI_WIFI_CONNECT,          // 等待连接
        STATE_WATI_GET_CORRECT_WIFIRSSI,  // 等待正确的wifi信号强度
        STATE_WATI_GET_CORRECT_BLERSSI,   // 等待正确的蓝牙信号强度
        STATE_WATI_CORRECT_BATTARY,       // 等待正确的蓝牙信号强度
        STATE_WATI_CORRECT_CURRENT,
        STATE_SAVE_RESULT  // 保存结果在本地
    } State;
    State state = STATE_IDLE;
    State getNextState(State currentState);
    QVector<int> testIndexes;  // 存储索引的容器
    void calculateGridPosition(const QPoint& globalPos, const QRect& area, int& row, int& col);
    void moveToGrid(QGridLayout* layout, QWidget* widget, int row, int col);
    void moveToLayout(QLayout* fromLayout, QLayout* toLayout, QWidget* widget);
    QPoint dragPos;  // 存储拖动位置
    void paintEvent(QPaintEvent* event) override;
    void initTimeline();   // 初始化（只干一次）
    void re_drawTimeline();
    void addTimelineEvent(const QString &timeStr,
                        const QString &title,
                        const QString &detail,
                        const QColor &color,   QVector<TimelineEvent> &my_events);
    void drawTimeline(
        const QVector<TimelineEvent> &events,
        int baseY,
        const QString &axisName
        );

        bool isBrushLogGet = 1;

signals:
    void refreshProductSerialPortState(int);

private:
    QString formatTime(int ms) const;

private:
    QDateTime m_endTime;

    QDateTime m_startTime;
    bool m_hasStartTime = false;

    QGraphicsScene *my_screen = nullptr;
    int m_maxX = 0;

    // ====== Timeline 参数 ======
    const double TIME_SCALE = 0.25;
    const int BASE_Y = 80;
    const int SCENE_HEIGHT = 320;
    const int RIGHT_MARGIN = 200;
};

#endif // FACTORY_ANALYZER_H
