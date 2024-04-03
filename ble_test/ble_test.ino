/**
 * 一个功能丰富的BLE客户端。
 * 串口接收mac连接蓝牙，接收命令转发出去，扫描会亮蓝灯，连接成功灭灯
 * 作者：何宇杰，梁建树
 * 更新时间2023/11/10/22：12
 */

// AT+MAC=ea:cb:3e:cf:00:13
// AT+MAC=F4:12:FA:C4:4C:66
// AT+MAC=F4:12:FA:C5:4C:62
// AT+MAC=F4:12:FA:C5:B6:36
// AT+MAC=3C:84:27:07:A8:D2
// AT+MAC=00:00:00:00:00:00
// AT+MAC=74:4D:BD:95:7D:EA
// AT+MAC=E1:74:07:34:52:F7
// AT+MAC=E2:5D:07:34:3D:F5
// AT+MAC=f5:3d:34:07:5d:e2
// AT+MAC=E1:74:07:34:52:F7
// AT+MAC=DA:46:13:38:0A:F5
// AT+MAC=e2:66:07:34:2d:f7
// AT+MAC=3C:84:27:07:A8:D2
// AT+BLELOG=1
// AT+GMAC

#include "Arduino.h"
#include "BLEDevice.h"
#include <Adafruit_NeoPixel.h>

int cmd_length = 0;

#define log     1   // 扫描日志也在这里面
#define wifiuse 1
int blelogs = 0;   // 蓝牙信号日志1表示默认开

String version = "dongle固件版本1.1.0";   // 默认的版本号

#define packetSize 1024
#define D2_PIN     2
#define RST_PIN    10
#define RGB_PIN    48   // WS2812B数据引脚
#define LED_COUNT  1    // LED数量

Adafruit_NeoPixel strip(LED_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

#if wifiuse == 1
    #include "my_wifi.h"
#endif

int wifistate = 1;
byte cmd[packetSize];      // AT指令的命令内容
byte packet[packetSize];   // 定义用于存储数据包的数组

char targetDeviceAddress[18] = "ea:cb:3e:cf:00:13";   // 历程的地址
static BLEUUID serviceUUID("a6ed0201-d344-460a-8075-b9e8ec90d71b");
static BLEUUID WriteUUID("a6ed0203-d344-460a-8075-b9e8ec90d71b");
static BLEUUID NotifyUUID("a6ed0202-d344-460a-8075-b9e8ec90d71b");

static BLEUUID serviceUUIDOTA("a6ed0101-d344-460a-8075-b9e8ec90d71b");
static BLEUUID WriteUUIDOTA("a6ed0103-d344-460a-8075-b9e8ec90d71b");
static BLEUUID NotifyUUIDOTA("a6ed0103-d344-460a-8075-b9e8ec90d71b");
static BLEUUID serviceUUIDNormal("1828");
static BLEUUID WriteUUIDNormal("2ACA");
static BLEUUID NotifyUUIDNormal("2ACA");

static enum
{
    FAC,
    CLIENT,
    OTA
} use_normal_service = FAC;
static BLEClient *pClient = nullptr;   // 蓝牙客户端的类
BLEClientCallbacks *connect_callback = nullptr;
static boolean doConnect = false;       // 是否可以开始连接
static boolean connected = false;       // 是否是连接的状态
static boolean doScan = false;          // 是否scan完成
static boolean isReceiveOver = false;   // 是否获取完成
static BLEScan *pBLEScan;
static BLERemoteCharacteristic *NotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *WriteCharacteristic = nullptr;
static BLERemoteService *pRemoteService = nullptr;
static BLEAdvertisedDevice *myDevice = nullptr;   // 这个设备要反初始化

enum ReceiveState
{
    IDLE_STATE,
    RECEIVED_A,
    RECEIVED_AT,
    RECEIVED_ATPLUS
};
enum CommandType
{
    MAC,
    WIFI,
    BLE,
    OTA_,
    BLELOG,
    GMAC,
    // 加入其他的at命令
};
// 将所有LED设置为指定颜色
void colorWipe(uint32_t color)
{
    for (int i = 0; i < strip.numPixels(); i++)
    {
        strip.setPixelColor(i, color);
        strip.show();
    }
}
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
        // delay(5000);
    }
};

bool connectToServer()
{
#if log == 1
    Serial.print("正在连接到：");
    Serial.println(targetDeviceAddress);
#endif

    if (pClient != nullptr)
    {
        delete pClient;
        pClient = nullptr;
#if log == 1
        Serial.println("已释放pClient");
#endif
    }

    // 创建BLE客户端
    pClient = BLEDevice::createClient();   // 需要反初始化
#if log == 1
    Serial.println("创建客户端...");
#endif

    // 设置客户端回调函数
    pClient->setClientCallbacks(connect_callback);

#if log == 1
    Serial.println("创建完成");
#endif
    // 连接到远程BLE服务器

    if (pClient->connect(myDevice))
    {
#if log == 1
        Serial.println("连接到UUID");
#endif

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
#if log == 1
            Serial.println("pClient 是空指针");
#endif
            return false;
        }
        if (pRemoteService != nullptr)
        {
#if log == 1
            Serial.println("找到我们的服务");
#endif
            if (use_normal_service == FAC)
            {
                NotifyCharacteristic = pRemoteService->getCharacteristic(NotifyUUID);
                WriteCharacteristic = pRemoteService->getCharacteristic(WriteUUID);
            }
            else if (use_normal_service == CLIENT)
            {
                NotifyCharacteristic = pRemoteService->getCharacteristic(NotifyUUIDNormal);
                WriteCharacteristic = pRemoteService->getCharacteristic(WriteUUIDNormal);
            }
            else
            {
                NotifyCharacteristic = pRemoteService->getCharacteristic(NotifyUUIDOTA);
                WriteCharacteristic = pRemoteService->getCharacteristic(WriteUUIDOTA);
            }

            if (NotifyCharacteristic != nullptr)
            {
#if log == 1
                Serial.println("找到我们的消息提醒特征");
#endif
                // 读取特征的值
                if (NotifyCharacteristic->canRead())
                {
                    std::string value = NotifyCharacteristic->readValue();
#if log == 1
                    Serial.print("特征值是：");
                    Serial.println(value.c_str());
#endif
                }
                else
                {
#if log == 1
                    Serial.println("特征值是不可读的");
#endif
                }
                // 注册特征通知回调
                if (NotifyCharacteristic->canNotify())
                {
#if log == 1
                    Serial.println("注册特征通知回调");
#endif
                    if (pClient->isConnected())
                    {
                        NotifyCharacteristic->registerForNotify(notifyCallback);
                        Serial.println("AT+CONNECT_SUCCESS");
                        colorWipe(strip.Color(0, 255, 0));   // 绿色
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
#if log == 1
                Serial.print("找不到消息提醒UUID：");
                Serial.println(NotifyUUID.toString().c_str());
#endif
            }
            if (WriteCharacteristic != nullptr)
            {
#if log == 1
                Serial.println("找到我们的写入特征");
#endif
                // 读取特征的值
                if (NotifyCharacteristic->canRead())
                {
                    std::string value = NotifyCharacteristic->readValue();
#if log == 1
                    Serial.print("写入特征值是：");
                    Serial.println(value.c_str());
#endif
                }
                else
                {
#if log == 1
                    Serial.println("写入特征值是不可读的");
#endif
                }
                // 连接成功
                return true;
            }
            else
            {
#if log == 1
                Serial.print("找不到写入UUID：");
                Serial.println(NotifyUUID.toString().c_str());
#endif
            }
        }
        else
        {
#if log == 1
            Serial.print("找不到服务UUID：");
            Serial.println(serviceUUID.toString().c_str());
#endif
        }
        // 连接失败，断开连接
        pClient->disconnect();
    }
    else
    {
#if log == 1
        Serial.println("MAC地址连接失败");
#endif
    }
    // 连接失败
    return false;
}

bool isValidMacAddress(const byte *address, size_t length)
{
    // 检查字节数是否为 17
    if (length != 17)
    {
#if log == 1
        Serial.print("长度不对");
#endif

        return false;
    }
    // 检查冒号的位置是否正确
    for (int i = 2; i < 17; i += 3)
    {
        if (address[i] != ':')
        {
#if log == 1
            Serial.print("格式不对");
#endif

            return false;
        }
    }
    // 检查每个字节是否在有效的范围内
    for (size_t i = 0; i < length; i++)
    {
        if (!((address[i] >= '0' && address[i] <= '9') ||
              (address[i] >= 'A' && address[i] <= 'F') ||
              (address[i] >= 'a' && address[i] <= 'f') || (address[i] == ':')))
        {
#if log == 1
            Serial.print("内容不对");
#endif

            return false;
        }
    }
    return true;
}

// AT得到的是去头去尾的
void processATCommand(byte *get_cmd, int length)
{
    CommandType commandType;

#if log == 1
    Serial.print("收到AT命令,命令内容长度：");
    Serial.println(length);
    for (int i = 0; i < length; i++)
        Serial.print(char(get_cmd[i]));   // 打印命令内容
    Serial.println();
#endif

    // 检查命令是否以 "MAC=" 开头
    if (strncmp(reinterpret_cast<char *>(get_cmd), "MAC=", 4) == 0)
    {
#if log == 1
        Serial.printf("receive MAC-command");
#endif
        commandType = MAC;   // 假设 MAC 命令
        length = length - 4;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 4];
        use_normal_service = FAC;
    }

    // 检查命令是否以 "BLE=" 开头
    if (strncmp(reinterpret_cast<char *>(get_cmd), "BLE=", 4) == 0)
    {
#if log == 1
        Serial.printf("receive BLE-command");
#endif
        commandType = BLE;
        length = length - 4;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 4];

        use_normal_service = CLIENT;
    }
    if (strncmp(reinterpret_cast<char *>(get_cmd), "OTA=", 4) == 0)
    {
#if log == 1
        Serial.printf("receive OTA-command");
#endif
        commandType = OTA_;
        length = length - 4;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 4];

        use_normal_service = OTA;
    }
    if (strncmp(reinterpret_cast<char *>(get_cmd), "GMAC", 4) == 0)
    {
#if log == 1
        Serial.println("请求mac地址");
#endif
        commandType = GMAC;   // 假设 WIFI 命令
    }

    if (strncmp(reinterpret_cast<char *>(get_cmd), "BLELOG=", 7) == 0)
    {
#if log == 1
        Serial.printf("receive BLELOG-command");
#endif
        commandType = BLELOG;   // 假设 WIFI 命令
        length = length - 7;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 7];
    }

    // 检查命令是否以 "WIFI=" 开头
    if (strncmp(reinterpret_cast<char *>(get_cmd), "WIFI=", 5) == 0)
    {
#if log == 1
        Serial.printf("receive wifi-command");
#endif
        commandType = WIFI;   // 假设 WIFI 命令
        length = length - 5;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 5];
    }
    switch (commandType)
    {
    case MAC:
    case BLE:
    case OTA_:
        if (isValidMacAddress(get_cmd, length))
        {
            String packetString = "";
            for (int i = 0; i < length; i++)
            {
                packetString += char(get_cmd[i]);
            }
            strcpy(targetDeviceAddress, packetString.c_str());
#if log == 1
            Serial.print("已设置新的目标设备 MAC 地址：");
            Serial.println(targetDeviceAddress);
#endif
            doScan = true;   // 是否完成了scan

            if (connected)
            {
                if (pClient != nullptr)
                {
                    pClient->disconnect();   // 必须先断开连接触发回调函数再初始化回调函数
#if log == 1
                    Serial.println("已断开pClient");
#endif
                }
                if (myDevice != nullptr)
                {
                    delete myDevice;
                    myDevice = nullptr;
                }
            }
        }
        break;

#if wifiuse == 1
    case WIFI:
        if (get_cmd[0] == '1')
        {
            send_img_flag = 1;
            send_video_flag = 0;
    #if log == 1
            Serial.println("prepare to send message info");
    #endif
        }
        else if (get_cmd[0] == '0')
        {
            send_img_flag = 0;
            send_video_flag = 1;
        }
        break;
#endif
    case BLELOG:
        if (get_cmd[0] == '1')
        {
            blelogs = 1;
        }
        else if (get_cmd[0] == '0')
        {
            blelogs = 0;
        }
        break;

    case GMAC:

    {
#if log == 1
        Serial.println("收到mac请求");
#endif
        digitalWrite(RST_PIN, LOW);   // 将 RST_PIN 设置为高电
        delay(100);
        digitalWrite(RST_PIN, HIGH);   // 将 RST_PIN 设置为高电平
    }
    break;

    default:
        break;
    }
}

void processATChar(byte currentChar)
{
    static ReceiveState currentState = IDLE_STATE;
    static int over = 0;
    // 根据当前字符进行状态处理
    // #if log == 1
    //     Serial.print(currentState);
    // #endif
    switch (currentState)
    {
    case IDLE_STATE:

        if (currentChar == 'A')
        {
            currentState = RECEIVED_A;
            cmd_length = 0;
            isReceiveOver = false;
        }
        break;

    case RECEIVED_A:
        if (currentChar == 'T')
        {
            currentState = RECEIVED_AT;
        }
        else
        {
            // 如果不是'T'，回到初始状态
            currentState = IDLE_STATE;
        }
        break;

    case RECEIVED_AT:
        if (currentChar == '+')
        {
            currentState = RECEIVED_ATPLUS;
        }
        else
        {
            // 如果不是'+'，回到初始状态
            currentState = IDLE_STATE;
        }
        break;

    case RECEIVED_ATPLUS:

        if (currentChar == '\r' || over)
        {
            over = 1;
            if (currentChar == '\n')
            {
                isReceiveOver = true;
                currentState = IDLE_STATE;
                over = 0;
            }
        }
        else
        {
            if (cmd_length > 1023)
            {
                currentState = IDLE_STATE;
#if log == 1
                Serial.println("cmd_length 长度超过1024");
#endif
            }

            cmd[cmd_length++] = currentChar;   // 将当前字符添加到正在接收的AT指令中
        }
        break;
    }
}

void serialEvent()
{
    if (Serial.available() > 0)
    {
        int bytesRead = Serial.readBytes(packet, packetSize);

        // #if log == 1
        Serial.print("接收到数据数量：");
        Serial.println(bytesRead);

        //         Serial.print("接收到的内容（十六进制）：");
        //         for (int i = 0; i < bytesRead; i++)
        //         {
        //             Serial.print("0x");
        //             if (packet[i] < 0x10)
        //             {
        //                 Serial.print("0");   // 如果字节小于0x10，补0
        //             }
        //             Serial.print(packet[i], HEX);   // 打印字节的十六进制表示
        //             Serial.print(" ");
        //         }
        //         Serial.println();   // 换行
        // #endif

        // 透传部分
        if (connected)
        {
            WriteCharacteristic->writeValue(packet, bytesRead);
            // unsigned long currentMillis = millis();   // 或者使用 micros() 函数获取微秒级时间戳
            // String timestamp = String(currentMillis);   // 将时间戳转换为字符串
            // Serial.print("send over ");// 打印带有时间戳的消息
            // Serial.println(timestamp);
        }

        // AT指令部分
        for (int i = 0; i < bytesRead; i++)
        {
            processATChar(packet[i]);
        }
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
        digitalWrite(D2_PIN, HIGH);   // 将 D2_PIN 设置为高电平
        std::string deviceName = advertisedDevice.getName();
        std::string deviceAddress = advertisedDevice.getAddress().toString();
        int rssi = advertisedDevice.getRSSI();

#if log == 1
        Serial.print("deviceName:");
        Serial.print(deviceName.c_str());
        Serial.print(", deviceAddress:");
        Serial.print(deviceAddress.c_str());
        Serial.print(", deviceRssi:");
        Serial.println(rssi);
#endif

        if (advertisedDevice.getAddress().equals(BLEAddress(targetDeviceAddress)))
        {                                // mac地址可以，那么准备开始连接
            digitalWrite(D2_PIN, LOW);   // 将 D2_PIN 设置为高电

            // colorWipe(strip.Color(255, 0, 0));  // 红色
            // colorWipe(strip.Color(0, 255, 0));  // 绿色
            // colorWipe(strip.Color(0, 0, 255));  // 蓝色
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);   // 有释放内存
            doConnect = true;                                       // 是否可以开始连接
            doScan = true;                                          // 是否完成了scan
        }
    }
};

void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(5);   // 设置超时

#if wifiuse == 1
    wifi_init();
#endif
    strip.begin();             // 初始化WS2812B
    strip.show();              // 显示初始化状态（全部关灯）
    strip.setBrightness(10);   // 设置亮度为50% （取值范围为0-255）
    if (wifiuse == 0 && log == 0 && blelogs == 0)
        version =
            "dongle固件版本1.1.1(日志全关,没有wif,没有5s延迟重连,蓝牙信号日志默认关)(无wifi正式版)";

    if (wifiuse == 0 && log == 1 && blelogs == 1)
        version =
            "dongle固件版本1.1.1(日志全开,没有wif,没有5s延迟重连,蓝牙信号日志默认开)(无wifi调试版)";

    if (wifiuse == 1 && log == 1 && blelogs == 1)
        version =
            "dongle固件版本1.1.1(日志全开,没有wif,没有5s延迟重连,蓝牙信号日志默认开)(有wifi调试版)";

    if (wifiuse == 1 && log == 0 && blelogs == 0)
        version =
            "dongle固件版本1.1.1(日志全开,没有wif,没有5s延迟重连,蓝牙信号日志默认开)(有wifi正式版)";

    Serial.println(version);

    BLEDevice::init("");
    pinMode(D2_PIN, OUTPUT);             // 将 D2 管脚设置为输出模式
    pinMode(RST_PIN, OUTPUT);            // 将 D2 管脚设置为输出模式
    colorWipe(strip.Color(0, 0, 255));   // 蓝色

    connect_callback = new MyClientCallback();
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());   // 只会运行一次
    // pBLEScan->setInterval(1500);
    // pBLEScan->setWindow(500);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
}

void loop()
{
    // 如果标志“doConnect”为true，则表示我们已经扫描并找到了所需连接的BLE服务器。
    // 现在我们连接到它。一旦连接，我们将设置连接标志为true。
    if (isReceiveOver)   // 命令缓存区有数据
    {
        processATCommand(cmd, cmd_length);
        for (int i = 0; i < cmd_length; i++)
            cmd[i] = 0;
        isReceiveOver = 0;
    }
    if (doConnect == true)
    {
        connectToServer();   // 连接必须在loop里面，不能在回调里面
        doConnect = false;
    }
#if wifiuse == 1

    int numClients = WiFi.softAPgetStationNum();
    if (numClients)
    {
        if (wifistate)
        {
            Serial.println("AT+WIFI_CONNECT_SUCCESS");
            wifistate = 0;
        }

        for (int i = 0; i < numClients; i++)
        {
            Serial.print("AT+WIFI_DATA=");
            wifi_sta_list_t stationList;
            esp_wifi_ap_get_sta_list(&stationList);
            int WIFI_rssi = stationList.sta[i].rssi;
            uint8_t mac[6];
            memcpy(mac, stationList.sta[i].mac, 6);
            for (int i = 0; i < 6; i++)
            {
                Serial.print(mac[i], HEX);
                if (i < 5)
                    Serial.print(":");
            }
            Serial.println(WIFI_rssi);
        }
    }
    else
    {
        if (!wifistate)
        {
            Serial.println("AT+WIFI_DISCONNECT");
            wifistate = 1;
        }
    }
#endif

    // 这里是为了处理连接被断开的问题
    if (connected)
    {
        int ble_rssi = pClient->getRssi();
        if (blelogs)
        {
            Serial.print("AT+BLERSSI=");
            Serial.println(ble_rssi);
        }
    }
    else if (doScan)   // 没有连接且扫描被关闭了
    {
#if log == 1
        Serial.println("开启扫描5s");
#endif

        pBLEScan->start(1, false);           // 扫描10s如果没扫到，可以通过串口打断
        colorWipe(strip.Color(255, 0, 0));   // 红色
    }

    // int len = Udp.parsePacket();
    // // Serial.print("len:"+String(len));
    // if (len) {
    //   int readlen = minimum(data_n - data_read_n, len);
    //   // Serial.println("readlen = " + String(readlen));
    //   Udp.read(u_data + data_read_n, readlen);
    //   // Serial.println("receiving data:" + String(*(u_data + 1)));
    //   // Serial.println(data_n);
    //   // Serial.println(data_read_n);
    //   data_read_n += readlen;
    //   if (data_read_n >= data_n && ondonefunc != nullptr) {
    //     // Serial.println("executing ondonefunc");
    //     ondonefunc();
    //   }
    // }

    // if (millis() > cmdtime + 1000) {
    //   //  Serial.println("Time out");
    //   timeoutfunc();
    // }

    // delay(300);   // 循环之间延迟一秒。
}
