#include "Arduino.h"
#include "BLEDevice.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <vector>
#include <Adafruit_NeoPixel.h> //RGB的灯
#include "freertos/ringbuf.h"

#define maxWifiData 1024
#define MyPort 1024
#define minimum(a, b) (((a) < (b)) ? (a) : (b))
#define D2_PIN 2
#define RST_PIN 10
#define UART_RX_BUFFER_SIZE (2 * 1024)    // 串口接收setRxBufferSize
#define UART_SOLVE_BUFFER_SIZE (16 * 1024) // 串口读取处理一口气最多

#define UART_RING_BUFFER_SIZE (16 * 1024) // 环形队列缓冲区大小
#define MAX_RECEIVE_BUFFER_SIZE 8192      // 设置一个合理的最大缓冲区大小
#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4

// 设置当前日志级别
#define LOG_LEVEL LOG_LEVEL_DEBUG

#define LOG_ERROR(...)                \
    if (LOG_LEVEL >= LOG_LEVEL_ERROR) \
    {                                 \
        Serial.print("ERROR: ");      \
        Serial.println(__VA_ARGS__);  \
    }
#define LOG_WARN(...)                \
    if (LOG_LEVEL >= LOG_LEVEL_WARN) \
    {                                \
        Serial.print("WARN: ");      \
        Serial.println(__VA_ARGS__); \
    }
#define LOG_INFO(...)                \
    if (LOG_LEVEL >= LOG_LEVEL_INFO) \
    {                                \
        Serial.print("INFO: ");      \
        Serial.println(__VA_ARGS__); \
    }
#define LOG_DEBUG(...)                \
    if (LOG_LEVEL >= LOG_LEVEL_DEBUG) \
    {                                 \
        Serial.print(__VA_ARGS__);    \
    }
#define RGB_PIN 48  // WS2812B数据引脚
#define LED_COUNT 1 // LED数量
#define MY_MTU 247  // LED数量

extern int blelogs;        // 蓝牙信号日志1表示默认开
extern int finddevicelogs; // 蓝牙扫描日志1表示默认开
extern int data_n;
extern int data_read_n;
extern int image_len;
extern int image_get_n;
extern int image_get_time;
extern int cmdtime;
extern boolean candeleteble;
extern boolean StartSendOtaData;
extern boolean StartSendmainData;
extern boolean StartBombState;
extern String BOMBdevicename;     // 伤害设备
extern String damageDistance;     // 提取伤害距离
extern String connectionInterval; // 提取连接间隔时间
extern String sendCommand;        // 提取发送指令
extern boolean send_img_flag;
extern boolean send_video_flag;
extern boolean doConnect;            // 是否可以开始连接
extern boolean ble_connected;        // 是否是连接的状态
extern boolean ble_scan_over;        // 是否scan完成
extern char targetDeviceAddress[18]; // 历程的地址
extern bool is_need_reset_adress;
extern RingbufHandle_t ringBuffer;
void cleanupRingBuffer();
size_t bufferRead(byte *data, size_t length);
void bufferWrite(const byte *data, size_t length);

void initRingBuffer();
extern Adafruit_NeoPixel strip;
enum ServiceType
{
    FAC,
    CLIENT,
    OTA,
    MAIN

};
typedef enum
{
    PHY_CHANNEL_INVALID_SEND = 0, // 无效值
    PHY_CHANNEL_FAC,         // 工厂命令通道
    PHY_CHANNEL_APP,         // ota数据通道
    PHY_CHANNEL_MAIN,        // main数据通道

} ext_ble_phy_channel_send_e;
// 声明全局变量
extern ServiceType use_normal_service;
bool ServerStateCheck();
void wifi_init();
void getImage();
void cmd_len();
void cmd_data();
void serialEventTask(void *pvParameters);
void processDataTask(void *pvParameters);
void processATChar(byte currentChar);
bool isValidMacAddress(const byte *address, size_t length);
bool connectTobleServer();
void cameranotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                          size_t length, bool isNotify);
void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                    size_t length, bool isNotify);
extern void (*ondonefunc)();
extern void (*timeoutfunc)();
void ble_init();
void start_ble_scan();
void print_ble_rssi();
void deinit_ble();
void colorWipe(uint32_t color);
void send_ble_data(ext_ble_phy_channel_send_e channel, uint8_t *data, size_t length);
void print_wifi_rssi(int numClients);


