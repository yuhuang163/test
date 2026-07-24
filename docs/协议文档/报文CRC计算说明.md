# 报文 CRC / 校验计算说明

本文档汇总上位机当前各协议报文的校验算法，以仓库实现为准。无 CRC 的协议（如部分治具仅尾字节 `0xAA`）也一并标注，避免误用。

---

## 总览

| 协议 / 场景 | 算法类型 | 代码入口 | 帧上字节序 | 覆盖范围 |
|-------------|----------|----------|------------|----------|
| Modbus RTU（HQ/LX 校验、多路温度记录仪等） | CRC16-Modbus | `QModbusPdu::crc16ModbusRtuBigEndian` | 低字节在前 | 地址 + PDU（不含 CRC） |
| ASD9026A 程控电源 | CRC16-Modbus | `Asd9026aCodec::crc16Modbus` | 低字节在前 | 模块地址～payload（不含 CRC） |
| FCTP（工厂通信协议） | CRC16（Nordic/CCITT 变体） | `crc16_compute` / `CRC16` | 低字节在前 | Header + Payload |
| QPB | 同上，但只取低 8 位 | `Qpb::calCrc16` | 单字节 | 有效载荷（不含帧头 AA AA、不含 CRC 字节） |
| 摄像头图片帧 | 同上（完整 16 位） | `crc16_compute` / `CRC16` | 字段内数值 | `data` 区 |
| Bulk（DJI USB） | CRC8 头 + CRC16 尾 | `DjiBulkCodec` | CRC16 低字节在前 | 见 §6 |
| 杰理蓝牙盒子 | CRC16-XMODEM | `crc16Xmodem` | 低字节在前（帧首） | LEN + body |
| Qroot | 8 位累加取反（非 CRC） | `Qroot::checksum8` | 单字节 | 帧头～参数末 |
| HZ PCBA 治具串口 | 无 CRC | 尾字节固定 `0xAA` | — | — |

---

## 1. Modbus RTU CRC16

### 1.1 算法参数

| 项 | 值 |
|----|----|
| 多项式（反射形式） | `0xA001`（对应正序 `0x8005`） |
| 初值 | `0xFFFF` |
| 输入/输出反射 | 是（按位右移实现） |
| 最终异或 | `0x0000` |
| 常见别名 | CRC-16/MODBUS |

### 1.2 实现要点

源码：`agreement/modbus_protocol/codec/qmodbus_pdu.cpp` → `crc16ModbusRtuBigEndian`。

```cpp
quint16 crc = 0xFFFF;
for (each byte) {
    crc ^= byte;
    for (8 bits) {
        if (crc & 1) crc = (crc >> 1) ^ 0xA001;
        else          crc >>= 1;
    }
}
// 返回值再做字节对调，便于与「高字节在前」读出的帧尾比较
return ((crc & 0xFF) << 8) | ((crc & 0xFF00) >> 8);
```

说明：

- 标准 Modbus 线上顺序是 **CRC 低字节在前、高字节在后**。
- 本函数返回值已对调：高字节 = CRC_L，低字节 = CRC_H。因此：
  - 校验时用 `readUint16Be(末两字节)` 与返回值比较即可（见 `validateRtuFrame`）；
  - 组帧时（如多路温度记录仪）按「先追加 `crcBe >> 8`，再追加 `crcBe & 0xFF`」得到标准低字节在前。

### 1.3 使用位置

- HQ / LX 电流表应答校验：`ModbusRtuCodec::parse*HoldRegisterFrame` → `validateRtuFrame`
- 多路温度记录仪组帧：`multi_temp_logger_rtu.cpp` → `appendRtuCrc`
- 开放报文 `SendRaw`：由调用方保证整帧已含正确 CRC

### 1.4 示例

请求体（不含 CRC）：`01 03 00 00 00 0A`  
CRC16-Modbus = `0xC5CD` → 线上：`01 03 00 00 00 0A CD C5`

---

## 2. ASD9026A（治具程控电源）

### 2.1 算法

与 §1 同一套 CRC16-Modbus 位运算，**但函数返回未对调的原生 CRC 值**。

源码：`agreement/fixture_protocol/asd9026a/codec/asd9026a_codec.cpp` → `crc16Modbus`。

### 2.2 组帧

```
[模块地址][功能码][命令码][数据长度][payload...][CRC_L][CRC_H]
```

- 覆盖：从模块地址到 payload 末字节。
- 追加：`crc & 0xFF`，再 `(crc >> 8) & 0xFF`。

### 2.3 与 Modbus 工具函数的关系

| 函数 | 返回值含义 | 线上追加方式 |
|------|------------|--------------|
| `Asd9026aCodec::crc16Modbus` | 原生 `0xLLHH`（低字节在低 8 位） | 先低后高 |
| `QModbusPdu::crc16ModbusRtuBigEndian` | 对调后 `0xHHLL`（高 8 位为 CRC_L） | 先高字节字段再低字节字段（等价于线上先低后高） |

二者算出的 **线上两字节相同**，只是 API 返回值表示不同。自由工站改 ASD 通道地址后会用 `Asd9026aCodec::crc16Modbus` 重算尾两字节。

---

## 3. FCTP（工厂通信协议）CRC16

### 3.1 算法

源码：`agreement/factory_protocol/codec/fctp/comm_protocol_defs.h` → `crc16_compute`。

```cpp
unsigned short crc = 0xFFFF; // 或传入初值做增量
for (each byte) {
    crc = (unsigned char)(crc >> 8) | (crc << 8);
    crc ^= p_data[i];
    crc ^= (unsigned char)(crc & 0xFF) >> 4;
    crc ^= (crc << 8) << 4;
    crc ^= ((crc & 0xFF) << 4) << 1;
}
```

| 项 | 说明 |
|----|------|
| 初值 | `0xFFFF`（宏 `CRC16`）；增量用 `CRC16_UPDATE` |
| 宽度 | 16 位 |
| 帧尾字节序 | **小端**：低字节在前 |

该写法与 Nordic SoftDevice / 工程内 QPB、图片帧同源，**不是** Modbus 的 `0xA001` 算法。

### 3.2 覆盖与落盘

- 覆盖：`Header(6) + Payload`，不含 Footer。
- 写入：`comm_protocol_builder.cpp` 在 footer 写 `checksum & 0xFF`、`(checksum >> 8) & 0xFF`。
- 校验：`comm_protocol_parser.cpp` 对 `frame_size - 2` 字节算 CRC，与帧尾两字节比较。

帧常量：`COMM_PROTOCOL_SOF = 0x5CC5`，`COMM_PROTOCOL_FOOTER_SIZE = 2`。

---

## 4. QPB 校验

### 4.1 算法

`Qpb::calCrc16` 与 §3 的 `crc16_compute` **公式相同**（初值 `0xFFFF`）。

源码：`agreement/factory_protocol/protocol/qpb/qpb.cpp`。

### 4.2 与 FCTP 的差异（重要）

| 项 | FCTP | QPB |
|----|------|-----|
| 结果宽度 | 完整 16 位写入帧尾 2 字节 | **只取低 8 位**写入末 1 字节（`uint8_t` / `tx_buffer.back()`） |
| 覆盖范围 | Header+Payload | 从载荷起始到 CRC 前（不含 `AA AA` 类帧头、不含 CRC 字节本身） |

收包处：`uint8_t crc16 = calCrc16(ipack); if (crc16 == x)` —— 用 16 位结果的低字节与收到的单字节比较。

发包处典型写法：

```cpp
tx_buffer[len + 1] = calCrc16(/* payload 区间 */);
```

---

## 5. 摄像头图片帧 CRC16

源码：`mainlogic.cpp` / `mainwindow.h` 宏 `CRC16` → `crc16_compute`（与 §3 相同）。

- 物理层头中有 `data_crc16` 字段。
- 计算：`CRC16(head->data, head->data_size)`，与头内字段比较。
- 使用完整 16 位，**不是** QPB 那种截断低 8 位。

---

## 6. Bulk（DJI USB）双校验

源码：`agreement/bulk_protocol/codec/bulk_codec.cpp`  
初值：`agreement/bulk_protocol/access/bulk_types.h`

| 校验 | 初值 | 覆盖 | 位置 |
|------|------|------|------|
| CRC8（头） | `DUSS_MB_PACKAGE_V1_CRCH_INIT = 0x77` | 前 3 字节（SOF + VerLen） | 第 4 字节（下标 3） |
| CRC16（尾） | `DUSS_MB_PACKAGE_V1_CRC_INIT = 0x3692` | 整包除末 2 字节 | 帧尾 2 字节，**低字节在前** |

实现为查表：

- `crc8_calc`：`crc = crc8_table[(data ^ crc)]`
- `duss_util_crc16_calc`：`init_crc = ((init_crc >> 8) & 0xFF) ^ crc16_table[(uint8_t)init_crc ^ byte]`

组包顺序：先填头三字节 → 算 CRC8 写入 → 填其余字段与 data → 再对当前整包算 CRC16 追加。

---

## 7. 杰理蓝牙盒子（CRC16-XMODEM）

源码：`agreement/fixture_protocol/jieli_bt_box/codec/jieli_bt_box_codec.cpp`。

### 7.1 算法

| 项 | 值 |
|----|----|
| 多项式 | `0x1021` |
| 初值 | `0` |
| 方向 | 左移（MSB first） |
| 别名 | CRC-16/XMODEM |

```cpp
quint16 crc = 0;
for (each byte) {
    crc ^= (byte << 8);
    for (8 bits) {
        if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
        else               crc <<= 1;
    }
}
```

### 7.2 帧结构

```
[CRC_L][CRC_H] [LEN_L][LEN_H] [MASK...][TLV...]
```

- CRC 覆盖：从 **LEN 起**共 `2 + bodyLen` 字节（不含 CRC 自身）。
- 当前实现：CRC 不匹配时 **仅打日志，仍抽帧解析**（注释：实测部分帧算法不一致）。

---

## 8. Qroot 校验和（非 CRC）

详见 `docs/协议文档/qroot协议.md` §1.3。  
实现：`agreement/factory_protocol/protocol/qroot/qroot.cpp` → `checksum8`。

```cpp
quint32 sum = 0;
for (each byte before checksum)
    sum += byte;
return (quint8)(~sum);
```

覆盖：`AA 55` + CT + CID + Len + Body；结果作为末字节。

---

## 9. 无 CRC 的相关报文

| 场景 | 校验方式 | 说明 |
|------|----------|------|
| HZ PCBA 治具串口 | 帧尾固定 `0xAA` | `pcba_uart_codec.cpp`，长度由帧内 `length` 声明 |
| SCPI / 部分文本设备 | 无二进制 CRC | 按文本行协议处理 |
| 开放十六进制 `SendRaw` | 调用方自带 | 上位机不做自动补 CRC（除非设备侧封装如温度记录仪读通道） |

---

## 10. 选型与易混点

1. **Modbus / ASD9026A**：同一多项式 `0xA001`；注意 API 返回值是否已字节对调。
2. **FCTP / QPB / 图片帧**：同一套 `crc16_compute` 公式；QPB 只发/收 **1 字节（低 8 位）**，另两处为完整 16 位小端或字段。
3. **杰理**：XMODEM（`0x1021`、初值 0），与 Modbus / FCTP 均不同。
4. **Bulk**：独立 CRC8+CRC16 查表与固定初值，不可与上述混用。
5. **Qroot**：累加取反，不是 CRC。

---

## 11. 源码索引

| 模块 | 路径 |
|------|------|
| Modbus CRC | `agreement/modbus_protocol/codec/qmodbus_pdu.cpp` |
| 温度记录仪组帧 | `agreement/modbus_protocol/device/multi_temp_logger_rtu/multi_temp_logger_rtu.cpp` |
| ASD9026A | `agreement/fixture_protocol/asd9026a/codec/asd9026a_codec.cpp` |
| FCTP | `agreement/factory_protocol/codec/fctp/comm_protocol_defs.h` |
| FCTP 组/拆帧 | `comm_protocol_builder.cpp` / `comm_protocol_parser.cpp` |
| QPB | `agreement/factory_protocol/protocol/qpb/qpb.cpp` |
| Qroot | `agreement/factory_protocol/protocol/qroot/qroot.cpp` |
| Bulk | `agreement/bulk_protocol/codec/bulk_codec.cpp` |
| 杰理盒子 | `agreement/fixture_protocol/jieli_bt_box/codec/jieli_bt_box_codec.cpp` |
| 图片帧 | `mainlogic.cpp`（`crc16_compute`） |

文档随代码变更时，以对应源文件为准更新本节与总览表。
