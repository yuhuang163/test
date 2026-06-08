# agreement 目录分层重构方案

> 基于当前回退点（`55029b71`：华庄 MES、日志上传等）整理。  
> 目标：把 `agreement/` 从扁平 `q*` 子目录改为**可维护的分层结构**，**分阶段落地、每阶段可编译**。  
> 与 [自由工站协议调用说明.md](./自由工站协议调用说明.md)、[agreement外设与治具说明.md](./agreement外设与治具说明.md) 配套阅读。

---

## 1. 现状问题

| 现状 | 问题 |
|------|------|
| `qProtocol/` 内同时有 `qprotocol`、`qprotocolmanager`、`qfctp`、`qpb`、`common_protocl` | 编解码 / 接口 / 管理器 / 设备实现混在一起 |
| `qtransport/` 含 `qmodbus_pdu` + `qprocesschannel` | 协议 codec 与传输通道同名目录、职责不清 |
| `qplc/` 仅汇川 TCP，与 `qusb` 电流表 Modbus 思路重复 | 未统一 Modbus 抽象 |
| `qmes`、`qtuple`、`qadb` 与设备协议并列 | 实际是 MES/云端/工具，非设备协议栈 |
| `.pro` 中 15+ 条 `INCLUDEPATH` | 搬目录易牵动全仓 `#include` |

---

## 2. 目标目录结构

```text
agreement/
├── codec/                    # 纯编解码，无 QSerialPort/QTcpSocket
│   ├── fctp/                 # comm_protocol_*（从 qfctp/common_protocl 迁出）
│   ├── modbus/               # qmodbus_pdu
│   ├── fixture/              # fixture_*_uart_protocol、fixture_uart_types
│   └── nanopb/               # pb.h、pb_*.c、*.pb.c（可从 qProtocol/qpb 拆出运行时）
│
├── access/                   # 统一抽象接口 + 公共类型
│   ├── qprotocol.h/.cpp
│   ├── qprotocol_types.h
│   ├── qmodbus.h             # 后续 Modbus 统一接口（与 qProtocol 平行）
│   └── modbus_types.h
│
├── manager/                  # 工站统一入口
│   ├── qprotocolmanager.*
│   └── qmodbusmanager.*
│
├── device/                   # 设备/协议适配（set/get/parseCmd，绑串口/TCP）
│   ├── product/              # 产测三协议
│   │   ├── qfctp/
│   │   ├── qaiot/
│   │   └── qpb/              # qpb.cpp/.h + proto 生成物；generator/Python39 可留原位
│   ├── modbus/               # inovance_h5u、usb_modbus_rtu
│   ├── peripheral/           # 串口外设
│   │   ├── qusb/
│   │   ├── qjig/
│   │   ├── qat/
│   │   └── fixture/          # fixture_uart（UI+串口）
│   ├── product_io/           # qproduct、qvisa（可选）
│   └── ble/                  # root_ble_ota
│
├── transport/                # 纯 I/O 通道
│   ├── serial_channel.*      # 可从 platform/serial 迁入
│   └── qprocesschannel.*
│
└── service/                  # 非设备协议栈
    ├── mes/
    ├── tuple/
    ├── adb/
    ├── shell/
    └── bulk/
```

### 分层依赖规则（硬性）

```text
工站 → manager → access → device → codec
                ↘ transport（device 持有 channel/port）
service 独立，不依赖 device
codec 不得 include device/manager
```

---

## 3. 旧路径 → 新路径对照

| 当前路径 | 目标路径 | 层 |
|----------|----------|-----|
| `qProtocol/qfctp/common_protocl/*` | `codec/fctp/*` | codec |
| `qtransport/qmodbus_pdu.*` | `codec/modbus/*` | codec |
| `qfixture/protocol/*` | `codec/fixture/*` | codec |
| `qProtocol/qpb/pb*`、`*.pb.c` | `codec/nanopb/*`（或暂留 qpb 旁） | codec |
| `qProtocol/qprotocol.*` | `access/qprotocol.*` | access |
| `qProtocol/qprotocol_types.h` | `access/qprotocol_types.h` | access |
| `qProtocol/qprotocolmanager.*` | `manager/qprotocolmanager.*` | manager |
| `qProtocol/qfctp/` | `device/product/qfctp/` | device |
| `qProtocol/qaiot/` | `device/product/qaiot/` | device |
| `qProtocol/qpb/qpb.*` | `device/product/qpb/` | device |
| `qProtocol/root_ble_ota.*` | `device/ble/` | device |
| `qplc/inovance_*` | `device/modbus/` | device |
| `qusb/` | `device/peripheral/qusb/` | device |
| `qjig/` | `device/peripheral/qjig/` | device |
| `qat/` | `device/peripheral/qat/` | device |
| `qfixture/fixture_uart.*` | `device/peripheral/fixture/` | device |
| `qproduct/`、`qvisa/` | `device/product_io/` | device |
| `qtransport/qprocesschannel.*` | `transport/` | transport |
| `platform/serial/serial_channel.*` | `transport/serial_channel.*` | transport |
| `qmes/*` | `service/mes/` | service |
| `qtuple/*` | `service/tuple/` | service |
| `qadb/`、`qshell/`、`qbulk/` | `service/adb|shell|bulk/` | service |

---

## 4. 分阶段实施

### 阶段 0：约定与护栏（1 次小提交）

- 本文档 + 调用说明落 `docs/`
- **不改任何源码路径**
- 换行规范化与目录重构**分开提交**

### 阶段 1：只抽 codec（风险最低）

- 迁：`common_protocl` → `codec/fctp/`
- 迁：`qmodbus_pdu` → `codec/modbus/`
- 迁：`qfixture/protocol` → `codec/fixture/`
- `.pro` 增加 `INCLUDEPATH`，**保留旧路径**
- 适配 `qfctp.cpp`、`inovance_*`、`qusb` 的 include（或靠 INCLUDEPATH 短名）
- **验收**：全量编译 + 自由工站 FCTP / 治具 PCBA 冒烟

### 阶段 2：access + manager

- 迁：`qprotocol.*`、`qprotocol_types.h` → `access/`
- 迁：`qprotocolmanager.*` → `manager/`
- 工站：`#include "qprotocolmanager.h"`（靠 INCLUDEPATH）
- **验收**：`protocolManager.set/get` 正常

### 阶段 3：device/product（产测三协议）

- 建 `device/product/`，迁入 `qfctp`、`qaiot`、`qpb`
- **不搬** `Python39`、`generator`
- **验收**：PB + FCTP + AIoT 收发

### 阶段 4：device/peripheral + transport

- 迁 `qusb`、`qjig`、`qat`、`fixture_uart`
- 迁 `serial_channel` → `agreement/transport/`
- **验收**：Fixture PCBA + USB 电流 + jig 口开关

### 阶段 5：Modbus 统一（可选）

- `access/qmodbus` + `manager/qmodbusmanager`
- `device/modbus/`：`inovance_h5u` + `usb_modbus_rtu`
- **验收**：自由工站 PLC 步骤

### 阶段 6：service 层

- 迁 `qmes`、`qtuple`、`qadb`、`qshell`、`qbulk`
- **验收**：MES、三元组、日志上传

### 阶段 7：清理

- 删空目录 `qProtocol/`、`qtransport/`、`qplc/` 等
- 收紧 `.pro` INCLUDEPATH（约 6～8 条顶层路径）
- 删无效 `qbrush`、`adb` 空路径

---

## 5. 与上次失败经验的差异

| 上次 | 这次 |
|------|------|
| 一次 `git mv` 全仓 + Python39 | **分阶段**，每阶段一个 commit |
| access/adapter 同时上 | 先 **codec → access/manager → device** |
| 未适配全仓 include | 每阶段改完 **立即编译** |
| 混进 CRLF / 大提交 | **换行与目录重构分开** |
| `adapter/qProtocol` 命名绕 | 用 **`device/product`** |

---

## 6. 自由工站验收清单（每阶段）

1. 打开/连接：dongle、product、fixture（菜单「连接治具串口」）
2. test_case：`Channel=Fixture` 的 PCBA 发令/等包
3. `protocolManager` FCTP set/get
4. 华庄 MES / 日志上传（service 阶段后）

---

## 7. 相关文档

- [自由工站协议调用说明.md](./自由工站协议调用说明.md)
- [agreement外设与治具说明.md](./agreement外设与治具说明.md)
- [协议解耦迁移说明.md](./协议解耦迁移说明.md)（`Protocol*Data` 与 Fac* 解耦）
- [agreement通讯分层与qtransport说明.md](./agreement通讯分层与qtransport说明.md)（历史阶段 1 传输层说明，迁移时以本文为准更新）
