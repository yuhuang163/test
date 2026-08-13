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
#define ADC_PIN 2                        // ADC1_CH1 (ADC1: GPIO1~GPIO10)
#define ADC_BITS 12
#define ADC_FULL_SCALE 4095.0f           // Dmax, 12bit 单次读取最大值
#define ADC_VMAX_MV 3100                 // Vmax, ADC_ATTEN_DB_11 可测 0~3100mV
#define ADC_VREF_V 2.048f                // 原理图运放输出设计基准(非芯片 Vref)
#define ADC_VOLTAGE_OFFSET_MV 40           // 读数偏低补偿(mV)，偏高改为负数
// INA180 低侧电流采样 (VCOM: -0.2V~30V, SR: 4V/us)
#define CURRENT_SHUNT_OHM 0.02f          // 采样电阻 20mΩ
#define CURRENT_AMP_GAIN 50.0f           // 放大倍数
#define CURRENT_AMP_INPUT_OFFSET_V 0.0005f // 输入失调 500µV(MAX)
#define LS_CURRENT_MIN_A 0.25f           // 量程 0.25A
#define LS_CURRENT_MAX_A 1.6f            // 量程 1.6A

#define RST_PIN 10
#define WATER_PUMP_PIN 41
#define VALVE_PIN 42
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
#define BLE_MTU_DEFAULT 247
#define BLE_MTU_MIN 23
#define BLE_MTU_MAX 1000
#define OTA_BLE_PACKET_SIZE_DEFAULT 242        // 默认满包长度
#define OTA_BLE_PACKET_BUF_SIZE (BLE_MTU_MAX - 3)
#define OTA_BLE_PACKET_SIZE_MIN 1
#define OTA_TX_BUFFER_SIZE (16 * 1024)         // OTA 发送累积缓冲
#define OTA_TX_FLUSH_TIMEOUT_MS 4             // 未满包时最大等待时间(ms)，超时发余包
#define OTA_TX_TASK_POLL_MS 1                  // OTA 发送任务轮询间隔(ms)
#define OTA_UART_READ_WAIT_MS 1                // OTA 模式环形缓冲读取等待(ms)
#define OTA_TX_BURST_MAX 6                     // 控制器有槽位时连续发送上限
#define OTA_TX_TASK_STACK_SIZE (4 * 1024)
#define OTA_TX_TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define OTA_LOG_UART_RX_TOTAL 0 // 1=打印串口累计「处理总数」
#define OTA_LOG_BLE_TX 0        // 1=打印「OTA_BLE_TX len=...」

extern int suction_data;        // 1表示打印传感器数据日志
extern uint8_t suction_osr_level; // 吸力采样档位 1~4，默认4(8192X/16ms)
extern int hsadc_data;          // 1表示高量程ADC采集日志
extern int lsadc_data;          // 1表示打印低量程(Modbus)采样日志
extern int blelogs;        // 蓝牙信号日志1表示默认开
extern int finddevicelogs; // 蓝牙扫描日志1表示默认开
extern int data_n;
extern int data_read_n;
extern int image_len;
extern int image_get_n;
extern int image_get_time;
extern int cmdtime;
extern boolean StartSendOtaData;
extern uint16_t bleMtuSize;
extern uint16_t otaBlePacketSize;
extern boolean StartSendmainData;
extern boolean StartBombState;
extern String BOMBdevicename;     // 伤害设备
extern String damageDistance;     // 提取伤害距离
extern String connectionInterval; // 提取连接间隔时间
extern String sendCommand;        // 提取发送指令
extern boolean send_img_flag;
extern boolean send_video_flag;
extern char targetDeviceAddress[18]; // 历程的地址
extern bool is_need_reset_adress;
extern RingbufHandle_t ringBuffer;
void cleanupRingBuffer();
size_t bufferRead(byte *data, size_t length);
size_t bufferReadOta(byte *data, size_t length);
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
enum BleConnectMode
{
    CONNECT_BY_SCAN,
    CONNECT_DIRECT
};
enum BleState
{
    BLE_IDLE,
    BLE_SCANNING,
    BLE_SCAN_FOUND,
    BLE_CONNECTING,
    BLE_CONNECTED,
    BLE_DISCONNECTING
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
BleState get_ble_state();
BleConnectMode get_ble_connect_mode();
bool is_ble_connected();
void set_ble_connect_mode(BleConnectMode mode);
void set_ble_state(BleState state);
void cameranotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                          size_t length, bool isNotify);
void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                    size_t length, bool isNotify);
extern void (*ondonefunc)();
extern void (*timeoutfunc)();
void ble_init();
void start_ble_scan();
void print_ble_rssi();
void deinit_ble(BleState nextState = BLE_IDLE);
void clear_ble_scan_device();
void ble_request_connect(BleConnectMode mode); // 登记连接请求，由 loop() 状态机应用
bool ble_process_pending_connect();            // loop() 安全点应用请求，返回是否已处理
void colorWipe(uint32_t color);
void send_ble_data(ext_ble_phy_channel_send_e channel, uint8_t *data, size_t length);
void otaBleTxInit(void);
void otaBleReset(void);
void otaBleFeed(const uint8_t *data, size_t length);
bool bleSetMtu(uint16_t mtu);
uint16_t bleMtuPayloadMax(void);
bool otaBleSetPacketSize(uint16_t size);
void print_wifi_rssi(int numClients);


