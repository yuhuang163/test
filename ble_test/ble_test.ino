/**
 * 串口接收mac连接蓝牙，接收命令转发出去，扫描会亮蓝灯，连接成功灭灯
 * 作者：何宇杰
 * 更新时间2023/11/10/22：12
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
#include "Arduino.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**配置区*/
#define wifiuse 1
int blelogs = 0;           // 蓝牙信号日志1表示默认开
int finddevicelogs = 1;    // 蓝牙扫描日志1表示默认开
String version = "1.2.1";  // 默认的版本号
int wifistate = 1;
/**配置区*/



// 将所有LED设置为指定颜色
void colorWipe(uint32_t color) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5);  // 设置超时
  Serial.println("");
#if wifiuse == 1
  wifi_init();
#endif
  strip.begin();            // 初始化WS2812B
  strip.show();             // 显示初始化状态（全部关灯）
  strip.setBrightness(10);  // 设置亮度为50% （取值范围为0-255）

  Serial.print("dongle固件版本");
  Serial.println(version);

  Serial.print("AT+DONGLEVER=");
  Serial.println(version);

  pinMode(D2_PIN, OUTPUT);  // 将 D2 管脚设置为输出模式
  pinMode(RST_PIN, INPUT);  // 将引脚设置为输入模式，即高阻态

  xQueue = xQueueCreate(1024, sizeof(char));  // 创建一个队列，存储最多 10 个字符

  xTaskCreate(serialEventTask, "Serial Event Task", 2048, NULL, 1, NULL);
  xTaskCreate(processDataTask, "Process Data Task", 2048, NULL, 1, NULL);
  colorWipe(strip.Color(0, 0, 255));  // 蓝色

  ble_init();
}

void loop() {
  
  // LOG_DEBUG("心跳包\r\n");
  // LOG_DEBUG(connected);
  // LOG_DEBUG(doScan);
  // LOG_DEBUG(doConnect);

  if (doConnect == true) {
    connectToServer();  // 连接必须在loop里面，不能在回调里面
    doConnect = false;
  }

  // 这里是为了处理连接被断开的问题
  if (connected) {
    print_ble_rssi();
  } else if (doScan)  // 没有连接且扫描被关闭了
  {
    start_ble_scan();
    colorWipe(strip.Color(255, 0, 0));  // 红色
  }

  delay(1000);  // 循环之间延迟一秒。

#if wifiuse == 1

  int numClients = WiFi.softAPgetStationNum();
  if (numClients) {
    if (wifistate) {
      Serial.println("AT+WIFI_CONNECT_SUCCESS");
      wifistate = 0;
    }

    for (int i = 0; i < numClients; i++) {
      Serial.print("AT+WIFI_DATA=");
      wifi_sta_list_t stationList;
      esp_wifi_ap_get_sta_list(&stationList);
      int WIFI_rssi = stationList.sta[i].rssi;
      uint8_t mac[6];
      memcpy(mac, stationList.sta[i].mac, 6);
      for (int i = 0; i < 6; i++) {
        Serial.print(mac[i], HEX);
        if (i < 5)
          Serial.print(":");
      }
      Serial.println(WIFI_rssi);
    }
  } else {
    if (!wifistate) {
      Serial.println("AT+WIFI_DISCONNECT");
      wifistate = 1;
    }
  }
#endif
}
