#ifndef CAMERATEST_H
#define CAMERATEST_H

#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QWheelEvent>

#include "Abini.h"
#include "fixture_uart.h"
#include "qudpsocket.h"
#include "test_base.h"
#include "ui_cameratest.h"
#include "usmile_ring_buffer.h"

namespace Ui {
    class cameratest;
}

// ImageViewer 类继承自 QGraphicsView
class ImageViewer : public QGraphicsView {
    Q_OBJECT
public:
    // 加载和显示图片
    QPixmap pixmap;
    QPixmap temporarypixmap;  //原图

    // 构造函数
    ImageViewer(const QString& filePath = "", QWidget* parent = nullptr) : QGraphicsView(parent) {
        // 创建 QGraphicsScene 对象并设置为当前视图的场景
        scene = new QGraphicsScene(this);
        setScene(scene);
        if (!filePath.isEmpty()) {
            pixmap.load(filePath);
            if (!pixmap.isNull()) {
                pixmapItem = new QGraphicsPixmapItem(pixmap);
                scene->addItem(pixmapItem);
            } else {
                qDebug() << "Failed to load default image.";
            }
        }
        // 创建 QGraphicsPixmapItem 对象并添加到场景中
        pixmapItem = new QGraphicsPixmapItem(pixmap);
        scene->addItem(pixmapItem);

        // 启用抗锯齿以获得更平滑的图像渲染
        setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        // 启用鼠标滚轮滚动功能
        setDragMode(QGraphicsView::ScrollHandDrag);
    }
    // 更新图像的方法
    void updateImage() {
        pixmapItem->setPixmap(pixmap);
        scene->setSceneRect(pixmap.rect());
        fitInView(scene->sceneRect(), Qt::KeepAspectRatio);  // 可选：根据图像大小调整视图
    }

protected:
    // 重载滚轮事件处理函数
    void wheelEvent(QWheelEvent* event) override {
        // 缩放因子
        const double scaleFactor = 1.15;
        if (event->delta() > 0) {
            // 放大
            scale(scaleFactor, scaleFactor);
        } else {
            // 缩小
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
    }

private:
    QGraphicsScene* scene;            // 场景对象
    QGraphicsPixmapItem* pixmapItem;  // 用于显示图片的图元对象
};

class cameratest : public test_base {
    Q_OBJECT
public:
    int cameradatasize = 0;
    QTimer* cameratimer = new QTimer(this);
    int dataNumber = 0;
    int CameraGetTime = 6000;
    int startDirtyTime = 2;

    /*摄像头传图部分*/
    std::atomic<bool> running;
    QFuture<void> future;
#define CRC16(data, len) crc16_compute((const uint8_t*)(data), len, NULL)
    unsigned short crc16_compute(unsigned char const* p_data, unsigned int size, unsigned short const* p_crc) {
        unsigned short crc = (p_crc == NULL) ? 0xFFFF : *p_crc;

        for (uint32_t i = 0; i < size; i++) {
            crc = (unsigned char)(crc >> 8) | (crc << 8);
            crc ^= p_data[i];
            crc ^= (unsigned char)(crc & 0xFF) >> 4;
            crc ^= (crc << 8) << 4;
            crc ^= ((crc & 0xFF) << 4) << 1;
        }

        return crc;
    }
#define EXT_UART_MAGIC 0xCCCCCCCCCCCCCCCC
#define UART_PHY_LAYER_HEAD_SIZE 9  // 头大小
#define UART_PHY_LAYER_LENGTH 1
#define UART_PHY_LAYER_HEADER_ADN_LEN (UART_PHY_LAYER_HEAD_SIZE + UART_PHY_LAYER_LENGTH)
#define EXT_PICTURE_PHY_LAYER_MAGIC 0xA5A5A5A5
#define PICTURE_PHY_LAYER_HEAD_SIZE sizeof(video_frame_data_struct)  // 头大小
#define PICTURE_PHY_LAYER_HEADER_ADN_CRC (PICTURE_PHY_LAYER_HEAD_SIZE)
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
    } ext_picture_layer_t;

    typedef struct {
        uint64_t magic;
        uint8_t length;
        uint8_t channel;
        // uint8_t index;
        uint8_t data[0];
    } ext_uart_phy_layer_t;
#pragma pack()
    void printSquareData(uint8_t* data, int data_size);
    ImageViewer* viewercamrea;
    ImageViewer* viewercamrea_py;
    typedef enum {
        PHY_CHANNEL_INVALID = 0,  //无效值
        PHY_CHANNEL_CAMREA,       //摄像头命令通道
        PHY_CHANNEL_LOG,          // ota数据通道
        PHY_CHANNEL_HARMONY_APP,  //鸿蒙app通道
        PHY_CHANNEL_FACTORY,      //厂测通道
        PHY_CHANNEL_USMILE_APP,   //笑容加app通道
        PHY_CHANNEL_USMILE_ROTA,  //笑容加ota通道
    } ext_ble_phy_channel_e;
    QMutex mutex;
    usmile_ring_buffer_t p_dongleRingBuffer;  // 串口的队列指针
    usmile_ring_buffer_t p_cameraRingBuffer;  // 摄像头的队列指针
    uint8_t dongle_ring_buffer[100 * 1024];   // 串口队列池
    uint8_t frame_buf[2 * 1024];              // 队列池
    uint8_t camera_ring_buf[100 * 1024];      // 摄像头队列池
    uint8_t frame_picture_buf[37 * 1024];     // 照片队列池
    int ext_ble_find_next_frame(void);
    int ext_ble_find_next_picture_frame(QByteArray& picturedata);
    void write_camera_data(uint8_t* p_data, int data_len);
    RingBuf* dongleRingBuf = nullptr;
    RingBuf* cameraRingBuf = nullptr;
    void solve_frame(void);
    void solve_picture_frame(QByteArray picturedata);
    /*摄像头传图部分*/
    explicit cameratest(int index, QWidget* parent = nullptr);
    ~cameratest();
    Ui::cameratest* ui;
    void startTask() override;
    QMap<int, QByteArray> packetMap;
    QVector<int> faultData;

private:
    QPixmap currentPixmap;

    typedef enum {
        STATE_IDLE,          // 休眠状态
        STATE_WATI_CONNECT,  // 等待连接
        STATE_WATI_BASE_INFO,
        STATE_DISABLE_SLEEP_1,     // 进入禁止休眠
        STATE_PROCESS_INSPECTION,  // 工序核对检查
        STATE_NET_SET,             // 配网
        CAMERA_TEST,
        STATE_SAVE_RESULT  // 保存结果在本地
    } State;
    bool isExecuted = false;
    QElapsedTimer TestTime;
    State state = STATE_IDLE;
    QTimer* cameraSendTimer = new QTimer(this);
    bool is_canGoNext = 0;
    QByteArray pictureByteArray = 0;
    QString stringsn;
    QUdpSocket* udpSocket;
    QByteArray sn;
    QString macAddress = "没有mac地址";
    bool is_camera_control = 0;
    bool can_start_dirty_test = 0;
    int picutre_dirty_times = 0;
    bool can_start_offset_test = 0;
    int refresh_times = 1;
    int picutre_offset_times = 0;
    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QCheckBox* getIsUseMes() override { return ui->isusemes; };
    QCheckBox* getIsFormMes() override { return ui->isformmes; };
    QLineEdit* getMacLineEdit() override { return ui->getMac; };               // sn输入口
    QLineEdit* macInputLineEdit() override { return ui->macInput; };           // mac地址输入口
    QPlainTextEdit* logEdit() override { return ui->log; };                    // mac地址输入口
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; };                // msg输入口
    QTableWidget* testResultTable() override { return ui->testResultTable; };  // 测试结果表格输入口
    QLabel* getMesStateQlabel() override { return ui->mes_state; };            // mes状态的qlab
    QPushButton* getEndTestButton() override { return ui->stopTest; };         // 结束测试按钮
    bool displayRectangles;
    bool displayDirty;

protected:
    void closeEvent(QCloseEvent*) override;

private slots:
    void deinit_distribution_network();
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void checkMissingPackets();
    void start_dirty_test();
    void start_offset_test();
    void addPacket(const QByteArray& packet);
    QByteArray reassembleData();
    void onDongleSerialFrame(const QByteArray& data) override;
    void getPictureSendOver(ProtocolPictureSendOverData x);
    void onTimeout();
    void processTheDatagram(QByteArray& datagram);
    void bandSnMacToCsv(const QString& macAddress, const QString& sn);
    void refreshCameraControl(ProtocolCameraControlData style) override;
    void canGoNextMechine(int x) override;
    void refreshBleState(int state) override;
    void refreshSn(ProtocolSnData data) override;
    void getDongleWifi(QString data) override;
    void readPendingDatagrams();
    void refreshDongleUartState(int state) override;
    void getTestValue(const int mechines, const QString value) override;
    void processGetMesTestValue();
    void on_pushButton_clicked();
    void on_lcdTestButton_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_macInput_returnPressed();
    void on_getMac_returnPressed();
    void processInspection(QString stringsn);
    void on_distribution_network_clicked();
    void on_open_camera_clicked();
    void on_close_camera_clicked();
    void on_close_camear_light_clicked();
    void on_open_camear_light_clicked();
    void on_save_photo_clicked();
    void on_normal_clicked();
    void on_abnormal_clicked();
    void on_exposure_time_edit_returnPressed();
    void on_DirtyTestButton_clicked();
    void on_OffsetTest_clicked();
    void on_stopTest_clicked();
    void updateImageOnMainThread();
    void on_ResolutionTestButton_clicked();

signals:
    void send_go_next_focus();
    void send_go_next_test(int data);
    void send_startTest(int data);
    void send_image_processed();
    void send_fault_data_packet(int, const QVector<int>&);
    void sendPicture_speed(int);
    void send_thread_date(QString);
    void send_camera_respone(FacErrorCode);
    void send_set_camera_action(camreaFixtureState);
};

#endif  // CAMERATEST_H
