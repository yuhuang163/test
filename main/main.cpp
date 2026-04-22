/**
 * 串口接收mac连接蓝牙，接收命令转发出去，扫描会亮蓝灯，连接成功灭灯
 * 作者：何宇杰
 * 更新时间2023/11/10/22：12
 * C:\Users\heyj\.espressif\tools\xtensa-esp32s3-elf\esp-12.2.0_20230208\xtensa-esp32s3-elf\bin\xtensa-esp32s3-elf-addr2line.exe -pfiaC -e newdongle.elf ADDRESS  0x42013334:0x3fcb1d40 0x420167ae:0x3fcb1d60 0x4200c51d:0x3fcb1dd0 0x4200b457:0x3fcb1df0 0x420110f0:0x3fcb1ed0 0x40383bb6:0x3fcb1ef0
 * ~/.espressif/tools/xtensa-esp32s3-elf/esp-12.2.0_20230208/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-addr2line -pfiaC -e build/newdongle.elf ADDRESS  0x40386d0f:0x3fccfb30 0x403866a2:0x3fccfb50 0x4037647d:0x3fccfb70 0x403764d5:0x3fccfb90 0x4037650a:0x3fccfbb0 0x40388764:0x3fccfbd0 0x40388788:0x3fccfbf0 0x4205a05a:0x3fccfc10 0x4002df25:0x3fccfc30 0x4000c6d5:0x3fccfc50 0x4000c7d1:0x3fccfc70 0x4000c841:0x3fccfc90 0x4000be27:0x3fccfcb0 0x40029487:0x3fccfcd0 0x4206cc17:0x3fccfcf0 0x4206a879:0x3fccfd30 0x40028f01:0x3fccfd50 0x4206f587:0x3fccfd70 0x4000d025:0x3fccfda0 0x4002c4a5:0x3fccfdc0 0x42063d0b:0x3fccfde0 0x40377e73:0x3fccfe00 0x40377fe2:0x3fccfe20 0x40383bb6:0x3fccfe50






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
/**配置区*/
#define wifiuse 1
int blelogs = 0;          // 蓝牙信号日志1表示默认开
int finddevicelogs = 1;   // 蓝牙扫描日志1表示默认开
String version = "1.0.0"; // 默认的版本号
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

  pinMode(D2_PIN, OUTPUT); // 将 D2 管脚设置为输出模式
  pinMode(RST_PIN, INPUT); // 将引脚设置为输入模式，即高阻态
  colorWipe(strip.Color(0, 0, 255)); // 蓝色

  ble_init();
}

void loop()
{

  // LOG_DEBUG("心跳包/r/n");
  // LOG_DEBUG(ble_connected);
  // LOG_DEBUG(ble_scan_over);
  // LOG_DEBUG(doConnect);
  // vTaskDelay(1000); // 循环之间延迟一秒。

  //  checkTaskStatus(serialEventTaskHandle, "串口事件任务");
  //   checkTaskStatus(processDataTaskHandle, "数据处理任务");
  // Serial.printf("Free heap memory: %lu bytes\r\n", ESP.getFreeHeap());
  // Serial.printf("Max allocatable block size: %lu bytes\r\n", ESP.getMaxAllocHeap());

  if (doConnect == true)
  {
    if (connectToServer()) // 连接必须在loop里面，不能在回调里面
    {
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
    doConnect = false;
  }

  if (ble_connected) // 这里是为了处理连接被断开的问题
  {
    print_ble_rssi();
  }
 
 
  if (!doConnect&&!ble_connected&&!ble_scan_over) // 没有连接且扫描被关闭了
  {
    start_ble_scan();
    colorWipe(strip.Color(255, 0, 0)); // 红色
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
