#ifndef FIXTURE_UART_H
#define FIXTURE_UART_H

#include "Abini.h"
#include "usmile_ring_buffer.h"
#include <QObject>
#include <QQueue>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
// 定义数据包结构体
typedef enum
{
    STATE_CYLINDER_OPEN,    // 开启气缸1
    STATE_RELAY1_OPEN,      // 继电器1吸合
    STATE_RELAY1_RESET,     // 继电器1复位
    STATE_CYLINDER_RESET,   // 气缸1复位
    STATE_RELAY4_OPEN,      // 继电器4吸合
    STATE_RELAY4_RESET,     // 继电器4复位
} FixtureState;
typedef enum
{
    STATE_START,         // 开始
    STATE_END,           // 结束
    STATE_RESET,         // 复位
    STATE_RETURN,        // 翻转
    STATE_40,            // 40度
    STATE_FU40,          // 负40度
    STATE_BRUSH_UP,      // 刷头朝上
    STATE_BRUSH_DOWN,    // 刷头朝下
    STATE_BRUSH_LEFT,    // 刷头朝左
    STATE_BRUSH_RIGHT,   // 刷头朝右
    STATE_HOME,          // 治具回零位
} imuFixtureState;
typedef struct FixturePacketData
{
    uchar sleep = 0;
    uchar machineNumber = 0;
    uchar overVoltageLight = 0;
    uchar button1 = 0;
    uchar button2 = 0;
    uchar music_state = 0;
    uint staticCurrent = 0;
    uint workingCurrent = 0;
    uint chargingCurrent = 0;
} FixturePacketData;
namespace Ui
{
    class Fixture_uart;
}

class Fixture_uart : public QWidget
{
    Q_OBJECT
public:
    explicit Fixture_uart(QWidget *parent = nullptr);
    ~Fixture_uart();
    Ui::Fixture_uart *ui;
    void sendimuData(imuFixtureState fixstate);
    void sendFixtureData(FixtureState fixstate);
private:
#define EXT_UART_PHY_LAYER_MAGIC      0x55
#define UART_PHY_LAYER_HEAD_SIZE      1   // 头大小
#define UART_PHY_LAYER_FRAME_SIZE     12
#define UART_PHY_LAYER_CRC_SIZE       1
#define UART_PHY_LAYER_HEADER_ADN_CRC (UART_PHY_LAYER_HEAD_SIZE + UART_PHY_LAYER_CRC_SIZE)

// 55 01 0E 00 26 00 C6 01 01 00 03 1F 01 AA        数据包
// 55 01 05 CC AA                                   开始休眠
// 55 AA 05 AA AA                                   开始测试


#pragma pack(1)
    typedef struct
    {
        uint8_t magic;
        uint8_t mechine;
        uint8_t length;
        uint8_t overVoltageLight;
        uint8_t button1;
        uint8_t button2;
        uint8_t music_state;
        uint16_t staticCurrent;
        uint16_t workingCurrent;
        uint16_t chargingCurrent;
        uint8_t end;
    } ext_uart_phy_layer_t;
#pragma pack()

    RingBuf *fixRingBuf = nullptr;
    usmile_ring_buffer_t p_fixRingBuffer;   // 串口的队列指针
    uint8_t fix_ring_buffer[100 * 1024];    // 串口队列池
    void solve_frame(void);
    int ext_ble_find_next_frame(void);
    uint8_t frame_buf[2 * 1024];   // 队列池

    QSerialPort *fixtureSerialPort;
    QString data_command = "";

    QTimer *fixtureSerialPortTimer = new QTimer(this);
    QByteArray fixtureSerialPortBuf = 0;

signals:
    void send_data_to_mechine(const FixturePacketData datapack);
    void send_data_to_mechine_imu(int state);
    void send_data_to_mechine_sleep(const FixturePacketData datapack);
    void send_data_to_mechine_start();
private slots:
    void readFixtureSerialPortData();

    void save_Fixture_uart_log(int txrx, QByteArray data);

    void processimuReceivedData(const QByteArray &data);
    void openFixtureSerialPort();
    void closeFixtureSerialPort();
    void refresh_Fixtureuart_state(int state);
    void on_FixtureconnectButton_clicked();
    void on_FixturerefreshCom_clicked();
    void on_FixturedisconnectButton_clicked();
    void FixturehandleError(QSerialPort::SerialPortError error);
    void processshortdData(const QByteArray &data);
    void processReceivedData(const QByteArray &data);
    void send_start_command(int i);
    void send_start_sleep_command(int i);
    void send_start_white_modle_command(int i);
};

#endif   // FIXTURE_UART_H
