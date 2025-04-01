#ifndef FIXTURE_UART_H
#define FIXTURE_UART_H

#include <QObject>
#include <QQueue>

#include "Abini.h"
#include "my_set/my_typedef.h"
#include "usmile_ring_buffer.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif
// 定义数据包结构体
typedef enum {
    STATE_CYLINDER_OPEN,   // 开启气缸1
    STATE_RELAY1_OPEN,     // 继电器1吸合
    STATE_RELAY1_RESET,    // 继电器1复位
    STATE_CYLINDER_RESET,  // 气缸1复位
    STATE_RELAY4_OPEN,     // 继电器4吸合
    STATE_RELAY4_RESET,    // 继电器4复位
} FixtureState;

// 定义数据包结构体
typedef enum {
    STATE_THOROUGHFARE1_IN,   // 通道1进入
    STATE_THOROUGHFARE1_OUT,  // 通道1出去
    STATE_THOROUGHFARE2_IN,   // 通道2进入
    STATE_THOROUGHFARE2_OUT,  // 通道2出去
    STATE_THOROUGHFARE3_IN,   // 通道3进入
    STATE_THOROUGHFARE3_OUT,  // 通道3出去

} camreaFixtureState;

typedef enum {
    STATE_START,        // 开始
    STATE_END,          // 结束
    STATE_RESET,        // 复位
    STATE_RETURN,       // 翻转
    STATE_40,           // 40度
    STATE_FU40,         // 负40度
    STATE_BRUSH_UP,     // 刷头朝上
    STATE_BRUSH_DOWN,   // 刷头朝下
    STATE_BRUSH_LEFT,   // 刷头朝左
    STATE_BRUSH_RIGHT,  // 刷头朝右
    STATE_HOME,         // 治具回零位
} imuFixtureState;

typedef enum {
    COMMAND_ID_INVALID,
    COMMAND_ID_BASE,

    // Y20P校准治具
    COMMAND_ID_TRAY_IN,
    COMMAND_ID_TRAY_OUT,
    COMMAND_ID_FIXED_BLOCK_UP,
    COMMAND_ID_FIXED_BLOCK_DOWN,
    COMMAND_ID_KEY_UP,
    COMMAND_ID_KEY_DOWN_200,
    COMMAND_ID_BTH_UP,
    COMMAND_ID_BTH_DOWN_200,

    // Y20P测试治具
    COMMAND_ID_CHEAK_STATUS,

    COMMAND_ID_BTH_PRESS_50,
    COMMAND_ID_BTH_PRESS_350,
    COMMAND_ID_BTH_PRESS_450,
    COMMAND_ID_KEY_PRESS_230,
    COMMAND_ID_BTH_PRESS_UP,
    COMMAND_ID_KEY_PRESS_UP,

    // U7校准治具
    COMMAND_ID_FAMA_50_O,
    COMMAND_ID_FAMA_50_C,
    COMMAND_ID_FAMA_200_O,
    COMMAND_ID_FAMA_200_C,

    COMMAND_ID_FAMA_100_O,
    COMMAND_ID_FAMA_100_C,
    COMMAND_ID_FAMA_300_O,
    COMMAND_ID_FAMA_300_C,
    COMMAND_ID_FAMA_UP,
    COMMAND_ID_FAMA_DOWN,

    COMMAND_ID_FAMA_400_O,
    COMMAND_ID_FAMA_400_C,
    COMMAND_ID_RESET,
    COMMAND_ID_RESULT,
    COMMAND_ID_RESULT_SUC,

    // 统一的指令

    // F20
    COMMAND_ID_F20_FIXED,
    COMMAND_ID_F20_UNFIXED,
    COMMAND_ID_KEY_DOWN,
    COMMAND_ID_KEY_SWITCH_MODE,
    COMMAND_ID_KEY_SWITCH_POWER,

    COMMAND_ID_MAX,

} machine_command_id_e;

typedef struct FixturePacketData {
    uchar sleep = 0;
    uchar machineNumber = 0;
    uchar overVoltageLight = 0;
    uchar button1 = 0;
    uchar button2 = 0;
    uint musicCurrent = 0;
    uint shipCurrent = 0;
    uchar music_state = 0;
    uint staticCurrent = 0;
    uint workingCurrent = 0;
    uint chargingCurrent = 0;
    uint8_t fixerro = 0;
    uint machine_get_mac_state = 0;
    machine_command_id_e machine_command_id = COMMAND_ID_BASE;
    uint argument = 0;
    uint machine_result_state = 0;

} FixturePacketData;

// 声明为元类型
Q_DECLARE_METATYPE(FixturePacketData)
namespace Ui {
    class Fixture_uart;
}

class Fixture_uart : public QWidget {
    Q_OBJECT
public:
    qint64 last_sent_timestamp = 0;
    std::atomic<bool> running;
    QFuture<void> future;
    // void closeEvent(QCloseEvent *)override;
    explicit Fixture_uart(QWidget* parent = nullptr);
    ~Fixture_uart();
    Ui::Fixture_uart* ui;
    void sendimuData(imuFixtureState fixstate);
    void sendFixtureData(FixtureState fixstate);

    const char* press_commands[COMMAND_ID_MAX][6];

    machine_command_id_e last_commid = COMMAND_ID_MAX;
    qint64 last_commid_timestamp = 0;

    void FixtureCommandInit(void);
    void send_command_to_machine(int command_id, int numb);
    void delay_msec(unsigned int msec);

    int fixBaudRate = 9600;

private:
#define EXT_UART_PHY_LAYER_MAGIC 0x55
#define FIX_PHY_LAYER_HEAD_SIZE 1  // 头大小
#define FIX_PHY_LAYER_FRAME_SIZE sizeof(ext_uart_phy_layer_t)

#define FIX_PHY_LAYER_CRC_SIZE 1
#define FIX_PHY_LAYER_HEADER_ADN_CRC (FIX_PHY_LAYER_HEAD_SIZE + FIX_PHY_LAYER_CRC_SIZE)

    // 55 01 0E 00 26 00 C6 01 01 00 03 1F 01 AA        数据包
    // 55 01 0F 00 26 00 C6 01 01 00 03 1F 01 02 AA  数据包带音频电流
    // 55 01 11 00 26 00 C6 01 01 00 03 1F 01 02 01 02 AA  数据包带音频电流带船运电流

    // 55 01 05 CC AA                                   开始休眠
    // 55 AA 05 AA AA                                   开始测试
    // 板厂的数据包通信协议：
    //     0x55  机号（1拖多的情况）  0x长度（整包长度）
    //     静态电流值，工作电流，过压灯正常，按键1，按键2，充电电流，音频情况，0xaa 0x55  0x01， 0x长度
    //     0x00(高)，0x00（低）（ua）， 0x00(高)，0x00（低）（ma）， 0x00 0x00 0x00 0x00(高)，0x00（低）（ma）0x01
    //     0xaa（旧的）
    //     ========================治具的测试过程===================
    // 55 01 0E 00 26 00 C6 01 01 00 03 1F 01 AA（旧的）

#pragma pack(1)
    typedef struct {
        uint8_t magic;
        uint8_t mechine;
        uint8_t length;
        uint16_t staticCurrent;
        uint16_t workingCurrent;
        uint8_t overVoltageLight;
        uint8_t button1;
        uint8_t button2;
        uint16_t chargingCurrent;
        uint8_t music_state;
        uint16_t musicCurrent;
        uint16_t shipCurrent;
        uint8_t fixerro;

        uint8_t end;
    } ext_uart_phy_layer_t;

#pragma pack()

    RingBuf* fixRingBuf = nullptr;
    usmile_ring_buffer_t p_fixRingBuffer;  // 串口的队列指针
    uint8_t fix_ring_buffer[100 * 1024];   // 串口队列池
    void solve_frame(void);
    int ext_ble_find_next_frame(void);
    uint8_t frame_buf[2 * 1024];  // 队列池
    QSerialPort* fixtureSerialPort;
    QTimer* fixtureSerialPortTimer = new QTimer(this);
    QByteArray fixtureSerialPortBuf = 0;

signals:
    void send_data_to_mechine(const FixturePacketData datapack);
    void send_data_to_mechine_imu(int state);
    void send_data_to_mechine_sleep(const FixturePacketData datapack);
    void send_data_to_mechine_start();
    void start_fix_action(int state);
    void send_data_to_mechine_press(int state);

private slots:
    void readFixtureSerialPortData();
    void set_camera_action(camreaFixtureState fixstate);

    void save_Fixture_uart_log(int txrx, QByteArray data);

    void processimuReceivedData(const QByteArray& data);
    void openFixtureSerialPort();
    void closeFixtureSerialPort();
    void refresh_Fixtureuart_state(int state);
    void on_FixtureconnectButton_clicked();
    void on_FixturerefreshCom_clicked();
    void on_FixturedisconnectButton_clicked();
    void FixturehandleError(QSerialPort::SerialPortError error);
    void processshortdData(const QByteArray& data);
    void processReceivedData(const QByteArray& data);
    void send_start_command(int i);
    void send_start_sleep_command(int i);
    void send_start_white_modle_command(int i);
};

#endif  // FIXTURE_UART_H
