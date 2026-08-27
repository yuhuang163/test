# New Product Test Framework (Qt)

> **一句话**：多工站产测上位机（FCT/ATE），配置驱动测试流程。  
> **读者**：新人或改代码前快速定位。**前提**：Qt 5.15 / MSVC / C++17。

更深的自由工站与分层说明见 [`docs/项目代码结构与自由工站梳理.md`](docs/项目代码结构与自由工站梳理.md)。

## 技术栈与编译

| 项 | 值 |
|----|-----|
| 工程入口 | `new_production.pro`（`CONFIG += c++17`，`/utf-8`） |
| 构建目录 | `build/Desktop_Qt_5_15_2_MSVC2019_64bit-Release` |
| 增量编译 | `scripts\编译Release版本.ps1 -SkipQmake` |
| 完整 qmake | 改 `.pro` / 增删源文件后去掉 `-SkipQmake` |
| 格式化 | `scripts\格式化代码.ps1`（clang-format + UTF-8 无 BOM） |
| 换行 | 仓库文本默认 CRLF；仅 `new_production.pro` 固定 LF |

专用工站由 `.pro` 顶部 `ENABLE_STATION_*` 宏控制是否编入；**自由工站始终编译**。改宏后须重新 qmake。

运行时配置（`上位机设置.ini`、功能块 INI）与 exe 同级，通常在 `build/.../bin/`，不在仓库根目录。

## 目录结构

```text
new_product_test/
├── new_production.pro          ← Qt 工程入口（模块开关 + 源文件列表）
├── main.cpp / mainwindow.* / mainlogic.cpp
├── common/                     ← CommonUtils（字节/时间/CRC/字符串等）
├── my_set/                     ← AbIni.h 版本宏、全局类型
├── qlog/                       ← 日志与测试记录
├── stytle/qss/                 ← 界面 QSS（勿在 .cpp 硬编码长样式）
│
├── platform/                   ← 平台基础设施
│   ├── driver/
│   │   ├── serial/             ← SerialChannel
│   │   ├── visa/               ← VisaChannel
│   │   └── process/            ← ProcessChannel
│   ├── settings/               ← qsetting + 测试流程编排 / 功能块编辑
│   ├── test_case/              ← 功能块引擎（store/gate/ini_param/manifest）
│   ├── cloud/                  ← 登录鉴权、用例同步、日志/测试数据上传、上位机 OTA
│   ├── label_print/            ← 标签打印
│   ├── instrument/             ← 仪器设备目录
│   └── debug/screen_inspect/   ← 屏幕巡检调试
│
├── agreement/                  ← 协议层（按域拆分，access/manager/codec/device）
│   ├── factory_protocol/       ← QProtocolManager、qpb/qfctp/qaiot/qroot
│   ├── at_protocol/            ← Dongle AT
│   ├── scpi_protocol/          ← SCPI（会凌电源、Agilent、CMW100 等）
│   ├── modbus_protocol/        ← PLC / 电流表 / 温湿度等
│   ├── fixture_protocol/       ← 治具（uart / 华勤 / 西威迪 / 杰理 / ASD9026A）
│   ├── mes_protocol/           ← 多工厂 MES
│   ├── product_protocol/       ← 产品串口 qproduct
│   ├── adb_protocol/           ← ADB
│   ├── bulk_protocol/          ← Bulk
│   └── shell_protocol/         ← Shell
│
├── business/                   ← 工站业务门面（非通讯协议）
│   ├── plc_v3_fixture/         ← PlcV3Facade（V3 治具 PLC）
│   ├── cmw_gprf/               ← CmwGprfFacade（CMW100 GPRF PER/burst）
│   ├── tuple/                  ← QTupleService 三元组
│   └── ble_ota/                ← 路特 BLE OTA（root_ble_ota / root_ble_ota2）
│
├── work_station/               ← 各工站 UI 与流程
│   ├── test_base.* / box_base.*
│   ├── freework/               ← 自由工站（始终编入；qfreework* + shared_instrument）
│   ├── key/ suction/ pressure/ pcba/ motor/ imu/
│   ├── screen/ camera/ ageing/ wifi_ble/ quiescent_current/
│   └── …（是否编译见 ENABLE_STATION_*）
│
├── tools/factory_analyzer/     ← DJI/高通分析页（与主工站解耦）
├── docs/                       ← 说明与协议资料（见下）
├── scripts/                    ← 编译 / 格式化 / 换行与辅助脚本
├── lib/                        ← 第三方（qcustomplot、xlsx、visa 头等）
├── advance/                    ← 演示 / 图像窗 / xlsx 等附属模块
└── build/                      ← 构建产物（勿手改）
```

## 文档索引（`docs/`）

| 目录/文件 | 用途 |
|-----------|------|
| `项目代码结构与自由工站梳理.md` | 分层、启动 case、自由工站 onboarding |
| `协议文档/` | Dongle / qroot / FCT&ATE / MES 等协议说明 |
| `开发参考资料/` | 仪表与 PLC 原始资料（CMW、66319D、信捷等） |
| `使用说明文档/` | 使用侧说明 |
| `对外说明/` | 对外材料 |
| `提示词文档/` | Agent / 提示词相关 |

## 分层与职责

| 层级 | 目录 | 职责 |
|------|------|------|
| 启动 / UI | 根目录 `main*` | 启动模式、工站箱子、主界面联动 |
| 平台 | `platform/` | 驱动通道、设置页、功能块引擎、云端服务 |
| 协议 | `agreement/` | 帧编解码、设备适配、MES |
| 业务门面 | `business/` | 跨协议的场景编排（PLC V3、CMW GPRF、三元组、BLE OTA） |
| 工站 | `work_station/` | 步骤、卡控、`passValue`/`failValue`、界面 |
| 工具 | `tools/factory_analyzer/` | 独立分析页，不依赖工站公共类 |

依赖方向（避免反向 include）：

```text
UI / 工站  →  business 门面 / platform 服务 / SerialChannel
         →  QProtocolManager / QScpiManager / QModbusManager
         →  agreement 具体 codec/device
```

## 维护约定

- 业务步骤优先改 `work_station/`；场景编排可落在 `business/`；协议帧与设备适配改 `agreement/`。
- 配置键统一 `SETTINGS`（`分类/键名`），新增项同步 `platform/settings/qsetting` 加载/保存与 UI。
- 公共可复用工具进 `common/common_utils.*`（`CommonUtils::`）；禁止新建匿名命名空间堆小工具。
- 调用点仅一处时不要额外抽函数；空实现 / 仅占位的 noop 不要保留。
- 不手改 `build/`；不要改动 `agreement/factory_protocol/protocol/qpb/Python39/`（检索时也可忽略）。
- 文本：UTF-8 无 BOM；Agent 改完 `.cpp/.h` 等后执行 `py -3 scripts/convert_to_crlf.py <路径>`。
- Cursor 规则见 `.cursor/rules/`（`qt-cpp-project`、`minimal-diff`、`no-anonymous-namespace`）。

## 验证

1. 增量编译：`scripts\编译Release版本.ps1 -SkipQmake`
2. 成功标志：`build/logs/build_*.log` 无 `error C` / `error LNK`，产出 `build/.../bin/new_production.exe`
3. 排障日志：`build/.../bin/所有log/上位机log/`（按修改时间取最新）
