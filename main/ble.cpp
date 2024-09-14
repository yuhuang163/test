
#include "config.h"
#include "esp_gatt_common_api.h"

uint16_t conn_id = -1;
static BLEClient *pClient = nullptr; // 蓝牙客户端的类
BLEClientCallbacks *connect_callback = nullptr;
boolean doConnect = false;     // 是否可以开始连接
boolean ble_connected = false; // 是否是连接的状态
boolean ble_scan_over = false; // 是否scan完成
boolean StartSendOtaData = false;
boolean StartSendmainData = false;
boolean StartBombState = false;
boolean candeleteble = true; // 是否可以销毁蓝牙实例

static BLEScan *pBLEScan;

static BLERemoteService *facRemoteService = nullptr;
static BLERemoteService *appRemoteService = nullptr;
static BLERemoteService *mainRemoteService = nullptr;
static BLERemoteService *normolRemoteService = nullptr;

static BLEAdvertisedDevice *myDevice = nullptr; // 这个设备要反初始化

static BLEUUID mainserviceUUID("a6ed0301-d344-460a-8075-b9e8ec90d71b"); // main
static BLEUUID mainWriteUUID("a6ed0302-d344-460a-8075-b9e8ec90d71b");
static BLEUUID mainNotifyUUID("a6ed0302-d344-460a-8075-b9e8ec90d71b");
static BLERemoteCharacteristic *mainNotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *mainWriteCharacteristic = nullptr;

static BLEUUID serviceUUID("a6ed0201-d344-460a-8075-b9e8ec90d71b"); // fac
static BLEUUID NotifyUUID("a6ed0202-d344-460a-8075-b9e8ec90d71b");  // 收产测指令
static BLEUUID WriteUUID("a6ed0203-d344-460a-8075-b9e8ec90d71b");   // 写产测指令
static BLEUUID CameraUUID("a6ed0204-d344-460a-8075-b9e8ec90d71b");  // 摄像头传图
static BLEUUID LogUUID("a6ed0205-d344-460a-8075-b9e8ec90d71b");     // 传输日志
static BLERemoteCharacteristic *LOGUUIDCharacteristic = nullptr;
static BLERemoteCharacteristic *CAMERAUUIDCharacteristic = nullptr;
static BLERemoteCharacteristic *NotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *WriteCharacteristic = nullptr;

static BLEUUID serviceUUIDOTA("a6ed0101-d344-460a-8075-b9e8ec90d71b"); // app
static BLEUUID WriteUUIDOTA("a6ed0103-d344-460a-8075-b9e8ec90d71b");   // 收发指令的服务
static BLEUUID NotifyUUIDOTA("a6ed0103-d344-460a-8075-b9e8ec90d71b");  // 收发指令的服务
static BLEUUID WriteOTADATA("a6ed0102-d344-460a-8075-b9e8ec90d71b");   // 发ota数据包的服务
static BLERemoteCharacteristic *AppDataCharacteristic = nullptr;       // 发送ota数据的特征
static BLERemoteCharacteristic *AppNotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *AppWriteCharacteristic = nullptr;

static BLEUUID serviceUUIDNormal("1828"); // normol
static BLEUUID WriteUUIDNormal("2ACA");
static BLEUUID NotifyUUIDNormal("2ACA");
static BLERemoteCharacteristic *normolNotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *normolWriteCharacteristic = nullptr;

typedef enum
{
    PHY_CHANNEL_INVALID = 0, // 无效值
    PHY_CHANNEL_CAMREA,      // 控制命令通道
    PHY_CHANNEL_LOG,         // ota数据通道

} ext_ble_phy_send_channel_e;

// 消息提醒函数
void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                    size_t length, bool isNotify)
{
    // vTaskDelay(10);   // 让出处理器

    unsigned long currentMillis = millis();   // 或者使用 micros() 函数获取微秒级时间戳
    String timestamp = String(currentMillis); // 将时间戳转换为字符串
    Serial.println();
    Serial.print("产测数据包的时间戳:"); // 打印带有时间戳的消息
    Serial.print(timestamp);
    Serial.print("长度为:"); // 打印带有时间戳的消息
    Serial.print(length);

    const int additionalBytes = 9;
    uint8_t modifiedData[length + additionalBytes];
    memset(modifiedData, 0xaa, 8);

    modifiedData[8] = length;
    memcpy(modifiedData + 9, pData, length);

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
// 连接状态函数
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *ppclient)
    {
        pClient->setMTU(MY_MTU);
        ble_connected = true;
        conn_id = pClient->getConnId();
    }
    void onDisconnect(BLEClient *ppclient)
    {
        ble_connected = false;
        Serial.println("AT+DISCONNECT");
        delay(100);
        Serial.println("AT+DISCONNECT");

        if (is_need_reset_adress)
        {
            // 禁止重连
            String packetString = "00:00:00:00:00:00";
            strcpy(targetDeviceAddress, packetString.c_str());
            // delay(5000);
            Serial.println("已经重置mac地址");
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
                digitalWrite(D2_PIN, LOW);

                if (ble_connected && candeleteble) // 连接上了且可以开始连接才会去清空，否则连接中途被删掉就出问题了，导致卡死
                {
                    deinit_ble(); // 重置蓝牙
                }

                BLEDevice::getScan()->stop();
                myDevice = new BLEAdvertisedDevice(advertisedDevice); // 有释放内存
                doConnect = true;                                     // 是否可以开始连接
                ble_scan_over = true;                                 // 是否完成了scan
            }
        }
        else
        {
            digitalWrite(D2_PIN, HIGH); // 将 D2_PIN 设置为高电平
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
            if (advertisedDevice.getAddress().equals(BLEAddress(targetDeviceAddress)))
            { // mac地址可以，那么准备开始连接

                esp_bd_addr_t target_device_addr;
                strToBdAddr(targetDeviceAddress, target_device_addr);

                // 设置首选连接参数
                esp_err_t status = esp_ble_gap_set_prefer_conn_params(
                    target_device_addr,
                    0x09, // 9 * 1.25ms = 11.25ms
                    0x0c, // 12 * 1.25ms = 15ms
                    0,    // 从机延迟// 从机延迟 (单位: 连接事件数)
                    2000  // 400 * 10ms = 4000ms
                );

                if (status == ESP_OK)
                {
                    Serial.println("连接间隔设置成功");
                }
                else
                {
                    Serial.printf("连接间隔设置失败，错误码 %d", status);
                }

                digitalWrite(D2_PIN, LOW); // 将 D2_PIN 设置为高电

                // colorWipe(strip.Color(255, 0, 0));  // 红色
                // colorWipe(strip.Color(0, 255, 0));  // 绿色
                // colorWipe(strip.Color(0, 0, 255));  // 蓝色
                BLEDevice::getScan()->stop();
                myDevice = new BLEAdvertisedDevice(advertisedDevice); // 有释放内存
                doConnect = true;                                     // 是否可以开始连接
                ble_scan_over = true;                                 // 是否完成了scan
            }
        }
    }
};
#define scantime 1

void ble_init()
{
    BLEDevice::init("");
    connect_callback = new MyClientCallback();
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // 只会运行一次

    // pBLEScan->setInterval(1349); // 间隔时间
    // pBLEScan->setWindow(449);    // 窗口时间
    // 目标设备地址（设置为你希望配置参数的设备地址）

    pBLEScan->setActiveScan(true);
    pBLEScan->start(scantime, false);
}
void start_ble_scan()
{
    Serial.println("开启扫描" + String(scantime) + "s");
    pBLEScan->clearResults();         // memory，释放扫描缓存消耗
    pBLEScan->start(scantime, false); // 设置太久在很多设备的情况下会崩溃
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

void deinit_ble()
{
    if (pClient != nullptr)
    {
        pClient->disconnect(); // 必须先断开连接触发回调函数再初始化回调函数
        Serial.println("已断开pClient");
    }
    if (myDevice != nullptr)
    {
        delete myDevice;
        myDevice = nullptr;
    }
}

void send_ble_data(ext_ble_phy_channel_send_e channel, uint8_t *data, size_t length)
{
    const size_t MAX_PACKET_SIZE = MY_MTU - 3; // 最大每包大小
    size_t offset = 0;                         // 当前数据的偏移量
                                               // size_t packetCount = 0;                    // 记录当前包数

    while (offset < length)
    {
        size_t remaining = length - offset;                                            // 剩余数据的长度
        size_t packetSize = remaining > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : remaining; // 当前包的大小

        // 提取当前包的数据
        uint8_t packet[MAX_PACKET_SIZE];
        memcpy(packet, data + offset, packetSize);
        // packetCount++;
        // Serial.printf("开始发送第%d包\r\n", packetCount);
        // 发送数据
        if (StartSendOtaData)
        {
            if (AppDataCharacteristic != nullptr)
            {
                while (1)
                {
                    if (pClient->isConnected())
                    {

                        int free_buff_num = esp_ble_get_cur_sendable_packets_num(conn_id);
                        if (free_buff_num > 0)
                        {
                            if (AppDataCharacteristic->writeValue(packet, packetSize))
                            {
                                break;
                            }
                            else
                            {
                                Serial.println("发送失败");
                                continue;
                            }
                        }
                        else
                        {
                            Serial.println("wait send");
                        }
                    }
                    else
                    {
                        Serial.println("蓝牙未连接取消发送");
                        break;
                    }
                    vTaskDelay(10);
                }
            }
        }
        else if (channel == PHY_CHANNEL_MAIN)
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
            Serial.println("fac通道");
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

bool connectToServer()
{
    candeleteble = false;
    Serial.print("正在连接到：");
    Serial.println(targetDeviceAddress);
    if (pClient != nullptr)
    {
        delete pClient;
        pClient = nullptr;
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

        Serial.println("已释放pClient和WriteCharacteristic");
    }
    pClient = BLEDevice::createClient(); // 需要反初始化
    Serial.println("创建客户端...");

    pClient->setClientCallbacks(connect_callback); // 设置客户端回调函数
    Serial.println("创建完成");
    if (pClient->connect(myDevice)) // 连接到远程BLE服务器
    {
        Serial.print("连接到UUID  ");
        Serial.println(use_normal_service);
        if (pClient != nullptr)
        {
            facRemoteService = pClient->getService(serviceUUID);
            appRemoteService = pClient->getService(serviceUUIDOTA);
            mainRemoteService = pClient->getService(mainserviceUUID);
            normolRemoteService = pClient->getService(serviceUUIDNormal);
        }
        else
        {
            Serial.println("pClient 是空指针"); // pClient 是空指针，处理错误逻辑
            candeleteble = true;
            return false;
        }

        if (facRemoteService != nullptr)
        {
            CAMERAUUIDCharacteristic = facRemoteService->getCharacteristic(CameraUUID);
            NotifyCharacteristic = facRemoteService->getCharacteristic(NotifyUUID);
            WriteCharacteristic = facRemoteService->getCharacteristic(WriteUUID);
            LOGUUIDCharacteristic = facRemoteService->getCharacteristic(LogUUID);
        }
        else
        {
            Serial.println("facRemoteService为空");
        }
        if (appRemoteService != nullptr)
        {
            Serial.println("找到appRemoteService服务");
            AppDataCharacteristic = appRemoteService->getCharacteristic(WriteOTADATA);
            AppNotifyCharacteristic = appRemoteService->getCharacteristic(NotifyUUIDOTA);
            AppWriteCharacteristic = appRemoteService->getCharacteristic(WriteUUIDOTA);
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
    }
    candeleteble = true;
    return false;
}

bool ServerStateCheck()
{

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
                candeleteble = true;
                return false;
            }
        }
    }
    else
    {
        Serial.print("找不到log消息提醒UUID：");
        Serial.println(LogUUID.toString().c_str());
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
                candeleteble = true;
                return false;
            }
        }
    }
    else
    {
        Serial.print("找不到摄像头消息提醒UUID：");
        Serial.println(CameraUUID.toString().c_str());
    }

    if (NotifyCharacteristic != nullptr)
    {
        if (NotifyCharacteristic->canNotify()) // 注册特征通知回调
        {
            Serial.println("注册特征通知回调");

            if (pClient->isConnected())
            {
                NotifyCharacteristic->registerForNotify(notifyCallback);
            }
            else
            {
                Serial.println("蓝牙没有连接");
                candeleteble = true;
                return false;
            }
        }
    }
    else
    {
        Serial.print("找不到消息提醒UUID：");
        Serial.println(NotifyUUID.toString().c_str());
    }

    if (AppNotifyCharacteristic != nullptr)
    {
        if (AppNotifyCharacteristic->canNotify()) // 注册特征通知回调
        {
            Serial.println("注册特征通知回调");

            if (pClient->isConnected())
            {
                AppNotifyCharacteristic->registerForNotify(notifyCallback);
            }
            else
            {
                Serial.println("蓝牙没有连接");
                candeleteble = true;
                return false;
            }
        }
        Serial.println("找到我们的DataCharacteristic写入特征");
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

    candeleteble = true;
    return true; // 连接成功
}
