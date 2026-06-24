#ifndef FIXTURE_UART_TYPES_H
#define FIXTURE_UART_TYPES_H

#include <QMetaType>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// 压感等工站：气缸/继电器 0x55 短帧（见 FixturePressUartProtocol::buildFixtureStateCommand）
typedef enum {
    STATE_CYLINDER_OPEN,
    STATE_RELAY1_OPEN,
    STATE_RELAY1_RESET,
    STATE_CYLINDER_RESET,
    STATE_RELAY4_OPEN,
    STATE_RELAY4_RESET,
} FixtureState;

// 摄像工站通道治具（ASCII）
typedef enum {
    STATE_THOROUGHFARE1_IN,
    STATE_THOROUGHFARE1_OUT,
    STATE_THOROUGHFARE2_IN,
    STATE_THOROUGHFARE2_OUT,
    STATE_THOROUGHFARE3_IN,
    STATE_THOROUGHFARE3_OUT,
} camreaFixtureState;

// 欣旺达六轴 IMU 治具（ASCII）
typedef enum {
    STATE_START,
    STATE_END,
    STATE_RESET,
    STATE_RETURN,
    STATE_40,
    STATE_FU40,
    STATE_BRUSH_UP,
    STATE_BRUSH_DOWN,
    STATE_BRUSH_LEFT,
    STATE_BRUSH_RIGHT,
    STATE_HOME,
} imuFixtureState;

// 压感 / Y20Pro / U7 等文本治具命令 ID
typedef enum {
    COMMAND_ID_INVALID,
    COMMAND_ID_BASE,

    COMMAND_ID_TRAY_IN,
    COMMAND_ID_TRAY_OUT,
    COMMAND_ID_FIXED_BLOCK_UP,
    COMMAND_ID_FIXED_BLOCK_DOWN,
    COMMAND_ID_KEY_UP,
    COMMAND_ID_KEY_DOWN_200,
    COMMAND_ID_BTH_UP,
    COMMAND_ID_BTH_DOWN_200,

    COMMAND_ID_CHEAK_STATUS,

    COMMAND_ID_BTH_PRESS_50,
    COMMAND_ID_BTH_PRESS_350,
    COMMAND_ID_BTH_PRESS_450,
    COMMAND_ID_KEY_PRESS_230,
    COMMAND_ID_BTH_PRESS_UP,
    COMMAND_ID_KEY_PRESS_UP,

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
    /** 治具长包字节 14~15：待机电流 uA（原 shipCurrent，与蓝牙船运模式无关） */
    uint standbyCurrentUa = 0;
    uchar music_state = 0;
    uint staticCurrent = 0;
    uint workingCurrent = 0;
    uint chargingCurrent = 0;
    uint pumpVoltageMv = 0;
    uint mcuVoltageMv = 0;
    /** 治具长包字节 20~21：阀电压 mV */
    uint valveVoltageMv = 0;
    uint8_t fixerro = 0;
    uint machine_get_mac_state = 0;
    machine_command_id_e machine_command_id = COMMAND_ID_BASE;
    uint argument = 0;
    uint machine_result_state = 0;

} FixturePacketData;

Q_DECLARE_METATYPE(FixturePacketData)

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // FIXTURE_UART_TYPES_H
