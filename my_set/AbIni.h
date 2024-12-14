

#ifndef ABINI_H
#define ABINI_H  // Qt库头文件
#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif
#include <qserialport.h>      // 串口通信类
#include <qserialportinfo.h>  // 串口信息类

#include <QAuthenticator>
#include <QDateTime>       // 日期和时间操作
#include <QDebug>          // 调试输出
#include <QDesktopWidget>  // 桌面信息
#include <QDir>            // 目录操作
#include <QFile>           // 文件操作
#include <QFile>
#include <QFileDialog>   // 文件对话框
#include <QInputDialog>  // 用户输入对话框
#include <QLibrary>      // 动态链接库加载
#include <QMainWindow>   // 主窗口类
#include <QMessageBox>   // 消息框
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>         // QObject基类
#include <QSettings>       // 持久设置
#include <QStandardPaths>  // 标准路径
#include <QTextStream>     // 文本输入输出
#include <QTimer>          // 定时器
#include <QUrl>            // URL操作
#include <QUrl>
#include <QWidget>  // QWidget基类
#include <QtConcurrent>

#include "qcombobox.h"
#include "qlineedit.h"

// C标准库头文件
#include <stdbool.h>  // 布尔类型

// 个人头文件
#include <hqmes.h>  // 华勤的mes头文件
#include <jjmes.h>  // 金进的mes头文件
#include <lxmes.h>  // 立讯的mes头文件
#include <qat.h>    // 与esp32的at指令
#include <qjig.h>
#include <qpb.h>  // 与牙刷的pb协议
#include <qproduct.h>
#include <qsetting.h>  //上位机设置界面
#include <qusb.h>      // 与治具的测量协议
#include <xwdmes.h>    // 欣旺达的mes头文件

#define WAITTIME 0  // 指令的300延时防止粘包

#define DEBUG_VER "电刷产测工具   V1.4.5"
#define CAMERA_VER "摄像测试工站   V1.2.9"
#define AGE_VER "老化测试工站   V1.2.6"
#define MOTOR_VER "电机校准工站   V1.2.7"
#define QC_VER "静态电流测试   V1.4.5"
#define SCREEN_VER "屏幕测试工站   V1.2.0"
#define LIGHT_VER "灯光测试工站   V1.2.1"
#define SINGLE_VER "信号测试工站   V1.4.9"
#define FREE_VER "自由测试工站   V1.1.3"
#define IMU_VER "IMU校准工站    V1.5.2"
#define PCBA_VER "电刷板子测试   V1.4.1"
#define PRESSURE_VER "压感校测工站   V1.3.0"

// [PCBA_VER] 伟克森mes添加mac查重功能，增加pb协议通道选择，牙刷日志保存加时间戳，增加清空所有sn功能，简易测试添加sn触发的功能，适应高分辨率屏幕
// [QC_VER] 增加pb协议通道选择，牙刷日志保存加时间戳，适应高分辨率屏幕
// [CAMERA_VER]  摄像头测试允许3路测试治具，增加pb协议通道选择，适应高分辨率屏幕
// [DEBUG_VER] 增加pb协议通道选择，保存的报告全部存到d盘，ota日志压测等稳定性添加，修改nfc读取多读，船运炸弹计数，UI预览清图阻塞式，允许多个版本的固件测试pass，适应高分辨率屏幕
// [MOTOR_VER] 增加pb协议通道选择，适应高分辨率屏幕
// [AGE_VER] 增加pb协议通道选择，适应高分辨率屏幕
// [IMU_VER] 增加pb协议通道选择，适应高分辨率屏幕
// [SCREEN_VER] 增加pb协议通道选择，屏幕测试修复离线获取mac的bug，适应高分辨率屏幕
// [SINGLE_VER] 增加pb协议通道选择，适应高分辨率屏幕
// [LIGHT_VER] 增加pb协议通道选择，适应高分辨率屏幕
// [FREE_VER] 增加pb协议通道选择，修改nfc读取多读，适应高分辨率屏幕
//

// 待整理
#define Y20_Pro_PRESS_TEST  0               // Y20 Pro压感校准

// #define F20_PRESS_TEST      0               // F20压感校准
#define U7_PRESS_TEST       0               // U7压感校准
#define P30_Pro_PRESS_TEST  01              // P30P压感校准

#if Y20_Pro_PRESS_TEST
    #define IS_INDEPENDENT   0   // 每个产品的校准是否是独立的
    #define USE_CALIB_FIXTURE 01
    #define USE_TEST_FIXTURE  0

#elif U7_PRESS_TEST
    #define IS_INDEPENDENT   1   // 每个产品的校准是否是独立的

#elif P30_Pro_PRESS_TEST
    #define IS_INDEPENDENT   1   // 每个产品的校准是否是独立的
    
#endif
// 待整理

#endif  // ABINI_H
