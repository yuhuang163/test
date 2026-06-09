# agreement 通讯分层与 qtransport 说明

本文说明 `agreement/` 目录的职责划分、**传输层与协议层分离**的目标，以及阶段 1 已落地的 `qtransport` 模块用法。与 [协议解耦迁移说明.md](./协议解耦迁移说明.md)（上层 `Protocol*Data`）互补：彼文解决「业务信号与 Fac* 解耦」，本文解决「串口/TCP/进程等连接方式复用」。

---

## 1. 为什么要分层

现状里常见两类重复：

| 层次 | 典型代码 | 问题 |
|------|----------|------|
| **传输** | 各模块自己 `QSerialPort::readyRead`、缓冲、定时器 debounce | `test_base` 与工站里多套相同收包样板 |
| **协议** | Qpb / Qfctp / Qaiot 各自组帧、`parseCmd` | 合理，但不应与 `write` 绑死在各处 |

**Dongle 串口** 是典型例子：物理上只有 **一根 `QSerialPort`**，同时跑 Qat（AT 文本）、QProtocolManager（qpb / qfctp / qaiot），差别只在 **命令编码**，不是连接方式不同。

目标结构：

```text
工站 / test_base
    ↓ DeviceCmd、QProtocolManager
协议层（qProtocol：Qpb / Qfctp / Qaiot …）  ← 组帧、解析、映射 DeviceCmd
    ↓ 字节流
传输层（qtransport）                      ← 只负责 open/write/收包防抖
    ↓
QSerialPort / QTcpSocket / QProcess / …
```

---

## 2. 目录职责（约定）

| 路径 | 职责 |
|------|------|
| `agreement/qtransport/` | **通用传输**：串口写统一入口、收包防抖 Reader（阶段 1） |
| `agreement/qProtocol/` | **设备产测协议**：qpb、qfctp、qaiot + `QProtocolManager` |
| `agreement/qat/` | Dongle AT 文本（并行解析，不并入 qProtocol） |
| `agreement/qusb/`、`qvisa/` | 仪器：SCPI / Modbus 等 |
| `agreement/qfixture/`、`qmomcozy/` | 治具 UART / 产品仪器固定帧 |
| `agreement/qjig/` | 治具气缸/继电器控制 |
| `agreement/qplc/` | Modbus TCP |
| `agreement/qadb/`、`qshell/` | 持久进程 + 命令队列 |
| `agreement/qmes/`、`qtuple/` | HTTP / HTTPS |
| `agreement/qbulk/` | USB Bulk 工厂协议 |

---

## 3. 阶段 1 已实现内容

### 3.1 `QSerialChannel`（写统一）

- 头文件：`agreement/qtransport/qchannel.h`
- 实现：`agreement/qtransport/qchannel.cpp`

```cpp
namespace QSerialChannel {
bool isPortOpen(const QSerialPort* port);
qint64 write(QSerialPort* port, const QByteArray& data);
}
```

**已接入写路径：**

- 协议层：`Qpb::writeSerial()`、`Qfctp::sendPacket()`、`Qaiot` 发送
- 仪器/治具：`qusb`、`qat`、`qjig`、`qfixture`（`fixture_uart`）
- Dongle AT：`qat::sendCmd`

新增串口发送时：**不要**再直接 `serialPort->write`，统一走 `QSerialChannel::write`。

### 3.2 `QSerialPortReader`（串口收包防抖）

- 头文件：`agreement/qtransport/qserialportreader.h`
- 实现：`agreement/qtransport/qserialportreader.cpp`

行为与原先 `test_base` 一致：`readyRead` 累积 → 单次定时器 debounce → 一次性回调。

**`test_base` 改动：**

- 构造时创建 `dongleSerialReader` / `usbSerialReader` / `jigSerialReader` / `productSerialReader`，回调里分别触发既有 `read*SerialPortData()`。
- 移除上述串口上重复的 `readyRead + QTimer` 防抖样板。
- `open*SerialPort` / `close*SerialPort` 与对应 reader 的 `bind` / `clear` 配合。

### 3.3 工程配置

`new_production.pro` 已增加：

- `INCLUDEPATH += agreement/qtransport`
- `SOURCES += agreement/qtransport/qchannel.cpp`、`qserialportreader.cpp`、`qmodbus_pdu.cpp`、`qprocesschannel.cpp`

### 3.4 Modbus 公共层（阶段 2 已落地）

- 新增 `qtransport/qmodbus_pdu.*`，统一提供：
  - PDU 构建（`buildReadCoilsRequestPdu` / `buildWriteSingleCoilRequestPdu`）
  - RTU CRC（`crc16ModbusRtuBigEndian`）
  - 响应校验（`isExceptionResponse` / `functionCodeMatches` / `validateRtuFrame`）
  - 异常码文字（`exceptionCodeText` / `formatExceptionMessage`）
  - 读线圈 PDU 解析（`parseReadCoilsPdu`）
- `qplc`、`qusb`、`qjig` 已接入上述公共能力；`fixture_uart` 写口已统一。

### 3.5 进程通道公共层（阶段 3 首刀已落地）

- 新增 `qtransport/qprocesschannel.*`，统一封装：
  - 持久进程启动/停止
  - 命令队列与 endMark 回包截断
  - 超时检测与回调
- `qshell` 已改为组合 `QProcessChannel`，仅保留 PowerShell 参数与初始化命令。
- `qadb` 已改为组合 `QProcessChannel`，仅保留 `adb shell` 启动路径与按键监控流程。
- `setRestartOnTimeout(true)`：`Qadb` 命令超时后自动重启 adb shell（与旧逻辑一致）。

---

## 4. 配置与运行时切换（不变）

| 配置键 | 含义 |
|--------|------|
| `SYSTEM/ProtocolType` | 设备协议手法：`qpb` / `qfctp` / `qaiot` |
| Dongle 串口 | 仍由界面选择 COM、`dongleBaudRate` 等（`test_base`） |

协议切换仍用 **`QProtocolManager`**，与传输层无关。

---

## 5. 新增界面配置项（设置页）

仍按 [qsetting_setting_binds.inc](../agreement/qset/qsetting_setting_binds.inc) 维护，见绑定表分区注释（`Ctrl+F` 搜 `[16]` 等）。与 qtransport 无直接耦合。

---

## 6. 后续阶段（规划，未全部实现）

| 阶段 | 内容 |
|------|------|
| **2** | 收包 Reader + Modbus PDU + 仪器写口统一（已完成第一版） |
| **3** | `Qadb` / `Qshell` 抽 `QProcessChannel`（已完成第一版） |
| **4** | 可选 `IProtocolCodec` 插件化，Manager 持 codec 指针 |

---

## 7. 新增 Dongle 相关协议字段 checklist

1. 在 `qprotocol_types.h` 补充 `DeviceCmd` / `Protocol*Data`（若需上报工站）。
2. 在对应协议 `set` / `get` / `parseCmd` 实现组帧（**不要**改 `QSerialChannel`）。
3. 发送统一 `QSerialChannel::write(serialPort, frame)` 或 `Qpb::writeSerial`。
4. 设置页绑定表增加 ini 键（若需可配置）。
5. 全量编译 + 串口抓包验证 TX/RX。

---

## 8. 相关文档

- [协议解耦迁移说明.md](./协议解耦迁移说明.md) — 上层 `Protocol*Data` 与 Qpb 映射
- [协议适配.md](./协议适配.md) / [协议处理.md](./协议处理.md) — 历史协议说明
- [协议适配改造方案.md](./协议适配改造方案.md) — 改造方案记录

---

*文档版本：与 qtransport 阶段 1 代码同步；如有接口变更请同步更新本节「已实现内容」。*
