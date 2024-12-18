#ifndef PRESSURESENSORFORM_H
#define PRESSURESENSORFORM_H

#include <WinSock2.h>

#include <QSerialPort>
#include <QTime>
#include <QWidget>

#include "Abini.h"
#include "fixture_uart.h"
#include "hqmes.h"
#include "ndt_sensor_cali.h"
#include "qat.h"
#include "qcustomplot.h"
#include "qpb.h"
#include "test_base.h"
#include "ui_pressuresensorform.h"
#include "xwdmes.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

typedef enum {
    STATE_INVALID,
    STATE_WAIT_START,
    STATE_CALIB_START,
    STATE_CALIB_STATIC_WAIT,
    STATE_CALIB_STATIC_START,
    STATE_CALIB_CH0,
    STATE_CALIB_CH1,
    STATE_CALIB_RESULT,
    STATE_MAX,
} STATE_INDEPENDENT_E;

namespace Ui {
    class PressureSensorForm;
}

class PressureSensorForm : public test_base {
    Q_OBJECT
public:
    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QLineEdit* macInputLineEdit() override { return ui->macInput; };     // mac地址输入口
    QLineEdit* getMacLineEdit() override { return ui->getMac; };         // sn输入口
    QPlainTextEdit* logEdit() override { return ui->log; };              // log输入口
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; };          // msg输入口
    QLabel* getMesStateQlabel() override { return ui->mes_state; };

    explicit PressureSensorForm(int index, QWidget* parent = nullptr);
    ~PressureSensorForm();
    Ui::PressureSensorForm* ui;

    void startTask() override;
    // void refreshBleState(int state) override;
    void show_product(QString name);
    void processInspection(QString stringsn);
    void PressureSensorForm::processGetMesTestValue();
    typedef enum {
        MODEL_ID_INVALID,
        MODEL_ID_Y20P,
        MODEL_ID_Y21,
        MODEL_ID_F20,
        MODEL_ID_U7,
        MODEL_ID_P30P,
        MODEL_ID_MAX,
    } MODEL_ID_E;

    typedef enum {
        GRAPH_SET_INVALID,
        GRAPH_SET_CLOSE,
        GRAPH_SET_USE_ADC,
        GRAPH_SET_USE_VALUE,
        GRAPH_SET_USE_BTH,
        GRAPH_SET_USE_KEY,
        GRAPH_SET_USE_ALL,
        GRAPH_SET_MAX,
    } GRAPH_SET_E;

    void over_task();
    void product_model_init(QString model);

    // graph
    void graph_init(MODEL_ID_E model, QString is_use_graph);
    void graph_update(FacUploadPresSensor x);
    void graph_reset(uint8_t argument);

    // save file
    void save_data(FacUploadPresSensor x);

    // calib and test
    void calib_process(FacUploadPresSensor x);
    void test_process(FacUploadPresSensor x);
    void calib_vector_init(MODEL_ID_E model);
    void calib_send_result(void);

    // state
    STATE_INDEPENDENT_E independent_state = STATE_INVALID;
    void set_independent_state(STATE_INDEPENDENT_E state);
    STATE_INDEPENDENT_E get_independent_state(void);

    uint8_t CheckNfcData();
    int findNfcDevicePort(QString name);

    int total_sensor = 0;  // 传感器数量
    int calib_chan = 0;    // 当前校准通道
    int test_chan = 0;     // 当前测试通道

    uint8_t TEST_STATE = 0;

signals:

    // uart
    void sendMessage(QString s);
    void send_data_to_mechine(FixturePacketData);

    // mes
    void send_processInspection(const int, const QString& sn, const QString& emp, const QString& machineNo);
    void send_testPass(const int, const QString& sn, const QString& result, const QString& userno,
                       const QString& machineno, const QString& error, const QString& itemvalue);
    void send_processInspection_lx(MesPacketData pack);
    void send_testPass_lx(MesPacketData pack);
    void send_getsn_lx(MesPacketData pack);

    void send_getTestData(const int, const QString& sn, const QString& wpCode, const QString& testItemName);
    void gonextfocus();

    // other
    void operator_instruct(int, int);

    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);

private:
    MesPacketData pack;
    QString result = "";
    QString passValue = "通过";
    QString failValue = "失败";

    typedef enum {
        STATE_IDLE,
        STATE_WATI_CONNECT,      // 等待连接
        STATE_WATI_ASKQR,        // 询问扫码操作
        STATE_WAIT_STATIC,       // 收集无负载时的数据
        STATE_WAIT_PRESS_START,  // 等待治具按下
        STATE_WAIT_READY,        // 等待治具就绪
        STATE_PROCESS_INSPECTION,
        STATE_DISABLE_SLEEP_CALIB,  // 进入禁止休眠
        STATE_SN_INSPECTION,        // 站前检测
        STATE_CALIB_CH_X,           //校准通道x
        STATE_CALIB_CH_X_RESULT,    //校准通道x结果
        STATE_CALIB_ALL_RESULT,     //校准ALL通道结果
        STATE_CHECK_CAIL_RESULT,    // 检查校准结果
        STATE_SEND_CAIL_RESULT,     // 发送校准结果
        STATE_SAVE_CAIL_RESULT,     // 重启
        STATE_CHECK_NFC,            // NFC数据检查(木星独有)
        STATE_WAIT_REST,            // 等待重启
        STATE_DISABLE_SLEEP_TEST,   // 开始禁止休眠
        STATE_SET_COLLECT_PARAM_2,  // 开始采集参数
        STATE_TEST_PARM_LIMIT,      // 校准系数拦截
        STATE_TEST_CH_X,            //测试通道x
        STATE_TEST_CH_X_RESULT,     //测试通道x结果
        STATE_TEST_ALL_RESULT,      //测试ALL通道结果
        STATE_OVERTIME_ERROR,       // 超时
        STATE_SAVE_RESULT,          // 复位牙刷
        STATE_END,                  // 上传mes

        STATE_TEST_1,
        STATE_TEST_2,
    } State;

    typedef enum {
        SYSTEM_COMMAND_ZERO,
        SYSTEM_COMMAND_FAST,
        SYSTEM_COMMAND_200,
        SYSTEM_COMMAND_250,
        SYSTEM_COMMAND_350,
    } system_command_e;

    typedef enum {
        FUNCTION_NONE,
        FUNCTION_CALIB,
        FUNCTION_TEST,
        FUNCTION_CALIB_TEST,
        FUNCTION_MAX,
    } function_e;

    QStringList system_command_list = {"/c:zero", "/c:-21000s", "/c:200g", "/c:250g", "/c:350g"};
    void system_cmd(QString command, system_command_e numb);

    State state = STATE_IDLE;

    int m_index;
    int getIndex() const { return m_index; }
    void clear_display();
    void reset_all();
    void set_fixture_movement(MODEL_ID_E model, State state, int argument);
    void Y20P_fixture(State state, int argument);
    void F20_fixture(State state, int argument);
    void U7_fixture(State state, int argument);
    void ui_msg_show(MODEL_ID_E model, State state, int argument);

    // QSerialPort *serialPort;
    bool isTestContinue = false;
    // Qpb *pb;
    // Qat *at;
    QSerialPort* FixtureserialPort;
    QTime transTime;
    QTime countdowntime;
    QTimer* waittime = new QTimer(this);
    int actual_wait_time = 15000;
    int measure_wait_time = actual_wait_time + 5000;
    bool isovertime = 0;

    std::vector<QCustomPlot*> graph_adc_vector;
    std::vector<QCustomPlot*> graph_value_vector;
    std::vector<ndt_sensor_cali> sensor_v;

    QString stringsn;
    QString macAddress = "没有mac地址";

    QString mUserno = "";    // 操作员工号
    QString line = "";       // 线体
    QString machineNo = "";  // 设备编号
    QString action = "";     // 动作

    QString test_station = "";  // 制程
    QString model = "";         // 机种

    QString nfcComName = "";
    QString Product_Name = "";
    QString product = "";
    MODEL_ID_E product_model = MODEL_ID_INVALID;  // 产品型号

    // ONLY_MODE 此处定义为了区分只校准测试刷头和只校准测试按键，目前只有F20用
    QString only_mode = "";

    // graph setting
    QString Is_use_graph = "";
    GRAPH_SET_E graph_set_argu = GRAPH_SET_INVALID;

    short adc_c[3] = {0, 0, 0};
    short value_c[3] = {0, 0, 0};

    press_calib_data_t cali_result = {0};
    press_calib_data_t LastCali = {0};
    uint8_t is_calib_suc = 0;
    bool IsSaveFail = 0;  // 是否校准完成

    bool mes_set_ok = 0;
    bool isCaliOk = 0;               // 是否校准完成
    bool isStartSendCaliResult = 0;  // 是否开始发送校验结果
    bool repeat_send_ok = 0;         // 校准结果需要重复发送，确保写入，重复发送完成的标志位
    bool donotmoveFlag = false;
    bool brushPlacingFlag = false;
    bool packWeightsFlag = false;
    bool bottonPlacingFlag = false;
    bool caliResultOkFlag = false;
    bool caliResultFailFlag = false;
    bool getaskbarcode = false;
    bool is_get_ready_command = false;
    bool is_get_set_ok_command = false;

    bool isStartcali = 0;  // 是否开始校准

    bool first_start_test = 1;  // 只进行一次
    bool start_calib_channel[3] = {0};
    bool isfirstsavedata = 0;  // 是否开始按键200校准

    int brush_before_calib_count = 0;
    int button_before_calib_count = 0;

    int counter = 0;
    int data_200_true_count = 0;
    int data_250_true_count = 0;
    int data_under_pressure_error_count = 0;
    int data_350_true_count = 0;
    int data_over_pressure_error_count = 0;

    function_e function_switch = FUNCTION_CALIB_TEST;  // 功能选择,校准or测试

    QString donotmove = "人员：别移动牙刷";
    QString brush_placing_200_weights = "人员：刷头放400g砝码";
    QString pack_weights = "人员：拿走400g砝码";
    QString botton_placing_200_weights = "人员：按键放230g砝码";
    QString cali_result_ok = "校准完成";
    QString cali_result_fail = "校准失败";
    void saveDataToLocalFolder(QStringList headers, int32_t* data, uint8_t len, bool appHeader);
    void save_press_test_data_to_csv(const QString& macAddress, const QString& resultunsigned,
                                     press_calib_data_t cali_result);

private slots:

    void solve_mes_data(const int mechines, QString msg);
    void solve_mes_sucess(const int mechines);
    void receive_uart_data(int state);
    void send_mac(QString data);
    void send_frame_rate(QString data);

    void refresh_uart_state(int state);
    void refresh_ble_state(int state);
    void openFixtureSerialPort();
    void closeFixtureSerialPort();
    void FixturereadData();
    void refresh_Fixtureuart_state(int state);
    void on_FixtureconnectButton_clicked();
    void on_FixturerefreshCom_clicked();
    void on_FixturedisconnectButton_clicked();
    void FixturehandleError(QSerialPort::SerialPortError error);
    void processReceivedData(const QByteArray& data);
    void send_start_command(machine_command_id_e command_id, int argument);

    void getTestvalue(const int mechines, const QString value);
    void refresh_sn(FacDevInfo data);
    void delay_msec(unsigned int msec);
    void refreshBaseData(FacGetDevBaseInfo data);
    void readData();
    void openSerialPort();
    void closeSerialPort();
    void handleError(QSerialPort::SerialPortError error);
    void on_refreshButton_clicked();
    void on_connectButton_clicked();

    void getPresscalidata(FacPreSensorCalibResult x);

    void on_end_clicked();

    void getPressSensorData(FacUploadPresSensor x);
    void getButtonState(FacButtonState x);

    void on_macInput_returnPressed();
    void on_getMac_returnPressed();
    void getMac(QString sn_to_search);

    void on_msg_textChanged();

    void on_button_get_calib_factor_clicked();
    QString ReadNfcDataProcess();

    void LogPrintDebug(QString debug);

    void on_SendCalib_clicked();

    void on_ClearGraph_clicked();

    void on_GetButton_clicked();

    void on_TestButton1_clicked();

    void on_TestButtonNFC_clicked();

protected:
    virtual void closeEvent(QCloseEvent*);
};

#endif  // PRESSURESENSORFORM_H
