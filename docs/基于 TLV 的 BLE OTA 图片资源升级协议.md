# 基于 TLV 的 BLE OTA 图片资源升级协议

## 1. 协议栈架构

| 层级 | 名称 | 职责 |
| --- | --- | --- |
| **数据链路层** | 帧协议 | 提供可靠帧传输：帧起始、序号、类型、长度、**CRC16 校验（覆盖整个帧）**，保证单帧无差错 |
| **应用层** | OTA 协议 | 定义升级业务逻辑：块划分、镜像 **CRC16 边收边累加**、断点续传、块完成确认 |

*   数据链路层向上层提交的每个 Payload 都是经过 CRC16 校验通过的完整业务数据包。
    
*   应用层不再对单帧做额外校验，仅对镜像整体进行 CRC16 完整性验证。
    

---

## 2. 数据链路层帧格式

| 区域 | 字段 | 字节数 | 值 / 说明 | 字节序 |
| --- | --- | --- | --- | --- |
| Header | SOF | 2 | `0x5CC5`（小端存储为 `0xC5 0x5C`） | 小端 |
|  | SEQ | 1 | 0~255，每发送一帧递增，用于重传检测 | 小端 |
|  | FrameType | 1 | `0x00` = REQ（请求）<br>`0x01` = RESP（响应）<br>`0x02` = Notify（通知） | 小端 |
|  | Length | 2 | Payload 的字节数（不包含 CRC 字段） | 小端 |
| Payload | 业务数据 | Length | **Server ID + Server ID 长度 +TLV 命令**（见第 3 节） | \- |
| Footer | Checksum | 2 | **CRC-16/** 覆盖整个 Header + Payload | 小端 |

**链路层处理**：

*   发送端：计算 CRC16 并附加到帧尾。
    
*   接收端：校验 CRC16，若错误则丢弃该帧；若正确则提取 Payload 提交给应用层。
    

---

## 3. OTA 应用层协议

### 3.1 Payload 通用格式

每个 Payload 由 **Server ID、Server ID 长度字段** 和后续 **TLV 命令** 组成：

| 字段 | 字节数 | 值 | 说明 |
| --- | --- | --- | --- |
| Server ID | 1 | `0x06` | 标识该包为 OTA 业务包 |
| Server ID Len | 1 | `0x01` | Server ID 字段的字节数（固定为 1） |
| TLV 命令 | 变长 |  | Type(1) + Length(2) + Value(Length) |

**TLV 中的 Length**：Value 的字节数，**小端序**。

> 整个 Payload 的长度 =  1 (Server ID) +1 (Server ID Len) + (1+2+ValueLen)。数据链路层的 `Length` 字段就是这个总长度。

### 3.2 命令类型定义（独立请求/应答）

| Type | 命令名称 | 方向 | Value 长度 | 说明 |
| --- | --- | --- | --- | --- |
| 0x01 | NEGOTIATE\_BS\_REQ | 手机→设备 | 2 | 请求协商块大小 |
| 0x02 | NEGOTIATE\_BS\_RESP | 设备→手机 | 2 | 响应协商块大小 |
| 0x03 | START\_OTA\_REQ | 手机→设备 | 18 | 请求开始升级，携带镜像 CRC16 等元数据 |
| 0x04 | START\_OTA\_RESP | 设备→手机 | 3 | 响应开始升级（确认就绪并告知下一个块号） |
| 0x05 | BLOCK\_DATA | 手机→设备 | 可变 (≥4) | 传输一个块的一个片段 |
| 0x06 | BLOCK\_COMPLETE | 手机→设备 | 2 | 通知设备：一个块的所有片段已发送完成 |
| 0x07 | BLOCK\_COMPLETE\_ACK | 设备→手机 | 2 | 对 BLOCK\_COMPLETE 的应答，确认块已处理 |
| 0x08 | END\_OTA\_REQ | 手机→设备 | 4 | 请求结束升级 |
| 0x09 | END\_OTA\_RESP | 设备→手机 | 2 | 响应结束升级（成功/失败） |
| 0x0A | NACK | 设备→手机 | 3 | 否定应答，请求重传块或报告失败 |

### 3.3 各命令的 Value 详细结构（小端序）

#### 3.3.1 NEGOTIATE\_BS\_REQ (Type=0x01) – Value 长度 2 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | SuggestedBlockSize | 2 | 手机建议的块大小（0 表示询问） |

#### 3.3.2 NEGOTIATE\_BS\_RESP (Type=0x02) – Value 长度 2 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | MaxBlockSize | 2 | 设备支持的最大块大小 |

**协商流程**：

1.  手机发送 `NEGOTIATE_BS_REQ`（建议值或 0）。
    
2.  设备回复 `NEGOTIATE_BS_RESP`（自身最大值）。
    
3.  手机选取实际块大小 = `min(建议值, 设备最大值)`，用于后续 `START_OTA_REQ` 和 `BLOCK_DATA`。
    

#### 3.3.3 START\_OTA\_REQ (Type=0x03) – Value 长度 18 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Image ID | 4 | 升级目标标识（例如 0x00000006 表示图片资源） |
| 4 | Total Blocks | 2 | 整个镜像划分的总块数 |
| 6 | Block Size | 2 | 协商后的实际块大小（字节） |
| 8 | Total Size | 4 | 整个镜像的原始字节数 |
| 12 | Version | 4 | 目标版本号（如 0x00010001） |
| 16 | Image CRC32 | 4 | 整个镜像的 CRC32 预期值 |

**设备处理**：

*   保存 `Image CRC32`。
    
*   初始化镜像 CRC32 累加器为 `0xFFFFFFFF`。
    
*   若接受升级，回复 `START_OTA_RESP` 携带 `Result=0x00` 和 `NextBlock=0`（或断点续传时的块号）；若资源不足等原因拒绝，回复 `Result=0x01`。
    

#### 3.3.4 START\_OTA\_RESP (Type=0x04) – Value 长度 3 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Result | 1 | `0x00` = 接受升级，准备就绪<br>`0x01` = 拒绝（参数不支持、资源不足等） |
| 1 | NextBlock | 2 | 当 Result=0x00 时，表示下一个待发送的块号（0 表示从头开始） |

**断点续传**：设备若之前有保存的进度，在此处将 `NextBlock` 设为已保存的下一个块号，手机从该块号继续发送。

#### 3.3.5 BLOCK\_DATA (Type=0x05) – Value 长度可变（≥ 4）

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Block Number | 2 | 块号（0 ~ TotalBlocks-1） |
| 2 | Fragment Offset | 2 | 该片段在块内的起始偏移（字节） |
| 4 | Fragment Data | Length-4 | 实际数据（推荐长度 ≤ 200 字节） |

**设备处理**：

*   按 `(Block Number, Fragment Offset)` 重组块。
    
*   **每收到一个完整的 Fragment Data**（来自链路层校验通过的帧），按正确的全局顺序（块号递增、块内偏移递增）将数据喂给**镜像 CRC16 累加器**（初始值 `0xFFFF`，使用 CRC-16/CCITT-FALSE 算法）。
    
*   不需要对 `Fragment Data` 做任何额外校验（链路层 CRC16 已保证正确性）。
    

**镜像 CRC16 累加示例**：

c

```plaintext
uint16_t image_crc = 0xFFFF;  // 初始值
// 每收到一个 Fragment Data
image_crc = crc16_update(image_crc, fragment_data, fragment_len);
// 最终不需要异或，直接与数据链路层帧中的 Checksum 比较
```

#### 3.3.6 BLOCK\_COMPLETE (Type=0x06) – Value 长度 2 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Block Number | 2 | 刚刚发送完成的块号 |

**发送时机**：手机在发送完一个块的所有 `BLOCK_DATA` 片段后，发送此命令。

#### 3.3.7 BLOCK\_COMPLETE\_ACK (Type=0x07) – Value 长度 2 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Block Number | 2 | 已确认完成的块号 |

**设备处理**：

*   收到 `BLOCK_COMPLETE` 后，检查该块的所有片段是否已收齐，且已累加入 CRC16。若完整，则写入 Flash，保存断点（`next_block = 块号+1`），然后回复 `BLOCK_COMPLETE_ACK`。
    
*   若块不完整（如缺少片段），应回复 `NACK(块号, 重组失败)`，手机重发该块。
    

#### 3.3.8 END\_OTA\_REQ (Type=0x08) – Value 长度 4 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Image ID | 4 | 与 START\_OTA\_REQ 中的 Image ID 一致 |

**发送时机**：手机发送完所有块后，发送此命令请求结束升级。

#### 3.3.9 END\_OTA\_RESP (Type=0x09) – Value 长度 2 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Result | 1 | `0x00` = 成功，`0x01` = 失败 |
| 1 | Reserved | 1 | 保留，填 0 |

**设备处理**：

*   收到 `END_OTA_REQ` 后，需要额外计算镜像CRC32，直接将累加结果与 `START_OTA_REQ` 中的 `Image CRC32` 比较。
    
*   若匹配，则确认升级有效（可写入生效标志），回复 `END_OTA_RESP(成功)`；否则回复 `END_OTA_RESP(失败)`。
    

#### 3.3.10 NACK (Type=0x0A) – Value 长度 3 字节

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Block Number | 2 | 需要重传的块号；`0xFFFF` 表示对 START\_OTA\_REQ 的否定 |
| 2 | Reason | 1 | `0x01` = 超时<br>`0x02` = 重组失败（块内片段缺失）<br>`0x03` = CRC 不匹配（镜像最终校验失败） |

## 4. 交互流程

### 4.1 断点续传流程

1.  设备每完成一个块（收到 `BLOCK_COMPLETE` 并回复 `BLOCK_COMPLETE_ACK` 后），将以下信息保存到非易失存储：
    
    *   `next_block` = 已完成块号 + 1
        
    *   `START_OTA_REQ` 中的所有参数（Image ID、Total Blocks、Block Size、Total Size、Image CRC32、Version）
        
2.  连接中断后重新连接，手机重新发送 `NEGOTIATE_BS_REQ` 和 `START_OTA_REQ`（参数相同）。
    
3.  设备读取保存的参数，若完全匹配，回复 `START_OTA_RESP(OK, NextBlock = 保存的 next_block)`。
    
4.  手机从 `NextBlock` 开始发送剩余块，设备继续累加 CRC16。
    
5.  所有块完成后，走正常结束流程。
    

### 4.2 异常处理

| 异常场景 | 处理方式 |
| --- | --- |
| 链路层帧 CRC16 校验失败 | 接收端丢弃该帧；发送端未收到对应响应（如 `BLOCK_COMPLETE_ACK`）超时后重传该帧。 |
| 手机未收到 `BLOCK_COMPLETE_ACK` | 超时后重发最后一个 `BLOCK_COMPLETE` 或重发该块的所有 `BLOCK_DATA` 片段。 |
| 设备收到 `BLOCK_COMPLETE` 但块不完整 | 回复 `NACK(块号, 重组失败)`，手机重新发送该块的所有片段。 |
| 设备最终 CRC16 不匹配 | 回复 `END_OTA_RESP(失败)`，手机可选择重发整个镜像或中止。 |
| 设备 Flash 写入失败 | 回复 `NACK(块号, 0x03)` 或直接断开连接。 |

---

## 5. 断点续传存储结构（设备 Flash）

每个 Image ID 分配 32 字节：

| 偏移 | 字段 | 大小 (字节) | 说明 |
| --- | --- | --- | --- |
| 0 | Magic | 2 | 固定 `0x5A5A`，标识记录有效 |
| 2 | Image ID | 4 | 与 START\_OTA\_REQ 一致 |
| 6 | Next Block | 2 | 下一个待接收的块号 |
| 8 | Total Blocks | 2 | 用于参数匹配 |
| 10 | Block Size | 2 | 用于参数匹配 |
| 12 | Total Size | 4 | 用于参数匹配 |
| 16 | Version | 4 | 版本号 |

---

## 6. CRC 算法规格

### 6.1 链路层 CRC16（CRC-16/CCITT-FALSE）

*   **多项式**：`0x1021`
    
*   **初始值**：`0xFFFF`
    
*   **结果异或**：`0x0000`
    
*   **字节序**：小端
    
*   **覆盖范围**：整个 Header（SOF+SEQ+FrameType+Length） + Payload
    

**CRC16 计算函数**：

c

```plaintext
uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

### 6.2 块校验增量计算 CRC16

*   **算法**：CRC-16/CCITT-FALSE
    
*   **初始值**：`0xFFFF`
    
*   **结果异或**：`0x0000`
    
*   **累加方式**：每收到一个 `Fragment Data`，按正确的全局顺序更新累加器；所有数据接收完成后，累加器的值即为整个镜像的 CRC16，直接与 `START_OTA_REQ` 中的 `Image CRC16` 比较（无需额外异或）。
    

**CRC16 累加示例**：

c

```plaintext
uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

uint16_t image_crc = 0xFFFF;
image_crc = crc16_update(image_crc, fragment_data, fragment_len);
// 最终直接比较
if (image_crc == expected_crc16) success;
```
---

### 6.3 镜像校验计算 CRC32

```plaintext
#include <stdint.h>

// 预计算的 CRC-32 表（256 项）
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    while (len--) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ *data++) & 0xFF];
    }
    return crc;
}

uint32_t calculate_image_crc32(const uint8_t *image_data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;   // 初始值
    crc = crc32_update(crc, image_data, size);
    crc ^= 0xFFFFFFFF;           // 最终异或
    return crc;
}
```

## 7. 参数推荐

| 参数 | 推荐值 | 说明 |
| --- | --- | --- |
| BLE MTU | 247 字节 | 通过 GATT MTU 交换协商 |
| 链路层帧 SEQ | 每帧递增 | 可选，用于重传检测 |
| FrameType | REQ/RESP/Notify | 按命令方向设置 |
| 块大小 | 字节 | 协商决定 |
| 片段大小 | ≤ 200 字节 | 确保一个片段能放入一个链路层帧 |
| 块发送超时 | 500 ms | 等待 `BLOCK_COMPLETE_ACK` 的时间 |
| 最大重试次数 | 3 次 | 超过则中止（可断开连接） |

---

## 8. 命令速查表

| Type | 命令 | Value长度 | 方向 | 说明 |
| --- | --- | --- | --- | --- |
| 0x01 | NEGOTIATE\_BS\_REQ | 2 | 手机→设备 | 协商请求 |
| 0x02 | NEGOTIATE\_BS\_RESP | 2 | 设备→手机 | 协商响应 |
| 0x03 | START\_OTA\_REQ | 18 | 手机→设备 | 开始升级请求（含 Image CRC16） |
| 0x04 | START\_OTA\_RESP | 3 | 设备→手机 | 开始升级响应（含 NextBlock） |
| 0x05 | BLOCK\_DATA | 可变 | 手机→设备 | 数据片段 |
| 0x06 | BLOCK\_COMPLETE | 2 | 手机→设备 | 块完成通知 |
| 0x07 | BLOCK\_COMPLETE\_ACK | 2 | 设备→手机 | 块完成应答 |
| 0x08 | END\_OTA\_REQ | 4 | 手机→设备 | 结束升级请求 |
| 0x09 | END\_OTA\_RESP | 2 | 设备→手机 | 结束升级响应 |
| 0x0A | NACK | 3 | 设备→手机 | 否定应答 |