#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "Abini.h"
#include "quicklz_dec.h"

#include "imu_calibrate.h"
#include "qaudiorecorder.h"
#include "sensor_hub.h"
#include "testmodel.h"
#include <QApplication>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QAudioRecorder>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QImage>
#include <QInputDialog>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QUdpSocket>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>
// #include <iomanip>
// #include <iostream>
#include "camerabox.h"
#include "usmile_ring_buffer.h"
#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QWheelEvent>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:

    QTimer *scanSerialPortsTimer = new QTimer(this);

/*摄像头传图部分*/
    QByteArray pictureByteArray=0;
 int totalsize = 0;
    int dataNumber=0;
#define CRC16(data, len) crc16_compute((const uint8_t *)(data), len, NULL)
    unsigned short crc16_compute(unsigned char const *p_data, unsigned int size,
                                 unsigned short const *p_crc)
    {
        unsigned short crc = (p_crc == NULL) ? 0xFFFF : *p_crc;

        for (uint32_t i = 0; i < size; i++)
        {
            crc = (unsigned char)(crc >> 8) | (crc << 8);
            crc ^= p_data[i];
            crc ^= (unsigned char)(crc & 0xFF) >> 4;
            crc ^= (crc << 8) << 4;
            crc ^= ((crc & 0xFF) << 4) << 1;
        }

        return crc;
    }
//0xAAAAAAAAAAAAAAAA
//0xCCCCCCCCCCCCCCCC

#define EXT_UART_MAGIC      0xCCCCCCCCCCCCCCCC
#define UART_PHY_LAYER_HEAD_SIZE      9   // 头大小
// #define UART_PHY_LAYER_FRAME_SIZE 256
#define UART_PHY_LAYER_CRC_SIZE       0
// #define UART_PHY_LAYER_PLAYLOAD_SIZE 244
#define UART_PHY_LAYER_HEADER_ADN_CRC (UART_PHY_LAYER_HEAD_SIZE + UART_PHY_LAYER_CRC_SIZE)

#define EXT_PICTURE_PHY_LAYER_MAGIC      0xA5A5A5A5
#define PICTURE_PHY_LAYER_HEAD_SIZE      40   // 头大小
// #define PICTURE_PHY_LAYER_FRAME_SIZE 256
#define PICTURE_PHY_LAYER_CRC_SIZE       0
// #define PICTURE_PHY_LAYER_PLAYLOAD_SIZE 244
#define PICTURE_PHY_LAYER_HEADER_ADN_CRC (PICTURE_PHY_LAYER_HEAD_SIZE + PICTURE_PHY_LAYER_CRC_SIZE)

    typedef struct video_frame_data_struct
    {
        uint64_t timestamp;
        int width;
        int height;
        int format;
        int reserved;
        int data_crc16;               // 图像帧校验
        uint32_t data_size;           // 图像帧长度
        uint8_t exposure_time_max;    // 最大曝光时间
        uint8_t exposure_time_mini;   // 最小曝光时间
        uint8_t exposure_time;        // 设置曝光时间
        uint8_t
            exposure_time_rate_of_change;   // 最大曝光时间变化(控制平滑度,数字越大，曝光调整越平滑)
        uint8_t brightness_target;   // 目标亮度 范围：0~255
        uint8_t data[0];             // 图像帧内容.
    } ext_picture_layer_t;

#pragma pack(1)
    typedef struct
    {
        uint64_t magic;
        uint8_t length;
        // uint8_t channel;
        // uint8_t index;
        uint8_t data[0];
    } ext_uart_phy_layer_t;
#pragma pack()
    ImageViewer *viewercamrea;
    QMutex mutex;
    usmile_ring_buffer_t p_dongleRingBuffer;   // 串口的队列指针
    usmile_ring_buffer_t p_cameraRingBuffer;   // 摄像头的队列指针
    uint8_t dongle_ring_buffer[100 * 1024];    // 串口队列池
    uint8_t frame_buf[2 * 1024];               // 队列池
    uint8_t camera_ring_buf[100 * 1024];       // 摄像头队列池
    uint8_t frame_picture_buf[37 * 1024];      // 照片队列池
    int ext_ble_find_next_frame(void);
    int ext_ble_find_next_picture_frame(void);
    void write_camera_data(uint8_t *p_data, int data_len);
    RingBuf *dongleRingBuf = nullptr;
    RingBuf *cameraRingBuf = nullptr;
    void solve_frame(void);
    void solve_picture_frame(void);
    /*摄像头传图部分*/

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    ImuDataT orgData;
private:
    NewImuCalData calData;
    new_imu_calibrate *nqimuc = nullptr;
    QString getValueBySN(const QString &sn ) ;
    QByteArray subpid;

    bool eventFilter(QObject *watched, QEvent *event);
    QString receivedData = "";
    QString voltageresult;
    QString charageresult;
    void save_battary_data_to_csv(double vol, QString charge_state, QString chares, QString volres);
    double voltage;
    QString chargestate;
    double standbattary = 0;
    int is_battary_test = 0;
    QString motorresult = "";
    QString passValue = "通过";
    QString failValue = "失败";
    QString stringsn;
    QAudioRecorder *audioRecorder;
    QString generateOutputFilePath();
    void save_motor_to_csv(QString SN, QString Mac, QString csvresult);
    QMap<QString, QMap<QString, QString>> deviceMap;   // 存储设备信息
    QString WIFI_RSSI;
    QString BLE_RSSI;
    int HighRssi;
    int LowRssi;
    int BleHighRssi;
    int BleLowRssi;
    int RssiTestTime;
    QString productName;
    void save_RSSI_data_to_csv(int intwifirssi, int intblerssi, QString wifiresult,
                               QString bleresult);
    QLabel *bleStatusLabel = nullptr;
    QLabel *WifiStatusLabel = nullptr;
    QSerialPort *dongleSerialPort;   // dongle硬件层
    QLabel *uartStatusLabel = nullptr;
    QLabel *frame_rate = nullptr;
    QLabel *macLabel = nullptr;
    QLabel *board_sn = nullptr;
    QLabel *tail_sn = nullptr;
    QLabel *sub_pid = nullptr;
    void saveToExcel(const QString &filename, const FacUploadNineAlex &x);

  std::atomic<bool> running;
     QFuture<void> future;
    Qpb *pb;
    Qat *at;
    typedef enum
    {
        STATE_IDLE,              // 休眠状态
        STATE_WATI_CONNECT,      // 等待连接
        STATE_DISABLE_SLEEP_1,   // 进入禁止休眠
        MOTOR_CALI1,
        MOTOR_CALI2,
        MOTOR_CALI_DATA_SET,
        MOTOR_TESTING,
        CAMERA_TEST,
        STATE_SAVE_RESULT   // 保存结果在本地
    } motorState;
    QButtonGroup *OTAGroup = new QButtonGroup(this);
    bool is_motor_continue = false;
    motorState motorstate = STATE_IDLE;
    imu_calibrate *qimuc = nullptr;
    TestModel *basicInfoModel = nullptr;
    TestModel *displaybasicInfoModel = nullptr;
    TestModel *peripheralModel = nullptr;
    bool isimuCaliContinue = false;
    bool isrssiContinue = false;
    Ui::MainWindow *ui;
    void saveCustom();
    void recoverCustom();
    QString snbanding;
    QString macAddress = "没有mac地址";
    bool isimuCaliOk = 0;         // 是否校准完成
    bool is_start_ium_cali = 0;   // 是否开始六轴校准
    void update_main_style(QString style);
    QTimer *waittime = new QTimer(this);
    // 在您的类中声明定时器
    QTimer *cameratimer = new QTimer(this);

    int imu_wait_time = 15000;
    int music_time = 30000;
    bool isovertime = 0;              // 是否开始发送校验结果
    bool isfirstsavedata = 0;         // 是否开始按键200校准
    bool isStartSendCaliResult = 0;   // 是否开始发送校验结果
    void save_imu_test_data_to_csv(const QString &macAddress, const QString &result);
    void initBasicInfo();
    void initPeriphState();
    void band_sn_mac_to_csv(const QString &macAddress, const QString &sn);
    void writePeripheralDataToCSVFile();
    void writeDataToCSVFile();
    void clear_display();
    void SendRadomDataPushButton();
    void waitWork(int ms);
    void sendData(bool is_random);
    void SendRecord();
    bool isContinue = true;
    void on_sendRecord_clicked();
    void on_radomDataPushButton_clicked();
    QTimer *dongleSerialPortTimer = new QTimer(this);
    QByteArray dongleSerialPortBuf = 0;
    void updateHIDComboBox(QComboBox *comboBox);

    // 定义用于保存MAC地址的QString变量
    QString csvmac;
    QComboBox *comNameCombo;
    QUdpSocket *udpSocket;
protected:
    virtual void closeEvent(QCloseEvent *);

signals:
    void sendMessage(QString s);
    void send_uart_state(int data);
    void send_ble_state(int data);
    void send_mac(QString data);
    void send_frame_rate(QString data);
    void refreshDongleSerialPortState(int state);
    void imageProcessed();
    void send_thread_date(QString);


private slots:
    void updateImageOnMainThread();
    void refresh_log_data(QString data);

    void showlog(QString msg);
    void refresh_imu_cali_msg(QString msg);
    void send_picture(const QString &url, const QString &filePath);
    void renameFilesInFolder(const QString &folderPath);
    void convertImageTo16BitPaletteHigh(const QString &imagePath, const QString &outputFileName);
    void update_IMU_CALIB_result(FacImuCalibResult x);
    void updateComboBox();
    void getmacadress(const QByteArray &byte);
    void refresh_sn(FacDevInfo data);
    void refresh_WIFI_state(int state);
    void get_wifi_msg(QString data);
    void stopRecording();
    void closeDongleSerialPort(void);
    void refresh_dongle_uart_state(int state);
    void openDongleSerialPort(void);
    void readDongleSerialPortData(void);
    void handleDongleSerialPortError(QSerialPort::SerialPortError error);
    void on_connectButton_clicked();
    void on_getBasicInfoButton_clicked();
    void on_getperipheralButton_clicked();
    void on_disconnectButton_clicked();
    void on_imuCaliButton_clicked();
    void on_lcdTestButton_clicked();
    void on_enterShipModeButton_clicked();
    void on_macInput_returnPressed();
    void refresh_ble_rssi(QString data);
    void refresh_wifi_rssi(QString data);
    void refresh_ble_state(int state);
    void getimuData(FacUploadNineAlex x);
    void refresh_battary_data(FacDevInfo adc);
    void update_wifi(FacDevInfo wifi);
    void on_enterBurningMode_clicked();
    void on_exitBurningMode_clicked();
    void on_snInput_returnPressed();
    void on_stopimuCaliButton_clicked();
    void on_music_play_clicked();
    void on_entersleep_clicked();
    void on_forbidsleep_clicked();
    void on_pushButton_clicked();
    void on_fac_mode_clicked();
    void on_getwifi_clicked();
    void on_disconnectwifi_clicked();
    void on_connectwifi_clicked();
    void on_rebot_clicked();
    void on_exit_fac_mode_clicked();
    void on_fre_returnPressed();
    void on_duty_returnPressed();
    void on_close_motor_clicked();
    void on_start_motor_clicked();
    void on_setTimePushButton_clicked();
    void on_resetPushButton_clicked();
    void on_sendDataPushButton_clicked();
    void on_sendRandomData_clicked();
    void on_clearDataPushButton_clicked();
    void on_just_music_clicked();
    void on_start_wifible_test_clicked();
    void on_buruing1_clicked();
    void on_get_device_sn_clicked();
    void on_close_imu_collect_clicked();
    void on_open_imu_collect_clicked();
    void on_clear_scan_clicked();
    void on_pushButton_2_clicked();
    void on_mac_combo_textActivated(const QString &arg1);
    void banding_mac_sn(QString bandingmac, QString bandingsn);
    void get_mac(QString sn_to_search);
    void on_bleTestPushButton_clicked();
    void on_macInput_7_returnPressed();
    void update_local_ota_result(FacInternetOta x);
    void update_FactroyCmd_WIFI_DEMAND(FacWifiDemand);
    void on_stopTestPushButton_clicked();
    void on_configWifiPushButton_clicked();
    void on_getInfoPushButton_clicked();
    void on_snbanding_returnPressed();
    void on_get_mac_returnPressed();
    void on_damping_open_clicked();
    void on_otaTestPushButton_clicked();
    void on_damping_close_clicked();
    void on_motor_cali_clicked();
    void on_end_motor_cali_clicked();
    void on_start_scan_clicked();
    void on_enterwhitemode_clicked();
    void on_otaTestPushButton_2_clicked();
    void ota_source_set(int state);
    void ota_fw_set(int state);

    void refresh_pb_data(QString data);
    void on_close_camera_clicked();
    void on_open_camera_clicked();
    void on_open_camear_light_clicked();
    void on_close_camear_light_clicked();
    void on_configWifiPushButton_2_clicked();
    void on_exposure_time_edit_returnPressed();
    void on_sweeping_angle_returnPressed();
    void on_motor_cali_param_returnPressed();
    void on_start_brush_clicked();
    void on_open_press_collect_clicked();
    void getPressSensorData(FacUploadPresSensor x);
    void get_servo_motor_info_msg(FacMotorCalibResult data);
    QString getMotorStateString(FacMotoState state) ;
    QString getMotorFaultCodeString(FacMotorFaultCode faultCode);
    QString getCaliMarkString(CaliMark caliMark);

    void saveDataToLocalFolder(uint32_t data1, int data2, uint32_t data3, int data4,
                               bool appHeader);
    void on_close_press_collect_clicked();
    void on_test_cali_clicked();
    void on_open_motor_test_clicked();
    void on_close_motor_test_clicked();
    void on_start_local_ota_clicked();
    void readPendingDatagrams();
    void on_new_connectwifi_clicked();
    void on_get_imu_info_clicked();
    void on_swing_test_clicked();
    void on_light_test_clicked();
    void on_clear_picture_clicked();
    void on_start_search_clicked();
    void on_pick_device_textActivated(const QString &arg1);
    void on_open_motor_cali_clicked();
    void on_close_motor_cali_clicked();
    void on_R1_valueChanged(int value);
    void on_G1_valueChanged(int value);
    void on_B1_valueChanged(int value);
    void on_R2_valueChanged(int value);
    void on_G2_valueChanged(int value);
    void on_B2_valueChanged(int value);
    void on_R3_valueChanged(int value);
    void on_G3_valueChanged(int value);
    void on_B3_valueChanged(int value);
    void on_R4_valueChanged(int value);
    void on_G4_valueChanged(int value);
    void on_B4_valueChanged(int value);
    void refresh_color1();
    void refresh_color2();
    void refresh_color3();
    void refresh_color4();
    void on_pushButton_3_clicked();
    void on_pink_led_clicked();
    void on_white_led_clicked();
    void refresh_motor_cali_msg(QString msg);
    void processTheDatagram(QByteArray &datagram);
    void on_get_motor_log_clicked();
    void on_set_imu_info_clicked();
    void on_calculate_returnPressed();
    void on_distribution_network_clicked();
    void on_save_photo_clicked();
    void on_clean_mode_clicked();
    void on_nfc_write_read_clicked();
    void on_clear_nfc_data_clicked();
    void on_app_connect_clicked();
    void on_open_screen_show_camera_clicked();
    void on_close_screen_show_camera_clicked();
    void on_open_support_camera_clicked();
    void on_close_support_camera_clicked();
    void on_open_camera_picture_clicked();
    void on_close_camera_picture_clicked();
    void on_nfc_decode_clicked();
    void on_nfc_read_clicked();
    void on_nfcComFresh_clicked();
    void on_nfc_encode_clicked();
    void on_get_device_subpid_clicked();
    void on_add_data_clicked();
    void on_init_ui_data_clicked();
    void on_get_battery_clicked();
    void on_get_motor_info_clicked();
    void on_get_board_sn_clicked();
    void on_write_device_sn_clicked();
    void on_write_board_sn_clicked();
    void on_write_device_subpid_clicked();
    void scanSerialPorts();

    void on_send_wifi_config_clicked();
    void on_get_battery_level_clicked();
    void on_up_picture_clicked();
    void on_down_picture_clicked();
    void on_play_picture_clicked();
};
#endif   // MAINWINDOW_H
