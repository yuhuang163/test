#ifndef QBULK_H
#define QBULK_H

#include <QByteArray>
#include <cstdint>
#include <lusb0_usb.h>
// extern "C" {
// #include <lusb0_usb.h>   // libusb 0.1 API
// }
#include <QMessageBox>
#include <QObject>
#include <QQueue>
#define USB_ERROR_IO        -1
#define USB_ERROR_INVALID_PARAM -2
#define USB_ERROR_ACCESS    -3
#define USB_ERROR_NO_DEVICE -4   // 有些版本
#define USB_ERROR_NOT_FOUND -5   // ★ 常见
#define USB_ERROR_NOT_DATA -116
#define DUSS_MB_PACKAGE_V1_CRCH_INIT    0x77            /**< CRC 8 init value */
#define DUSS_MB_PACKAGE_V1_CRC_INIT     0x3692          /**< CRC 16 init value */
#define HOST_ID_FROM_16BIT_TO_8BIT(host_id)          (((((host_id) & 0x07) << 5) | (((host_id) >> 8) & 0x1F)) & 0xFF)
#define HOST_ID_FROM_8BIT_TO_16BIT(host_id)          (((((host_id) & 0x1F) << 8) | (((host_id) >> 5) & 0x07)) & 0xFFFF)


typedef enum _djiFactroyCmd {
    djiFactroyCmd_FIRST_COMMAND_NO_USE = 0,
    djiFactroyCmd_get_version = 1,

} djiFactroyCmd;
typedef struct _djiFactoryDataPackage {
    FactroyCmd cmd_id;
    pb_size_t which_command_data;
    union {
        FacGetDevBaseInfo get_dev_base_info;
        FacDevState set_dev_state;
        FacDevState get_dev_state;
        FacGetPeriphState get_periph_state;
        FacDevInfo get_dev_info;
        FacDevInfo set_dev_info;
        FacSetDevBaseInfo set_dev_base_info;
        FacCollectParam set_collect_param;
        FacCollectParam get_collect_param;
        FacUploadPresSensor press_sensor;
        FacUploadNineAlex upload_nine_alex;
        FacButtonState get_button_state;
        FacButtonState upload_button_state;
        FacMotorCalibResult upload_motorcali_data;
        FacPictureDataAck upload_picture_data;
        FacPreSensorCalibResult set_fsensor_calib;
        FacPreSensorCalibResult get_fsensor_calib;
        FacImuCalibResult set_imu_calib;
        FacImuCalibResult get_imu_calib;
        FacLedControl led_control;
        FacLcdControl lcd_control;
        FacMotoControl moto_control;
        FacBrushControl brush_control;
        FacSetTime set_time;
        FacCameraControl camera_control;
        FacAgeingTest ageing;
        FacSetBrushRecord set_brush_record;
        FacWifiDemand wifi_demand;
        FacBrushLog fac_log;
        FacMicControl mic_control;
        FacInternetOta internet_ota;
    } command_data;
} djiFactoryDataPackage;
class QBulk : public QObject
{
     Q_OBJECT
public:
    QBulk();
    ~QBulk();

    // 打开 USB 设备，vid/pid，指定接口号
    bool openDevice(uint16_t vid, uint16_t pid, int interfaceNumber);

    // 关闭设备
    void closeDevice();

    // Bulk 读取
    bool bulkRead(unsigned char ep, QByteArray &data, unsigned int timeout = 1000);

    // Bulk 写入
    bool bulkWrite(unsigned char ep, const QByteArray &data, unsigned int timeout = 1000);
    // 数据解包/命令解析
    void parseCmd( QByteArray &buffer);
    void handlePacket(const QByteArray &packet);
    QByteArray buildPacket( uint8_t receiver,
                                uint8_t cmdType,
                                  uint8_t cmdSet, uint8_t cmdID,
                                  const QByteArray &data, uint8_t encryptionType = 0);
    uint16_t duss_util_crc16_calc(const uint8_t *data, uint32_t len, uint16_t init_crc);
    uint8_t crc8_calc(const uint8_t *data, uint32_t len, uint8_t init_crc);
public slots:
    void startRead();   // 线程里跑
    void stopRead();

       int isOpen() { return is_open; }
public slots:
    void get_dev_ver();

        void set_amt_test(QString data);
    void set_amt_clean_flag();
        void set_amt_check_clean_flag();
signals:
    void readyRead( QByteArray &data);
    void error(int code, const QString &msg);
        void send_bulk_data(QString data);
     void sendGetDjiResponse(int data);
private slots:
    void process_djiFactroyCmd_get_version(QByteArray& f);

private:
    void registerCommand();
    typedef std::function<void(QByteArray&)> callback;
    std::map<djiFactroyCmd, callback> djifactoryCommandList;
    struct usb_dev_handle *handle = nullptr;
    std::atomic_bool running { false };
    bool is_open=false;
    // CRC 校验
    bool checkHeaderCRC(const QByteArray &packet);
    bool checkDataCRC(const QByteArray &packet);
       std::atomic<uint16_t> m_seqNum {0};   // 线程安全
};

#endif // QBULK_H
