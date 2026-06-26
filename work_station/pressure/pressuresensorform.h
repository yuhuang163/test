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
#include "qatmanager.h"
#include "qcustomplot.h"
#include "qpb.h"
#include "test_base.h"
#include "ui_pressuresensorform.h"
#include "xwdmes.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace Ui {
class PressureSensorForm;
}

class PressureSensorForm : public test_base {
    Q_OBJECT
  public:
    explicit PressureSensorForm(int index, QWidget* parent = nullptr);
    ~PressureSensorForm();
    Ui::PressureSensorForm* ui;

    // clang-format off
    // test_base 控件桥接
    QComboBox* getComNameCombo() override { return ui->comNameCombo; }
    QComboBox* getJigcomNameCombo() override { return ui->jigComNameCombo; }
    QLineEdit* macInputLineEdit() override { return ui->macInput; }
    QLineEdit* getMacLineEdit() override { return ui->getMac; }
    QPlainTextEdit* logEdit() override { return ui->log; }
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; }
    QLabel* getMesStateQlabel() override { return ui->mes_state; }
    QPushButton* getEndTestButton() override { return ui->end; }
    QCheckBox* getIsUseMes() override { return ui->isusemes; }
    QCheckBox* getIsFormMes() override { return ui->isformmes; }
    QComboBox* getNfcComboBox() override { return ui->NfcComboBox; }
    // clang-format on

    void startTask() override;
    void show_product(QString name);
    void processInspection(QString inputSnText);
    void processGetMesTestValue();
    typedef enum {
        MODEL_ID_INVALID,
        MODEL_ID_Y20P,
        MODEL_ID_Y20,
        MODEL_ID_Y21,
        MODEL_ID_F20,
        MODEL_ID_U7,
        MODEL_ID_P30P,
        MODEL_ID_Y30S,
        MODEL_ID_Y20PS,
        MODEL_ID_Y30P,

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
    typedef enum {
        SET_INVALID,
        BTH_ONLY,
        KEY_ONLY,
        ALL_ONLY,
    } ONLY_SET_E;

    void overTask() override;
    void product_model_init(QString model);

    // graph
    void graph_init(MODEL_ID_E model);
    void graph_update(FacUploadPresSensor x);
    void graph_reset(uint8_t argument);

    // save file
    void save_pressure_data(FacUploadPresSensor x);

    // calib and test
    void calib_process(FacUploadPresSensor x);
    void test_process(FacUploadPresSensor x);
    void calib_vector_init(MODEL_ID_E model);
    void calib_send_result(void);

    uint8_t CheckNfcData();
    int findNfcDevicePort(QString name);
    int total_sensor = 0; // 传感器数�?
    int calib_chan = 0;   // 当前校准通道
    int test_chan = 0;    // 当前测试通道

    uint8_t TEST_STATE = 0;

  signals:

    // uart

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
    void operator_instruct(int);
    void send_go_next_focus();
    void send_start_test(int data);
    void send_go_next_test(int data);

  private:
    QString result = "";
    QString passValue = "通过";
    QString failValue = "失败";
    typedef enum {
        STATE_IDLE,
        STATE_WATI_CONNECT,     // 等待连接
        STATE_WATI_ASKQR,       // 询问扫码操作
        STATE_WAIT_STATIC,      // 收集无负载时的数�?
        STATE_WAIT_PRESS_START, // 等待治具按下
        STATE_WAIT_READY,       // 等待治具就绪
        STATE_PROCESS_INSPECTION,
        STATE_DISABLE_SLEEP_CALIB, // 进入禁止休眠
        STATE_SN_INSPECTION,       // 站前检�?
        STATE_CALIB_CH_X,          //校准通道x
        STATE_CALIB_CH_X_RESULT,   //校准通道x结果
        STATE_CALIB_ALL_RESULT,    //校准ALL通道结果
        STATE_CHECK_CAIL_RESULT,   // 检查校准结�?
        STATE_SEND_CAIL_RESULT,    // 发送校准结�?
        STATE_SAVE_CAIL_RESULT,    // 重启
        STATE_CHECK_NFC,           // NFC数据检�?木星独有)
        STATE_WAIT_REST,           // 等待重启
        STATE_DISABLE_SLEEP_TEST,  // 开始禁止休�?
        STATE_SET_COLLECT_PARAM_2, // 开始采集参�?
        STATE_TEST_PARM_LIMIT,     // 校准系数拦截
        STATE_TEST_CH_X,           //测试通道x
        STATE_TEST_CH_X_RESULT,    //测试通道x结果
        STATE_TEST_ALL_RESULT,     //测试ALL通道结果
        STATE_OVERTIME_ERROR,      // 超时
        STATE_AMPLITUEDE,          //摆幅测试

        STATE_SAVE_RESULT, // 复位设备
        STATE_END,         // 上传mes

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

    State state = STATE_IDLE;

    void clear_display();
    void reset_all();
    void set_fixture_movement(MODEL_ID_E model, State state, int argument);
    void Y20P_fixture(State state, int argument);
    void F20_fixture(State state, int argument);
    void U7_fixture(State state, int argument);
    void Y20PS_fixture(State state, int argument);
    void ui_msg_show(MODEL_ID_E model, State state, int argument);

    QTime transTime;
    QTime countdowntime;
    QTimer* waittime = new QTimer(this);
    int pressTestTime = 5000;      //测试时间
    int measure_wait_time = 15000; //校准时间
    bool isovertime = 0;
    int AdcShakeValue = 100;

    std::vector<QCustomPlot*> graph_adc_vector;
    std::vector<QCustomPlot*> graph_value_vector;
    std::vector<ndt_sensor_cali*> sensor_v;

    QString stringsn;
    QString macAddress = "没有mac地址";
    QString readProduct = "";
    MODEL_ID_E product_model = MODEL_ID_INVALID; // 产品型号

    // ONLY_MODE 此处定义为了区分只校准测试电机和只校准测试按键，目前只有F20�?
    ONLY_SET_E only_mode = SET_INVALID;

    // graph setting
    QString Is_use_graph = "";
    GRAPH_SET_E graph_set_argu = GRAPH_SET_INVALID;

    short adc_c[3] = {0, 0, 0};
    short value_c[3] = {0, 0, 0};

    press_calib_data_t cali_result = {{0}};
    press_calib_data_t LastCali = {{0}};
    uint8_t is_calib_suc = 0;
    bool IsSaveFail = 0;            // 是否校准完成
    bool isCaliOk = 0;              // 是否校准完成
    bool isStartSendCaliResult = 0; // 是否开始发送校验结�?
    bool repeat_send_ok = 0;        // 校准结果需要重复发送，确保写入，重复发送完成的标志�?
    bool donotmoveFlag = false;
    bool brushPlacingFlag = false;
    bool packWeightsFlag = false;
    bool bottonPlacingFlag = false;
    bool caliResultOkFlag = false;
    bool caliResultFailFlag = false;
    bool getaskbarcode = false;
    bool is_get_ready_command = false;
    bool is_get_set_ok_command = false;
    bool isStartcali = 0;    // 是否开始校�?
    int Amplituderesult = 0; // 只进行一�?
    int Amplitudetimes = 0;
    bool first_start_test = 1; // 只进行一�?
    bool start_calib_channel[3] = {0};
    bool isfirstsavedata = 0; // 是否开始按�?00校准
    int brush_before_calib_count = 0;
    int button_before_calib_count = 0;
    int counter = 0;
    int data_200_true_count = 0;
    int data_250_true_count = 0;
    int data_under_pressure_error_count = 0;
    int data_350_true_count = 0;
    int data_over_pressure_error_count = 0;

    function_e function_switch = FUNCTION_CALIB_TEST; // 功能选择,校准or测试

    QString donotmove = "人员：别移动设备";
    QString brush_placing_200_weights = "人员：电机放400g砝码";
    QString pack_weights = "人员：拿�?00g砝码";
    QString botton_placing_200_weights = "人员：按键放230g砝码";
    QString cali_result_ok = "校准完成";
    QString cali_result_fail = "校准失败";
    void savePressDataToLocalFolder(const ProtocolPressSampleData& x, bool appHeader);
    void save_press_test_data_to_csv(const QString& macAddress, press_calib_data_t cali_result);

  public slots:
    void updateGraphs(); // 界面更新槽函�?
  signals:
    void dataReady();

  private:
    QQueue<QPair<uint32_t, QVector<int16_t>>> adcDataQueue;
    QQueue<QPair<uint32_t, QVector<int16_t>>> valueDataQueue;

    QTimer graphUpdateTimer;

  private slots:
    void refreshPressSensorData(ProtocolPressSampleData x) override;
    void refreshAmplitudeData(QString data) override;
    void receive_uart_data(int state);
    void send_frame_rate(QString data);
    void refreshDongleUartState(int state) override;
    void refreshBleState(int state) override;
    void refreshJigUartState(int state) override;
    void on_jigConnectButton_clicked();
    void on_jigDisconnectButton_clicked();
    void processFixReceivedData(const QByteArray& data);
    void send_start_command(machine_command_id_e command_id, int argument);
    void getTestValue(const int mechines, const QString value) override;
    void refreshSn(ProtocolSnData data) override;
    void delay_msec(unsigned int msec);
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void on_connectButton_clicked();
    void refreshButton(ProtocolButtonStateData data) override;
    void refreshPressCalibData(ProtocolPressCalibResultData x) override;
    void on_end_clicked();
    void on_macInput_returnPressed();
    void on_getMac_returnPressed();
    void on_button_get_calib_factor_clicked();
    QString ReadNfcDataProcess();
    void on_ClearGraph_clicked();
    void on_GetButton_clicked();
    void on_TestButton1_clicked();
    void on_TestButtonNFC_clicked();
    void on_disconnectButton_clicked();
};

#endif // PRESSURESENSORFORM_H
