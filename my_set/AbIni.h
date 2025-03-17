

#ifndef ABINI_H
#define ABINI_H  // Qt库头文件
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
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

#define DEBUG_VER "电刷产测工具   V1.5.6"
#define CAMERA_VER "摄像测试工站   V1.3.8"
#define AGE_VER "老化测试工站   V1.3.6"
#define MOTOR_VER "电机校准工站   V1.3.4"
#define QC_VER "静态电流测试   V1.5.2"
#define SCREEN_VER "屏幕测试工站   V1.2.4"
#define LIGHT_VER "灯光测试工站   V1.2.2"
#define SINGLE_VER "信号测试工站   V1.5.4"
#define FREE_VER "自由测试工站   V1.1.5"
#define IMU_VER "IMU校准工站    V1.5.5"
#define PCBA_VER "电刷板子测试   V1.5.1"
#define PRESSURE_VER "压感校测工站   V1.3.8"


// [DEBUG_VER] 添加ota加密解密的log
//
// [IMU_VER] 增加无mes厂进船运的功能

// [SCREEN_VER] 增加日志
// [SINGLE_VER] 修复wifi无响应问题
// [PRESSURE_VER] 增加日志，调整校准砝码
//

// [DEBUG_VER] 移除ttl音频功能（部分用户会闪退）修复u7ota的兼容问题，关闭加密通信当正常连接的时候，ota密钥数字显示修复
// [PRESSURE_VER] 增加压感校准震荡限制，ndt校准不良原因打印，修改不同设备的结构，默认参数选择产品自动配置出现
// [AGE_VER] 移除电量检测
// [PCBA_VER] 适配q20 141的工作电流测试
//

// [CAMERA_VER] 摄像头测试重复获取设备信息 
// [PCBA_VER] mes上报内容修改，y20机械压感测试
//

// [SCREEN_VER] 修改subpid读取方式为从文件获取

// [SINGLE_VER] 修改subpid读取方式为从文件获取
//

// [PRESSURE_VER] 添加日志显示，修复变量命名错误
// [PCBA_VER] 电机测试需要关闭p20ps
// [DEBUG_VER] 增加北美的wifiota和bleota
//


// [DEBUG_VER] 添加成功和失败的ota显示，修复关闭不退出的问题
//
// [QC_VER] 修复设备外设信息多次获取问题
// [AGE_VER] 给足开机时间
// [SCREEN_VER] 给足开机时间
// [MOTOR_VER] 修复mac比对功能
//

// [AGE_VER] 修改版本判断比对方式
// [CAMERA_VER] 修改版本判断比对方式
// [FREE_VER] 增加绑定功能
// [MOTOR_VER] 修改版本判断比对方式
// [PCBA_VER] 修改版本判断比对方式
// [QC_VER] 修改版本判断比对方式
// [SINGLE_VER]  修改版本判断比对方式
// [PRESSURE_VER] 添加日志显示
//

// [QC_VER] 静态电流显示异常
// [AGE_VER] 适配正常手柄的设备skuid
//

// [SINGLE_VER] 修复充电电流表格显示异常问题，设置每次打开上位机都显示tab的第一页
// [SCREEN_VER] 设置每次打开上位机都显示tab的第一页
// [QC_VER] 设置每次打开上位机都显示tab的第一页，优化发指令的效率
// [PRESSURE_VER] 做y20po和y25s的压感适配，添加新的双通道算法。
// [PCBA_VER] 设置每次打开上位机都显示tab的第一页，增加对无mes的工厂适配
// [MOTOR_VER] 设置每次打开上位机都显示tab的第一页
// [IMU_VER] 设置每次打开上位机都显示tab的第一页
// [FREE_VER]  设置每次打开上位机都显示tab的第一页
// [CAMERA_VER] 设置每次打开上位机都显示tab的第一页,增加对q30的适配，以及配网反初始化，测试完成重启手柄，wifi在总设置里面配置，测试的时候会自动加载wif设备名字密码ip端口，不用重启上位机


// [PCBA_VER] 适配新的电机测试功能
// [DEBUG_VER] 增加v3的ai，允许语音输入控制牙刷。支持ui预览多组动图传输
//

// [PRESSURE_VER] 做y20po和y25s的压感治具适配
// [DEBUG_VER] 增加音频预览的python脚本，以及适配一般手柄的subpid
// [CAMERA_VER] 开放脏污测试的图片测试时间，适配Q20P的摄像头测试
//


// 修复静态电流配置失败的问题
// [DEBUG_VER] 添加音频文件的批量处理，以及音频文件发送，wifi测试在主页面配置
// [CAMERA_VER]  删除冗余代码
// [AGE_VER] 可以勾选是否写入skuid功能
// [PCBA_VER] 伟克森mes添加对版本的管控
// [PRESSURE_VER] 合入压感上位机，添加压感配置内容到上位机配置页面
// [QC_VER] 
// [MOTOR_VER] 电机校准测试过程获取设备的mac地址
//


// 

// [IMU_VER] 修改p30p为y25se，适配P20PS

// [SCREEN_VER] 添加电脑的名字存日志，增加锁定mes的功能，本地打印时间，允许重新获取压感信息一次

// [LIGHT_VER] 添加电脑的名字存日志，增加锁定mes的功能，本地打印时间
// [FREE_VER] 添加电脑的名字存日志，增加锁定mes的功能，本地打印时间
//

#endif  // ABINI_H
