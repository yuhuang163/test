---
name: protocol-docs
description: 生成 / 整理协议文档。当用户要"生成协议文档""写协议文档""整理/补全/输出 XX 协议说明""根据代码出协议文档""写 fwq/上位机 接口文档"等时使用。覆盖上位机串口/产测/治具/AT/Modbus/SCPI/产品串口协议与 fwq 云端 REST 协议，产出 docs/协议文档/ 下符合现有风格的简体中文 .md（帧结构 / 命令字 / 字段表 / 校验 / 字节序 / 源码索引）。
---

# 生成协议文档

根据**代码实现**或**外部规范**，生成符合 `docs/协议文档/` 现有风格、覆盖上位机与 fwq 平台的协议文档 `.md`。

## 0. 读取注意

- 源码（`.cpp/.h`）与已有文档（`.md`）被公司透明加密，**用 Bash `cat` / `sed -n '行号p'` / `grep` 读**，不要用 `Read` 工具（会乱码）。
- 产出用 `Write` 写文件后，**用 `perl -pi -e 's/\r?\n/\r\n/g' <文件>` 转 CRLF**（UTF-8 无 BOM），与仓库 `.md` 约定一致。

## 1. 目标与范围

本 skill 生成两类协议文档：

| 类型 | 覆盖对象 | 已有文档示例 |
|------|---------|-------------|
| **上位机串口 / 帧协议** | 产测（qroot/qaiot/qfctp）、Dongle AT、治具（杰理/ASD9026A 等）、Modbus RTU、SCPI、产品串口仪器、ADB、shell、bulk | `qroot协议.md`、`dongle协议.md`、`杰理蓝牙测试盒MES_LOG协议.md` |
| **fwq 云端 REST 协议** | 上位机 ↔ 工厂数据平台 HTTP 接口 | `fwq通信协议.md` |
| **校验算法说明**（可选） | 跨协议的 CRC/校验和汇总 | `报文CRC计算说明.md` |

## 2. 输出规范

- **位置**：`docs/协议文档/<协议名>.md`；校验算法类固定写进已有 `报文CRC计算说明.md`（追加小节），不另开文件。
- **命名**：与现有一致——`<协议名>协议.md`、`<产品>-<用途>协议规范.md`、`<设备>通信协议.md` 等。
- **编码**：UTF-8 无 BOM + CRLF；**语言简体中文**；代码/枚举/字段名保留原文。

## 3. 生成前准备

1. **确认类型**：串口二进制帧 / AT 文本 / HTTP REST / 校验算法。
2. **读实现代码**（从代码生成时）：按 §4 映射定位 `agreement/...` 的 `.h`（枚举=命令字、常量=帧头/上限）与 `.cpp`（组帧 / 解析 / 校验函数）。
3. **读同类型已有文档做风格参照**：先 `sed -n '1,120p'` 看一份同类文档的表格与措辞，再照其排版写。

## 4. 代码 → 文档映射（信息提取来源）

| 协议 | 代码位置 | 提取要点 |
|------|---------|---------|
| qroot（AA55 帧） | `agreement/factory_protocol/protocol/qroot/qroot.h/.cpp` | `CommandId`/`CommandType` 枚举、`checksum8`、`buildPacket`、`set`/`get`/`handleFrame` |
| FCT&ATE / AIOT | `agreement/factory_protocol/protocol/qaiot/` + `codec/aiot/` | 分层（PHY 8×CC → SOF 0x5A → Service+Command+TLV）、Service ID、CID 枚举 |
| qfctp（FCTP） | `agreement/factory_protocol/protocol/qfctp/` + `codec/fctp/` | comm_protocol 组帧/解析 |
| Dongle（AT + PHY 透传） | `agreement/at_protocol/` + `factory_protocol/access/dongle_phy*` | AT 命令字、PHY 外层帧、通道号 |
| fwq 云端 REST | `platform/cloud/client/factory_cloud_client.cpp`（客户端）+ `D:\code\fwq\factory-api\`（服务端） | 接口路径、方法、请求/响应 JSON、鉴权、错误码 |
| 治具 | `agreement/fixture_protocol/`（`jieli_bt_box`/`asd9026a`/`hz_fixture`/`xwd_fixture`/`uart_fixture`） | 帧格式、校验 |
| Modbus RTU | `agreement/modbus_protocol/` | 从站/功能码、CRC16-Modbus |
| SCPI | `agreement/scpi_protocol/` | 指令文本、设备（会凌 WFP60H、罗德 CMW100） |
| 产品串口仪器 | `agreement/product_protocol/`（帧格式见 `docs/测试.md`） | 复位/收包指令 |
| ADB / shell / bulk | `agreement/adb_protocol/`、`shell_protocol/`、`bulk_protocol/` | 命令字枚举 |

## 5. 文档结构模板

### 5.1 串口 / 帧协议（qroot / dongle / 治具 / FCT&ATE）

```markdown
# <协议名>

<一段概述：适用设备 / 传输方式 / 用途，1~3 句。>

## 1. 数据包格式
### 1.1 帧结构        ← 表格：偏移 | 长度 | 字段 | 说明；帧头、长度、校验逐行写清
### 1.2 命令类型       ← 表格：值 | 名称 | 说明（Req/Ack/Nack/Notify 等）
### 1.3 校验和/CRC     ← 给算法 + ```c 代码块（含覆盖范围）
### 1.4 字节序说明     ← 16 位小端、MAC 反序、字符串截断上限等

## 2. 命令字列表        ← 表格：命令字 | 说明 | 交互方式
## 3. 命令说明
### 3.x 0xNN — <命令名>   ← 每个命令一节：请求字段表 + 应答字段表 + 示例字节
## 4. 与上位机实现对应关系 / 源码索引   ← 类名、文件、枚举位置
```

> 参考：`qroot协议.md`（帧+命令字+逐命令）、`dongle协议.md`（PHY 外层 + AT 指令 + 指令说明）、`杰理蓝牙测试盒MES_LOG协议.md`（含范围/接线/示例/验证/常见问题）。

### 5.2 fwq 云端 REST 协议

```markdown
# <平台> 通信协议 — <路径前缀> REST API

## 1. 通信约定
### 1.1 连接与路径前缀     ← BaseUrl、前缀（如 /api/factory-tool）
### 1.2 JSON 响应包络      ← 业务码/消息/数据统一结构 + ```json 示例
### 1.3 请求头            ← 通用 + 上位机建议携带（对应 FactoryCloudClient）
### 1.4 鉴权              ← Token 获取/携带、过期
### 1.5 错误码            ← 表格：业务码 | 说明

## 2. 接口列表             ← 表格：路径 | 方法 | 说明 | 主要调用方 | 鉴权
## 3. 接口说明
### 3.x /path — 名称      ← 每个接口一节：上位机→平台 / 平台→上位机 字段表 + 请求/响应 ```json 示例
## 4. 上位机配置项          ← 相关 SETTINGS 键（如 FactoryCloud/Token、Tuple/BaseUrl）
## 5. 与设备协议的关系
```

> 参考：`fwq通信协议.md`。字段表约定 `| 字段 | 值 | 说明 |`，JSON 字段名与代码一致（camelCase）。

### 5.3 校验算法说明（追加进已有文档）

```markdown
## N. <协议名>（算法名）
### N.1 算法 / 参数
### N.2 实现要点 / 覆盖范围
### N.3 使用位置         ← 代码文件
### N.4 示例             ← 输入字节 → 校验结果
```

## 6. 写作规范（与现有文档一致）

- **帧字段表**统一 `| 偏移 | 长度 | 字段 | 说明 |`；**命令/接口总表**统一 `| 命令字/路径 | 说明 | 交互方式/调用方 |`。
- **校验和/CRC** 单独小节并附可编译的 ```c 代码块，写明「覆盖到哪个字节、不含哪个」。
- **字节序** 单独小节，16 位小端、MAC 反序、字符串 UTF-8 截断上限等一一写清，避免实现时踩坑。
- **示例字节/请求体** 给出具体示例（`AA 55 01 90 ...` 或 ```json），便于对拍。
- **结尾必附「源码索引 / 与上位机实现对应关系」**，指向类名、文件、枚举，方便回头改代码。
- 只写**有依据**（代码或规范）的内容；命令字、字段名、错误码与代码枚举/常量保持一致，发现文档与代码不一致时以代码为准并在文中标注。
- 不要在正文堆大段无关历史；不要为单条命令拆过多空行（与仓库 `.md` 空行风格一致）。

## 7. 生成流程

1. 确认协议类型与文档名 → 查 §2 定输出位置。
2. 读代码（§4 映射）+ 读同类文档风格（§3）。
3. 按 §5 对应模板写 `.md`，逐命令/逐接口补齐字段表与示例。
4. `perl -pi -e 's/\r?\n/\r\n/g' <文件>` 转 CRLF。
5. 自查 §8。

## 8. 输出前自查

- 类型判断对不对（串口帧 / AT / REST / 校验算法），模板选对没有。
- 覆盖了上位机协议 + fwq 协议中**本次要求**的范围，没漏命令/接口。
- 命令字 / 字段 / 错误码与**代码枚举一致**，字节序 / 校验 / 截断上限已写明。
- 有示例字节 / 请求体，结尾有源码索引。
- 文件位置、命名、UTF-8 无 BOM + CRLF、简体中文均符合 §2。
