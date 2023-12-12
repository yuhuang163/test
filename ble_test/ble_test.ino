/**
 * 一个功能丰富的BLE客户端。
 * 串口接收mac连接蓝牙，接收命令转发出去。
 * 作者：何宇杰
 * 更新时间2023/11/10/22：12
 */

// AT+MAC=ea:cb:3e:cf:00:13
// AT+MAC=F4:12:FA:C4:4C:66
// AT+MAC=F4:12:FA:C5:4C:62
// AT+MAC=F4:12:FA:C5:B6:36
// AT+MAC=f4:12:fa:c5:51:c6

#include "BLEDevice.h"
#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <vector>
#include <esp_wifi.h>
#define CONFIG_BLUEDROID_ENABLED // 配置使能
#define log 1
#define D2_PIN 2
#define packetSize 1024
#define LED_BUILTIN 2 // Set the GPIO pin where you connected your test LED or comment this line out if your dev board has a built-in LED
#define maxData 1024
#define Port 1024
#define minimum(a, b) (((a) < (b)) ? (a) : (b))

int cmd_length = 0;
int data_n = 0;
int data_read_n = 0;
uint8_t *u_data;
uint8_t *imagedata;
int image_len = 0;
int image_get_n = 0;
int image_get_time = 0;
int cmdtime = 0;

byte cmd[packetSize];    // AT指令的命令内容
byte packet[packetSize]; // 定义用于存储数据包的数组

const char *ssid = "usmile_test";
const char *password = "usmile123";
char targetDeviceAddress[18] = "ea:cb:3e:cf:00:13"; // 历程的地址
// 我们希望连接的远程服务。
static BLEUUID serviceUUID("a6ed0201-d344-460a-8075-b9e8ec90d71b");
// 我们写入的远程服务的特征。
static BLEUUID WriteUUID("a6ed0203-d344-460a-8075-b9e8ec90d71b");
// 我们读取的远程服务的特征。
static BLEUUID NotifyUUID("a6ed0202-d344-460a-8075-b9e8ec90d71b");

BLEClient *pClient = nullptr; // 蓝牙客户端的类
BLEClientCallbacks *connect_callback = nullptr;

static boolean doConnect = false; // 是否可以开始连接
static boolean connected = false; // 是否是连接的状态
static boolean doScan = false;    // 是否scan完成
static boolean send_img_flag = false;
static boolean send_video_flag = false;
static boolean isReceiveOver = false; // 是否获取完成

BLEScan *pBLEScan;
static BLERemoteCharacteristic *NotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *WriteCharacteristic = nullptr;
static BLEAdvertisedDevice *myDevice = nullptr; // 这个设备要反初始化
WiFiUDP Udp;
WiFiServer server(80);

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
  // Add other command types as needed
};

void getImage();
void cmd_len();
void cmd_data();
void (*ondonefunc)();
void (*timeoutfunc)();

// 消息提醒函数
void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
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
    Serial.println("AT+CONNECT_SUCCESS");
  }
  void onDisconnect(BLEClient *ppclient)
  {
    connected = false;
    Serial.println("AT+DISCONNECT");
  }
};

bool connectToServer()
{
#if log == 1
  Serial.print("正在连接到：");
  Serial.println(targetDeviceAddress);
#endif

  // 创建BLE客户端
  pClient = BLEDevice::createClient();
#if log == 1
  Serial.println("创建客户端...");
#endif

  // 设置客户端回调函数
  pClient->setClientCallbacks(connect_callback);

#if log == 1
  Serial.println("创建完成");
#endif
  // 连接到远程BLE服务器
  if (pClient->connect(BLEAddress(targetDeviceAddress)))
  {
#if log == 1
    Serial.println("连接到服务器");
#endif

    // 获取远程BLE服务器中我们感兴趣的服务的引用
    BLERemoteService *pRemoteService = pClient->getService(serviceUUID); // 获取服务
    if (pRemoteService != nullptr)
    {
#if log == 1
      Serial.println("找到我们的服务");
#endif

      // 获取远程BLE服务器服务中特征的引用
      NotifyCharacteristic = pRemoteService->getCharacteristic(NotifyUUID);
      WriteCharacteristic = pRemoteService->getCharacteristic(WriteUUID);
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

          NotifyCharacteristic->registerForNotify(notifyCallback);
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

    std::string deviceAddress = advertisedDevice.getAddress().toString();
    std::string deviceName = advertisedDevice.getName();
#if log == 1
    Serial.print("发现设备：");
    Serial.print(deviceName.c_str());
    Serial.print(", MAC地址：");
    Serial.println(deviceAddress.c_str());
#endif

    if (advertisedDevice.getAddress().equals(BLEAddress(targetDeviceAddress)))
    { // mac地址可以，那么准备开始连接
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice); // 有释放内存
      doConnect = true;                                     // 是否可以开始连接
      doScan = true;                                        // 是否完成了scan
    }                                                       // 找到我们的服务器
  }
};

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
    if (!((address[i] >= '0' && address[i] <= '9') || (address[i] >= 'A' && address[i] <= 'F') || (address[i] >= 'a' && address[i] <= 'f') || (address[i] == ':')))
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
    Serial.print(char(get_cmd[i])); // 打印命令内容
  Serial.println();
#endif

  // 检查命令是否以 "MAC=" 开头
  if (strncmp(reinterpret_cast<char *>(get_cmd), "MAC=", 4) == 0)
  {
    commandType = MAC; // 假设 MAC 命令
    length = length - 4;
    for (int i = 0; i < length; i++)
      get_cmd[i] = get_cmd[i + 4];
  }
  // 检查命令是否以 "WIFI=" 开头
  if (strncmp(reinterpret_cast<char *>(get_cmd), "WIFI=", 5) == 0)
  {
    Serial.printf("receive wifi-command");
    commandType = WIFI; // 假设 MAC 命令
    length = length - 5;
    for (int i = 0; i < length; i++)
      get_cmd[i] = get_cmd[i + 5];
  }

  switch (commandType)
  {
  case MAC:
    if (isValidMacAddress(get_cmd, length))
    {
      if (connected)
      {

        if (pClient != nullptr)
        {
          pClient->disconnect(); // 必须先断开连接触发回调函数再初始化回调函数
        }
        if (myDevice != nullptr)
        {
          delete myDevice;
          myDevice = nullptr;
        }
      }

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
      pBLEScan->start(5, false); // 扫描10s如果没扫到，可以通过串口打断
    }
    break;

  case WIFI:
    // Serial.printf(String(get_cmd[0]));
    if (get_cmd[0] == '1')
    {
      send_img_flag = 1;
      send_video_flag = 0;
      Serial.println("prepare to send message info");
    }
    else if (get_cmd[0] == '0')
    {
      send_img_flag = 0;
      send_video_flag = 1;
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
  switch (currentState)
  {
    Serial.print(currentState);
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
        currentState = IDLE_STATE;
      cmd[cmd_length++] = currentChar; // 将当前字符添加到正在接收的AT指令中
    }
    break;
  }
}

void serialEvent()
{
  Serial.setTimeout(1);
  if (Serial.available() > 0)
  {
    int bytesRead = Serial.readBytes(packet, packetSize);

#if log == 1
    Serial.print("接收到数据数量：");
    Serial.println(bytesRead);

    Serial.print("接收到的内容（十六进制）：");
    for (int i = 0; i < bytesRead; i++)
    {
      Serial.print("0x");
      if (packet[i] < 0x10)
      {
        Serial.print("0"); // 如果字节小于0x10，补0
      }
      Serial.print(packet[i], HEX); // 打印字节的十六进制表示
      Serial.print(" ");
    }
    Serial.println(); // 换行
#endif

    // 透传部分
    if (connected)
    { // 0x00 0x08 0x05 0x3A 0x0A 0x0A 0x08 0x08 0x02 0x22 0x04 0x35 0x34 0x34 0x35 0x67 //0x00 0x08 0x06 0x32 0x06 0x0A 0x04 0x08 0x02
      WriteCharacteristic->writeValue(packet, bytesRead);
    }
    // AT指令部分
    for (int i = 0; i < bytesRead; i++)
    {
      processATChar(packet[i]);
    }
  }
}

void construct_and_send_packet_with_CRC16(uint8_t *pData, size_t length)
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

void receive_data(int n, void (*donef)(), void (*timeoutf)())
{
  // 接收指定长度数据
  ondonefunc = donef;
  timeoutfunc = timeoutf;
  cmdtime = millis();
  data_n = n;
  data_read_n = 0;
  if (u_data != nullptr)
    delete[] u_data;
  u_data = new uint8_t[n];
}

void getImage()
{
  // Serial.println("getImage is running!");
  Udp.begin(Port);
  image_get_time = millis();
  // Serial.print(WiFi.softAPIP());
  // Serial.println(Udp.remoteIP());
  // Serial.println(Udp.remotePort());
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.print("getCam");
  Udp.endPacket();
  receive_data(3, cmd_len, getImage);
}

void cmd_len()
{
  // 收到数据 [0x00, 0xXX, 0xXX]
  //          命令  数据1  数据2
  // Serial.println("cmd_len is running!");
  if (u_data[0] != 0x00 || (u_data[1] == 0 && u_data[2] == 0))
  {
    image_get_n = 0;

    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.print("getCam");
    Udp.endPacket();
    Serial.println("fail to receive length!");
    receive_data(3, cmd_len, getImage);
    return;
  }
  image_len = (uint16_t)u_data[1] * 256 + u_data[2];
  // Serial.println("image_len:" + String(image_len));
  image_get_n = 0;
  if (imagedata != nullptr)
    delete[] imagedata;
  imagedata = new uint8_t[image_len];
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.print("getCam ");
  Udp.print(image_get_n);
  Udp.endPacket();
  receive_data(2 + minimum(image_len - image_get_n * maxData, maxData), cmd_data, getImage);
}

void cmd_data()
{
  // 收到数据 [0x01, 0xXX, 0xXX, 0xXX, ...... 0xXX]
  //          命令   编号  数据
  // Serial.println("cmd_data is running!");
  if (u_data[1] != image_get_n)
  {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.print("getCam ");
    Udp.print(image_get_n);
    Udp.endPacket();
    Serial.println("fail to receive image!");
    receive_data(2 + minimum(image_len - image_get_n * maxData, maxData), cmd_data, getImage);
    return;
  }
  for (int i = 0; i < data_n - 2; i++)
  {
    imagedata[image_get_n * maxData + i] = u_data[2 + i]; // 将收到的数据读取到imagedata
  }

  image_get_n++;
  if (image_get_n * maxData >= image_len)
  {
    // 数据读取完成
    // uint8_t u_data1[3] = { 1, 2, 3 };
    if (send_img_flag == true)
    {
      construct_and_send_packet_with_CRC16(imagedata, sizeof(imagedata));
      send_img_flag = 0;
    }
    else if (send_video_flag == true)
    {
      construct_and_send_packet_with_CRC16(imagedata, sizeof(imagedata));
    }
#if log == 1
    Serial.print("读取完成 耗时");
    Serial.print(millis() - image_get_time);
    Serial.print("ms ( ");
    Serial.print((float)1000 / (float)(millis() - image_get_time));
    Serial.println(" fps)");
#endif
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.print("finished");
    Udp.endPacket();
    getImage(); // 继续读取图片
    return;
  }
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.print("getCam ");
  Udp.print(image_get_n);
  Udp.endPacket();
  receive_data(2 + minimum(image_len - image_get_n * maxData, maxData), cmd_data, getImage);
}

void wifi_init()
{
  if (!WiFi.softAP(ssid, password))
  {
    log_e("Soft AP creation failed.");
    while (1)
      ;
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();

  Serial.println("Server started");
  if (Udp.begin(Port))
  {
    Serial.println("UDP启动成功");
    Serial.print(WiFi.softAPIP());
    Serial.print(":");
    Serial.println(Port);
  }
  getImage();
}

void setup()
{
  Serial.begin(115200);
  wifi_init();
#if log == 1
  Serial.println("开始Arduino BLE客户端应用程序...");
#endif

  BLEDevice::init("");
  // pinMode(D2_PIN, OUTPUT); // 将 D2 管脚设置为输出模式
  //  指定我们要进行主动扫描，并启动扫描运行5秒。获取扫描器并设置我们想要使用的回调，以便在检测到新设备时通知我们。
  connect_callback = new MyClientCallback(); // 释放错了会导致死机
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // 只会运行一次
  pBLEScan->setInterval(1500);                                               // 设置扫描间隔为1500*0.625ms = 937.5ms。扫描间隔是指相邻的两次扫描之间的时间间隔。
  pBLEScan->setWindow(500);                                                  // 设置扫描窗口为500*0.625ms = 312.5ms。扫描窗口是指在一个扫描间隔内，设备实际进行扫描的时间。//每937.5ms内，花312.5ms时间进行BLE扫描，剩余625ms处于空闲状态。这种设置可以减少扫描过程中花费的功耗
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false); // 非阻塞
} // 设置结束。

// 这是Arduino的主循环函数。
void loop()
{
  // 如果标志“doConnect”为true，则表示我们已经扫描并找到了所需连接的BLE服务器。
  // 现在我们连接到它。一旦连接，我们将设置连接标志为true。
  if (isReceiveOver) // 命令缓存区有数据
  {
    processATCommand(cmd, cmd_length);
    for (int i = 0; i < cmd_length; i++)
    cmd[i] = 0;
    isReceiveOver = 0;
  }
  if (doConnect == true)
  {
    connectToServer(); // 连接必须在loop里面，不能在回调里面
    doConnect = false;
  }










  int numClients = WiFi.softAPgetStationNum();
  if (numClients)
  {
    for (int i = 0; i < numClients; i++)
    {
      wifi_sta_list_t stationList;
      esp_wifi_ap_get_sta_list(&stationList);
      int WIFI_rssi = stationList.sta[i].rssi;
      Serial.print("AT+WIFIRSSI=");
      Serial.println(WIFI_rssi);

#if log == 1

      uint8_t mac[6];
      memcpy(mac, stationList.sta[i].mac, 6);
      Serial.print("连接数");
      Serial.print(i + 1);
      Serial.print(" MAC: ");
      for (int i = 0; i < 6; i++)
      {
        Serial.print(mac[i], HEX);
        if (i < 5)
          Serial.print(":");
      }
       Serial.println();
#endif
    }

 

  }







  // 这里是为了处理连接被断开的问题
  if (connected)
  {
    int ble_rssi = pClient->getRssi();
    Serial.print("AT+BLERSSI=");
    Serial.println(ble_rssi);
  }
  else if (doScan) // 没有连接且扫描被关闭了
  {
    Serial.println("开启扫描5s");
    pBLEScan->start(5, false); // 扫描10s如果没扫到，可以通过串口打断
  }

  int len = Udp.parsePacket();
  // Serial.print("len:"+String(len));
  if (len)
  {
    int readlen = minimum(data_n - data_read_n, len);
    // Serial.println("readlen = " + String(readlen));
    Udp.read(u_data + data_read_n, readlen);
    // Serial.println("receiving data:" + String(*(u_data + 1)));
    // Serial.println(data_n);
    // Serial.println(data_read_n);
    data_read_n += readlen;
    if (data_read_n >= data_n && ondonefunc != nullptr)
    {
      // Serial.println("executing ondonefunc");
      ondonefunc();
    }
  }

  if (millis() > cmdtime + 1000)
  {
    //  Serial.println("Time out");
    timeoutfunc();
  }

  delay(100); // 循环之间延迟一秒。
} // 循环结束
