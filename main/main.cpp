/**
 * 串口接收mac连接蓝牙，接收命令转发出去，扫描会亮蓝灯，连接成功灭灯
 * 作者：何宇杰
 * 更新时间2023/11/10/22：12
 * C:\Users\heyj\.espressif\tools\xtensa-esp32s3-elf\esp-12.2.0_20230208\xtensa-esp32s3-elf\bin\xtensa-esp32s3-elf-addr2line.exe -pfiaC -e newdongle.elf ADDRESS  0x42013334:0x3fcb1d40 0x420167ae:0x3fcb1d60 0x4200c51d:0x3fcb1dd0 0x4200b457:0x3fcb1df0 0x420110f0:0x3fcb1ed0 0x40383bb6:0x3fcb1ef0
 * ~/.espressif/tools/xtensa-esp32s3-elf/esp-12.2.0_20230208/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-addr2line -pfiaC -e build/newdongle.elf ADDRESS  0x40056ee7:0x3fcd2b10 0x42013662:0x3fcd2b30 0x42013679:0x3fcd2b60 0x42016c71:0x3fcd2b80 0x42017054:0x3fcd2bf0 0x420170a1:0x3fcd2c20 0x420173e3:0x3fcd2c60 0x42014902:0x3fcd2ea0 0x42022191:0x3fcd2ef0 0x42023032:0x3fcd2f10 0x42053232:0x3fcd2f30 0x42053106:0x3fcd2f50 0x40383b81:0x3fcd2f80







 */

// AT+MAC=B4:56:5D:BF:53:71
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
// AT+MAC=C0:C5:31:98:39:B3
// AT+MAC=3C:84:27:07:A8:D2
// AT+MAC=6E:FD:6B:90:36:41
// AT+MAC=00:00:00:00:00:00
// AT+MAC=b4:56:5d:bf:57:9d
// AT+BLELOG=1
// AT+GMAC
// AT+BLEDEVICELOG=1
// AT+BOMB=P20P,-40,10,0008021a0408051001e6
// 设备名字  伤害距离 连接间隔时间 发送指令

#include "Arduino.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "i2c.h"
#if 0
#include "ina236.h"
#endif
#include "adc.h"
/**配置区*/
#define wifiuse 1
int blelogs = 0;          // 蓝牙信号日志1表示默认开
int finddevicelogs = 1;   // 蓝牙扫描日志1表示默认开
int adc_data = 0;         // 1表示打印ADC数据日志
String version = "1.0.9"; // 默认的版本号
int wifistate = 1;
/**配置区*/

// 定义任务句柄
TaskHandle_t serialEventTaskHandle = NULL;
TaskHandle_t processDataTaskHandle = NULL;
// 将所有LED设置为指定颜色
void colorWipe(uint32_t color)
{
  for (int i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, color);
    strip.show();
  }
}
void checkTaskStatus(TaskHandle_t taskHandle, const char *taskName)
{
  if (taskHandle != NULL)
  {
    eTaskState state = eTaskGetState(taskHandle);
    Serial.print(taskName);
    Serial.print("的状态: ");

    switch (state)
    {
    case eRunning:
      Serial.println("正在运行");
      break;
    case eReady:
      Serial.println("准备好运行");
      break;
    case eBlocked:
      Serial.println("等待运行");
      break;
    case eSuspended:
      Serial.println("已挂起");
      break;
    case eDeleted:
      Serial.println("已删除");
      break;
    default:
      Serial.println("状态未知");
      break;
    }
  }
  else
  {
    Serial.print(taskName);
    Serial.println(" 句柄无效");
  }
}
void setup()
{
  Serial.setRxBufferSize(UART_RX_BUFFER_SIZE); // 设置缓存
  Serial.begin(921600);
  delay(1); // 给串口硬件一个最小稳定时间，尽早进入接收流程

  initRingBuffer();
  otaBleTxInit();

  // 串口任务尽早创建，确保上电后第一时间接收并处理数据
  xTaskCreate(
      serialEventTask,          // 任务函数
      "Serial Event Task",      // 任务名称
      4 * 1024,                 // 堆栈大小
      NULL,                     // 任务参数
      configMAX_PRIORITIES - 1, // 任务优先级
      &serialEventTaskHandle    // 任务句柄
  );

  xTaskCreate(
      processDataTask,          // 任务函数
      "Process Data Task",      // 任务名称
      24 * 1024,                // 堆栈大小
      NULL,                     // 任务参数
      configMAX_PRIORITIES - 1, // 任务优先级
      &processDataTaskHandle    // 任务句柄
  );

  // 检查任务句柄
  if (serialEventTaskHandle != NULL)
  {
    Serial.println("串口接收任务创建完成");
  }
  else
  {
    Serial.println("串口接收任务创建失败");
  }

  if (processDataTaskHandle != NULL)
  {
    Serial.println("数据处理任务创建完成");
  }
  else
  {
    Serial.println("数据处理任务创建失败");
  }

  Serial.print("wifi设置");
  Serial.println(wifiuse);

#if wifiuse == 1
  wifi_init();
#endif
  strip.begin();           // 初始化WS2812B
  strip.show();            // 显示初始化状态（全部关灯）
  strip.setBrightness(10); // 设置亮度为50% （取值范围为0-255）

  Serial.print("AT+DONGLEVER=");
  Serial.println(version);

  pinMode(RST_PIN, INPUT); // 将引脚设置为输入模式，即高阻态
  ADCDriver::init();
#if 0
  INA236Driver::init();
#endif
  // pinMode(WATER_PUMP_PIN, OUTPUT);
  // pinMode(VALVE_PIN, OUTPUT);
  // digitalWrite(WATER_PUMP_PIN, HIGH);
  // digitalWrite(VALVE_PIN, HIGH);
  colorWipe(strip.Color(0, 0, 255)); // 蓝色

  ble_init();
  esp_log_level_set("gpio", ESP_LOG_NONE);
  // 初始化所有I2C
  I2CDriver::init_all();

  // 扫描所有I2C总线
  I2CDriver::scan_all();

}

void loop()
{

  // LOG_DEBUG("心跳包/r/n");
  // vTaskDelay(1000); // 循环之间延迟一秒。

  //  checkTaskStatus(serialEventTaskHandle, "串口事件任务");
  //   checkTaskStatus(processDataTaskHandle, "数据处理任务");
  // Serial.printf("Free heap memory: %lu bytes\r\n", ESP.getFreeHeap());
  // Serial.printf("Max allocatable block size: %lu bytes\r\n", ESP.getMaxAllocHeap());

  switch (get_ble_state())
  {
  case BLE_SCAN_FOUND:
  case BLE_CONNECTING:
    set_ble_state(BLE_CONNECTING);
    if (connectTobleServer()) // 连接必须在loop里面，不能在回调里面
    {
      set_ble_state(BLE_CONNECTED);
      Serial.println("AT+CONNECT_SUCCESS");
      colorWipe(strip.Color(0, 255, 0)); // 绿色
      if (StartBombState)
      {
        size_t length = sendCommand.length() / 2;
        uint8_t data[length];
        for (size_t i = 0; i < length; ++i)
        {
          String byteString = sendCommand.substring(2 * i, 2 * i + 2);
          data[i] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
        }
        send_ble_data(PHY_CHANNEL_FAC,data, length);
        Serial.println("已发送船运");
      }
    }
    else if (get_ble_connect_mode() == CONNECT_BY_SCAN)
    {
      clear_ble_scan_device();
      set_ble_state(BLE_SCANNING);
    }
    else
    {
      set_ble_state(BLE_IDLE);
    }
    break;

  case BLE_CONNECTED:
    print_ble_rssi();
    break;

  case BLE_SCANNING:
    start_ble_scan();
    colorWipe(strip.Color(255, 0, 0)); // 红色
    break;

  case BLE_IDLE:
  case BLE_DISCONNECTING:
  default:
    break;
  }

  I2CDriver::read_and_print_three_pressures();
  ADCDriver::read_and_print();

#if wifiuse == 1

  int numClients = WiFi.softAPgetStationNum();
  if (numClients)
  {
    if (wifistate)
    {
      Serial.println("AT+WIFI_CONNECT_SUCCESS");
      wifistate = 0;
    }

      print_wifi_rssi(numClients);
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
}
