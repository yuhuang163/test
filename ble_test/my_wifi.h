#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <vector>
#include <esp_wifi.h>
#include "Arduino.h"
#define maxData 1024
#define MyPort 1024
#define minimum(a, b) (((a) < (b)) ? (a) : (b))

extern int data_n ;
extern int data_read_n ;
extern int image_len ;
extern int image_get_n ;
extern int image_get_time ;
extern int cmdtime ;
extern  boolean send_img_flag;
extern  boolean send_video_flag;


void wifi_init();
void getImage();
void cmd_len();
void cmd_data();
extern  void (*ondonefunc)();
extern void (*timeoutfunc)();

