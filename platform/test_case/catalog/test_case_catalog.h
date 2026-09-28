#ifndef PLATFORM_TEST_CASE_CATALOG_H
#define PLATFORM_TEST_CASE_CATALOG_H

#include "cmd_catalog_base.h"
#include "modbus_device_catalog.h"
#include "scpi_types.h"
#include "test_case_types.h"

#include <QString>
#include <QStringList>

class DeviceCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
    static TestCaseProductProtocol productProtocolFromIni(const QString& text);
    static QString productProtocolToIni(TestCaseProductProtocol protocol);
    static QString productProtocolUiLabel(TestCaseProductProtocol protocol);
    /** 产品通道指令名：在 Set/Get 过滤基础上再按产品协议收窄。 */
    static QStringList allCmdNames(TestCaseSendAction action, TestCaseProductProtocol protocol);
};

class DongleCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

class UsbCameraCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

class VesLightCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

enum class ProductSerialCmd {
    InstrumentReset,
    StartRx2402Ble1M,
    StartRx2440Ble1M,
    StartRx2480Ble1M,
    StartRx2402Ble2M,
    StartRx2440Ble2M,
    StartRx2480Ble2M,
    StopRxAndPer,
};

/** ASD9026A 双通道模拟电池串口协议（Send/Channel=Fixture 且 Send/Protocol=ASD9026A）。 */
enum class Asd9026aCmd {
    ConfigureProgrammablePower,
    ConfigureCurrentMeasureRange,
    ProgrammablePowerOutput,
    ReadProgrammableVoltage,
    ReadProgrammableCurrent,
    SendRaw,
};

class Asd9026aCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

/** 欣旺达 XWD raw 治具（与气缸 XwdFixtureCmd 区分；Protocol=XWD，兼容 XWD_BLE/XWD_SUCTION）。 */
enum class XwdRawFixtureCmd {
    SendRaw,
    QueryDown, // 检测下到位：发 DOWN?，到位回 ALL_DOWN_OK
    Up,        // 原本单台上升：发 UP，回 OK
    SetUp,     // 上升：发 SET_UP，回 OK
    MoveUp,    // 一拖多上升：发 SET_ALL_UP，回 OK
    Relay1On,  // 继电器1接通 RELAY_1_ON
    Relay1Off, // 继电器1断开 RELAY_1_OFF
    Relay2On,  // 继电器2接通 RELAY_2_ON
    Relay2Off, // 继电器2断开 RELAY_2_OFF
    LoadOn,    // 负载总开 LOAD_ON（控制板 UART 文本）
    LoadOff,   // 负载总关 LOAD_OFF
    AddLoad1,  // 加负载1：Modbus 写帧（工位治具口）
    AddLoad2,  // 加负载2
    CloseLoad1,// 关闭负载1
    CloseLoad2,// 关闭负载2
};

class XwdRawFixtureCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

/** 杰理蓝牙盒子：串口 TLV 上报频偏/RSSI（Send/Channel=Fixture 且 Protocol=JieliBtBox）。 */
enum class JieliBtBoxCmd {
    WaitRfInfo,
};

class JieliBtBoxCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

/** PCBA 治具 0x55 协议指令（Send/Channel=Fixture 且 Send/Protocol=Pcba）。 */
enum class FixturePcbaCmd {
    StartTest,
    StartSleep,
    StartWhiteMode,
    WaitFixturePacket,
    WaitStartTestAck,
    WaitSleepRequest,
    WaitWorkCurrentDoneAck,
};

class FixturePcbaCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
    static TestCaseFixtureProtocol fixtureProtocolFromIni(const QString& text);
    static QString fixtureProtocolToIni(TestCaseFixtureProtocol protocol);
    static QString fixtureProtocolUiLabel(TestCaseFixtureProtocol protocol);
};

class ProductSerialCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

/**
 * Modbus 外设测试项配置：工站只选「设备」「该设备 Cmd」「Set/Get」
 * 协议/Tcp/Rtu 由设备绑定内部处理，配置页不出现协议类型
 */
class ModbusPeriphCmdCatalog {
  public:
    static QStringList allDeviceKeys();

    static QStringList allCmdNames(ModbusDeviceRoute device, TestCaseSendAction action);
    static bool isCmdForDevice(ModbusDeviceRoute device, const QString& enumName, TestCaseSendAction action);
    static QString cmdUiLabel(ModbusDeviceRoute device, const QString& enumName);
    static QString paramUiHint(ModbusDeviceRoute device, const QString& enumName);
};

class ScpiPeriphCmdCatalog {
  public:
    static QStringList allDeviceKeys();
    static QString deviceUiLabel(ScpiDeviceRoute device);
    static ScpiDeviceRoute deviceFromIni(const QString& text);
    static QString deviceToIni(ScpiDeviceRoute device);

    static QStringList allCmdNames(ScpiDeviceRoute device, TestCaseSendAction action);
    static bool isCmdForDevice(ScpiDeviceRoute device, const QString& enumName, TestCaseSendAction action);
    static QString cmdUiLabel(ScpiDeviceRoute device, const QString& enumName);
    static QString paramUiHint(ScpiDeviceRoute device, const QString& enumName);
};

class TupleCmdCatalog {
  public:
    static CmdManifestCatalog& catalog();
};

#endif // PLATFORM_TEST_CASE_CATALOG_H
