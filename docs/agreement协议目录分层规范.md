# agreement 协议目录分层规范

本文约定 `agreement/` 下**各协议域**的目录分层、职责边界与命名习惯。与下列文档互补，不重复迁移排期全文：

- [agreement按外设域拆分重构方案.md](./agreement按外设域拆分重构方案.md) — 按外设域拆分、阶段计划
- [factory_protocol产测协议四层架构.md](./factory_protocol产测协议四层架构.md) — dongle 产测域四层详解
- [agreement通讯分层与qtransport说明.md](./agreement通讯分层与qtransport说明.md) — 传输层与协议层分离

---

## 1. 总则

| 原则 | 说明 |
|------|------|
| **一外设一模块** | 一条业务链路或一类仪器对应一个域目录；不跨域硬塞同一套虚基类。 |
| **驱动与协议分离** | `platform/driver` 只管字节 I/O；`codec` 只管帧/行切分；`device` 管厂商命令语义。 |
| **Cmd 放对层** | **跨工站门面**用该域 `access/*_types.h`；**单台仪表**用 `device/<厂商>_<型号>/*_types.h`。 |
| **manager 不含设备命令表** | Manager 只做路由、会话、绑定实例；**不得**在 manager 里定义某台表的 SCPI/Modbus 指令枚举。 |
| **device 目录命名** | `<厂商>_<型号>_<链路>`，如 `inovance_h5u_tcp`、`hq_ammeter_rtu`、`huiling_wfp60h_scpi`。 |

### 1.1 分层子目录（按域选用，非每层都必须）

```text
agreement/<域>/
├── access/      # 工站可见：路由枚举、统一 Cmd、上报结构（可无跨设备 Cmd）
├── manager/     # 域入口：绑定设备、转发 set/get/parse（或纯传输会话）
├── codec/       # 与厂商无关的组帧/粘包/行缓冲
├── device/      # 每台设备：Profile、设备 Cmd、set/get 组具体指令
└── protocol/    # 仅 factory_protocol / product_serial 等「多实现切换」时使用
```

```text
工站 test_base / 各工站
        │
        ▼
  门面 manager（可选，工站唯一入口）
        │
        ▼
  access：Route / Cmd / Report
        │
        ▼
  device：厂商 Cmd → profile 组行/组 PDU
        │
        ▼
  codec + driver：行切分、CRC、SerialChannel / VISA / TCP
```

---

## 2. 各域对照总表

| 域目录 | 工站入口 | access | manager | codec / 其它 | device / protocol |
|--------|----------|--------|---------|--------------|-------------------|
| **factory_protocol** | `QProtocolManager` | `DeviceCmd`、`ProtocolReport` | `qprotocolmanager.*` | `codec/fctp`、nanopb | `protocol/qpb`、`qfctp`、`qaiot`、`qroot` |
| **modbus** | `QModbusManager` | `ModbusDeviceRoute`；各 device 自有 Cmd | `qmodbusmanager.*`、`modbus_device_catalog.*` | `codec/qmodbus_pdu`、`qmodbus_rtu_*` | `device/inovance_h5u_tcp`（`PlcCmd`+`InovanceH5uTcpDevice`）、`hq_ammeter_rtu`、`lx_ammeter_rtu` |
| **scpi** | `QScpiManager` | `ScpiDeviceRoute`、`ScpiLinkKind`、`ScpiTransport`（**无**跨设备 `ScpiCmd`） | `qscpimanager.*`、`qscpiserialsession.*`、`qscpivisasession.*` | `codec/scpi_line_codec.*`；底层 I/O：`platform/driver/serial`、`platform/driver/visa` | `device/huiling_wfp60h_scpi`（`HuilingScpiCmd`）、`device/rs_cmw100_scpi`（`CmwScpiCmd`） |
| **qusb**（**待废弃**） | `Qusb` | `QusbProtocolRoute` 等 | `qusb.*` | — | 内嵌串口 `QScpiManager`；见 §6.1 |
| **工站** | `test_base` | `execScpi`、`execVisaHuiling`、`execAmmeterMeasure` | `scpiVisaManager_`（VISA 实例） | — | COM 电流表 + VISA 程控/CMW 门面 |
| **product_serial** | 工站直调 | — | — | — | `protocol/qproduct.*`（**独立域，勿迁**） |
| **peripheral_protocol** | 待定 | — | — | 自定义 UART 帧 codec | `device/<厂商>_<型号>_<帧型>/` |
| **fixture** | `qjig`、fixture_uart | — | `jig/qjig.*` | `codec/fixture_*_uart_protocol.*` | `uart_ui/fixture_uart.*` |
| **business/**（仓库根） | `CmwGprfFacade`、`PlcV3Facade` 等 | 场景命令枚举 | — | — | 依赖 `agreement/scpi`、`agreement/modbus`；**非**协议域 |
| **qat** | `qat.*` | — | — | — | Dongle AT 文本（并行于 factory_protocol） |
| **服务** | 各工站 | — | — | — | `qbulk`、`qadb`、`qshell`、`qmes`（非协议分层） |

---

## 3. factory_protocol（dongle 产测）

**唯一**使用 `qProtocol` 虚接口 + 跨协议 `DeviceCmd` 的域。详见 [factory_protocol产测协议四层架构.md](./factory_protocol产测协议四层架构.md)。

```text
agreement/factory_protocol/
├── access/          qprotocol.h、qprotocol_types.h（DeviceCmd）
├── manager/         QProtocolManager
├── codec/fctp/      FCTP 帧
└── protocol/        qpb、qfctp、qaiot、qroot
```

**禁止**：PLC、电流表、治具 UART 继承 `qProtocol` 或复用 `DeviceCmd`。

---

## 4. scpi（SCPI 协议域：串口 + VISA）

### 4.1 与 modbus 对齐的分工

与 `QModbusManager` 同理：**access 仅放路由与传输抽象；各 device 目录维护自有 Cmd；manager 按路由 `exec` 重载分发**。

| 子目录 | 类 / 文件 | 职责 |
|--------|-----------|------|
| `access/scpi_types.h` | `ScpiLinkKind`、`ScpiDeviceRoute`、`ScpiVisaLinkConfig` | 链路类型 + 设备路由（**不含**跨设备 Cmd 枚举） |
| `access/scpi_transport.h` | `ScpiTransport` | SCPI 协议传输抽象（`writeLine` / `queryLine`） |
| `codec/scpi_line_codec.*` | `ScpiLineCodec` | `\r\n` 行切分（COM 异步收行；VISA 同步读可 trim） |
| `manager/qscpimanager.*` | `QScpiManager` | 域入口：`exec(HuilingScpiCmd)` / `exec(CmwScpiCmd)` + `feedRx`（串口） |
| `manager/qscpiserialsession.*` | `QScpiSerialSession` | 串口 SCPI 会话 |
| `manager/qscpivisasession.*` | `QScpiVisaSession` | VISA 上 SCPI 同步 query（内部 `VisaChannel`） |
| `platform/driver/serial/serial_channel.*` | `SerialChannel` | 平台串口字节 I/O |
| `platform/driver/visa/visa_channel.*` | `VisaChannel` | 平台 VISA 字节 I/O，无 SCPI 语义 |
| `device/huiling_wfp60h_scpi/` | `HuilingScpiCmd`、`HuilingWfp60hScpiDevice` | 会凌 WFP60H：电流/程控电源 profile 组行 |
| `device/rs_cmw100_scpi/` | `CmwScpiCmd`、`RsCmw100ScpiDevice` | 罗德 CMW100 GPRF SCPI 组行 |

### 4.2 双实例：USB 电流表 vs VISA 仪器

同一工位可能**同时**接 USB 会凌电流表与 VISA 程控电源/CMW，须用 **两个 `QScpiManager` 实例**：

| 实例 | 挂载位置 | 链路 | 典型路由 | 配置来源 |
|------|----------|------|----------|----------|
| 串口 | `Qusb::scpiManager()` | `ScpiLinkKind::Serial` | `HuilingWfp60h` | 工站 `usb->setProtocolConfig` |
| VISA | `test_base::scpiVisaManager_` | `ScpiLinkKind::Visa` | `HuilingWfp60h`（电源）或 `RsCmw100`（CMW） | `loadHuilingVisaFromSettings()` / `loadCmwVisaFromSettings()` |

切换 VISA 设备前调用对应 `load*FromSettings()` 设置路由与地址；CMW 与程控电源**共用** `scpiVisaManager_`，按步骤切换 `ScpiDeviceRoute`。

### 4.3 会凌设备（`device/huiling_wfp60h_scpi/`）

| 文件 | 职责 |
|------|------|
| `huiling_wfp60h_scpi_types.h` | `HuilingScpiCmd` 枚举 |
| `huiling_wfp60h_profile.*` | 命令模板、量程、程控电源字符串 |
| `huiling_wfp60h_scpi_device.*` | `set/get(HuilingScpiCmd)`；串口异步 / VISA 同步解包 |

工站调用（**只走 exec 重载，勿直接 device set/get**）：

```cpp
// COM 读电流（自动 RTU / SCPI）
execAmmeterMeasure();

// COM 显式 SCPI
execScpi(HuilingScpiCmd::ReadMeasureCurrent);

// VISA 程控电源
execVisaHuiling(HuilingScpiCmd::ConfigureProgrammablePower);
execVisaHuiling(HuilingScpiCmd::ProgrammablePowerOutput, true);

// COM 收包（onUsbSerialFrame）
usb->scpiManager()->feedRx(data);
```

### 4.4 CMW100 设备（`device/rs_cmw100_scpi/`）

| 文件 | 职责 |
|------|------|
| `rs_cmw100_scpi_types.h` | `CmwScpiCmd`（GPRF 写/读指令） |
| `rs_cmw100_scpi_device.*` | Cmd → SCPI 行；VISA 同步 query |

PER/burst **业务编排**在仓库根 `business/cmw_gprf/CmwGprfFacade`（与 `PlcV3Facade` 同级），注入 `QScpiManager*`：

```cpp
CmwGprfRunParams params;
params.scpi = scpiVisaManager();
params.scpi->loadCmwVisaFromSettings();
cmwFacade_.run(CmwGprfCommand::ParallelBurstForProfile, params);
```

BLE 工站直调协议示例：

```cpp
scpiVisaManager()->loadCmwVisaFromSettings();
scpiVisaManager()->exec(CmwScpiCmd::QueryLine, QStringLiteral("*IDN?"));
```

### 4.5 新增 SCPI 设备

1. 在 `ScpiDeviceRoute` 增加枚举项。
2. 新建 `device/<厂商>_<型号>_scpi/`：`*_scpi_types.h`（`XxxScpiCmd`）+ `XxxScpiDevice`。
3. 在 `QScpiManager` 增加 `exec(XxxScpiCmd)` 重载及路由分支。
4. **不要**在 access 增加「全设备共用」Cmd 枚举；**不要**在 manager 内堆设备 switch 组行。

### 4.6 当前目录结构

```text
agreement/scpi/
├── access/
│   ├── scpi_types.h
│   └── scpi_transport.h
├── codec/
│   └── scpi_line_codec.*
├── manager/
│   ├── qscpimanager.*
│   ├── qscpiserialsession.*
│   └── qscpivisasession.*
└── device/
    ├── huiling_wfp60h_scpi/
    │   ├── huiling_wfp60h_scpi_types.h
    │   ├── huiling_wfp60h_profile.*
    │   └── huiling_wfp60h_scpi_device.*
    └── rs_cmw100_scpi/
        ├── rs_cmw100_scpi_types.h
        └── rs_cmw100_scpi_device.*

business/cmw_gprf/          ← CMW PER/burst 业务（非 agreement 协议域）
└── cmw_gprf_facade.*
```

### 4.7 工站调用链（现网）

SCPI 域工站入口在 `test_base`；`Qusb` 仅为历史壳（内嵌**串口** `QScpiManager`），见 §6.1。

#### 初始化

| 步骤 | 代码位置 | 做什么 |
|------|----------|--------|
| 串口 SCPI | `test_base` 构造 | `usb = new Qusb(usbSerialPort)` → 内嵌 `QScpiManager(port)` |
| VISA SCPI | `test_base` 成员 | `scpiVisaManager_`（与 USB **独立实例**） |
| Modbus RTU | `setupModbusManager()` | `modbusManager.attachSerialChannel`；`usb->setModbusManager` |
| COM 链路 | 各工站 `apply*ProtocolConfig()` | `usb->setProtocolConfig` → SCPI/RTU 路由 + 会凌 profile |
| VISA 路由 | 发命令前 | `loadHuilingVisaFromSettings()` 或 `loadCmwVisaFromSettings()` |

#### 发送

```text
读 COM 电流
  execAmmeterMeasure()
    ├─ RTU → modbusManager.exec(Hq/LxAmmeterRtuCmd)
    └─ SCPI → execScpi(HuilingScpiCmd::ReadMeasureCurrent)
              └─ usb->scpiManager()->exec(...)

VISA 程控电源
  execVisaHuiling(HuilingScpiCmd::...) → scpiVisaManager_.exec(...)

VISA CMW
  scpiVisaManager()->exec(CmwScpiCmd::...)
  或 cmwFacade_.run(..., params.scpi = scpiVisaManager())
```

#### 接收（COM 口）

```text
onUsbSerialFrame
  ├─ RTU → modbusManager.feedRtuRx
  └─ SCPI → usb->scpiManager()->feedRx → HuilingWfp60hScpiDevice::handleLineReceived
```

#### 各工站（SCPI / VISA）

| 工站 | COM 读电流 | VISA |
|------|------------|------|
| `quiescent_current` | `execAmmeterMeasure()` | `execVisaHuiling` 程控电源 |
| `suction` | 循环 `execAmmeterMeasure()` | 同上 |
| `key_test` | `execAmmeterMeasure()` | — |
| `wifibletest` | — | `scpiVisaManager` + `CmwScpiCmd` / `CmwGprfFacade` |
| 自由工站 | hook `execAmmeterMeasure` | `cmwFacade_.run` + `plcFacade_.run` |

SETTINGS：`Current/ProtocolType`；程控 `VisaPower/*`；CMW `BlePer/CmwVisaAddress` 等。

### 4.8 SCPI 协议域 vs「电流表」工站语义

**SCPI 是通用文本协议**；`ammeter` 语义只应出现在工站层（`execAmmeterMeasure`、`measure_ammeter`），不应写进 `agreement/scpi/` 抽象。

| 层次 | Cmd 约定 | 命名带 `ammeter`？ |
|------|----------|-------------------|
| **access** | 仅 `ScpiDeviceRoute`，**无**跨设备 Cmd | 否 |
| **device** | 各设备自有 Cmd（`HuilingScpiCmd`、`CmwScpiCmd`…） | 否 |
| **工站** | `execAmmeterMeasure`、`ProtocolAmmeterReadingData` | 可以 |

**已移除**：`agreement/qvisa/`、`agreement/visa_instrument/`；VISA 字节 I/O 在 `platform/driver/visa`，SCPI 协议在 `agreement/scpi/`，CMW 业务在 `business/cmw_gprf/`。

**待收尾（§6.2）**：`ammeterReadingReceived` 信号改名；access 层不再出现历史 `ScpiCmd` 文档描述。

```text
【正确】VISA 程控电源
  execVisaHuiling(HuilingScpiCmd::ConfigureProgrammablePower)

【正确】COM 读电流（工站步骤名）
  execAmmeterMeasure()

【错误】在 access 维护全局 ScpiCmd
【错误】在 scpi/manager 增加 execAmmeterMeasure
```

---

## 5. modbus

```text
agreement/modbus/
├── access/modbus_types.h       # ModbusDeviceRoute（工站设备列表）
├── access/modbus_device_catalog.*  # 设备 → 内部链路/目录（工站不选协议）
├── manager/qmodbusmanager.*    # PLC TCP + RTU 电流表路由
├── codec/                      # PDU、CRC、RTU 粘包
└── device/
    ├── inovance_h5u_tcp/       # *_types.h（PlcCmd）+ *_device（set/get）+ TCP 传输/PlcModbusSession
    ├── hq_ammeter_rtu/         # HqAmmeterRtuCmd + HqAmmeterModbusRtu（IModbusRtuDevice）
    └── lx_ammeter_rtu/         # LxAmmeterRtuCmd + LxAmmeterModbusRtu（IModbusRtuDevice）
```

- **RTU 电流表** 与 SCPI 同构：`IModbusRtuDevice` + 模板 `exec<CmdType>`；PLC 仍用 `InovanceH5uTcpDevice` 的 **`set` / `get`**。
- **device/** 只放 Modbus 链路设备；SCPI 表不得放入 modbus。
- RTU 组帧统一走 `codec/qmodbus_pdu` / `qmodbus_rtu_codec`，设备内维护寄存器/功能码语义与 RX 解析。

---

## 6. qusb（**待废弃**，COM 电流表历史门面）

> **状态**：过渡代码；**不是协议域**。协议只在 `scpi/`、`modbus/`。  
> **计划**：门面并入 `work_station/test_base`，删除 `agreement/qusb/`（见 §6.1）。

现网 `agreement/qusb/` 职责：

```text
agreement/qusb/
├── qusb_types.h    # QusbProtocolRoute、QusbCmd（将迁入 test_base 或 platform/settings）
├── qusb.h/.cpp     # 内嵌 QScpiManager；setProtocolConfig 同步 modbus RTU 路由
```

| 路由 | 发送（现网） | 收包 |
|------|--------------|------|
| SCPI | `test_base::execScpi` → `usb->scpiManager()->exec` | `scpiManager()->feedRx` |
| HQ/LX RTU | `test_base::execAmmeterMeasure` → `modbusManager.exec` | `modbusManager.feedRtuRx` |

新代码**不要**再扩展 `Qusb` API；COM SCPI 用 `execScpi(HuilingScpiCmd)`，VISA 用 `execVisaHuiling` / `scpiVisaManager()->exec(CmwScpiCmd)`。

### 6.1 阶段计划：废弃 `qusb`，门面并进 `test_base`

**目标结构**（与 `modbusManager`、`protocolManager` 并列，均在工站基类）：

```text
test_base
├── QScpiManager scpiVisaManager_      # VISA 实例（程控电源 / CMW）
├── QModbusManager modbusManager_      # 已有
├── usbSerialPort_ / usbSerialChannel_ # 已有；Qusb 内嵌串口 scpiManager
├── execScpi(HuilingScpiCmd)           # COM SCPI
├── execVisaHuiling(HuilingScpiCmd)    # VISA 会凌电源
├── execAmmeterMeasure()               # RTU / COM SCPI 读电流
└── onUsbSerialFrame()                 # feedRtuRx / feedRx
```

**迁移步骤**（最后再改代码，顺序建议）：

1. `test_base` 增加成员 `QScpiManager scpiManager_`（与 `usbSerialPort` 同生命周期），`execScpi` / `onUsbSerialFrame` 改直调，**暂保留** `Qusb* usb` 委托到同一 `scpiManager_`（双跑验证）。
2. `Qusb::ProtocolConfig` / `QusbProtocolRoute` 迁到 `test_base`（或 `platform/settings` 读写 SETTINGS），各工站 `apply*ProtocolConfig` 改调 `test_base::applyComAmmeterLinkConfig`。
3. `reportReceived`：工站改连 `scpiManager_::ammeterReadingReceived`（或与 modbus RTU 一样在 `test_base::signalAndslot` 统一转 `onUsbInstrumentReport`）。
4. 删除 `test_base::usb`、`agreement/qusb/` 目录及 `.pro` 引用；`Qusb::PowerAction` 等历史别名如需保留，在 `test_base.h` 用 `using` 过渡一个版本。
5. 全量编译验收：静态电流、按键、吸力 SCPI/RTU 读电流；程控电源（`execVisaHuiling`）；CMW（`CmwGprfFacade`）。

**删除后 agreement 协议相关只剩**：

```text
agreement/scpi/     ← 通用 SCPI 协议 + 各 device
agreement/modbus/   ← Modbus + 各 device（含 RTU 电流表）
work_station/       ← 工站门面（test_base；含 execAmmeterMeasure 等业务 API）
```

### 6.2 阶段计划：SCPI 层去除 `ammeter` 命名（与 §6.1 同批或紧随其后）

与废弃 `qusb` **同一收尾阶段**处理，避免半改半留。

| 步骤 | 内容 |
|------|------|
| 1 | `HuilingWfp60hScpiDevice` / `QScpiManager`：`ammeterReadingReceived` → `scpiLineReceived`（或 `measurementLineReceived`）；电流解析仍在 device 内 |
| 2 | `QScpiManager` 可增加 `deviceReportReceived(ScpiDeviceRoute, …)` 供多设备扩展；工站电流表步骤再转 `ProtocolAmmeterReadingData` |
| 3 | `test_base::signalAndslot`：SCPI 上行统一入口，**仅当业务是读电流**时写入 `measure_ammeter` / `ProtocolAmmeterReadingData` |
| 4 | 全文检索 `ammeterReading`：禁止在 `agreement/scpi/` 新增；`modbus` 各 device Cmd、`ProtocolAmmeterReadingData` 保持（工站/RTU 产线语义） |
| 5 | 文档：access **无**跨设备 Cmd；各 device 目录 `*_scpi_types.h`（**已完成** 2026-06） |

**不改**：`execAmmeterMeasure`、`ProtocolAmmeterReadingData`、`measure_ammeter` 成员（工站层）；VISA 程控路径。

---

## 7. product_serial（冻结）

```text
agreement/product_serial/protocol/qproduct.*
```

产品校频、PER、开始/停止接收等 — **独立域**，不得迁入 `peripheral_protocol` 或与 SCPI/Modbus 合并。

---

## 8. peripheral_protocol（外设自定义 UART 帧）

用于**非** Modbus、**非** SCPI、**非** dongle 产测的厂商私有帧（治具、吸奶器 PCBA 等后续可迁入）。

```text
agreement/peripheral_protocol/
├── codec/                          # 粘包、定长帧（可复用）
└── device/<厂商>_<型号>_<帧型>/    # 组帧/解帧 + 命令表
```

与 `fixture/codec` 并存期：治具帧暂留 `fixture/`，新外设优先本域。

---

## 9. fixture（治具）

```text
agreement/fixture/
├── jig/qjig.*                      # 气缸/继电器
├── codec/fixture_*_uart_protocol.* # PCBA/压感/IMU/相机 UART
└── uart_ui/fixture_uart.*          # 带 UI 调试
```

---

## 10. 平台级底层驱动 (Platform Driver)

不再将通用传输逻辑散落在协议目录中，系统所有底层物理和系统通道统一汇聚于 `platform/driver/` 下，作为纯粹的 I/O 提供者：

| 路径 | 职责 | 对应上层协议域 |
|------|------|----------------|
| `platform/driver/serial/serial_channel.*` | 封装系统 COM 串口读写 | 所有依赖串口的协议 (Fixture, Modbus等) |
| `platform/driver/visa/visa_channel.*` | NI-VISA 字节 I/O（无 SCPI 语义） | SCPI 协议 (`scpi_protocol`) |
| `platform/driver/process/process_channel.*` | 封装系统本地进程管道通信与生命周期管理 | ADB 协议 (`adb_protocol`)、Shell 协议 (`shell_protocol`) |

*(注：原有的 `agreement/qtransport/` 以及 `agreement/qvisa/` 已被彻底删除，其通信机制已被提取或下沉为上述的三大底层核心驱动)*

工站使用 `test_base::scpiVisaManager_`（VISA）与 `usb->scpiManager()`（串口）两个实例；VISA 与串口仅物理驱动不同，协议层流转均在 `agreement/scpi_protocol/`。

---

## 11. 协议域全局后缀对齐（ADB/Shell/MES 四层改造）

原 `services/` 目录下的 `qadb`、`qshell` 和 `qmes` 本质上与外部系统交互，也属于协议。它们将整体迁移并重构入 `agreement/` 下，且**整个协议目录体系均统一增加了 `_protocol` 后缀**，并执行 `access/manager/codec/device` 的标准四层规范：

| 统一后的目录 | 职责说明 | 底层驱动依托 |
|------|----------|----------|
| `adb_protocol/` | ADB 进程通信协议 | 依赖 `platform/driver/process` |
| `shell_protocol/` | 本地进程控制协议 | 依赖 `platform/driver/process` |
| `mes_protocol/` | 各工厂 MES HTTP 交互 | 内部 HTTP 调用封装 |
| `modbus_protocol/` | Modbus RTU/TCP 通信 | 依赖串口或 TCP |
| `scpi_protocol/` | 仪器通信协议 | 依赖 `platform/driver/visa` 或串口 |
| `fixture_protocol/` | 治具通信协议 | 依赖 `platform/driver/serial` |
| `product_protocol/` | 产线测试特有数据协议 | 内部业务相关 |

---

## 12. 反模式（避免）

| 反模式 | 正确做法 |
|--------|----------|
| 工站直接 `writeLine` 或绕过 manager | `QScpiManager::exec(HuilingScpiCmd)` / `exec(CmwScpiCmd)` |
| 在 access 增加跨设备 `ScpiCmd` | 各 device 目录自有 Cmd（与 modbus 一致） |
| CMW 业务塞进 `scpi/device` | 协议在 `rs_cmw100_scpi`；PER 编排在 `business/cmw_gprf` |
| 同一工位 USB+VISA 共用一个 manager | 串口 `usb->scpiManager()` + VISA `scpiVisaManager_` 两实例 |
| 在 `scpi/` 使用 `ammeter*` 命名 / `execAmmeterMeasure` | 协议层用 device Cmd + `exec`；电流表语义只在 `test_base`（§4.8） |

---

## 13. 工程与包含路径

`new_production.pro` 中典型的 `INCLUDEPATH` 现已全部应用后缀对齐机制：

- `agreement/scpi_protocol/access`、`manager`、`codec`、`device/...`
- `agreement/adb_protocol/...`、`agreement/mes_protocol/...`、`agreement/modbus_protocol/...`
- `platform/driver/visa`、`platform/driver/serial`、`platform/driver/process` (底层三大物理驱动)
- `business/cmw_gprf`

*(不再有 `agreement/qtransport` 或未加后缀的遗留杂项)*

改 `.ui` 或新增 device 目录后需 **重新 qmake + 全量编译**。

---

---

## 14. 工站用途名与协议设备映射（`platform/instrument`）

产线按 **工厂采购 + 工位用途** 选外设，与协议层 **厂商型号目录** 解耦：

```text
Mes/FACTORY（byd、hq、lx…）
        + 用途中文名（程控电源、COM电流表…）
                ↓  InstrumentDeviceCatalog
        ModbusDeviceRoute / ScpiDeviceRoute
                ↓
        agreement/modbus|scpi/device/<厂商>_<型号>_<链路>/
```

### 14.1 三层命名（勿混用）

| 层 | 命名 | 示例 | 谁用 |
|----|------|------|------|
| **用途** | `InstrumentPurposeId` + **中文 labelZh** | `programmable_power` / 「程控电源」 | 工站、设置页、test_case |
| **路由** | `ModbusDeviceRoute` / `ScpiDeviceRoute` | `HuilingWfp60h` | manager、`exec` |
| **协议 device** | 目录 `<厂商>_<型号>_<链路>` | `huiling_wfp60h_scpi` | 组帧/Cmd 实现 |

**规则**：同一台会凌电源在 BYD 叫「程控电源」，在协议层仍是 `huiling_wfp60h_scpi`；华勤线「COM电流表」可能映射 `hq_ammeter_rtu`，另一家映射 `huiling_wfp60h_scpi`。

### 14.2 映射表维护

- 代码：`platform/instrument/instrument_device_catalog.cpp` 的 `kPurposeRows`
- `factoryId`：小写 `Mes/FACTORY`；`**` 表示通用默认
- 解析：**先工厂专属行，再 `*` 回退**
- 工站：`test_base::resolveInstrumentPurpose(u8"程控电源", &ref)`

### 14.3 新增外设 checklist

1. 在 `agreement/<域>/device/` 实现协议与 `XxxCmd`
2. 在 `access` 增加 `*DeviceRoute` 枚举
3. 在 `kPurposeRows` 增加「工厂 + 中文用途 → route」
4. 在 `*_cmd_manifest` 增加 Cmd 中文（test_case 用）

---

*文档版本：v1.5；§14 工厂用途映射 platform/instrument。*
