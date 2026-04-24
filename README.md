# New Product Test Framework (Qt)
基于模块化与分层设计的多工站产测上位机工程框架。

## 目录结构

```text
new_product_test/
├── new_production.pro                    ← Qt工程入口（模块与编译配置）
├── main.cpp                              ← 程序启动入口
├── mainwindow.h/.cpp/.ui                 ← 主界面与全局交互入口
├── mainlogic.cpp                         ← 主流程调度与业务联动
├── 上位机设置.ini                         ← 运行时配置（协议/阈值/开关）
├── new_production.qrc                    ← 资源索引（图片/QSS等）
├── factory_analyzer.h/.cpp/.ui           ← 分析与诊断页面
├── djitestfunction.cpp                   ← 测试辅助函数
│
├── docs/                                 ← 文档说明与改造记录
│   ├── 代码开发流程.md                    ← 开发流程规范
│   ├── 上位机版本发布.md                  ← 版本发布流程
│   ├── 协议适配.md                        ← 协议适配说明
│   └── 协议适配改造方案.md                 ← 协议改造设计文档
│
├── my_set/                               ← 工程公共定义与聚合头
│   ├── AbIni.h                           ← 公共头聚合/版本宏/全局依赖
│   └── my_typedef.h                      ← 全局类型定义
│
├── qlog/                                 ← 日志与测试记录模块
│   └── qlog.h/.cpp                       ← 日志保存、CSV记录等
│
├── agreement/                            ← 协议层与外设通信封装
│   ├── qProtocol/                        ← 统一协议抽象与协议管理
│   │   ├── qprotocol.h/.cpp              ← 协议接口基类
│   │   ├── qprotocolmanager.h/.cpp       ← 协议选择/切换/统一调用
│   │   ├── qprotocol_types.h             ← 协议类型定义
│   │   ├── qpb/                          ← PB协议实现与生成文件
│   │   │   ├── qpb.h/.cpp                ← PB协议主实现
│   │   │   ├── ble_protocol/*.pb.h       ← BLE协议生成头
│   │   │   ├── factory_protocol/*.pb.h   ← 工厂协议生成头
│   │   │   ├── pb*.h                     ← nanopb编解码头
│   │   │   └── Python39/                 ← 工具链附带文件（第三方）
│   │   └── qfctp/
│   │       ├── qfctp.h/.cpp              ← FCTP协议实现
│   │       └── common_protocl/comm_protocol_parser.cpp
│   │                                        ← FCTP帧组包/解析
│   │
│   ├── qat/                              ← dongle AT协议
│   │   └── qat.h/.cpp
│   ├── qusb/                             ← USB电流表协议（SCPI/HQ/LX统一入口）
│   │   └── qusb.h/.cpp
│   ├── qjig/                             ← 治具串口协议（气缸/继电器）
│   │   ├── qjig.h/.cpp
│   │   └── fixture_uart.h/.cpp/.ui       ← 治具串口调试工具
│   ├── qbrush/
│   │   └── qproduct.h/.cpp               ← 产品串口通信封装
│   ├── qbulk/
│   │   ├── qbulk.h/.cpp                  ← 大包/批量传输处理
│   │   └── crc_md5.cpp                   ← CRC/MD5工具
│   ├── qshell/
│   │   └── qshell.h/.cpp                 ← shell命令封装
│   ├── qadb/
│   │   └── qadb.h/.cpp                   ← ADB交互封装
│   ├── qset/
│   │   └── qsetting.h/.cpp/.ui           ← 设置界面与配置读写
│   └── qmes/                             ← 多工厂MES适配
│       ├── qmes.h/.cpp                   ← MES基类
│       ├── mesmanager.h/.cpp             ← MES路由与管理
│       ├── hqmes.* / jjmes.* / lxmes.*   ← 工厂MES实现（华勤/金进/立讯）
│       ├── xwdmes.* / wksmes.* / bydmes.*
│       └── ydmmes.*
│
├── work_station/                         ← 各工站业务模块
│   ├── test_base.h/.cpp                  ← 工站公共基类（串口/通用流程）
│   ├── box_base.h/.cpp                   ← 工站容器基类
│   ├── common_class.h/.cpp               ← 公共业务逻辑
│   │
│   ├── quiescent_current/                ← 静态电流工站
│   │   ├── quiescent_current.h/.cpp/.ui  ← 主流程与界面
│   │   └── quiescent_current_box.h/.cpp/.ui
│   │                                        ← 工站容器页
│   ├── wifi_ble/                         ← WiFi/BLE工站
│   │   ├── wifibletest.h/.cpp/.ui
│   │   └── wifibox.h/.cpp/.ui
│   ├── pressure/                         ← 压感工站
│   │   ├── pressuresensorform.h/.cpp/.ui
│   │   ├── PressCalibBox.h/.cpp/.ui
│   │   └── ndt_sensor_cali.h/.cpp        ← 校准算法
│   ├── pcba/                             ← PCBA工站
│   │   ├── pcbaform.h/.cpp/.ui
│   │   └── pcbabox.h/.cpp/.ui
│   ├── motor/                            ← 电机工站
│   │   ├── motor.h/.cpp/.ui
│   │   └── motorbox.h/.cpp/.ui
│   ├── imu/                              ← IMU校准工站
│   │   ├── imucali.h/.cpp/.ui
│   │   └── imubox.h/.cpp/.ui
│   ├── screen/                           ← 屏幕工站
│   │   ├── screentest.h/.cpp/.ui
│   │   └── screenbox.h/.cpp/.ui
│   ├── camera/                           ← 摄像头工站
│   │   ├── cameratest.h/.cpp/.ui
│   │   └── camerabox.h/.cpp/.ui
│   ├── ageing/                           ← 老化工站
│   │   ├── ageing.h/.cpp/.ui
│   │   └── ageingbox.h/.cpp/.ui
│   └── freework/                         ← 自由工站
│       ├── qfreework.h/.cpp/.ui
│       ├── qfreeworkbox.h/.cpp/.ui
│       └── testFunction.cpp              ← 自由测试函数集
│
├── lib/                                  ← 第三方库与算法库
│   ├── qcustomplot/                      ← 图表库
│   ├── quicklz/                          ← 压缩库
│   ├── productlicense/                   ← 授权库
│   ├── md5/                              ← MD5库
│   ├── aes/                              ← AES库
│   ├── imu/                              ← IMU算法库
│   ├── form/testmodel.h/.cpp             ← 表格模型
│   ├── nfc/dcrf32.h                      ← NFC接口
│   └── libusb-*/                         ← libusb头文件与示例
│
└── build/                                ← 构建产物目录（自动生成）
    ├── .../ui_*.h                        ← Qt UI自动生成头
    └── .../bin/上位机设置.ini              ← 编译输出配置拷贝
```

## 维护约定

- 业务改动优先在 `work_station/`、`agreement/`、`mainlogic.cpp`、`agreement/qset/`。
- 不建议手改 `build/` 与 `agreement/qProtocol/qpb/Python39/`。
- 新增协议优先扩展 `agreement/qusb` 或 `agreement/qProtocol`，并通过 `test_base` 接入统一入口。
