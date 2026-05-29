# New Product Test Framework (Qt)
基于模块化与分层设计的多工站产测上位机工程框架。

## 克隆后首次设置（Git 提交钩子）

在**仓库根目录**执行下面**任选一种**（每个克隆只需一次），启用提交说明校验：

- **Windows（cmd）**：`scripts\setup-git-hooks.cmd`
- **Windows（PowerShell）**：`powershell -NoProfile -ExecutionPolicy Bypass -File scripts\setup-git-hooks.ps1`
- **Git Bash / Linux / macOS**：`sh scripts/setup-git-hooks.sh`
- **手动**：`git config core.hooksPath .githooks`

确认：`git config --get core.hooksPath` 应输出 `.githooks`。约定说明见 `docs/上位机版本信息生成说明.md`「提交信息校验」。

## 目录结构

```text
new_product_test/
├── new_production.pro                    ← Qt工程入口（模块与编译配置）
├── main.cpp                              ← 程序启动入口
├── mainwindow.h/.cpp/.ui                 ← 主界面与全局交互入口
├── mainlogic.cpp                         ← 主流程调度与业务联动
├── 上位机设置.ini                         ← 运行时配置（协议/阈值/开关）
├── new_production.qrc                    ← 资源索引（图片/QSS等）
│
├── tools/                                ← 独立工具/分析模块（与主工站解耦）
│   └── factory_analyzer/                 ← DJI/高通产测分析与 ADB/Bulk 调试
│       ├── factory_analyzer.h/.cpp/.ui
│       └── djitestfunction.cpp           ← 按名称执行 Bulk 测试项
│
├── docs/                                 ← 文档说明与改造记录
│   ├── 代码开发流程.md                    ← 开发流程规范
│   ├── 上位机版本发布.md                  ← 版本发布流程
│   ├── 协议适配.md                        ← 协议适配说明
│   ├── 协议适配改造方案.md                 ← 协议改造设计文档
│   └── 基于 TLV 的 BLE OTA 图片资源升级协议.md  ← 路特 BLE OTA 协议说明
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
│   │   ├── root_ble_ota.h/.cpp           ← 路特 TLV BLE OTA 客户端
│   │   ├── qaiot/                        ← AIOT 协议实现
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
│   │   ├── qjig.h/.cpp                   ← 治具串口调试工具（不带ui，每一路都单独控制）
│   │   └── fixture_uart.h/.cpp/.ui       ← 治具串口调试工具（带ui，一个治具）
│   ├── qmomcozy/
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
│   ├── qtuple/                           ← 三元组/云端相关
│   ├── qplc/                             ← PLC（汇川 Modbus TCP 等）
│   ├── qvisa/                            ← NI-VISA 仪器（可选编译）
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
│   ├── common_class.h/.cpp               ← MainWindow 用 TestFunctionExecutor（PB 按名执行测试项）
│   ├── key/                              ← 按键工站
│   ├── suction/                          ← 吸力工站
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

## 分层与职责（协作边界）

| 层级 | 目录 | 职责 | 典型改动人 |
|------|------|------|------------|
| 平台/启动 | `main.cpp`、`qlog/`、`my_set/` | 启动、崩溃/日志、全局宏与类型 | 平台 |
| 界面入口 | `mainwindow.*`、`mainlogic.cpp` | UI、工站切换、流程联动 | 业务 + 平台 |
| 协议层 | `agreement/` | 设备协议、MES、配置、OTA/ADB/Bulk | 协议 |
| 工站业务 | `work_station/` | 测试步骤、卡控、结果判定 | 业务 |
| 独立工具 | `tools/factory_analyzer/` | DJI 分析页，不依赖 `common_class.h` | 工具/协议 |

依赖建议（自上而下，避免反向 include）：

```text
UI / 工站  →  QProtocolManager / 平台服务  →  具体协议实现
```

- `tools/factory_analyzer` 使用自有 `FactoryNamedFunction`（定义在 `factory_analyzer.h`），勿 include `common_class.h`，避免与 `MainWindow` 侧 `NamedFunction` 在 `main.cpp` 中重定义。
- `MainWindow` 通过 `TestFunctionExecutor`（`common_class`）按名称执行 PB 测试命令；`factory_analyzer` 自行维护 Bulk 脚本测试表。

## 维护约定

- 业务改动优先在 `work_station/`、`agreement/`、`mainlogic.cpp`、`agreement/qset/`。
- `tools/factory_analyzer/` 改动保持模块内聚，勿再 include `common_class.h`。
- 不建议手改 `build/` 与 `agreement/qProtocol/qpb/Python39/`。
- 新增协议优先扩展 `agreement/qProtocol`（经 `QProtocolManager` 统一入口）或 `agreement/qusb` / `qat` 等适配层；工站通过 `test_base` 接入。
- 配置键统一走 `SETTINGS`（`上位机设置.ini`），新增项同步 `qsetting` 加载/保存与 UI。
- Git 提交说明见 `.copilot-commit-message-instructions.md`；本地钩子见上文「克隆后首次设置」。
