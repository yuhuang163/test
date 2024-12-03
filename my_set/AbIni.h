

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

#define DEBUG_VER "电刷产测工具   V1.4.6"
#define CAMERA_VER "摄像测试工站   V1.3.0"
#define AGE_VER "老化测试工站   V1.2.8"
#define MOTOR_VER "电机校准工站   V1.2.9"
#define QC_VER "静态电流测试   V1.4.6"
#define SCREEN_VER "屏幕测试工站   V1.2.1"
#define LIGHT_VER "灯光测试工站   V1.2.2"
#define SINGLE_VER "信号测试工站   V1.5.0"
#define FREE_VER "自由测试工站   V1.1.4"
#define IMU_VER "IMU校准工站    V1.5.4"
#define PCBA_VER "电刷板子测试   V1.4.2"


// [MOTOR_VER] 允许配置是否测这个基础信息项和外设状态项
// [AGE_VER] 允许配置是否测这个基础信息项和外设状态项
// [SINGLE_VER] 允许配置是否测这个基础信息项和外设状态项
// [PCBA_VER] 允许配置是否测这个基础信息项和外设状态项
// [QC_VER] 允许配置是否测这个基础信息项和外设状态项
//
// [CAMERA_VER]  添加电脑的名字存日志，增加锁定mes的功能，本地打印时间
// [DEBUG_VER] 添加电脑的名字存日志，增加锁定mes的功能，本地打印时间，修改摄像头变量命名，ui预览传输日志和计时，摄像头测试允许断开服务器
// [IMU_VER] 修改p30p为y25se，适配P20PS 

// [SCREEN_VER] 添加电脑的名字存日志，增加锁定mes的功能，本地打印时间，允许重新获取压感信息一次

// [LIGHT_VER] 添加电脑的名字存日志，增加锁定mes的功能，本地打印时间
// [FREE_VER] 添加电脑的名字存日志，增加锁定mes的功能，本地打印时间
//

#endif  // ABINI_H
