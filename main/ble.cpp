
#include "config.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"

// 首选连接间隔（单位 1.25 ms）。目标 5 ms 对应 0x04，但 BLE/ESP-IDF 合法下限为 7.5 ms (0x06)。
#define BLE_PREFER_CONN_INT_MIN 0x06
#define BLE_PREFER_CONN_INT_MAX 0x06
#define BLE_PREFER_CONN_LATENCY 0
#define BLE_PREFER_CONN_SUP_TOUT 2000

uint16_t conn_id = -1;
static BLEClient *pClient = nullptr; // 蓝牙客户端的类
BLEClientCallbacks *connect_callback = nullptr;
boolean StartSendOtaData = false;
uint16_t bleMtuSize = BLE_MTU_DEFAULT;
uint16_t otaBlePacketSize = OTA_BLE_PACKET_SIZE_DEFAULT;
boolean StartSendmainData = false;
boolean StartBombState = false;

struct BleRuntime
{
    BleState state;
    BleConnectMode connectMode;
    bool scanStarted;
    bool hasPendingStateAfterDisconnect;
    BleState pendingStateAfterDisconnect;
    bool hasScanDevice;
    char scanDeviceAddress[18];
    esp_ble_addr_type_t scanDeviceAddressType;
    int scanDeviceRssi;
};

static BleRuntime bleRuntime = {
    BLE_IDLE,
    CONNECT_BY_SCAN,
    false,
    false,
    BLE_IDLE,
    false,
    "00:00:00:00:00:00",
    BLE_ADDR_TYPE_PUBLIC,
    0};

static BLEScan *pBLEScan;

static BLERemoteService *facRemoteService = nullptr;
static BLERemoteService *appRemoteService = nullptr;
static BLERemoteService *mainRemoteService = nullptr;
static BLERemoteService *normolRemoteService = nullptr;

static BLEUUID mainserviceUUID("a6ed0301-d344-460a-8075-b9e8ec90d71b"); // main
static BLEUUID mainWriteUUID("a6ed0302-d344-460a-8075-b9e8ec90d71b");
static BLEUUID mainNotifyUUID("a6ed0302-d344-460a-8075-b9e8ec90d71b");
static BLERemoteCharacteristic *mainNotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *mainWriteCharacteristic = nullptr;

struct FactoryBleProfile
{
    const char *name;
    BLEUUID serviceUUID;
    BLEUUID notifyUUID;
    BLEUUID writeUUID;
    bool hasExtraCharacteristics;
};

struct OtaBleProfile
{
    const char *name;
    BLEUUID serviceUUID;
    BLEUUID notifyUUID;
    BLEUUID writeUUID;
    BLEUUID dataUUID;
};

static FactoryBleProfile factoryProfiles[] = {
    {"V3",
     BLEUUID("524f4f54-9000-0080-0010-000000020001"),
     BLEUUID("524f4f54-9000-0080-0010-000000020002"),
     BLEUUID("524f4f54-9000-0080-0010-000000020003"),
     false},
    {"TONGYONG",
     BLEUUID("9F6C1A20-3C4D-4E5F-A601-7B8C9D0E1122"),
     BLEUUID("9F6C1A22-3C4D-4E5F-A601-7B8C9D0E1122"),
     BLEUUID("9F6C1A21-3C4D-4E5F-A601-7B8C9D0E1122"),
     false},
    {"usmile",
     BLEUUID("a6ed0201-d344-460a-8075-b9e8ec90d71b"),
     BLEUUID("a6ed0202-d344-460a-8075-b9e8ec90d71b"),
     BLEUUID("a6ed0203-d344-460a-8075-b9e8ec90d71b"),
     true},
    // UART Over BLE: RX=0xAF01(Write Without Response), TX=0xAF02(Notify)
    {"root",
     BLEUUID("AF00"),
     BLEUUID("AF02"),
     BLEUUID("AF01"),
     false},
};

static FactoryBleProfile *activeFactoryProfile = nullptr;

static BLEUUID CameraUUID("a6ed0204-d344-460a-8075-b9e8ec90d71b");  // 摄像头传图
static BLEUUID LogUUID("a6ed0205-d344-460a-8075-b9e8ec90d71b");     // 传输日志
static BLERemoteCharacteristic *LOGUUIDCharacteristic = nullptr;
static BLERemoteCharacteristic *CAMERAUUIDCharacteristic = nullptr;
static BLERemoteCharacteristic *NotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *WriteCharacteristic = nullptr;

static OtaBleProfile otaProfiles[] = {
    {"usmile",
     BLEUUID("a6ed0101-d344-460a-8075-b9e8ec90d71b"),
     BLEUUID("a6ed0103-d344-460a-8075-b9e8ec90d71b"),
     BLEUUID("a6ed0103-d344-460a-8075-b9e8ec90d71b"),
     BLEUUID("a6ed0102-d344-460a-8075-b9e8ec90d71b")},
    {"V3",
     BLEUUID("524f4f54-9000-0080-0010-000000030001"),
     BLEUUID("524f4f54-9000-0080-0010-000000030002"),
     BLEUUID("524f4f54-9000-0080-0010-000000030003"),
     BLEUUID("524f4f54-9000-0080-0010-000000030003")},
};

static OtaBleProfile *activeOtaProfile = nullptr;
static BLERemoteCharacteristic *AppDataCharacteristic = nullptr;       // 发送ota数据的特征
static BLERemoteCharacteristic *AppNotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *AppWriteCharacteristic = nullptr;

static BLEUUID serviceUUIDNormal("1828"); // normol
static BLEUUID WriteUUIDNormal("2ACA");
static BLEUUID NotifyUUIDNormal("2ACA");
static BLERemoteCharacteristic *normolNotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *normolWriteCharacteristic = nullptr;

static BLERemoteService *findFactoryRemoteService()
{
    activeFactoryProfile = nullptr;

    for (size_t i = 0; i < sizeof(factoryProfiles) / sizeof(factoryProfiles[0]); i++)
    {
        FactoryBleProfile *profile = &factoryProfiles[i];
        BLERemoteService *service = pClient->getService(profile->serviceUUID);

        if (!pClient->isConnected())
        {
            Serial.println("服务发现中连接已断开(facRemoteService)");
            return nullptr;
        }

        if (service != nullptr)
        {
            activeFactoryProfile = profile;
            Serial.print("匹配factory服务：");
            Serial.println(profile->name);
            return service;
        }
    }

    return nullptr;
}

static BLERemoteService *findOtaRemoteService()
{
    activeOtaProfile = nullptr;

    for (size_t i = 0; i < sizeof(otaProfiles) / sizeof(otaProfiles[0]); i++)
    {
        OtaBleProfile *profile = &otaProfiles[i];
        BLERemoteService *service = pClient->getService(profile->serviceUUID);

        if (!pClient->isConnected())
        {
            Serial.println("服务发现中连接已断开(appRemoteService)");
            return nullptr;
        }

        if (service != nullptr)
        {
            activeOtaProfile = profile;
            Serial.print("匹配app服务：");
            Serial.println(profile->name);
            return service;
        }
    }

    return nullptr;
}

typedef enum
{
    PHY_CHANNEL_INVALID = 0, // 无效值
    PHY_CHANNEL_CAMREA,      // 控制命令通道
    PHY_CHANNEL_LOG,         // ota数据通道

} ext_ble_phy_send_channel_e;

const char *ble_state_name(BleState state)
{
    switch (state)
    {
    case BLE_IDLE:
        return "BLE_IDLE";
    case BLE_SCANNING:
        return "BLE_SCANNING";
    case BLE_SCAN_FOUND:
        return "BLE_SCAN_FOUND";
    case BLE_CONNECTING:
        return "BLE_CONNECTING";
    case BLE_CONNECTED:
        return "BLE_CONNECTED";
    case BLE_DISCONNECTING:
        return "BLE_DISCONNECTING";
    default:
        return "BLE_UNKNOWN";
    }
}

const char *ble_connect_mode_name(BleConnectMode mode)
{
    switch (mode)
    {
    case CONNECT_BY_SCAN:
        return "CONNECT_BY_SCAN";
    case CONNECT_DIRECT:
        return "CONNECT_DIRECT";
    default:
        return "CONNECT_UNKNOWN";
    }
}

BleState get_ble_state()
{
    return bleRuntime.state;
}

BleConnectMode get_ble_connect_mode()
{
    return bleRuntime.connectMode;
}

bool is_ble_connected()
{
    return bleRuntime.state == BLE_CONNECTED;
}

void remember_scan_device(BLEAdvertisedDevice &advertisedDevice)
{
    String address = advertisedDevice.getAddress().toString();
    strncpy(bleRuntime.scanDeviceAddress, address.c_str(), sizeof(bleRuntime.scanDeviceAddress) - 1);
    bleRuntime.scanDeviceAddress[sizeof(bleRuntime.scanDeviceAddress) - 1] = '\0';
    bleRuntime.scanDeviceAddressType = advertisedDevice.getAddressType();
    bleRuntime.scanDeviceRssi = advertisedDevice.getRSSI();
    bleRuntime.hasScanDevice = true;

    Serial.print("记录扫描设备 addr=");
    Serial.print(bleRuntime.scanDeviceAddress);
    Serial.print(", addrType=");
    Serial.print(bleRuntime.scanDeviceAddressType);
    Serial.print(", rssi=");
    Serial.println(bleRuntime.scanDeviceRssi);
}

void set_ble_connect_mode(BleConnectMode mode)
{
    if (bleRuntime.connectMode == mode)
    {
        return;
    }
    Serial.print("BLE_CONNECT_MODE: ");
    Serial.print(ble_connect_mode_name(bleRuntime.connectMode));
    Serial.print(" -> ");
    Serial.println(ble_connect_mode_name(mode));
    bleRuntime.connectMode = mode;
}

void set_ble_state(BleState state)
{
    if (bleRuntime.state == state)
    {
        return;
    }
    Serial.print("BLE_STATE: ");
    Serial.print(ble_state_name(bleRuntime.state));
    Serial.print(" -> ");
    Serial.println(ble_state_name(state));
    bleRuntime.state = state;
    bleRuntime.scanStarted = false;
}

// 消息提醒函数
void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                    size_t length, bool isNotify)
{
    // vTaskDelay(10);   // 让出处理器

    unsigned long currentMillis = millis();   // 或者使用 micros() 函数获取微秒级时间戳
    String timestamp = String(currentMillis); // 将时间戳转换为字符串
    Serial.println();
    Serial.print("产测设备数据包的时间戳:"); // 打印带有时间戳的消息
    Serial.print(timestamp);
    Serial.print("长度为:"); // 打印带有时间戳的消息
    Serial.print(length);

    const int additionalBytes = 10;
    uint8_t modifiedData[length + additionalBytes];
    memset(modifiedData, 0xaa, 8);

    modifiedData[8] = PHY_CHANNEL_FAC;

    modifiedData[9] = length;
    memcpy(modifiedData + 10, pData, length);

    Serial.print("内容为:"); // 打印带有时间戳的消息
    Serial.write(modifiedData, length + additionalBytes);
    Serial.println();
    Serial.println();
}

void AppNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                       size_t length, bool isNotify)
{
    // vTaskDelay(10);   // 让出处理器

    unsigned long currentMillis = millis();   // 或者使用 micros() 函数获取微秒级时间戳
    String timestamp = String(currentMillis); // 将时间戳转换为字符串
    Serial.println();
    Serial.print("App数据包的时间戳:"); // 打印带有时间戳的消息
    Serial.print(timestamp);
    Serial.print("长度为:"); // 打印带有时间戳的消息
    Serial.print(length);

    const int additionalBytes = 10;
    uint8_t modifiedData[length + additionalBytes];
    memset(modifiedData, 0xaa, 8);

    modifiedData[8] = PHY_CHANNEL_APP;

    modifiedData[9] = length;
    memcpy(modifiedData + 10, pData, length);

    Serial.print("内容为:"); // 打印带有时间戳的消息
    Serial.write(modifiedData, length + additionalBytes);
    Serial.println();
    Serial.println();
}
void mainNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                        size_t length, bool isNotify)
{
    // vTaskDelay(10);   // 让出处理器

    unsigned long currentMillis = millis();   // 或者使用 micros() 函数获取微秒级时间戳
    String timestamp = String(currentMillis); // 将时间戳转换为字符串
    Serial.println();
    Serial.print("main数据包的时间戳:"); // 打印带有时间戳的消息
    Serial.print(timestamp);
    Serial.print("长度为:"); // 打印带有时间戳的消息
    Serial.print(length);

    const int additionalBytes = 10;
    uint8_t modifiedData[length + additionalBytes];
    memset(modifiedData, 0xaa, 8);

    modifiedData[8] = PHY_CHANNEL_MAIN;

    modifiedData[9] = length;
    memcpy(modifiedData + 10, pData, length);

    Serial.print("内容为:"); // 打印带有时间戳的消息
    Serial.write(modifiedData, length + additionalBytes);
    Serial.println();
    Serial.println();
}

// 消息提醒函数
void cameranotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                          size_t length, bool isNotify)
{

    const int additionalBytes = 10;
    uint8_t modifiedData[length + additionalBytes];
    memset(modifiedData, 0xcc, 8);
    modifiedData[8] = length;
    modifiedData[9] = PHY_CHANNEL_CAMREA;
    memcpy(modifiedData + additionalBytes, pData, length);
    Serial.write(modifiedData, length + additionalBytes);
}
// 消息提醒函数
void brushLogNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                            size_t length, bool isNotify)
{

    const int additionalBytes = 10;
    uint8_t modifiedData[length + additionalBytes];
    memset(modifiedData, 0xcc, 8);
    modifiedData[8] = length;
    modifiedData[9] = PHY_CHANNEL_LOG;
    memcpy(modifiedData + additionalBytes, pData, length);
    Serial.write(modifiedData, length + additionalBytes);
}

static void requestActiveConnParams(const char *addressStr);

// 连接状态函数
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *ppclient)
    {
        pClient->setMTU(bleMtuSize);
        Serial.printf("BLE MTU 已请求: %u\r\n", bleMtuSize);
        set_ble_state(BLE_CONNECTED);
        conn_id = pClient->getConnId();

        const char *connAddr = (get_ble_connect_mode() == CONNECT_BY_SCAN && bleRuntime.hasScanDevice)
                                   ? bleRuntime.scanDeviceAddress
                                   : targetDeviceAddress;
        requestActiveConnParams(connAddr);
    }
    void onDisconnect(BLEClient *ppclient)
    {
        BleState state = get_ble_state();
        if (state == BLE_DISCONNECTING && bleRuntime.hasPendingStateAfterDisconnect)
        {
            BleState nextState = bleRuntime.pendingStateAfterDisconnect;
            bleRuntime.hasPendingStateAfterDisconnect = false;
            bleRuntime.pendingStateAfterDisconnect = BLE_IDLE;
            set_ble_state(nextState);
        }
        else if (state == BLE_CONNECTED || state == BLE_CONNECTING || state == BLE_DISCONNECTING)
        {
            set_ble_state(BLE_IDLE);
        }
        Serial.println("AT+DISCONNECT");
        delay(100);
        Serial.println("AT+DISCONNECT");
        colorWipe(strip.Color(255, 0, 0)); // 红色
        if (is_need_reset_adress)
        {
            // 禁止重连
            String packetString = "00:00:00:00:00:00";
            strcpy(targetDeviceAddress, packetString.c_str());
            // delay(5000);
            Serial.println("已经重置mac地址");
            Serial.printf("状态重置后: ble_state=%d\r\n", get_ble_state());
           
        }
    }
};
#define MAX_STORED_DEVICES 20
struct Device
{
    char address[18];
    bool isStored;
    int count; // 用于记录设备出现的次数
};
Device storedDevices[MAX_STORED_DEVICES];
bool isDeviceStored(const char *address)
{
    for (int i = 0; i < MAX_STORED_DEVICES; i++)
    {
        if (storedDevices[i].isStored && strcmp(storedDevices[i].address, address) == 0)
        {
            return true;
        }
    }
    return false;
}
void storeDevice(const char *address)
{
    bool deviceFound = false;

    // 首先检查设备是否已经存在于列表中
    for (int i = 0; i < MAX_STORED_DEVICES; i++)
    {
        if (storedDevices[i].count > 0 && strcmp(storedDevices[i].address, address) == 0)
        {
            // 设备已存在，增加计数器
            storedDevices[i].count++;
            if (storedDevices[i].count >= 2)
            {
                // 如果计数器达到2次，设置为已存储
                storedDevices[i].isStored = true;
            }
            deviceFound = true;
            break;
        }
    }

    // 如果设备不在列表中，添加新设备
    if (!deviceFound)
    {
        for (int i = 0; i < MAX_STORED_DEVICES; i++)
        {
            if (storedDevices[i].count == 0)
            {
                strcpy(storedDevices[i].address, address);
                storedDevices[i].count = 1; // 记录设备出现的次数
                break;
            }
        }
    }
}
void printStoredDevices()
{
    Serial.println("黑名单的mac：");
    for (int i = 0; i < MAX_STORED_DEVICES; i++)
    {
        if (storedDevices[i].isStored)
        {
            Serial.println(storedDevices[i].address);
        }
    }
}
// 将 MAC 地址字符串转换为 esp_bd_addr_t 类型
void strToBdAddr(const char *str, esp_bd_addr_t addr)
{
    unsigned int addrBytes[6];
    if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &addrBytes[0], &addrBytes[1], &addrBytes[2], &addrBytes[3], &addrBytes[4], &addrBytes[5]) == 6)
    {
        for (int i = 0; i < 6; i++)
        {
            addr[i] = static_cast<uint8_t>(addrBytes[i]);
        }
    }
    else
    {
        Serial.println("MAC 地址格式错误");
    }
}

static void applyPreferConnParams(const char *addressStr)
{
    esp_bd_addr_t bd_addr;
    strToBdAddr(addressStr, bd_addr);
    esp_err_t status = esp_ble_gap_set_prefer_conn_params(
        bd_addr,
        BLE_PREFER_CONN_INT_MIN,
        BLE_PREFER_CONN_INT_MAX,
        BLE_PREFER_CONN_LATENCY,
        BLE_PREFER_CONN_SUP_TOUT);
    if (status == ESP_OK)
    {
        Serial.printf("首选连接间隔已设置: min=0x%02x max=0x%02x (x1.25ms)\r\n",
                      BLE_PREFER_CONN_INT_MIN, BLE_PREFER_CONN_INT_MAX);
    }
    else
    {
        Serial.printf("首选连接间隔设置失败, err=%d\r\n", status);
    }
}

static void requestActiveConnParams(const char *addressStr)
{
    esp_ble_conn_update_params_t connParams = {};
    strToBdAddr(addressStr, connParams.bda);
    connParams.min_int = BLE_PREFER_CONN_INT_MIN;
    connParams.max_int = BLE_PREFER_CONN_INT_MAX;
    connParams.latency = BLE_PREFER_CONN_LATENCY;
    connParams.timeout = BLE_PREFER_CONN_SUP_TOUT;

    esp_err_t status = esp_ble_gap_update_conn_params(&connParams);
    if (status == ESP_OK)
    {
        Serial.printf("主动请求连接参数更新: min=0x%02x max=0x%02x latency=%u timeout=%u\r\n",
                      connParams.min_int, connParams.max_int, connParams.latency, connParams.timeout);
    }
    else
    {
        Serial.printf("主动请求连接参数更新失败, err=%d\r\n", status);
    }
}

static void customGapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event != ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT)
    {
        return;
    }

    Serial.printf("实际连接参数: status=%d conn_int=%u %.2fms min=%u max=%u latency=%u timeout=%u\r\n",
                  param->update_conn_params.status,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.conn_int * 1.25f,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
}

/**
 * 扫描BLE服务器，找到第一个广告我们所寻找的服务的服务器。
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
     * 为每个广告的BLE服务器调用。
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        if (get_ble_state() != BLE_SCANNING || get_ble_connect_mode() != CONNECT_BY_SCAN)
        {
            return;
        }

        if (StartBombState)
        {

            int rssi = advertisedDevice.getRSSI();
            String deviceName = advertisedDevice.getName();
            if (deviceName.indexOf(BOMBdevicename) != -1 && rssi > damageDistance.toInt() && !isDeviceStored(advertisedDevice.getAddress().toString().c_str()))
            {
                Serial.print("找到设备:" + deviceName);
                Serial.print(", deviceRssi:");
                Serial.println(rssi);
                strcpy(targetDeviceAddress, advertisedDevice.getAddress().toString().c_str());
                storeDevice(targetDeviceAddress);

                printStoredDevices();

                if (is_ble_connected())
                {
                    deinit_ble(); // 重置蓝牙
                }

                BLEDevice::getScan()->stop();
                remember_scan_device(advertisedDevice);
                set_ble_state(BLE_SCAN_FOUND);
            }
        }
        else
        {
            String deviceName = advertisedDevice.getName();
            String deviceAddress = advertisedDevice.getAddress().toString();
            int rssi = advertisedDevice.getRSSI();
            if (finddevicelogs)
            {
                Serial.print("deviceName:");
                Serial.print(deviceName.c_str());
                Serial.print(", deviceAddress:");
                Serial.print(deviceAddress.c_str());
                Serial.print(", deviceRssi:");
                Serial.println(rssi);
            }
            is_need_reset_adress = true;
               BLEAdvertisedDevice myadvertisedDevice=advertisedDevice;
            if (myadvertisedDevice.getAddress().equals(BLEAddress(targetDeviceAddress)))
            { // mac地址可以，那么准备开始连接

                Serial.printf("AT+DEVICENAME=%s\r\n", deviceName.c_str());
                {
                    uint8_t addrType = myadvertisedDevice.getAddressType();
                    Serial.print("目标设备地址类型 addrType=");
                    Serial.print(addrType);
                    Serial.print(" (");
                    Serial.print((addrType == BLE_ADDR_TYPE_PUBLIC) ? "public" : ((addrType == BLE_ADDR_TYPE_RANDOM) ? "random" : "other"));
                    Serial.println(")");
                }

                // colorWipe(strip.Color(255, 0, 0));  // 红色
                // colorWipe(strip.Color(0, 255, 0));  // 绿色
                // colorWipe(strip.Color(0, 0, 255));  // 蓝色
                BLEDevice::getScan()->stop();
                remember_scan_device(myadvertisedDevice);
                set_ble_state(BLE_SCAN_FOUND);
            }
        }
    }
};
#define scantime 1

void scanCompleteCallback(BLEScanResults scanResults)
{
    if (get_ble_state() == BLE_SCANNING)
    {
        bleRuntime.scanStarted = false;
        if (finddevicelogs)
        {
            Serial.print("本轮扫描结束，设备数：");
            Serial.println(scanResults.getCount());
        }
    }
}

void ble_init()
{
    BLEDevice::init("");
    BLEDevice::setCustomGapHandler(customGapEventHandler);
    connect_callback = new MyClientCallback();
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // 只会运行一次

    // pBLEScan->setInterval(1349); // 间隔时间
    // pBLEScan->setWindow(449);    // 窗口时间
    // 目标设备地址（设置为你希望配置参数的设备地址）

    pBLEScan->setActiveScan(true);
    set_ble_state(BLE_SCANNING);
    start_ble_scan();
}
void start_ble_scan()
{
    set_ble_state(BLE_SCANNING);
    if (bleRuntime.scanStarted)
    {
        return;
    }
    if (pBLEScan == nullptr)
    {
        Serial.println("BLE 扫描器未初始化，无法开启扫描");
        return;
    }
    bleRuntime.scanStarted = true;

    if (finddevicelogs)
    {
        Serial.println("开启扫描" + String(scantime) + "s");
    }
    pBLEScan->clearResults();         // memory，释放扫描缓存消耗
    if (!pBLEScan->start(scantime, scanCompleteCallback, false))
    {
        bleRuntime.scanStarted = false;
        Serial.println("开启扫描失败");
    }
}

const int numReadings = 100; // 设置读取的数量
int readings[numReadings];

int blemedianFilter()
{
    // 对数组排序
    for (int i = 0; i < numReadings - 1; i++)
    {
        for (int j = i + 1; j < numReadings; j++)
        {
            if (readings[i] > readings[j])
            {
                int temp = readings[i];
                readings[i] = readings[j];
                readings[j] = temp;
            }
        }
    }

    // 返回中位数
    return readings[numReadings / 2];
}

int numberBleRssi = 0;
void print_ble_rssi()
{
    if (blelogs)
    {
        if (numberBleRssi >= numReadings)
        {
            numberBleRssi = 0;
            int stableStrength = blemedianFilter();
            Serial.print("AT+BLERSSI=");
            Serial.println(stableStrength + 10); // 与之前的idf进行修正
        }
        readings[numberBleRssi] = pClient->getRssi(); // 获取蓝牙信号强度
        numberBleRssi++;
    }
}

void deinit_ble(BleState nextState)
{
    bleRuntime.hasPendingStateAfterDisconnect = true;
    bleRuntime.pendingStateAfterDisconnect = nextState;
    if (pClient != nullptr)
    {
        if (pClient->isConnected())
        {
            set_ble_state(BLE_DISCONNECTING);
            pClient->disconnect(); // 必须先断开连接触发回调函数再初始化回调函数
            Serial.println("已断开pClient");
        }
        else
        {
            bleRuntime.hasPendingStateAfterDisconnect = false;
            bleRuntime.pendingStateAfterDisconnect = BLE_IDLE;
            set_ble_state(nextState);
        }
    }
    else
    {
        bleRuntime.hasPendingStateAfterDisconnect = false;
        bleRuntime.pendingStateAfterDisconnect = BLE_IDLE;
        set_ble_state(nextState);
    }
    bleRuntime.hasScanDevice = false;
    strcpy(bleRuntime.scanDeviceAddress, "00:00:00:00:00:00");
}

struct OtaTxContext
{
    uint8_t buf[OTA_TX_BUFFER_SIZE];
    size_t len;
    size_t readOff;
    uint32_t firstByteMs;
    SemaphoreHandle_t mutex;
    TaskHandle_t task;
};

static OtaTxContext otaTx = {};

static bool otaBleCanSend()
{
    return AppDataCharacteristic != nullptr
           && pClient != nullptr
           && pClient->isConnected()
           && esp_ble_get_cur_sendable_packets_num(conn_id) > 0;
}

static bool otaBleTrySend(const uint8_t *data, size_t length)
{
    if (!otaBleCanSend())
    {
        return false;
    }
    return AppDataCharacteristic->writeValue((uint8_t *)data, length, false) != 0;
}

static void otaBleNotifyTask()
{
    if (otaTx.task != nullptr)
    {
        xTaskNotifyGive(otaTx.task);
    }
}

static size_t otaTxPendingUnlocked()
{
    return otaTx.len > otaTx.readOff ? (otaTx.len - otaTx.readOff) : 0;
}

static void otaTxCompactUnlocked()
{
    if (otaTx.readOff == 0)
    {
        return;
    }
    if (otaTx.readOff >= otaTx.len)
    {
        otaTx.readOff = 0;
        otaTx.len = 0;
        return;
    }
    size_t pending = otaTx.len - otaTx.readOff;
    memmove(otaTx.buf, otaTx.buf + otaTx.readOff, pending);
    otaTx.readOff = 0;
    otaTx.len = pending;
}

void otaBleReset()
{
    if (otaTx.mutex == nullptr)
    {
        return;
    }
    xSemaphoreTake(otaTx.mutex, portMAX_DELAY);
    otaTx.len = 0;
    otaTx.readOff = 0;
    otaTx.firstByteMs = 0;
    xSemaphoreGive(otaTx.mutex);
}

void otaBleFeed(const uint8_t *data, size_t length)
{
    if (data == nullptr || length == 0 || otaTx.mutex == nullptr)
    {
        return;
    }

    xSemaphoreTake(otaTx.mutex, portMAX_DELAY);
    if (otaTx.readOff > 0 && otaTx.readOff >= (OTA_TX_BUFFER_SIZE / 2))
    {
        otaTxCompactUnlocked();
    }

    if (otaTxPendingUnlocked() == 0)
    {
        otaTx.firstByteMs = millis();
    }

    if (otaTx.len + length > sizeof(otaTx.buf))
    {
        LOG_ERROR("OTA 发送缓冲区溢出");
        xSemaphoreGive(otaTx.mutex);
        return;
    }

    memcpy(otaTx.buf + otaTx.len, data, length);
    otaTx.len += length;
    xSemaphoreGive(otaTx.mutex);
    otaBleNotifyTask();
}

uint16_t bleMtuPayloadMax()
{
    return (uint16_t)(bleMtuSize);
}

bool bleSetMtu(uint16_t mtu)
{
    if (StartSendOtaData)
    {
        return false;
    }
    if (mtu < BLE_MTU_MIN || mtu > BLE_MTU_MAX)
    {
        return false;
    }
    bleMtuSize = mtu;
    if (otaBlePacketSize > bleMtuPayloadMax())
    {
        otaBlePacketSize = bleMtuPayloadMax();
        otaBleReset();
    }
    if (pClient != nullptr && pClient->isConnected())
    {
        return pClient->setMTU(bleMtuSize);
    }
    return true;
}

bool otaBleSetPacketSize(uint16_t size)
{
    if (StartSendOtaData)
    {
        return false;
    }
    if (size < OTA_BLE_PACKET_SIZE_MIN || size > bleMtuPayloadMax())
    {
        return false;
    }
    otaBlePacketSize = size;
    otaBleReset();
    return true;
}

static void otaBleTxTask(void *pvParameters)
{
    uint8_t sendPkt[OTA_BLE_PACKET_BUF_SIZE];
    int burstLeft = 0;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(OTA_TX_FLUSH_TIMEOUT_MS));

        if (!StartSendOtaData)
        {
            continue;
        }

        while (StartSendOtaData)
        {
            size_t sendLen = 0;

            xSemaphoreTake(otaTx.mutex, portMAX_DELAY);
            size_t pending = otaTxPendingUnlocked();
            if (pending == 0)
            {
                otaTx.firstByteMs = 0;
                xSemaphoreGive(otaTx.mutex);
                break;
            }

            uint32_t now = millis();
            const uint16_t pktSize = otaBlePacketSize;
            if (pending >= pktSize)
            {
                sendLen = pktSize;
            }
            else if ((now - otaTx.firstByteMs) >= OTA_TX_FLUSH_TIMEOUT_MS)
            {
                sendLen = pending;
            }
            else
            {
                xSemaphoreGive(otaTx.mutex);
                break;
            }

            memcpy(sendPkt, otaTx.buf + otaTx.readOff, sendLen);
            xSemaphoreGive(otaTx.mutex);

            if (!otaBleCanSend())
            {
                vTaskDelay(1);
                break;
            }

            if (!otaBleTrySend(sendPkt, sendLen))
            {
                vTaskDelay(1);
                break;
            }

#if OTA_LOG_BLE_TX
            Serial.printf("[%lu] OTA_BLE_TX len=%u (%s)\r\n",
                          millis(),
                          (unsigned)sendLen,
                          sendLen < pktSize ? "timeout" : "mtu");
#endif

            xSemaphoreTake(otaTx.mutex, portMAX_DELAY);
            otaTx.readOff += sendLen;
            if (otaTx.readOff >= otaTx.len)
            {
                otaTx.readOff = 0;
                otaTx.len = 0;
                otaTx.firstByteMs = 0;
            }
            else
            {
                otaTx.firstByteMs = millis();
            }
            xSemaphoreGive(otaTx.mutex);

            if (sendLen < pktSize)
            {
                break;
            }

            burstLeft++;
            if (burstLeft >= OTA_TX_BURST_MAX)
            {
                burstLeft = 0;
                break;
            }

            if (esp_ble_get_cur_sendable_packets_num(conn_id) <= 0)
            {
                break;
            }
        }
        burstLeft = 0;
    }
}

void otaBleTxInit()
{
    if (otaTx.mutex != nullptr)
    {
        return;
    }

    otaTx.mutex = xSemaphoreCreateMutex();
    if (otaTx.mutex == nullptr)
    {
        Serial.println("OTA 发送互斥量创建失败");
        return;
    }

    BaseType_t created = xTaskCreate(
        otaBleTxTask,
        "OTA BLE TX Task",
        OTA_TX_TASK_STACK_SIZE,
        NULL,
        OTA_TX_TASK_PRIORITY,
        &otaTx.task);

    if (created != pdPASS)
    {
        Serial.println("OTA 发送任务创建失败");
        otaTx.task = nullptr;
    }
}

void clear_ble_scan_device()
{
    BleState state = get_ble_state();
    if (state == BLE_CONNECTING || state == BLE_DISCONNECTING)
    {
        Serial.println("当前连接流程中，暂不清理扫描设备缓存");
        return;
    }
    if (bleRuntime.hasScanDevice)
    {
        bleRuntime.hasScanDevice = false;
        strcpy(bleRuntime.scanDeviceAddress, "00:00:00:00:00:00");
        Serial.println("已清理扫描设备缓存");
    }
}

void send_ble_data(ext_ble_phy_channel_send_e channel, uint8_t *data, size_t length)
{
    if (StartSendOtaData)
    {
        otaBleFeed(data, length);
        return;
    }

    const size_t MAX_PACKET_SIZE = bleMtuPayloadMax();
    size_t offset = 0;
    uint8_t packet[OTA_BLE_PACKET_BUF_SIZE];

    while (offset < length)
    {
        size_t remaining = length - offset;
        size_t packetSize = remaining > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : remaining;
        memcpy(packet, data + offset, packetSize);

        if (channel == PHY_CHANNEL_MAIN)
        {
            if (mainWriteCharacteristic != nullptr)
                mainWriteCharacteristic->writeValue(packet, packetSize);
            Serial.println("main通道");
        }
        else if (channel == PHY_CHANNEL_APP)
        {
            if (AppWriteCharacteristic != nullptr)
                AppWriteCharacteristic->writeValue(packet, packetSize);
            Serial.println("app通道");
        }
        else if (channel == PHY_CHANNEL_FAC)
        {
            if (WriteCharacteristic != nullptr)
                WriteCharacteristic->writeValue(packet, packetSize);
            Serial.printf("[%lu] fac通道=%d\r\n", millis(), packetSize);
        }
        else
        {
            Serial.print("未知蓝牙传输通道为：");
            Serial.println(channel);
        }

        // 更新偏移量
        offset += packetSize;
    }
}

bool connectTobleServer()
{
    Serial.print("正在连接到：");
    Serial.println(targetDeviceAddress);
    if (pClient != nullptr)
    {
        // 先请求断开，并等待异步 DISCONNECT_EVT 回调收敛。
        // 这里不立即 delete，避免 BT 线程仍在回调旧对象导致 UAF。
        if (pClient->isConnected())
        {
            pClient->disconnect();
            uint32_t waitMs = 0;
            while (waitMs < 1000)
            {
                if (!pClient->isConnected() && pClient->getGattcIf() == ESP_GATT_IF_NONE)
                {
                    break;
                }
                delay(10);
                waitMs += 10;
            }
            if (waitMs >= 1000)
            {
                Serial.println("等待旧连接回落超时，继续尝试重连");
            }
        }

        WriteCharacteristic = nullptr;
        mainNotifyCharacteristic = nullptr;
        mainWriteCharacteristic = nullptr;
        NotifyCharacteristic = nullptr;
        CAMERAUUIDCharacteristic = nullptr;
        LOGUUIDCharacteristic = nullptr;
        AppDataCharacteristic = nullptr;
        AppNotifyCharacteristic = nullptr;
        AppWriteCharacteristic = nullptr;
        normolNotifyCharacteristic = nullptr;
        normolWriteCharacteristic = nullptr;
    }
    else
    {
        pClient = BLEDevice::createClient(); // 首次创建
        Serial.println("创建客户端...");
    }

    pClient->setClientCallbacks(connect_callback); // 设置客户端回调函数
    Serial.println("创建完成,连接到设备");

    const char *connAddr = (get_ble_connect_mode() == CONNECT_BY_SCAN && bleRuntime.hasScanDevice)
                               ? bleRuntime.scanDeviceAddress
                               : targetDeviceAddress;
    applyPreferConnParams(connAddr);

    bool connected = false;
    if (get_ble_connect_mode() == CONNECT_BY_SCAN)
    {
        if (!bleRuntime.hasScanDevice)
        {
            Serial.println("扫描连接模式缺少扫描设备信息");
            return false;
        }
        Serial.print("使用扫描设备连接：");
        Serial.print(bleRuntime.scanDeviceAddress);
        Serial.print(", addrType=");
        Serial.println(bleRuntime.scanDeviceAddressType);
        BLEAddress addr(bleRuntime.scanDeviceAddress);
        connected = pClient->connect(addr, bleRuntime.scanDeviceAddressType, 15000);
        if (connected && !pClient->isConnected())
        {
            Serial.println("扫描连接返回成功但 isConnected() 为 false，视为失败");
            connected = false;
        }
    }
    else if (get_ble_connect_mode() == CONNECT_DIRECT)
    {
        Serial.println("未提供扫描设备信息，尝试按 MAC 直连");
        BLEAddress addr(targetDeviceAddress);
        // 按用户要求：只尝试一次连接，不做地址类型重试。
        const uint32_t dconTimeoutMs = 15000;
        const esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM;
        Serial.print("直连尝试 addrType=");
        Serial.print((type == BLE_ADDR_TYPE_PUBLIC) ? "public" : "random");
        Serial.print(", timeoutMs=");
        Serial.println(dconTimeoutMs);

        connected = pClient->connect(addr, type, dconTimeoutMs);
        if (connected && !pClient->isConnected())
        {
            Serial.println("connect() 返回成功但 isConnected() 为 false，视为失败");
            connected = false;
        }
    }
    else
    {
        Serial.println("未知连接模式");
    }

    if (connected) // 连接到远程BLE服务器
    {
        Serial.print("连接到UUID  ");
        Serial.println(use_normal_service);
        if (!pClient->isConnected())
        {
            Serial.println("connect 返回成功但实际已断开，取消服务发现");
            return false;
        }
        if (pClient != nullptr)
        {
            // 连接在服务发现过程中可能被对端断开，逐个检查避免阻塞卡住 loop。
            facRemoteService = findFactoryRemoteService();
            if (!pClient->isConnected())
            {
                return false;
            }

            appRemoteService = findOtaRemoteService();
            if (!pClient->isConnected())
            {
                Serial.println("服务发现中连接已断开(appRemoteService)");
                return false;
            }

            mainRemoteService = pClient->getService(mainserviceUUID);
            if (!pClient->isConnected())
            {
                Serial.println("服务发现中连接已断开(mainRemoteService)");
                return false;
            }

            normolRemoteService = pClient->getService(serviceUUIDNormal);
            if (!pClient->isConnected())
            {
                Serial.println("服务发现中连接已断开(normolRemoteService)");
                return false;
            }
        }
        else
        {
            Serial.println("pClient 是空指针"); // pClient 是空指针，处理错误逻辑
            return false;
        }

        if (facRemoteService != nullptr && activeFactoryProfile != nullptr)
        {
            Serial.println("找到facRemoteService服务");
            NotifyCharacteristic = facRemoteService->getCharacteristic(activeFactoryProfile->notifyUUID);
            WriteCharacteristic = facRemoteService->getCharacteristic(activeFactoryProfile->writeUUID);
            if (activeFactoryProfile->hasExtraCharacteristics)
            {
                CAMERAUUIDCharacteristic = facRemoteService->getCharacteristic(CameraUUID);
                LOGUUIDCharacteristic = facRemoteService->getCharacteristic(LogUUID);
            }
        }
        else
        {
            Serial.println("facRemoteService为空");
        }
        if (appRemoteService != nullptr && activeOtaProfile != nullptr)
        {
            Serial.println("找到appRemoteService服务");
            AppDataCharacteristic = appRemoteService->getCharacteristic(activeOtaProfile->dataUUID);
            AppNotifyCharacteristic = appRemoteService->getCharacteristic(activeOtaProfile->notifyUUID);
            AppWriteCharacteristic = appRemoteService->getCharacteristic(activeOtaProfile->writeUUID);
        }
        else
        {
            Serial.println("appRemoteService为空");
        }
        if (mainRemoteService != nullptr)
        {
            Serial.println("找到mainRemoteService服务");
            mainNotifyCharacteristic = mainRemoteService->getCharacteristic(mainNotifyUUID);
            mainWriteCharacteristic = mainRemoteService->getCharacteristic(mainWriteUUID);
        }
        else
        {
            Serial.println("mainRemoteService为空");
        }

        if (normolRemoteService != nullptr)
        {
            Serial.println("找到normolRemoteService服务");
            normolNotifyCharacteristic = normolRemoteService->getCharacteristic(NotifyUUIDNormal);
            normolWriteCharacteristic = normolRemoteService->getCharacteristic(WriteUUIDNormal);
        }
        else
        {
            Serial.println("normolRemoteService为空");
        }
     
        return ServerStateCheck();
    }
    else
    {
        Serial.println("MAC地址连接失败");
        Serial.println("AT+DISCONNECT");

    }
    return false;
}

bool ServerStateCheck()
{
    if (mainNotifyCharacteristic != nullptr)
    {
        if (mainNotifyCharacteristic->canNotify()) // 注册特征通知回调
        {
            Serial.println("注册mainNotifyCharacteristic特征通知回调");
            if (pClient->isConnected())
            {
                mainNotifyCharacteristic->registerForNotify(mainNotifyCallback);
            }
            else
            {
                Serial.println("蓝牙没有连接");
                return false;
            }
        }
    }
    else
    {
        Serial.println("找不到mainNotifyCharacteristic的数据传输特征");
    }


    if (NotifyCharacteristic != nullptr)
    {
        if (NotifyCharacteristic->canNotify()) // 注册特征通知回调
        {
            Serial.println("注册NotifyCharacteristic特征通知回调");
            if (pClient->isConnected())
            {
                NotifyCharacteristic->registerForNotify(notifyCallback);
            }
            else
            {
                Serial.println("蓝牙没有连接");
                return false;
            }
        }
    }
    else
    {
        Serial.print("找不到消息提醒UUID：");
        if (activeFactoryProfile != nullptr)
        {
            Serial.println(activeFactoryProfile->notifyUUID.toString().c_str());
        }
        else
        {
            Serial.println("未匹配factory服务");
        }
    }

    if (CAMERAUUIDCharacteristic != nullptr)
    {
        Serial.println("找到我们的摄像头传输特征");
        if (CAMERAUUIDCharacteristic->canNotify()) // 注册特征通知回调
        {
            Serial.println("注册摄像头特征通知回调");
            if (pClient->isConnected())
            {
                CAMERAUUIDCharacteristic->registerForNotify(cameranotifyCallback);
            }
            else
            {
                Serial.println("蓝牙还没有连接");
                return false;
            }
        }
    }
    else
    {
        Serial.print("找不到摄像头消息提醒UUID：");
        Serial.println(CameraUUID.toString().c_str());
    }


    if (LOGUUIDCharacteristic != nullptr)
    {
        Serial.println("找到我们的日志传输特征");
        if (LOGUUIDCharacteristic->canNotify()) // 注册特征通知回调
        {
            Serial.println("注册日志特征通知回调");
            if (pClient->isConnected())
            {
                LOGUUIDCharacteristic->registerForNotify(brushLogNotifyCallback);
            }
            else
            {
                Serial.println("蓝牙还没有连接");
                return false;
            }
        }
    }
    else
    {
        Serial.print("找不到log消息提醒UUID：");
        Serial.println(LogUUID.toString().c_str());
    }


    if (AppNotifyCharacteristic != nullptr)
    {
        if (AppNotifyCharacteristic->canNotify()) // 注册特征通知回调
        {
            Serial.println("注册AppNotifyCharacteristic特征通知回调");
            if (pClient->isConnected())
            {
                AppNotifyCharacteristic->registerForNotify(AppNotifyCallback);
            }
            else
            {
                Serial.println("蓝牙没有连接");
                return false;
            }
        }
    }
    else
    {
        Serial.println("找不到AppDataCharacteristic的数据传输特征");
    }



    if (WriteCharacteristic != nullptr)
    {
        Serial.println("找到我们的WriteCharacteristic写入特征");
    }
    else
    {
        Serial.println("找不到写入特征");
    }

    return true; // 连接成功
}
