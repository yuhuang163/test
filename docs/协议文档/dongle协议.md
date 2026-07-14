# dongle 协议 — 路特 Dongle 串口通信

> 本文描述产测**上位机 ↔ Dongle 模块**之间的串口通信：**AT 文本指令**（控制蓝牙/WiFi/吸力等）与 **PHY 外层透传帧**（承载 qroot / qfctp 等产测内层包）。  
> 上位机实现：`agreement/at_protocol/`（AT）、`agreement/factory_protocol/protocol/qroot/`、`qfctp/`（PHY 透传）。  
> 内层产测帧格式见 [qroot协议.md](./qroot协议.md) 等，本文只写 Dongle 侧封装与 AT 约定。

---

## 1. 数据包格式

Dongle 与上位机共用**一根串口**。线上同时可能出现 **AT 文本行** 与 **二进制 PHY 帧**，上位机分别用 `AtLineCodec` / `AtSuctionFrameCodec` 与 qroot、qfctp 的 PHY 解析器处理。

---

### 1.1 PHY 外层帧（产测透传）

qroot、qfctp 发往 Dongle 的产测数据须先套 PHY 外层，再写入串口；Dongle 回包同样带 PHY 头，解包后得到内层协议（如 qroot 的 `AA 55` 帧）。

#### 1.1.1 上位机 → Dongle（发送）

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 8 | 发送帧头 | 固定 8 字节 `0xCC` |
| 8 | 1 | 载荷长度 | 内层包字节数，最大 `0xFF` |
| 9 | 1 | 通道号 | 见 §1.2 |
| 10 | 变长 | 内层载荷 | qroot / qfctp 等完整内层包 |
| — | — | 校验 | **无**；靠长度字段定界 |

#### 1.1.2 Dongle → 上位机（接收）

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 8 | 接收帧头 | 固定 8 字节 `0xAA` |
| 8 | 1 | 通道号 | 见 §1.2 |
| 9 | 1 | 载荷长度 | 内层包字节数 |
| 10 | 变长 | 内层载荷 | 解包后交 qroot / qfctp 解析 |

状态机：连续匹配 8 字节帧头 → 读通道 → 读长度 → 读满载荷字节 → 输出内层包。

**qpb** 产测走 Dongle 自有 BLE 分包格式，**不**经过本节 PHY 外层（见 §5）。

---

### 1.2 通道号

| 值 | 名称 | 说明 |
|----|------|------|
| `1` | 产测通道 | 默认；qroot 固定使用；qfctp 工厂指令 |
| `2` | App 通道 | qfctp App 透传 |
| `3` | 主通道 | qfctp Main 透传 |

qroot 仅接受通道 `1`；qfctp 接受 `1`、`2`、`3`。

---

### 1.3 AT 文本行格式

Dongle 控制指令与异步上报均为 **ASCII 文本行**，以 **`CRLF`（`\r\n`）** 结尾。

| 字段 | 说明 |
|------|------|
| 命令前缀 | 固定以 `AT` 开头 |
| 参数 | 可选；有参数时为 `命令=参数`，参数至行末（不含 `\r\n`） |
| 行结束 | `\r\n` |

**上位机 → Dongle** 示例：`AT+MAC=AA:BB:CC:DD:EE:FF\r\n`  
**Dongle → 上位机** 示例：`AT+CONNECT_SUCCESS\r\n`、`AT+BLERSSI=-65\r\n`

解析规则（`AtLineCodec`）：

- 从字符 `A` 起同步，识别 `AT` 后收命令名；
- 遇 `=` 进入参数区；
- 遇 `\r\n` 结束一行，拆成 **命令** + **参数** 两字段上报；
- 命令与参数仅允许可打印 ASCII（`0x20`～`0x7E`），单行最长约 1024 字符。

---

### 1.4 双通道吸力上报格式

开启吸力读取（`AT+SUCTION=1`）后，Dongle 可推送吸力数据，上位机支持两种格式：

| 格式 | 示例 | 说明 |
|------|------|------|
| AT 行 | `AT+SUCTION_DATA=左,右` 或 `AT+SUCTION_DATA=左,右,第三路` | 逗号分隔，单位 **kPa**；由 `DongleAtDevice` 解析 |
| Pico 兼容帧 | `$左 右 ...;` | `$` 与 `;` 之间提取数字；由 `AtSuctionFrameCodec` 解析 |

`AT+TEMP_DATA` 行当前上位机**忽略**。

---

## 2. 指令列表

### 2.1 上位机下发（AT 命令）

| 命令 | 说明 | 交互方式 |
|------|------|----------|
| `AT+MAC` | 按 MAC 扫描/连接 BLE；全 0 MAC 表示断开 | 下发 → 异步状态/扫描上报 |
| `AT+DCON` | 直连指定 MAC | 同上 |
| `AT+OTA` | OTA 通道 BLE 连接 | 同上 |
| `AT+BLE` | App 通道 BLE 连接 | 同上 |
| `AT+MAIN` | 主通道 BLE 连接 | 同上 |
| `AT+OTADATA` | OTA 数据透传开关 | 下发 |
| `AT+OTAPKTSIZE` | OTA 切包字节数 | 下发 |
| `AT+BLEMTU` | BLE MTU 字节数 | 下发 |
| `AT+MAINDATA` | 主通道数据透传开关 | 下发 |
| `AT+BLELOG` | Dongle BLE 日志开关 | 下发 |
| `AT+SUCTION` | 双通道吸力读取开关 | 下发 → 持续 `SUCTION_DATA` 上报 |
| `AT+ADC` | ADC 上报开关 | 下发 |
| `AT+BLEDEVICELOG` | 被连设备 BLE 日志开关 | 下发 |
| `AT+BOMB` | 广播注入（测试用） | 下发 |
| `AT+GMAC` | 读取 Dongle GMAC | 查询 → 设备应答（若固件支持） |

**说明**：`BleScanConnectByName`（按广播名连接）为**上位机工站逻辑**（轮询 `ATCH` 扫描结果后改发 `AT+MAC`），**无**单独 AT 命令字。

---

### 2.2 Dongle 主动上报（异步）

| 命令 | 说明 |
|------|------|
| `AT` | 帮助/存活（调试） |
| `AT+CONNECT_SUCCESS` | BLE 连接成功 |
| `AT+DISCONNECT` | BLE 断开 |
| `AT+BLERSSI` | BLE RSSI，参数以 `-` 开头 |
| `AT+DONGLEVER` | Dongle 固件版本 |
| `AT+DEVICENAME` | Dongle 设备名称 |
| `AT+WIFINAME` | WiFi SSID 相关 |
| `AT+WIFIRSSI` | WiFi RSSI（同时视为 WiFi 已连接） |
| `AT+WIFI_CONNECT_SUCCESS` | WiFi 连接成功 |
| `AT+WIFI_DISCONNECT` | WiFi 断开 |
| `AT+WIFI_DATA` | WiFi 透传数据 |
| `AT+WIFIIP` | WiFi IP |
| `ATCH` | BLE 扫描结果 |
| `AT+SUCTION_DATA` | 双通道吸力（见 §1.4） |

---

## 3. 指令说明

以下「上位机 → Dongle」为下发的 AT 行；「Dongle → 上位机」为异步上报或连接结果。  
表列为 **字段 | 值 | 说明**。

---

### 3.1 `AT+MAC` — 扫描/连接 BLE

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 命令 | `AT+MAC` | — |
| MAC 地址 | `AA:BB:CC:DD:EE:FF` | 目标设备 |
| MAC 地址 | `00:00:00:00:00:00` | 断开当前 BLE |

行示例：`AT+MAC=C4:27:8C:9D:97:77\r\n`

**Dongle → 上位机**

| 字段 | 值 | 说明 |
|------|-----|------|
| `AT+CONNECT_SUCCESS` | 无参 | BLE 已连接；上位机置连接态 |
| `AT+DISCONNECT` | 无参 | BLE 已断开 |
| `ATCH` | 见 §3.15 | 扫描过程中上报周边设备 |

---

### 3.2 `AT+DCON` / `AT+OTA` / `AT+BLE` / `AT+MAIN` — 各通道连接

**上位机 → Dongle**

| 命令 | 参数 | 说明 |
|------|------|------|
| `AT+DCON` | MAC | 直连 |
| `AT+OTA` | MAC | OTA 服务连接 |
| `AT+BLE` | MAC | App 通道连接 |
| `AT+MAIN` | MAC | 主通道连接 |

连接成功/失败上报同 §3.1（`CONNECT_SUCCESS` / `DISCONNECT`）。

---

### 3.3 `AT+OTADATA` — OTA 数据透传

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 开关 | `0`、`1` | 关、开 |

行示例：`AT+OTADATA=1\r\n`

---

### 3.4 `AT+OTAPKTSIZE` — OTA 切包大小

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 字节数 | 整数 | 如 `200` |

行示例：`AT+OTAPKTSIZE=200\r\n`

---

### 3.5 `AT+BLEMTU` — BLE MTU

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 字节数 | 整数 | 如 `247` |

行示例：`AT+BLEMTU=247\r\n`

---

### 3.6 `AT+MAINDATA` — 主通道透传

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 开关 | `0`、`1` | 关、开 |

---

### 3.7 `AT+BLELOG` / `AT+BLEDEVICELOG` — 日志开关

**上位机 → Dongle**

| 命令 | 开关 | 说明 |
|------|------|------|
| `AT+BLELOG` | `0`、`1` | Dongle 侧 BLE 日志 |
| `AT+BLEDEVICELOG` | `0`、`1` | 被连设备 BLE 日志 |

---

### 3.8 `AT+SUCTION` — 吸力读取开关

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 开关 | `0`、`1` | 关、开 |

**Dongle → 上位机**（开关为 `1` 时持续上报）

| 字段 | 值 | 说明 |
|------|-----|------|
| 命令 | `AT+SUCTION_DATA` | — |
| 左通道 | 浮点字符串 | 单位 kPa |
| 右通道 | 浮点字符串 | 单位 kPa |
| 第三路 | 可选 | 部分固件三路 |

行示例：`AT+SUCTION_DATA=12.34,56.78\r\n`

---

### 3.9 `AT+ADC` — ADC 上报开关

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 开关 | `0`、`1` | 关、开 |

---

### 3.10 `AT+BOMB` — 广播注入

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 设备名 | — | 广播名称 |
| RSSI | — | 信号值 |
| 连接间隔 | — | 连接参数 |
| 命令 | — | 注入内容 |

行示例：`AT+BOMB=GT 5-777,-84,20,01\r\n`（四段逗号分隔，以固件为准）

---

### 3.11 `AT+GMAC` — 读 GMAC

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 命令 | `AT+GMAC` | 无 `=` 参数 |

行示例：`AT+GMAC\r\n`

应答格式依 Dongle 固件；上位机 `get(DongleCmd::GetGmac)` 仅发送该命令。

---

### 3.12 `AT+BLERSSI` — BLE 信号强度（上报）

**Dongle → 上位机**

| 字段 | 值 | 说明 |
|------|-----|------|
| 命令 | `AT+BLERSSI` | — |
| RSSI | 以 `-` 开头的整数 | 如 `-65` |

---

### 3.13 `AT+DONGLEVER` / `AT+DEVICENAME` / `AT+WIFINAME`

**Dongle → 上位机**

| 命令 | 参数 | 说明 |
|------|------|------|
| `AT+DONGLEVER` | 版本字符串 | Dongle 固件版本 |
| `AT+DEVICENAME` | 名称 | Dongle 设备名 |
| `AT+WIFINAME` | SSID 等 | WiFi 信息 |

---

### 3.14 WiFi 状态与数据

**Dongle → 上位机**

| 命令 | 参数 | 说明 |
|------|------|------|
| `AT+WIFI_CONNECT_SUCCESS` | 无 | WiFi 已连接 |
| `AT+WIFI_DISCONNECT` | 无 | WiFi 已断开 |
| `AT+WIFIRSSI` | RSSI 字符串 | WiFi 信号；上位机同时置 WiFi 已连接 |
| `AT+WIFI_DATA` |  payload | WiFi 透传数据 |
| `AT+WIFIIP` | IP 字符串 | 获取到的 IP |

---

### 3.15 `ATCH` — BLE 扫描结果

**Dongle → 上位机**

| 字段 | 值 | 说明 |
|------|-----|------|
| 命令 | `ATCH` | — |
| 广播名 | 第一段 | 如 `GT 5-777` |
| 设备地址 | `deviceAddress:` 前缀 | 如 `c4:27:8c:9d:97:77` |
| 信号 | `deviceRssi:` 前缀 | 如 `-84` |

行示例：`ATCH GT 5-777, deviceAddress:c4:27:8c:9d:97:77, deviceRssi:-84\r\n`

上位机解析为 `ProtocolDongleScanResultData`，供按名称选 MAC（`BleScanConnectByName` 工站步骤）使用。

---

### 3.16 PHY 透传 — 发送产测内层包

**上位机 → Dongle**

| 字段 | 值 | 说明 |
|------|-----|------|
| 发送帧头 | 8×`CC` | 见 §1.1.1 |
| 载荷长度 | 内层字节数 | ≤ 255 |
| 通道号 | `01`（产测）等 | qroot 固定 `01` |
| 内层载荷 | 二进制 | 如 qroot 帧 `AA 55 ...` |

**Dongle → 上位机**

| 字段 | 值 | 说明 |
|------|-----|------|
| 接收帧头 | 8×`AA` | 见 §1.1.2 |
| 通道号 | `01` 等 | — |
| 载荷长度 | 内层字节数 | — |
| 内层载荷 | 二进制 | 交 qroot / qfctp 解帧 |

---

## 4. 上位机 DeviceCmd 对照

工站 `DeviceCmd` / `DongleCmd` 与 AT 行映射见 `platform/test_case/manifest/dongle_cmd_manifest.cpp` 与 `dongle_at_device.cpp`。

| DeviceCmd | AT 命令 |
|-----------|---------|
| `BleScanConnect` | `AT+MAC=` |
| `BleDirectConnect` | `AT+DCON=` |
| `BleOtaConnect` | `AT+OTA=` |
| `BleAppConnect` | `AT+BLE=` |
| `BleMainConnect` | `AT+MAIN=` |
| `OtaDataPassthrough` | `AT+OTADATA=` |
| `OtaPktSize` | `AT+OTAPKTSIZE=` |
| `BleMtu` | `AT+BLEMTU=` |
| `MainDataPassthrough` | `AT+MAINDATA=` |
| `BleLog` | `AT+BLELOG=` |
| `GetSuction` | `AT+SUCTION=` |
| `AdcSwitch` | `AT+ADC=` |
| `BleDeviceLog` | `AT+BLEDEVICELOG=` |
| `Bomb` | `AT+BOMB=` |
| `GetGmac` | `AT+GMAC` |

自定义 AT：经 `sendCustomMessage`，字段 `line` 整行、或 `at` + `value`、或 `bomb` 对象。

---

## 5. 与产测内层协议的关系

```text
上位机                    Dongle 串口                    被测设备（BLE）
  │                                                                 │
  ├─ AT+MAC / AT+BLE … ──► 控制连接、WiFi、吸力等                    │
  │                                                                 │
  ├─ [8×CC][len][ch][内层] ──► 透传 ──► BLE ──► qroot/qfctp 内层帧   │
  │◄─ [8×AA][ch][len][内层] ── 回传 ──                               │
  │                                                                 │
  └─ qpb 自有分包 ──────────► 不经过 §1.1 PHY 头                      │
```

| 内层协议 | 经 Dongle PHY | 文档 |
|----------|---------------|------|
| qroot | 是（通道 1） | [qroot协议.md](./qroot协议.md) |
| qfctp | 是（通道 1/2/3） | 见 factory_protocol 与 FCTP 原厂说明 |
| qpb | 否（Dongle BLE 分包） | PB 产测协议 |
| qaiot | 依产品线实现 | — |

同一串口上 **AT 文本** 与 **PHY 二进制帧** 可能交替出现；上位机 `QatManager::parseCmd` 对同一字节流并行喂给 AT 解析与吸力解析，产测协议在各自 `parseCmd` 中先解 PHY 再解内层。
