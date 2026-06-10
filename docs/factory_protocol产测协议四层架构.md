# factory_protocol 产测协议四层架构



本文说明 **dongle 产测协议域**（`agreement/factory_protocol/`）的分层设计与目录落位，对应概念：



- **协议虚函数层** — 统一管理协议接口  

- **访问层** — 为上层提供统一命令字与数据结构  

- **管理协议层** — 绑定实例、切换协议、转发调用  

- **协议层（字符/功能）** — 具体协议组帧、解析与 `DeviceCmd` 实现  



与 [agreement按外设域拆分重构方案.md](./agreement按外设域拆分重构方案.md)（按外设域拆分、迁移阶段）互补；与 [协议解耦迁移说明.md](./协议解耦迁移说明.md)（`Protocol*Data` 信号体系）互补。



---



## 1. 四层 ↔ 目录对照



| 概念层 | 职责 | 路径 / 类 |

|--------|------|-----------|

| **协议虚函数层** | 所有产测实现同一套虚接口，上行统一 `reportReceived` | `access/qprotocol.h` → **`qProtocol`**：`parseCmd` / `set` / `get` / `sendCustomMessage` |

| **访问层** | 统一命令字、上报信封、各协议共用数据结构 | `access/qprotocol_types.h` → **`DeviceCmd`**、`Protocol*Data`、`ProtocolReport` |

| **管理协议层** | 绑定 qpb/qfctp/qaiot/qroot，按配置切换，转发 `set/get/parseCmd` | `manager/qprotocolmanager.*` → **`QProtocolManager`** |

| **协议层（功能）** | 继承 `qProtocol`，把 `DeviceCmd` 映射到具体字节/TLV/PB | `protocol/qpb/`、`qfctp/`、`qaiot/`、`qroot/` **仅此四者** |



**适用范围**：仅 **dongle 产测多协议切换**（qpb / qfctp / qaiot / qroot）。



**不在本域内**：



- **业务封装**：仓库根 `business/`（`plc_v3_fixture`、`tuple`、`ble_ota`），见重构方案 §3.2。
- **工具服务**：`qbulk`、`qadb`、`qshell`、`qmes` 仍 `agreement/q*`。

- **外设域**：VISA、治具 UART、USB 电流表、Modbus PLC 等不继承 `qProtocol`。



---



## 2. 目录结构（阶段 1）



```text

agreement/factory_protocol/

├── access/              # 虚函数层 + 访问层

│   ├── qprotocol.h

│   └── qprotocol_types.h

├── manager/             # 管理协议层

│   ├── qprotocolmanager.h

│   └── qprotocolmanager.cpp

├── codec/               # 纯字符编解码（无业务、无串口）

│   └── fctp/            # comm_protocol_*（FCTP 帧组包/解析）

└── protocol/            # 四协议实现（功能层）

    ├── qpb/

    ├── qfctp/

    ├── qaiot/

    └── qroot/

```



工站调用方式 **不变**：



- `protocolManager.set(DeviceCmd::…)` / `get` / `parseCmd`

- 设置页 `SYSTEM/ProtocolType` 切换 `Qpb` / `Qfctp` / `Qaiot` / `Qroot`



---



## 3. 服务层与产测域的边界（BLE OTA 等）



| 模块 | 路径 | 与 `factory_protocol` 关系 |

|------|------|---------------------------|

| V3 治具 PLC | `business/plc_v3_fixture/` | `PlcV3Facade::run`，不经 `protocolManager` |

| 三元组 | `business/tuple/` | `QTupleService` |

| **BLE OTA** | `business/ble_ota/` | `RootBleOtaClient` |

| 真 USB Bulk | `agreement/qbulk/` | 工具类 |

| ADB / Shell | `agreement/qadb/`、`qshell/` | 工具类 |

| MES | `agreement/qmes/` | HTTP/各厂 SDK |

**勿混淆**：`protocol/qpb/ble_protocol` 是产测 PB 定义；`business/ble_ota` 是 OTA 业务封装。



---



## 4. 调用链（以 FCTP 为例）



```text

工站 / test_case

    ↓  protocolManager.set(DeviceCmd::XXX, data)

QProtocolManager::set()          // 管理协议层：转发到当前协议实例

    ↓

Qfctp::set()                       // 协议层（功能）：命令 → Service/TLV

    ↓

comm_protocol_builder_*            // codec/fctp（字符）：组帧、CRC

    ↓

QSerialPort / SerialChannel        // 驱动层（platform/driver/serial）

```



收包方向：



```text

串口 readyRead → parseCmd(QByteArray)

    → Qfctp::parseCmd → codec 拆帧

    → handleRsp* → Protocol*Data

    → emit reportReceived(ProtocolReport)

```



---



## 5. FCTP 的「字符」子层



FCTP 在「协议层」下再拆一层 **纯 codec**，与业务 `Qfctp` 分离：



| 子层 | 路径 | 内容 |

|------|------|------|

| **FCTP 帧 codec** | `codec/fctp/comm_protocol_*` | 组包/拆包、TLV、CRC；**无** `DeviceCmd`、**无** 串口 |

| **FCTP 业务协议** | `protocol/qfctp/qfctp.*` | `DeviceCmd` → 调 codec 发帧；收帧 → 填 `Protocol*Data` 并 `emitReport` |



其它协议字符编解码位置（阶段 1 **未** 强行统一到 `codec/`）：



| 协议 | 字符编解码位置 |

|------|----------------|

| **qpb** | `protocol/qpb/` 旁 nanopb 生成代码与 `qpb.cpp` 内编解码 |

| **qaiot** | `protocol/qaiot/` 内实现 |

| **qroot** | `protocol/qroot/` 内实现（见 [Root吸奶器PCBA串口测试协议.md](./Root吸奶器PCBA串口测试协议.md)） |



---



## 6. 访问层与管理协议层的区别



两者都「方便上层调用」，职责不同：



| | **访问层** | **管理协议层** |

|---|------------|----------------|

| **回答的问题** | 上层能发哪些命令？回包长什么样？ | 当前用的是哪一种协议？谁去执行？ |

| **典型类型** | `DeviceCmd`、`ProtocolSnData`、`ProtocolReport` | `QProtocolManager`、`ProtocolType` |

| **工站是否直接 include** | 通过 `qprotocol_types.h` 使用枚举与结构 | 通过 `protocolManager` 单例/成员调用，一般不直接 `new Qfctp` |



---



## 7. 阶段 1 已做 / 未做



### 已做（目录对齐 + 搬迁）



- `agreement/qProtocol/` → `agreement/factory_protocol/`（access / manager / codec/fctp / protocol/qpb|qfctp|qaiot|qroot）

- FCTP 原 `common_protocl` → `codec/fctp/`

- BLE OTA 在根目录 **`business/ble_ota/`**（不进 `factory_protocol`）

- `.pro` 的 `INCLUDEPATH` 与 `SOURCES`/`HEADERS` 已更新；工站仍用 `protocolManager` + `DeviceCmd`

- `QProtocolManager::bindQroot` 等需完整类型头（避免 C2664）



### 未做（后续阶段）



- `Qfctp` / `Qpb` **类内部**再拆小模块（如按 Service 分文件）— 当前仍以较大 `.cpp` 为主

- **驱动层**统一迁到 `platform/driver/serial`（阶段 2）

- **qusb**、**Modbus**、**VISA**、治具等其它域（见重构方案阶段 3～6）

- 未强行把 qpb 的 nanopb 全部挪到 `codec/nanopb/`（可后续按需整理）



---



## 8. 与其它模块的边界



| 场景 | 应使用的入口 | 域 |

|------|--------------|-----|

| Dongle 读 SN / RSSI / 写产测命令等 | `protocolManager.set/get` | `factory_protocol` |

| BLE 图片/资源 OTA | `RootBleOtaClient` + 发送回调 | `business/ble_ota` |

| 三元组上报 | `QTupleService` | `business/tuple` |

| PLC 气缸 / 寄存器 | `modbusManager` + `PlcCmd` | `modbus`（勿与 `DeviceCmd` 混用） |

| 串口电流表供电 | `Qusb::sendPowerInstruction` 等 | `device/serial_ammeter`（阶段 6 拆 qusb） |

| 产品 COM 固定帧 | `product->writeRaw` / 专用步骤 | `product_serial` |

| 治具 UART | `fixture_uart` + fixture codec | `fixture` |



**注意**：`QModbusManager.useBackend(UsbRtu)` **不是** 切换 USB 电流表的推荐用法；PLC 与 USB 电流并行，电流走 `Qusb` 门面。



---



## 9. 相关文档



- [agreement按外设域拆分重构方案.md](./agreement按外设域拆分重构方案.md) — 全仓按域拆分与迁移顺序  

- [agreement通讯分层与qtransport说明.md](./agreement通讯分层与qtransport说明.md) — 传输层与协议层分离（`qtransport`）  

- [协议解耦迁移说明.md](./协议解耦迁移说明.md) — `Protocol*Data` 与工站信号  

- [基于 TLV 的 BLE OTA 图片资源升级协议.md](./基于 TLV 的 BLE OTA 图片资源升级协议.md) — `qble_ota` 协议说明  

- [Root吸奶器PCBA串口测试协议.md](./Root吸奶器PCBA串口测试协议.md) — qroot 产测协议说明  


