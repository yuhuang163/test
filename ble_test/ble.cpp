
#include "config.h"

static BLEClient *pClient = nullptr;   // 蓝牙客户端的类
BLEClientCallbacks *connect_callback = nullptr;
boolean doConnect = false;   // 是否可以开始连接
boolean connected = false;   // 是否是连接的状态
boolean doScan = false;      // 是否scan完成

boolean  candeleteble= true;      // 是否可以销毁蓝牙实例

static BLEScan *pBLEScan;

static BLERemoteCharacteristic *CAMERAUUIDCharacteristic = nullptr;

static BLERemoteCharacteristic *NotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *WriteCharacteristic = nullptr;
static BLERemoteService *pRemoteService = nullptr;
static BLEAdvertisedDevice *myDevice = nullptr;   // 这个设备要反初始化
static BLEUUID serviceUUID("a6ed0201-d344-460a-8075-b9e8ec90d71b");
static BLEUUID NotifyUUID("a6ed0202-d344-460a-8075-b9e8ec90d71b");
static BLEUUID WriteUUID("a6ed0203-d344-460a-8075-b9e8ec90d71b");
static BLEUUID CameraUUID("a6ed0204-d344-460a-8075-b9e8ec90d71b");

static BLEUUID serviceUUIDOTA("a6ed0101-d344-460a-8075-b9e8ec90d71b");
static BLEUUID WriteUUIDOTA("a6ed0103-d344-460a-8075-b9e8ec90d71b");
static BLEUUID NotifyUUIDOTA("a6ed0103-d344-460a-8075-b9e8ec90d71b");

static BLEUUID serviceUUIDNormal("1828");
static BLEUUID WriteUUIDNormal("2ACA");
static BLEUUID NotifyUUIDNormal("2ACA");

// 消息提醒函数
void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                    size_t length, bool isNotify)
{
    const int additionalBytes = 9;
    uint8_t modifiedData[length + additionalBytes];
    for (int i = 0; i < 8; ++i)
    {
        modifiedData[i] = 0xaa;
    }
    modifiedData[8] = length;
    for (int i = 0; i < length; ++i)
    {
        modifiedData[9 + i] = pData[i];
    }
    Serial.write(modifiedData, length + additionalBytes);
}
// 消息提醒函数
void cameranotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                          size_t length, bool isNotify)
{
    unsigned long currentMillis = millis();   // 或者使用 micros() 函数获取微秒级时间戳
    String timestamp = String(currentMillis);   // 将时间戳转换为字符串
    Serial.println();
    Serial.print("数据包的时间戳:");   // 打印带有时间戳的消息
    Serial.print(timestamp);
    Serial.print("长度为:");   // 打印带有时间戳的消息
    Serial.println(length);

    const int additionalBytes = 9;
    uint8_t modifiedData[length + additionalBytes];
    for (int i = 0; i < 8; ++i)
    {
        modifiedData[i] = 0xcc;
    }
    modifiedData[8] = length;
    for (int i = 0; i < length; ++i)
    {
        modifiedData[9 + i] = pData[i];
    }
    Serial.write(modifiedData, length + additionalBytes);
}
// 连接状态函数
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *ppclient)
    {
        pClient->setMTU(247);
        connected = true;
    }
    void onDisconnect(BLEClient *ppclient)
    {
        connected = false;
        Serial.println("AT+DISCONNECT");
        delay(100);
        Serial.println("AT+DISCONNECT");

        if (is_need_reset_adress)
        {
            Serial.println("已经重置mac地址");
            // 禁止重连
            String packetString = "00:00:00:00:00:00";
            strcpy(targetDeviceAddress, packetString.c_str());
            // delay(5000);
        }
    }
};

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
        digitalWrite(D2_PIN, HIGH);   // 将 D2_PIN 设置为高电平
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
        {                                // mac地址可以，那么准备开始连接
            digitalWrite(D2_PIN, LOW);   // 将 D2_PIN 设置为高电

            // colorWipe(strip.Color(255, 0, 0));                 // 红色
            // colorWipe(strip.Color(0, 255, 0));  // 绿色
            // colorWipe(strip.Color(0, 0, 255));  // 蓝色
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);   // 有释放内存
            doConnect = true;                                       // 是否可以开始连接
            doScan = true;                                          // 是否完成了scan
        }
    }
};

void ble_init()
{
    BLEDevice::init("");
    connect_callback = new MyClientCallback();
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());   // 只会运行一次
    // pBLEScan->setInterval(800);
    // pBLEScan->setWindow(500);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(2, false);
}

void start_ble_scan()
{
    Serial.println("开启扫描5s");
    pBLEScan->clearResults();    // memory，释放扫描缓存消耗
    pBLEScan->start(5, false);   // 扫描10s如果没扫到，可以通过串口打断
}
void print_ble_rssi()
{
    int ble_rssi = pClient->getRssi();
    if (blelogs)
    {
        Serial.print("AT+BLERSSI=");
        Serial.println(ble_rssi);
    }
}

void deinit_ble()
{
    if (pClient != nullptr)
    {
        pClient->disconnect();   // 必须先断开连接触发回调函数再初始化回调函数
        Serial.println("已断开pClient");
    }
    if (myDevice != nullptr)
    {
        delete myDevice;
        myDevice = nullptr;
    }
}

void send_ble_data(uint8_t *data, size_t length)
{
    if (WriteCharacteristic != nullptr)
        WriteCharacteristic->writeValue(data, length);
}



bool connectToServer()
{
    candeleteble=false;
    Serial.print("正在连接到：");
    Serial.println(targetDeviceAddress);

    if (pClient != nullptr)
    {
        delete pClient;
        pClient = nullptr;
        WriteCharacteristic = nullptr;
        NotifyCharacteristic = nullptr;
        CAMERAUUIDCharacteristic = nullptr;

        Serial.println("已释放pClient和WriteCharacteristic");
    }

    // 创建BLE客户端
    pClient = BLEDevice::createClient();   // 需要反初始化
    Serial.println("创建客户端...");
    // 设置客户端回调函数
    pClient->setClientCallbacks(connect_callback);

    Serial.println("创建完成");

    // 连接到远程BLE服务器

    if (pClient->connect(myDevice))
    {
        Serial.println("连接到UUID");

        if (pClient != nullptr)
        {
            if (use_normal_service == FAC)
                pRemoteService = pClient->getService(serviceUUID);
            else if (use_normal_service == CLIENT)
                pRemoteService = pClient->getService(serviceUUIDNormal);
            else
                pRemoteService = pClient->getService(serviceUUIDOTA);
        }
        else
        {
            // pClient 是空指针，处理错误逻辑

            Serial.println("pClient 是空指针");
            candeleteble=true;
            return false;
        }
        if (pRemoteService != nullptr)
        {
            Serial.println("找到我们的服务");

            if (use_normal_service == FAC)
            {
                CAMERAUUIDCharacteristic = pRemoteService->getCharacteristic(CameraUUID);
                NotifyCharacteristic = pRemoteService->getCharacteristic(NotifyUUID);
                WriteCharacteristic = pRemoteService->getCharacteristic(WriteUUID);
            }
            else if (use_normal_service == CLIENT)
            {
                CAMERAUUIDCharacteristic = pRemoteService->getCharacteristic(CameraUUID);
                NotifyCharacteristic = pRemoteService->getCharacteristic(NotifyUUIDNormal);
                WriteCharacteristic = pRemoteService->getCharacteristic(WriteUUIDNormal);
            }
            else
            {
                CAMERAUUIDCharacteristic = pRemoteService->getCharacteristic(CameraUUID);
                NotifyCharacteristic = pRemoteService->getCharacteristic(NotifyUUIDOTA);
                WriteCharacteristic = pRemoteService->getCharacteristic(WriteUUIDOTA);
            }
            if (CAMERAUUIDCharacteristic != nullptr)
            {
                Serial.println("找到我们的摄像头传输特征");

                // 读取特征的值
                if (CAMERAUUIDCharacteristic->canRead())
                {
                    String value = CAMERAUUIDCharacteristic->readValue();

                    Serial.print("摄像头特征值是：");
                    Serial.println(value.c_str());
                }
                else
                {
                    Serial.println("摄像头特征值是不可读的");
                }
                // 注册特征通知回调
                if (CAMERAUUIDCharacteristic->canNotify())
                {
                    Serial.println("注册摄像头特征通知回调");

                    if (pClient->isConnected())
                    {
                        CAMERAUUIDCharacteristic->registerForNotify(cameranotifyCallback);
                    }
                    else
                    {
                        Serial.println("蓝牙还没有连接");
                          candeleteble=true;
                        return false;
                    }
                }
            }
            else
            {
                Serial.print("找不到消息提醒UUID：");
                Serial.println(CameraUUID.toString().c_str());
            }

            if (NotifyCharacteristic != nullptr)
            {
                Serial.println("找到我们的消息提醒特征");
                if (NotifyCharacteristic->canRead()) // 读取特征的值
                {
                    String value = NotifyCharacteristic->readValue();

                    Serial.print("特征值是：");
                    Serial.println(value.c_str());
                }
                else
                {
                    Serial.println("特征值是不可读的");
                }
                // 注册特征通知回调
                if (NotifyCharacteristic->canNotify())
                {
                    Serial.println("注册特征通知回调");

                    if (pClient->isConnected())
                    {
                        NotifyCharacteristic->registerForNotify(notifyCallback);
                        Serial.println("AT+CONNECT_SUCCESS");
                        colorWipe(strip.Color(0, 255, 0));   // 绿色
                    }
                    else
                    {
                        Serial.println("蓝牙没有连接");
                          candeleteble=true;
                        return false;
                    }
                }
            }
            else
            {
                Serial.print("找不到消息提醒UUID：");
                Serial.println(NotifyUUID.toString().c_str());
            }

            if (WriteCharacteristic != nullptr)
            {
                Serial.println("找到我们的写入特征");

                // 读取特征的值
                if (NotifyCharacteristic->canRead())
                {
                    String value = NotifyCharacteristic->readValue();
                    Serial.print("写入特征值是：");
                    Serial.println(value.c_str());
                }
                else
                {
                    Serial.println("写入特征值是不可读的");
                }
                  candeleteble=true;
                // 连接成功
                return true;
            }
            else
            {
                Serial.print("找不到写入UUID：");
                Serial.println(NotifyUUID.toString().c_str());
            }
        }
        else
        {
            Serial.print("找不到服务UUID：");
            Serial.println(serviceUUID.toString().c_str());
        }
        // 连接失败，断开连接
        pClient->disconnect();
    }
    else
    {
        Serial.println("MAC地址连接失败");
    }
   candeleteble=true;
    // 连接失败
    return false;


}
