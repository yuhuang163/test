#include "config.h"
char targetDeviceAddress[18] = "ea:cb:3e:cf:00:13";  // 历程的地址
bool is_need_reset_adress = true;
Adafruit_NeoPixel strip(LED_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

#define packetSize 1024
byte cmd[packetSize];      // AT指令的命令内容
byte packet[packetSize];   // 定义用于存储数据包的数组
int cmd_length = 0;
static boolean isReceiveOver = false;   // 是否获取完成
QueueHandle_t xQueue;
enum ReceiveState
{
    IDLE_STATE,
    RECEIVED_A,
    RECEIVED_AT,
    RECEIVED_ATPLUS
};


ServiceType use_normal_service = FAC;


enum CommandType
{
    MAC,
    WIFI,
    BLE,
    OTA_,
    BLELOG,
    BLEDEVICELOG,
    GMAC,
    // 加入其他的at命令
};
bool isValidMacAddress(const byte *address, size_t length)
{
    // 检查字节数是否为 17
    if (length != 17)
    {
        Serial.print("长度不对");
        return false;
    }
    // 检查冒号的位置是否正确
    for (int i = 2; i < 17; i += 3)
    {
        if (address[i] != ':')
        {
            Serial.print("格式不对");
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
            Serial.print("内容不对");
            return false;
        }
    }
    return true;
}

// AT得到的是去头去尾的
void processATCommand(byte *get_cmd, int length)
{
    CommandType commandType;

    Serial.print("收到AT命令,命令内容长度：");
    Serial.println(length);
    Serial.print("内容为：");
    for (int i = 0; i < length; i++)
        Serial.print(char(get_cmd[i]));   // 打印命令内容
    Serial.println();

    // 检查命令是否以 "MAC=" 开头
    if (strncmp(reinterpret_cast<char *>(get_cmd), "MAC=", 4) == 0)
    {
        Serial.printf("receive MAC-command");
        commandType = MAC;   // 假设 MAC 命令
        length = length - 4;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 4];
        use_normal_service = FAC;
    }

    // 检查命令是否以 "BLE=" 开头
    if (strncmp(reinterpret_cast<char *>(get_cmd), "BLE=", 4) == 0)
    {
        Serial.printf("receive BLE-command");
        commandType = BLE;
        length = length - 4;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 4];

        use_normal_service = CLIENT;
    }
    if (strncmp(reinterpret_cast<char *>(get_cmd), "OTA=", 4) == 0)
    {
        Serial.printf("receive OTA-command");
        commandType = OTA_;
        length = length - 4;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 4];

        use_normal_service = OTA;
    }
    if (strncmp(reinterpret_cast<char *>(get_cmd), "GMAC", 4) == 0)
    {
        Serial.println("请求mac地址");
        commandType = GMAC;   // 假设 WIFI 命令
    }

    if (strncmp(reinterpret_cast<char *>(get_cmd), "BLELOG=", 7) == 0)
    {
        Serial.printf("receive BLELOG-command");
        commandType = BLELOG;   // 假设 WIFI 命令
        length = length - 7;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 7];
    }
    if (strncmp(reinterpret_cast<char *>(get_cmd), "BLEDEVICELOG=", 13) == 0)
    {
        Serial.printf("receive BLEDEVICELOG-command");

        commandType = BLEDEVICELOG;   // 假设 WIFI 命令
        length = length - 13;
        for (int i = 0; i < length; i++)
            get_cmd[i] = get_cmd[i + 13];
    }

    // 检查命令是否以 "WIFI=" 开头
    if (strncmp(reinterpret_cast<char *>(get_cmd), "WIFI=", 5) == 0)
    {
        Serial.printf("receive wifi-command");

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
            is_need_reset_adress = false;

            Serial.print("已设置新的目标设备 MAC 地址：");
            Serial.println(targetDeviceAddress);

            doScan = true;   // 是否完成了scan
            if (connected&&candeleteble)   // 连接上了且可以开始连接才会去清空，否则连接中途被删掉就出问题了，导致卡死
            {
               deinit_ble();   // 重置蓝牙
            }
        }
        break;

#if wifiuse == 1
    case WIFI:
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
    case BLEDEVICELOG:
        if (get_cmd[0] == '1')
        {
            finddevicelogs = 1;
        }
        else if (get_cmd[0] == '0')
        {
            finddevicelogs = 0;
        }
        break;

    case GMAC:
    {
        Serial.println("收到mac请求");

        pinMode(RST_PIN, OUTPUT);      // 将 D2 管脚设置为输出模式
        digitalWrite(RST_PIN, HIGH);   // 将 RST_PIN 设置为高电平
        delay(50);
        digitalWrite(RST_PIN, LOW);   // 将 RST_PIN 设置为高电平
        // delay(100);
        // digitalWrite(RST_PIN, HIGH);  // 将 RST_PIN 设置为高电平
        pinMode(RST_PIN, INPUT);
    }
    break;

    default:
        break;
    }
}

void serialEventTask(void *pvParameters)
{
    char inChar;
    while (1)
    {
        while (Serial.available() > 0)
        {
            inChar = (char)Serial.read();
            // Serial.print("存入队列数据: ");
            // Serial.println(inChar);
            xQueueSend(xQueue, &inChar, pdMS_TO_TICKS(10));
        }
        vTaskDelay(1);   // 让出处理器
    }
}

void processATChar(byte currentChar)
{
    static ReceiveState currentState = IDLE_STATE;
    static int over = 0;
    // 根据当前字符进行状态处理
    //
    //     Serial.print(currentState);
    //
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
                //   isReceiveOver = true;
                processATCommand(cmd, cmd_length);
                for (int i = 0; i < cmd_length; i++)
                    cmd[i] = 0;

                currentState = IDLE_STATE;
                over = 0;
            }
        }
        else
        {
            if (cmd_length > 1023)
            {
                currentState = IDLE_STATE;

                Serial.println("cmd_length 长度超过1024");
            }

            cmd[cmd_length++] = currentChar;   // 将当前字符添加到正在接收的AT指令中
        }
        break;
    }
}

void processDataTask(void *pvParameters)
{
    char inChar;
    int bytesRead = 0;
    while (1)
    {
        // portMAX_DELAY会一直阻塞
        // pdMS_TO_TICKS(10)阻塞10ms
        while (xQueueReceive(xQueue, &inChar, pdMS_TO_TICKS(10)) == pdPASS)
        {
            // Serial.print("处理队列数据: ");
            // Serial.println(inChar);
            packet[bytesRead++] = inChar;
        }

        if (connected && bytesRead)
        {
            Serial.println("发送给牙刷");
            send_ble_data(packet, bytesRead);
            // unsigned long currentMillis = millis();   // 或者使用 micros() 函数获取微秒级时间戳
            // String timestamp = String(currentMillis);   // 将时间戳转换为字符串
            // Serial.print("send over ");// 打印带有时间戳的消息
            // Serial.println(timestamp);
        }

        // AT指令部分
        for (int i = 0; i < bytesRead; i++)
        {
            // Serial.println("自己处理");
            processATChar(packet[i]);
        }

        if (bytesRead)
        {
            Serial.print("接收到数据数量：");
            Serial.println(bytesRead);
            bytesRead = 0;
            Serial.println("串口中断运行结束");
        }

        vTaskDelay(10);   // 延时一段时间，避免空转
    }
}