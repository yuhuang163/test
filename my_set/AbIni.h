

#ifndef ABINI_H
#define ABINI_H   // Qt库头文件
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
#include <QtConcurrent>
#include "qcombobox.h"
#include "qlineedit.h"
#include <QAuthenticator>
#include <QDateTime>        // 日期和时间操作
#include <QDebug>           // 调试输出
#include <QDesktopWidget>   // 桌面信息
#include <QDir>             // 目录操作
#include <QFile>            // 文件操作
#include <QFile>
#include <QFileDialog>    // 文件对话框
#include <QInputDialog>   // 用户输入对话框
#include <QLibrary>       // 动态链接库加载
#include <QMainWindow>    // 主窗口类
#include <QMessageBox>    // 消息框
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>          // QObject基类
#include <QSettings>        // 持久设置
#include <QStandardPaths>   // 标准路径
#include <QTextStream>      // 文本输入输出
#include <QTimer>           // 定时器
#include <QUrl>             // URL操作
#include <QUrl>
#include <QWidget>             // QWidget基类
#include <qserialport.h>       // 串口通信类
#include <qserialportinfo.h>   // 串口信息类


// C标准库头文件
#include <stdbool.h>   // 布尔类型

// 个人头文件
#include <hqmes.h>   // 华勤的mes头文件
#include <jjmes.h>   // 金进的mes头文件
#include <lxmes.h>   // 立讯的mes头文件
#include <qat.h>     // 与esp32的at指令
#include <qjig.h>
#include <qpb.h>   // 与牙刷的pb协议
#include <qproduct.h>
#include <qusb.h>     // 与治具的测量协议
#include <xwdmes.h>   // 欣旺达的mes头文件

#define WAITTIME 0   // 指令的300延时防止粘包
#define NEW_XWD_QUIESCENT_CURRENT
#define NEW_MUSIC_CURRENT

#define SETTING_NAME "上位机设置.ini"

#define DEBUG_VER  "电刷产测工具   V1.2.1"        // 支持1拖多
#define AGE_VER    "老化测试工站   V1.0.9"            // 支持1拖多
#define MOTOR_VER  "电机校准工站   V1.0.9"            // 加定时器判断休
#define QC_VER     "静态电流测试   V1.2.7"        // 增加定时器判断休眠
#define SCREEN_VER "屏幕测试工站   V1.0.7"        // 加定时器判断休
#define LIGHT_VER  "灯光测试工站   V1.0.7"        // 加定时器判断休
#define SINGLE_VER "信号测试工站   V1.2.4"        // 立讯bug
#define FREE_VER   "自由测试工站   V1.0.0"        // 立讯bug
#define PCBA_VER   "电刷板子测试   V1.2.3"      // 加定时器判断休
#define CAMERA_VER "摄像测试工站   V1.0.6"      // 支持摄像头工站
#define IMU_VER    "IMU校准工站    V1.3.0"         // 支持自动化治具

// [DEBUG_VER] 修改生成的采测的xls文件的名字
// [IMU_VER] 适配欣旺达的六轴新治具
// [AGE_VER] 增加了自动化测试功能
// [CAMERA_VER] 更新了摄像头测试工站的图像处理算法

// [MOTOR_VER] 调整了电机校准的定时器判断逻辑
// [QC_VER] 增加了静态电流测试的定时器判断休眠功能
// [SCREEN_VER] 修复了屏幕测试工站的显示异常问题
// [LIGHT_VER] 更新了灯光测试工站的亮度调节功能
// [SINGLE_VER] 修复了信号测试工站的bug
// [FREE_VER] 解决了自由测试工站的bug
// [PCBA_VER] 优化了电刷PCBA测试的定时器处理逻辑

#endif   // ABINI_H
