

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
#include <qpb.h>  // 与设备的pb协议
#include <qproduct.h>
#include <qsetting.h>  //上位机设置界面
#include <qusb.h>      // 与治具的测量协议
#include <xwdmes.h>    // 欣旺达的mes头文件

#define WAITTIME 0  // 指令的300延时防止粘包

#define DEBUG_VER "产品产测工具   V1.6.0"
#define CAMERA_VER "摄像测试工站   V1.3.8"
#define AGE_VER "老化测试工站   V1.3.7"
#define MOTOR_VER "电机校准工站   V1.3.4"
#define QC_VER "静态电流测试   V1.5.3"
#define SUCTION_VER "吸力测试工站   V1.0.0"
#define KEY_VER "按键测试工站   V1.0.0"
#define SCREEN_VER "屏幕测试工站   V1.2.4"
#define LIGHT_VER "灯光测试工站   V1.2.2"
#define SINGLE_VER "信号测试工站   V1.5.5"
#define FREE_VER "自由测试工站   V1.1.14"
#define IMU_VER "IMU校准工站    V1.5.5"
#define PCBA_VER "产品板子测试   V1.5.2"
#define PRESSURE_VER "压感校测工站   V1.3.9"

// V3Pro2604290383   这个是过程条码
// V3P0R1913G00260328000011
// V3P0DFA110001D5D1FMB63P00007
// AIR1123456C29BA91BMB5A700501  这个是pcba的sn

// [PCBA_VER] 增加治具错误码，打印船运电流，只有q系列才会进纯享一开机的时候,立讯增加错误码，mac——sn保存路径设置，y20po改成y20ps，做y30p的适配
// [QC_VER] 增加y20ps，y30，y30s，按键开机时长，mes多上传软件版本，轴压感卡控,立讯增加错误码，mac——sn保存路径设置，y20po改成y20ps，做y30p的适配
// [PRESSURE_VER] 摆幅测试调整，所有指令加换行，修复摆幅测试没勾选发指令的问题,立讯增加错误码，mac——sn保存路径设置，y20po改成y20ps，做y30p的适配
// [AGE_VER] 取消移除p20p的电量检测,立讯增加错误码，mac——sn保存路径设置，y20po改成y20ps，做y30p的适配
// [SINGLE_VER] 轴压感卡控,立讯增加错误码，mac——sn保存路径设置，y20po改成y20ps，做y30p的适配
// [DEBUG_VER] 搜索设备添加下拉列表，ota压测，添加勾选的互斥，取消不互斥，一机一密多上传产品key来获取秘钥,立讯增加错误码，mac——sn保存路径设置，y20po改成y20ps，做y30p的适配
//

// [DEBUG_VER] 添加轴压感版本
// [SCREEN_VER] 添加轴压感版本
// [PCBA_VER] 添加轴压感版本
//
// [IMU_VER] 增加无mes厂进船运的功能

// [SCREEN_VER] 增加日志
// [SINGLE_VER] 修复wifi无响应问题
// [PRESSURE_VER] 摆幅测试完成开发
//

// [DEBUG_VER] 移除ttl音频功能（部分用户会闪退）修复u7ota的兼容问题，关闭加密通信当正常连接的时候，ota密钥数字显示修复
// [PRESSURE_VER] 增加压感校准震荡限制，ndt校准不良原因打印，修改不同设备的结构，默认参数选择产品自动配置出现
// [AGE_VER] 移除电量检测

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
// [DEBUG_VER] 增加v3的ai，允许语音输入控制设备。支持ui预览多组动图传输
//

// [PRESSURE_VER] 做y20po和y25s的压感治具适配
// [DEBUG_VER] 增加音频预览的python脚本，以及适配一般手柄的subpid
// [CAMERA_VER] 开放脏污测试的图片测试时间，适配Q20P的摄像头测试
//


// 修复静态电流配置失败的问题
// [DEBUG_VER] 添加音频文件的批量处理，以及音频文件发送，wifi测试在主页面配置
// [DEBUG_VER] 设置第二页增加产品串口仪器 BrushInstrument 测包/PER 与超时项
// [DEBUG_VER] 设置第二页增加 SYSTEM/ProtocolType（qpb、qfctp）下拉
// [DEBUG_VER] 测试流程可选区按产品/治具/连接/三元组 Tab 分类展示
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
// [FREE_VER] 仪器流程六连复位步骤，测试项 id 顺延
// [FREE_VER] 仪器流程六连停止接收与PER，非信令项 id 顺延
// [FREE_VER] 停止接收应答改长期信号槽，处理放 qfreework_data.cpp
// [FREE_VER] dongle 断开时仍推进已开始步骤的收尾，避免卡在产品串口异步步
// [FREE_VER] dongle 未连时队列当前步为产品串口仪器亦跑状态机，避免步间卡死
// [FREE_VER] MES 电量项拆 BATTERY_INFO 与 BATTERY_PERCENT，testData 仅数字
// [FREE_VER] 并联CMW六步播放(id111～116)，GPRF仅首次初始化、PER步内GPRF可选关 BleBrushCmwOnStopPer
//

#endif  // ABINI_H
