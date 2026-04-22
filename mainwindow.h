
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QApplication>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QAudioRecorder>
#include <QAuthenticator>
#include <QButtonGroup>
#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QImage>
#include <QInputDialog>
#include <QMap>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QQmlApplicationEngine>
// #include <QTextToSpeech>
#include <QUdpSocket>
#include <QUrl>
#include <QVector>
#include <algorithm>
#include <QMessageBox>
#include "Abini.h"
#include "common_class.h"
#include "imu_calibrate.h"
#include "qaudiorecorder.h"
#include "qprotocolmanager.h"
#include "sensor_hub.h"
#include "testmodel.h"
// #include <iomanip>
// #include <iostream>
#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QWheelEvent>
#include "qfctp.h"



extern "C" {
#include "md5.h"  // 引入 tiny-AES-c 的头文件
}
#include "camerabox.h"
#include "usmile_ring_buffer.h"
Q_DECLARE_METATYPE(FacErrorCode)

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}

QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /*摄像头传图部分*/
    QByteArray pictureByteArray = 0;
    int cameradatasize = 0;
    int dataNumber = 0;
#define CRC16(data, len) crc16_compute((const uint8_t*)(data), len, NULL)
#define EXT_UART_MAGIC 0xCCCCCCCCCCCCCCCC  // 0xAAAAAAAAAAAAAAAA

#define UART_PHY_LAYER_HEAD_SIZE 9  // 头大小
#define UART_PHY_LAYER_LENGTH 1
#define UART_PHY_LAYER_HEADER_ADN_LEN (UART_PHY_LAYER_HEAD_SIZE + UART_PHY_LAYER_LENGTH)

#define EXT_PICTURE_PHY_LAYER_MAGIC 0xA5A5A5A5
#define PICTURE_PHY_LAYER_HEAD_SIZE sizeof(video_frame_data_struct)  // 头大小
#define PICTURE_PHY_LAYER_HEADER_ADN_CRC (PICTURE_PHY_LAYER_HEAD_SIZE)

    typedef enum {
        PHY_CHANNEL_INVALID = 0,  //无效值
        PHY_CHANNEL_CAMREA,       //摄像头通道
        PHY_CHANNEL_LOG,          // LOG数据通道

    } ext_ble_phy_channel_e;

#pragma pack(1)

    typedef struct video_frame_data_struct {
        uint64_t timestamp;
        int width;
        int height;
        int format;
        int reserved;
        int data_crc16;                        // 图像帧校验
        uint32_t data_size;                    // 图像帧长度
        uint8_t exposure_time_max;             // 最大曝光时间
        uint8_t exposure_time_mini;            // 最小曝光时间
        uint8_t exposure_time;                 // 设置曝光时间
        uint8_t exposure_time_rate_of_change;  // 最大曝光时间变化
        uint8_t brightness_target;             // 目标亮度 范围：0~255
        uint8_t data[0];                       // 图像帧内容.
        // 20 12 00 00 00 00 00 00 b4000000 c8000000 00000000 a5a5a5a5 53040000
        // data_size开始 :a08c0000 78 01 3c 05 78
    } ext_picture_layer_t;

    typedef struct {
        uint64_t magic;
        uint8_t length;
        uint8_t channel;
        // uint8_t index;
        uint8_t data[0];
    } ext_uart_phy_layer_t;
#pragma pack()
    ImageViewer* viewercamrea;
    QMutex mutex;
    usmile_ring_buffer_t p_dongleRingBuffer;  // 串口的队列指针
    usmile_ring_buffer_t p_cameraRingBuffer;  // 摄像头的队列指针
    uint8_t dongle_ring_buffer[100 * 1024];   // 串口队列池
    uint8_t frame_buf[2 * 1024];              // 队列池
    uint8_t camera_ring_buf[100 * 1024];      // 摄像头队列池
    uint8_t frame_picture_buf[50 * 1024];     // 照片队列池
    int ext_ble_find_next_frame(void);
    int ext_ble_find_next_picture_frame(QByteArray& picturedata);
    void write_camera_data(uint8_t* p_data, int data_len);
    RingBuf* dongleRingBuf = nullptr;
    RingBuf* cameraRingBuf = nullptr;
    void solve_frame(void);
    void solve_picture_frame(QByteArray picturedata);

    /*摄像头传图部分*/
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    ImuDataT orgData;
    QNetworkAccessManager* updatamanager;

private:
    int totalBleSendData = 0;
    int stopBleOta = 0;
    void saveDongleUartLog(QString data);
    NewImuCalData calData;
    new_imu_calibrate* nqimuc = nullptr;
    QByteArray subpid;
    QString receivedData = "";
    QString voltageresult;
    QString charageresult;
    double voltage;
    QString chargestate;
    double standbattary = 0;
    int is_battary_test = 0;
    QString motorresult = "";
    QString passValue = "通过";
    QString failValue = "失败";
    QString stringsn;
    QAudioRecorder* audioRecorder;
    QString generateOutputFilePath();
    void save_motor_to_csv(QString SN, QString Mac, QString csvresult);
    QMap<QString, QMap<QString, QString>> deviceMap;  // 存储设备信息
    QString WIFI_RSSI;
    QString BLE_RSSI;
    int HighRssi;
    int LowRssi;
    int BleHighRssi;
    int BleLowRssi;
    int RssiTestTime;
    QString connectProductName;
    QLabel* bleStatusLabel = nullptr;
    QLabel* WifiStatusLabel = nullptr;
    QSerialPort* dongleSerialPort;  // dongle硬件层
    QLabel* uartStatusLabel = nullptr;
    QLabel* frame_rate = nullptr;
    QLabel* macLabel = nullptr;
    QLabel* board_sn = nullptr;
    QLabel* tail_sn = nullptr;
    QLabel* sub_pid = nullptr;
    QLabel* sku_id = nullptr;

    std::atomic<bool> running;
    QFuture<void> future;
    QProtocolManager protocolManager;
    Qfctp* qfctp = nullptr;
    Qpb* pb= nullptr;
    Qat* at= nullptr;
    TestFunctionExecutor executor;
    typedef enum {
        STATE_IDLE,             // 休眠状态
        STATE_WATI_CONNECT,     // 等待连接
        STATE_DISABLE_SLEEP_1,  // 进入禁止休眠
        MOTOR_CALI1,
        MOTOR_CALI2,
        MOTOR_CALI_DATA_SET,
        MOTOR_TESTING,
        CAMERA_TEST,
        STATE_SAVE_RESULT  // 保存结果在本地
    } motorState;
    QButtonGroup* OTAGroup = new QButtonGroup(this);
    qsetting* qsetting_ui = NULL;
    bool is_motor_continue = false;
    bool is_need_noisy_data = false;
    motorState motorstate = STATE_IDLE;
    imu_calibrate* qimuc = nullptr;
    TestModel* basicInfoModel = nullptr;
    TestModel* displaybasicInfoModel = nullptr;
    TestModel* peripheralModel = nullptr;
    bool isimuCaliContinue = false;
    bool isrssiContinue = false;
    Ui::MainWindow* ui;
    QString snbanding;
    QString macAddress = "没有mac地址";
    bool isimuCaliOk = 0;        // 是否校准完成
    bool is_start_ium_cali = 0;  // 是否开始六轴校准
    void updateMainStyle(QString style);
    QTimer* waittime = new QTimer(this);
    QTimer* noisytimer = nullptr;

    QTimer* cameratimer = new QTimer(this);
    QTimer* scanSerialPortsTimer = new QTimer(this);
    QTimer* dongleSerialPortTimer = new QTimer(this);
    int imu_wait_time = 15000;
    int music_time = 30000;
    bool isovertime = 0;             // 是否开始发送校验结果
    bool isfirstsavedata = 0;        // 是否开始按键200校准
    bool isStartSendCaliResult = 0;  // 是否开始发送校验结果
    bool isWifiOtaContinue = true;
    int otaTesttimes = 1;
    int wifiotaFaiTimes = 0;
    int wifiotaSuctimes = 0;
    QTime totalwifiOtaTime;
    // 存储数据包的容器，按序号排序
    QMap<int, QByteArray> packetMap;
    QVector<int> faultData;
    QByteArray dongleSerialPortBuf = 0;
    QTime bleOtaTestTime;

    // 定义用于保存MAC地址的QString变量
    QString csvmac;
    QComboBox* comNameCombo;
    QUdpSocket* udpSocket = new QUdpSocket(this);
    // QTextToSpeech* tts;
    QString result = "";
    bool otaFinish = false;
    QStringList otaResults;
    QTimer* bleotatimer = new QTimer(this);
    int currentChunk = 0;

private:
    QNetworkAccessManager* aimanager;

protected:
    void closeEvent(QCloseEvent*) override;

private slots:
    void solve_photosensitive_info(FacDevInfo x);
    void solve_sd_info(FacDevInfo x) ;
    void appendAndSaveWifiOtaLog(const QString& msg);
    void sendAifile(QString file_id);
    void renameAndProcessFolders(const QString& directoryPath);
    void uploadFile(const QString& filePath);
    void myAudioRecorde(bool delay);
    void sendAiMessage();
    void onRequestFinished(QNetworkReply* reply);
    void renameAduioFilesInFolder(const QString& folderPath);
    void processAudio(const QString& inputFile, const QString& outputFile, const QString& volumeChangeDb,
                      const QString& play_speed);
    void setting_ui();
    void saveblackbox(QString data);
    void sendNoisyData();
    void setBleOtaState(int);
    void checkAndUpdateFile();
    void deleteFile(const QString& remoteUrl);
    void provideAuthentication(QNetworkReply* reply, QAuthenticator* authenticator);
    void downloadMyApp(const QString& urlStr, const QString& savePath);
    void uploadFile(const QString& localFilePath, const QString& remoteUrl);
    void checkMissingPackets();
    void addPacket(const QByteArray& packet);
    void printSquareData(uint8_t* data, int data_size);
    bool deleteCsvFile(const QString& filePath);
    void saveBattaryDataToCsv(double vol, QString charge_state, QString chares, QString volres);
    bool eventFilter(QObject* watched, QEvent* event);
    void saveCustom();
    void recoverCustom();
    void saveRssiDataToCsv(int intwifirssi, int intblerssi, QString wifiresult, QString bleresult);
    void updateHIDComboBox(QComboBox* comboBox);
    void saveImuTestDataToCsv(const QString& macAddress, const QString& result);
    void initBasicInfo();
    void initPeriphState();
    void bandSnMacToCsv(const QString& macAddress, const QString& sn);
    void writePeripheralDataToCSVFile();
    void writeDataToCSVFile();
    void clearDisplay();
    void SendRadomDataPushButton();
    void solveNosiyData(QByteArray dataTemp);
    void waitWork(int ms);
    void sendBrushData(bool is_random);
    void sendRecord();
    void getPictureSendOver(FacPictureDataAck x);
    void updateImageOnMainThread();
    void refreshLogData(QString data);
    void saveToCsv(const QString& filename, const FacUploadNineAlex& x);
    void imu_normol_saveToCsv(const QString& filename, const FacUploadNineAlex& x);
    void showlog(QString msg);
    void refreshImuCaliMsg(QString msg);
    void sendPicture(const QString& url, const QString& filePath);
    void renamePictureFilesInFolder(const QString& folderPath);
    void convertImageTo16BitPaletteHigh(const QString& imagePath, const QString& outputFileName);
    void refreshImuCaliResult(FacImuCalibResult x);
    void updateComboBox();
    void getmacadress(const QByteArray& byte);
    void refreshSn(FacDevInfo data);
    void refreshWifiState(int state);
    void getWifiMsg(QString data);
    void getWifiIp(QString data);

    void getDongleVer(QString data);
    void getDongleWifi(QString data);
    void stopRecording();
    void closeDongleSerialPort(void);
    void refreshDongleUartState(int state);
    void openDongleSerialPort(void);
    void readDongleSerialPortData(void);
    void handleDongleSerialPortError(QSerialPort::SerialPortError error);
    void refreshBleRssi(QString data);
    void refreshWifiRssi(QString data);
    void refreshBleState(int state);
    void getimuData(FacUploadNineAlex x);
    void refreshBattaryData(FacDevInfo adc);
    void updateWifi(FacDevInfo wifi);
    void bandingMacSn(QString bandingmac, QString bandingsn);
    void getMac(QString sn_to_search);
    void updateLocalOtaResult(FacInternetOta x);
    void refreshWifiDemand(FacWifiDemand);
    void otaSourceSet(int state);
    void otaFwSet(int state);
    void refreshPbData(QString data);
    void getPressSensorData(FacUploadPresSensor x);
    void getServoMotorInfoMsg(FacMotorCalibResult data);
    void convertCsvToXls(const QString& csvFilename, const QString& xlsFilename);
    void savePressDataToLocalFolder(const FacUploadPresSensor& x, bool appHeader);
    void readPendingDatagrams();
    void refreshColor1();
    void refreshColor2();
    void refreshColor3();
    void refreshColor4();
    void refreshMotorCaliMsg(QString msg);
    void processTheDatagram(QByteArray& datagram);
    void scanSerialPorts();
    QString getValueBySN(const QString& sn);
    QByteArray reassembleData();
    QString getMotorStateString(FacMotoState state);
    QString getMotorFaultCodeString(FacMotorFaultCode faultCode);
    QString getCaliMarkString(CaliMark caliMark);
    void refreshMusicState(FacDevInfo data);
    void getPresscalidata(FacPreSensorCalibResult x);
    void scanIpPorts();
    void checkbutton(FacButtonState data);
private slots:
    void on_connectButton_clicked();
    void on_getBasicInfoButton_clicked();
    void on_getperipheralButton_clicked();
    void on_disconnectButton_clicked();
    void on_imuCaliButton_clicked();
    void on_lcdTestButton_clicked();
    void on_enterShipModeButton_clicked();
    void on_macInput_returnPressed();
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
    void on_sendBrushDataPushButton_clicked();
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
    void on_mac_combo_textActivated(const QString& arg1);
    void on_bleTestPushButton_clicked();
    void on_wifiOtaMacInput_returnPressed();
    void on_stopTestPushButton_clicked();
    void on_configWifiPushButton_clicked();
    void on_getInfoPushButton_clicked();
    void on_snbanding_returnPressed();
    void on_getMac_returnPressed();
    void on_damping_open_clicked();
    void on_otaTestPushButton_clicked();
    void on_damping_close_clicked();
    void on_motor_cali_clicked();
    void on_end_motor_cali_clicked();
    void on_start_scan_clicked();
    void on_enterwhitemode_clicked();
    void on_otaTestPushButton_2_clicked();
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
    void on_close_press_collect_clicked();
    void on_test_cali_clicked();
    void on_open_motor_test_clicked();
    void on_close_motor_test_clicked();
    void on_start_local_ota_clicked();
    void on_new_connectwifi_clicked();
    void on_get_imu_info_clicked();
    void on_swing_test_clicked();
    void on_light_test_clicked();
    void on_clear_picture_clicked();
    void on_start_search_clicked();
    void on_pick_device_textActivated(const QString& arg1);
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
    void on_pushButton_3_clicked();
    void on_pink_led_clicked();
    void on_white_led_clicked();
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

    void on_get_battery_level_clicked();
    void on_up_picture_clicked();
    void on_down_picture_clicked();
    void on_play_picture_clicked();
    void on_open_imu_collect_solve_clicked();
    void on_py_test_clicked();

    void on_close_imu_collect_solve_clicked();
    void on_transfer_xls_clicked();
    void on_nfc_close_clicked();
    void on_close_motor_adc_switch_clicked();
    void on_open_motor_adc_switch_clicked();
    void on_downloadapp_clicked();
    void on_uploadapp_clicked();
    void on_delefile_clicked();
    void on_startBleOta_clicked();
    void on_bleotamacInput_returnPressed();
    void on_selectPath_clicked();
    void on_ship_bomb_clicked();
    void on_get_noisy_clicked();

    void on_stop_noisy_clicked();

    void on_getBackLog_clicked();
    void on_write_device_skuid_clicked();

    void on_get_device_skuid_clicked();

    void on_set_hw_ver_clicked();

    // void on_set_battery_clicked();

    void on_brush_relocation_clicked();

    void on_stopBleOta_clicked();

    void on_closeconnect_clicked();

    void on_get_now_music_clicked();

    void on_send_audio_clicked();

    void on_audio_volume_valueChanged(int value);

    void on_is_audio_mode_stateChanged(int arg1);

    void on_play_speed_returnPressed();

    void on_uipasswordInput_returnPressed();

    void on_ui_ypos_returnPressed();

    void on_get_press_info_clicked();

    void on_set_press_info_clicked();

    void on_AITestLine_returnPressed();

    void on_speakAi_released();

    void on_speakAi_pressed();

    void on_get_botton_state_clicked();

    void on_selectPath_source_clicked();

    void on_set_mode_returnPressed();

    void on_btn_startRecording_clicked();

    void on_btn_stopRecording_clicked();

    void on_btn_startUpload_clicked();

    void on_btn_stopUpload_clicked();

    void on_btn_getSDCardStatus_clicked();

    void on_btn_getLDRInfo_clicked();

    void on_enterSuctionMode_clicked();

    void on_exitSuctionMode_clicked();

    void on_readBurningModestatus_clicked();

    void on_kTlvKeyWrite_clicked();

    void on_kTlvKeyread_clicked();

    void on_read_current_charge_clicked();

    void on_read_light_sensor_clicked();

    void on_set_light_sensor_clicked();

    void on_light_repo_start_clicked();

    void on_light_repo_stop_clicked();

    void on_getCaseDeviceException_clicked();

    void on_openCompensationSet_clicked();

    void on_closeCompensationSet_clicked();

    void on_factory_flag_clicked();

    void on_get_trim_data_clicked();

    void on_set_trim_data_clicked();

    void on_factory_flag_read_clicked();

    void on_enter_ble_cali_clicked();

    void on_exit_ble_cali_clicked();

    void on_enter_ble_test_clicked();

    void on_exit_ble_test_clicked();

    void on_enter_no_ble_test_clicked();

    void on_exit_no_ble_test_clicked();

    void on_get_device_mac_clicked();

    void on_set_device_mac_clicked();

    void on_night_brightness_clicked();

    void on_reset_factory_clicked();

    void on_get_rssi_device_clicked();

    void on_backlight_start_clicked();

    void on_backlight_stop_clicked();

    void on_get_battery_2_clicked();

    void on_get_keysignal_clicked();

signals:
    void send_uart_state(int data);
    void send_ble_state(int data);
    void send_mac(QString data);
    void send_frame_rate(QString data);
    void send_dongle_serialPort_state(int state);
    void send_image_processed();
    void send_thread_date(QString);
    void send_camera_respone(FacErrorCode);
    void send_fault_data_packet(int, const QVector<int>&);
    void sendPicture_speed(int);
    void sendBelOtaSpeed(int);
    void sendBelSourceOtaSpeed(int);
};

#endif  // MAINWINDOW_H
