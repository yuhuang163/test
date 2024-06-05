#include "my_wifi.h"

boolean send_img_flag = false;
boolean send_video_flag = false;
int data_n = 0;
int data_read_n = 0;
int image_len = 0;
int image_get_n = 0;
int image_get_time = 0;

uint8_t *u_data;
uint8_t *imagedata;
WiFiUDP Udp;
WiFiServer server(80);

void wifi_init()
{
    // 获取MAC地址
    uint8_t mac[6];
    WiFi.macAddress(mac);

    // 将MAC地址转换为字符串
    char macStr[18];
    sprintf(macStr, "%02X%02X", mac[4], mac[5]);

    // 设置热点名称
    String ssid = "WIFI_TEST_" + String(macStr);
    const char *password = "usmile123";
    Serial.println("wifi名字为" + ssid);
   
      
Serial.print("AT+WIFINAME=");
Serial.println(ssid);

    if (!WiFi.softAP(ssid, password, 1, 0, 10))
    {
        Serial.println("ap设置失败");
        while (1)
            ;
    }

#if log == 1
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
#endif
    server.begin();
#if log == 1
    Serial.println("Server started");
#endif

    if (Udp.begin(MyPort))
    {
#if log == 1
        Serial.println("UDP启动成功");
        Serial.print(WiFi.softAPIP());
        Serial.print(":");
        Serial.println(MyPort);
#endif
    }
    // getImage();
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
    Udp.begin(MyPort);
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
#if log == 1
        Serial.println("fail to receive length!");
#endif

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
#if log == 1
        Serial.println("fail to receive image!");
#endif

        receive_data(2 + minimum(image_len - image_get_n * maxData, maxData), cmd_data, getImage);
        return;
    }
    for (int i = 0; i < data_n - 2; i++)
    {
        imagedata[image_get_n * maxData + i] = u_data[2 + i];   // 将收到的数据读取到imagedata
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
        getImage();   // 继续读取图片
        return;
    }
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.print("getCam ");
    Udp.print(image_get_n);
    Udp.endPacket();
    receive_data(2 + minimum(image_len - image_get_n * maxData, maxData), cmd_data, getImage);
}
