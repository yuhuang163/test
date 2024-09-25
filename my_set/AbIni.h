

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
#include <qusb.h>    // 与治具的测量协议
#include <xwdmes.h>  // 欣旺达的mes头文件

#define WAITTIME 0  // 指令的300延时防止粘包
#define NEW_XWD_QUIESCENT_CURRENT

#define SETTING_NAME "上位机设置.ini"

#define DEBUG_VER "电刷产测工具   V1.3.5"
#define CAMERA_VER "摄像测试工站   V1.1.9"
#define AGE_VER "老化测试工站   V1.1.8"
#define MOTOR_VER "电机校准工站   V1.1.8"
#define QC_VER "静态电流测试   V1.3.6"
#define SCREEN_VER "屏幕测试工站   V1.1.5"
#define LIGHT_VER "灯光测试工站   V1.1.5"
#define SINGLE_VER "信号测试工站   V1.3.3"
#define FREE_VER "自由测试工站   V1.0.8"
#define IMU_VER "IMU校准工站    V1.4.5"
#define PCBA_VER "电刷板子测试   V1.3.4"

// [DEBUG_VER] f20摄像头支持ip自动搜索选择
// [QC_VER] 支持多牙刷版本测试
// [PCBA_VER] 支持木星复位牙刷
// [MOTOR_VER] 取消mac地址错误弹窗
// [IMU_VER] 支持适配博士的imu芯片
// [CAMERA_VER] 摄像头测试黑点bug解决
// 

// [IMU_VER] 增加测试要求的日志保存
// [AGE_VER] 增加测试要求的日志保存


// [SCREEN_VER] 增加测试要求的日志保存
// [LIGHT_VER] 增加测试要求的日志保存
// [SINGLE_VER] 增加测试要求的日志保存
// [FREE_VER] 增加测试要求的日志保存

//

#endif  // ABINI_H
