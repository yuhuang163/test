#include "config.h"

char targetDeviceAddress[18] = "00:00:00:00:00:00"; // 历程的地址
bool is_need_reset_adress = true;
Adafruit_NeoPixel strip(LED_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);
String BOMBdevicename;     // 伤害设备
String damageDistance;     // 提取伤害距离
String connectionInterval; // 提取连接间隔时间
String sendCommand;        // 提取发送指令

#define AT_PACKET_SIZE 1 * 1024
byte cmd[AT_PACKET_SIZE]; // AT指令的命令内容
int cmd_length = 0;
static boolean isReceiveOver = false; // 是否获取完成

int uartreceivesize = 0;

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
    MAIN_,
    BLELOG,
    BLEDEVICELOG,
    GMAC,
    BOMB,
    OTADATA,
    MAINDATA,
    DCON,
    // 加入其他的at命令
};
bool isValidMacAddress(const byte *address, size_t length)
{
    // 检查字节数是否为 17
    if (length != 17)
    {
        Serial.println("MAC长度不对");
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

// 默认设置 useNormalService 为 FAC
struct CommandInfo
{
    const char *prefix; // 前缀
    size_t prefixLength;
    CommandType commandType;
    ServiceType useNormalService = FAC; // 默认值为 FAC
};

CommandType handleCommand(byte *get_cmd, int &length, const CommandInfo &commandInfo)
{
    Serial.printf("前缀 %s\n", commandInfo.prefix);
    length -= commandInfo.prefixLength;

    // 移动命令数据去掉前缀部分
    memmove(get_cmd, get_cmd + commandInfo.prefixLength, length);

    use_normal_service = commandInfo.useNormalService;
    return commandInfo.commandType;
}

void processATCommand(byte *get_cmd, int length)
{
    CommandType commandType;
    StartBombState = false;

    Serial.print("收到AT命令,命令内容长度：");
    Serial.println(length);
    Serial.print("内容为：");
    for (int i = 0; i < length; i++)
        Serial.print(char(get_cmd[i])); // 打印命令内容
    Serial.println();

    CommandInfo commands[] = {
        {"MAC=", 4, MAC, FAC},               // 使用显式指定的 FAC
        {"BLE=", 4, BLE, CLIENT},            // 使用显式指定的 CLIENT
        {"OTA=", 4, OTA_, OTA},              // 使用显式指定的 OTA
        {"MAIN=", 5, MAIN_, MAIN},           // 使用显式指定的 MAIN
        {"DCON=", 5, DCON, FAC},          // FAC 通道）
        {"GMAC", 4, GMAC},                   // 默认使用 FAC
        {"BOMB=", 5, BOMB},                  // 默认使用 FAC
        {"BLELOG=", 7, BLELOG},              // 默认使用 FAC
        {"OTADATA=", 8, OTADATA},            // 默认使用 FAC
        {"MAINDATA=", 9, MAINDATA},          // 默认使用 FAC
        {"BLEDEVICELOG=", 13, BLEDEVICELOG}, // 默认使用 FAC
        {"WIFI=", 5, WIFI}                   // 默认使用 FAC
    };

    size_t commandCount = sizeof(commands) / sizeof(commands[0]);
    bool matched = false;

    for (size_t i = 0; i < commandCount; ++i)
    {
        if (strncmp(reinterpret_cast<char *>(get_cmd), commands[i].prefix, commands[i].prefixLength) == 0)
        {
            commandType = handleCommand(get_cmd, length, commands[i]);
            matched = true;
            break;
        }
    }

    if (!matched)
    {
        Serial.println("Unknown command");
        return;
    }

    switch (commandType)
    {
    case MAC:
    case MAIN_:
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

            ble_scan_over = false;
            Serial.printf("状态重置后: doConnect=%d, ble_connected=%d, ble_scan_over=%d\r\n",
                doConnect, ble_connected, ble_scan_over);
            if (ble_connected && candeleteble)
            {
                deinit_ble(); // 重置蓝牙
            }
        }
        break;

    case DCON:
        if (isValidMacAddress(get_cmd, length))
        {
            String packetString = "";
            for (int i = 0; i < length; i++)
            {
                packetString += char(get_cmd[i]);
            }
            strcpy(targetDeviceAddress, packetString.c_str());
            is_need_reset_adress = false;

            Serial.print("AT+DCON 直连目标 MAC：");
            Serial.println(targetDeviceAddress);

            // 不触发后台扫描，直接走 loop 内 connectTobleServer() 的直连分支
            ble_scan_over = true;

            // 直连命令只切换状态，不在此线程直接操作 BLEScan 结果容器，
            // 避免与 GAP 回调线程并发访问导致崩溃。
            doConnect = true;
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
        blelogs = (get_cmd[0] == '1') ? 1 : 0;
        break;

    case BLEDEVICELOG:
        finddevicelogs = (get_cmd[0] == '1') ? 1 : 0;
        break;

    case OTADATA:
        uartreceivesize = 0;
        StartSendOtaData = (get_cmd[0] == '1') ? 1 : 0;
        break;

    case MAINDATA:
        StartSendmainData = (get_cmd[0] == '1') ? 1 : 0;
        break;

    case GMAC:
        Serial.println("收到mac请求");
        pinMode(RST_PIN, OUTPUT);
        digitalWrite(RST_PIN, HIGH);
        delay(50);
        digitalWrite(RST_PIN, LOW);
        pinMode(RST_PIN, INPUT);
        break;

    case BOMB:
    {
        Serial.println("开始配置炸弹模式");
        String packetString = "";
        for (int i = 0; i < length; i++)
        {
            packetString += char(get_cmd[i]);
        }

        int firstComma = packetString.indexOf(',');
        int secondComma = packetString.indexOf(',', firstComma + 1);
        int thirdComma = packetString.indexOf(',', secondComma + 1);

        BOMBdevicename = packetString.substring(0, firstComma);
        damageDistance = packetString.substring(firstComma + 1, secondComma);
        connectionInterval = packetString.substring(secondComma + 1, thirdComma);
        sendCommand = packetString.substring(thirdComma + 1);

        Serial.println("伤害设备名字: " + BOMBdevicename);
        Serial.println("伤害距离: " + damageDistance);
        Serial.println("连接间隔时间: " + connectionInterval);
        Serial.println("发送指令: " + sendCommand);
        ble_scan_over = false;
        StartBombState = true;
        break;
    }

    default:
        break;
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
            if (cmd_length > AT_PACKET_SIZE)
            {
                currentState = IDLE_STATE;
                Serial.print("cmd_length 长度超过");
                Serial.println(AT_PACKET_SIZE);
            }

            cmd[cmd_length++] = currentChar; // 将当前字符添加到正在接收的AT指令中
        }
        break;
    }
}

void serialEventTask(void *pvParameters)
{
    while (1)
    {
        size_t availableBytes = Serial.available();
        size_t bufferSize = availableBytes; // 确定实际缓冲区大小

        if (bufferSize > 0)
        {
            // uartreceivesize = uartreceivesize + availableBytes;
            // Serial.print("接收总数");
            // Serial.println(uartreceivesize);
            byte *tempBuffer = new byte[bufferSize];
            size_t bytesRead = Serial.readBytes(tempBuffer, bufferSize);

            if (bytesRead > 0)
            {
                bufferWrite(tempBuffer, bytesRead); // 将数据写入环形缓冲区
            }
            delete[] tempBuffer; // 释放动态分配的内存
        }
    
        vTaskDelay(5); // 让出处理器
    }
    Serial.print("serialEventTask线程退出");
}
#define EXT_UART_MAGIC 0xCCCCCCCCCCCCCCCC // 0xAAAAAAAAAAAAAAAA
#define UART_PHY_LAYER_HEAD_SIZE 10       // 头大小
#define UART_PHY_LAYER_HEADER_ADN_CRC (UART_PHY_LAYER_HEAD_SIZE)
#define EXT_UART_MAGIC_SIZE 8       // 头大小

typedef struct
{
    uint64_t magic;
    uint8_t length;
    uint8_t channel;
    // uint8_t index;
    uint8_t data[0];
} ext_uart_phy_layer_t;

int ext_ble_find_next_frame(uint8_t *data, size_t *dataSize)
{

    // 包头的64位值
    const uint64_t header = 0xCCCCCCCCCCCCCCCC;
   if (*dataSize < EXT_UART_MAGIC_SIZE || *dataSize - EXT_UART_MAGIC_SIZE > 2 * 1024)
    {
        Serial0.print("ext_uart1_find_next_frame有重大错误");
        Serial0.print("*dataSize - EXT_UART_MAGIC_SIZE=");
        Serial0.println(*dataSize - EXT_UART_MAGIC_SIZE);
        Serial0.print("*dataSize=");
        Serial0.println(*dataSize);
        return 0;
    }
    // 遍历数据流以查找包头
    for (size_t i = 0; i <= *dataSize - EXT_UART_MAGIC_SIZE; ++i)
    {
        uint64_t currentHeader;
        memcpy(&currentHeader, &data[i], EXT_UART_MAGIC_SIZE);

        if (currentHeader == header)
        {
            // 找到包头，移除包头前的数据
            size_t remainingSize = *dataSize - i;
            memmove(data, &data[i], remainingSize);
            *dataSize = remainingSize;
            return 1; // 返回去除包头后的数据长度
        }
    }
   *dataSize = 0;
    return 0; // 没有找到包头
}
void processDataTask(void *pvParameters)
{
    byte packet[UART_SOLVE_BUFFER_SIZE];
    byte pbpacket[1024];
    size_t pboffset = 0;
    int uartsolvesize = 0;
    while (1)
    {
        size_t packetSize = bufferRead(packet, sizeof(packet)); // 从环形缓冲区读取数据

        if (packetSize > 0)
        {
            if (StartSendOtaData)
            {
                send_ble_data(PHY_CHANNEL_INVALID_SEND, packet, packetSize); // 发送ota数据包
                // uartsolvesize = uartsolvesize + packetSize;
                // Serial.print("处理总数");
                // Serial.println(uartsolvesize);
            }
            else
            {
                memcpy(pbpacket + pboffset, packet, packetSize);
                pboffset += packetSize;
                while (1) // 处理各种pb命令
                {

                    if (pboffset <= UART_PHY_LAYER_HEADER_ADN_CRC)
                    {
                        break;
                    }

                    ext_uart_phy_layer_t *head = (ext_uart_phy_layer_t *)pbpacket;

                    if (head->magic == EXT_UART_MAGIC)
                    {
                        int frame_size = UART_PHY_LAYER_HEADER_ADN_CRC + head->length;
                        if (frame_size > pboffset)
                        {
                            break;
                        }
                        ext_ble_phy_channel_send_e channel = (ext_ble_phy_channel_send_e)head->channel;

                        send_ble_data(channel, pbpacket + UART_PHY_LAYER_HEAD_SIZE, head->length);
                        pboffset = pboffset - frame_size;
                        memmove(pbpacket, &pbpacket[frame_size], pboffset);
                        Serial.print("偏移为");
                        Serial.println(pboffset);
                        continue;
                    }
                    else
                    {
                        if (ext_ble_find_next_frame(pbpacket, &pboffset))
                        {
                            continue;
                        }
                        else
                        {
                            Serial.println("找不到帧头");
                            break;
                        }
                    }
                }
            }
            for (size_t i = 0; i < packetSize; ++i)
            {
                processATChar((char)packet[i]);
            }
            // Serial.print("处理掉数据大小1：");
            // Serial.println(packetSize);
            packetSize = 0;
        }

        vTaskDelay(1); // 延时一段时间，避免空转
    }
    Serial.print("processDataTask线程退出");
}
