/**
 * 一个功能丰富的BLE客户端。
 * 串口接收mac连接蓝牙，接收命令转发出去。
 * 作者：何宇杰
 * 更新时间2023/11/10/22：12
 */

// AT+MAC=ea:cb:3e:cf:00:13
// AT+MAC=F4:12:FA:C4:4C:66
#include "BLEDevice.h"
#include "Arduino.h"
#define CONFIG_BLUEDROID_ENABLED
#define log 1
// 要连接的设备的MAC地址c0:4e:30:37:16:96
char targetDeviceAddress[18] = "ea:cb:3e:cf:00:13"; // 历程的地
// static   char* targetDeviceAddress = "ea:cb:3e:cf:00:13";//历程的地址
// static  const char* targetDeviceAddress = "48:27:E2:D2:FD:4E";//牙刷的地址
// static  const char* targetDeviceAddress = "C2:4F:02:38:68:F5";//牙刷的地址

// 我们希望连接的远程服务。
static BLEUUID serviceUUID("a6ed0201-d344-460a-8075-b9e8ec90d71b");
// 我们写入的远程服务的特征。
static BLEUUID WriteUUID("a6ed0203-d344-460a-8075-b9e8ec90d71b");
// 我们读取的远程服务的特征。
static BLEUUID NotifyUUID("a6ed0202-d344-460a-8075-b9e8ec90d71b");
// 我们流控的远程服务的特征。
// static BLEUUID FLOWUUID("a6ed0204-d344-460a-8075-b9e8ec90d71b");
#define D2_PIN 2
#define packetSize 1024
BLEClient *pClient = nullptr; // 蓝牙客户端的类
BLEClientCallbacks *connect_callback = nullptr;

static boolean doConnect = false; // 是否可以开始连接
static boolean connected = false; // 是否是连接的状态
static boolean doScan = false;    // 是否scan完成

static BLERemoteCharacteristic *NotifyCharacteristic = nullptr;
static BLERemoteCharacteristic *WriteCharacteristic = nullptr;

unsigned long retryTimeout = 30000;             // 重试超时时间（30秒）
static BLEAdvertisedDevice *myDevice = nullptr; // 这个设备要反初始化

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
  // Add other command types as needed
};
byte cmd[packetSize]; // AT指令的命令内容
int cmd_length = 0;

// 定义用于存储数据包的数组
byte packet[packetSize];

// 消息提醒函数
void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{

  for (size_t i = 0; i < length; ++i)
  {
    Serial.printf("%02X ", pData[i]);
  }

  // // Serial.print("特征通知回调：");
  // // Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  // Serial.print("AT+LENGTH=");
  // Serial.print(length);
  // Serial.print("DATA=");
  // for (size_t i = 0; i < length; ++i)
  // {
  //   Serial.printf("%02X ", pData[i]);
  // }

  // //  Serial.write((char*)pData, length);  // 使用Serial.write逐字节打印
  // Serial.println(); // 添加换行符
}

// 连接状态函数
class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *ppclient)
  {
    Serial.println("AT+CONNECT_SUCCESS");
    pClient->setMTU(247);
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

  connect_callback = new MyClientCallback(); // 释放错了会导致死机
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
        connected = true;
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
#if log == 1
          Serial.println("已经进行断连接");
#endif
        }

        if (connect_callback != nullptr)
        {
          delete connect_callback;
          connect_callback = nullptr;
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

      BLEDevice::getScan()->start(0);

#if log == 1
      Serial.println("没有运行到这");
#endif
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
  case IDLE_STATE:
    if (currentChar == 'A')
    {
      currentState = RECEIVED_A;
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
        currentState = IDLE_STATE;
        over = 0;
      }
    }
    else
    {
      cmd[cmd_length++] = currentChar; // 将当前字符添加到正在接收的AT指令中
    }
    break;
  }
}

void serialEvent()
{
  if (Serial.available() > 0)
  {
    int bytesRead = Serial.readBytes(packet, packetSize);

#if log == 1
    Serial.print("接收到数据数量：");
    Serial.println(bytesRead);

    Serial.print("接收到的内容：");
    for (int i = 0; i < bytesRead; i++)
    {
      Serial.print((char)packet[i]); // 打印字节对应的字符
    }
    Serial.println(); // 换行
#endif

    // 透传部分
    if (connected)
    {
      WriteCharacteristic->writeValue(packet, bytesRead);
    }
    // AT指令部分
    for (int i = 0; i < packetSize; i++)
    {
      processATChar(packet[i]);
    }
  }
}

void setup()
{
  Serial.begin(2000000);
#if log == 1
  Serial.println("开始Arduino BLE客户端应用程序...");
#endif

  BLEDevice::init("");
  pinMode(D2_PIN, OUTPUT); // 将 D2 管脚设置为输出模式
  // 指定我们要进行主动扫描，并启动扫描运行5秒。获取扫描器并设置我们想要使用的回调，以便在检测到新设备时通知我们。
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // 只会运行一次
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
} // 设置结束。

// 这是Arduino的主循环函数。
void loop()
{
  // 如果标志“doConnect”为true，则表示我们已经扫描并找到了所需连接的BLE服务器。
  // 现在我们连接到它。一旦连接，我们将设置连接标志为true。

  if (doConnect == true)
  {
    connectToServer(); // 连接必须在loop里面，不能在回调里面
    doConnect = false;
  }

  // 这里是为了处理连接被断开的问题
  if (connected)
  {
    int rssi = pClient->getRssi();
#if log == 1
    // Serial.print("AT+RSSI=");
    // Serial.println(rssi);
#endif
  }
  else if (doScan)
  { // 没有连接且扫描被关闭了
    Serial.println("AT+DISCONNECT");
    BLEDevice::getScan()->start(0); // 这只是一个示例，在断开连接后重新启动扫描，可能有更好的方法在Arduino中实现。
  }

  if (cmd_length) // 命令缓存区有数据
  {
    processATCommand(cmd, cmd_length);
    cmd_length = 0;
  }
  delay(100); // 循环之间延迟一秒。
} // 循环结束
