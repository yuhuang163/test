# Momcozy 智能制造专项\-FCT\&ATE协议规范v1\.0\.0



|**文档编号：**|MC\-AIOT|
|---|---|
|**版本号：**|V 1\.0\.0|
|**发布日期：**||
|**密级：？？待定**|内部公开 / 合作伙伴受控|
|**归口部门：**|Momcozy AIOT|
|**变更**||

# 文档介绍

## 目的

本文档旨在规范 Momcozy 设备与移动终端（APP）hehe之间基于 BLE（Bluetooth Low Energy）的通信接口设计与实现方式，统一蓝牙通信协议的数据结构、交互流程及处理规则，以确保不同设备型号、固件版本及移动端应用在通信实现上的一致性与兼容性。

通过本规范的制定，实现以下目标：

1. 明确定义蓝牙通信各层级职责及接口边界，降低系统设计复杂度，提高模块可维护性与可扩展性。

2. 统一数据帧格式、命令字定义及交互流程，保证设备与 APP 在数据交互中的互操作性。

3. 为设备固件开发、移动端开发及测试验证提供统一的协议依据，减少因接口理解差异导致的通信问题。

4. 为后续新设备接入、功能扩展及协议升级提供标准化基础，提升平台化开发效率。

5. 通过规范化通信流程与校验机制，提高数据传输的可靠性与系统稳定性。

## 范围

本规范适用于 Momcozy 设备与移动终端（APP）之间基于 BLE（Bluetooth Low Energy）的通信接口定义，包括但不限于：

1. 设备广播、扫描、连接建立及断开流程规范

2. BLE Service 与 Characteristic 的数据交互定义

3. 数据链路层数据帧封装、拆分、重组及校验机制

4. 应用层命令字、参数格式、状态上报及响应机制

5. 通信异常处理机制（超时、重传、错误码处理）

6. 设备固件版本升级、状态监控及业务数据传输接口规范

本规范不涉及：

- BLE 协议栈底层实现。

- 操作系统蓝牙驱动实现细节。

- 上层业务逻辑 UI 展现方式。

# 接口分层介绍

## 分层架构

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MWJkNzVmN2ZhZDAxM2FmYTI1MjUwZmJmM2ZhMDdiZmVfZDk4M2E4OWE4YjkyNGZmMmI3ZjdiNGM0Mzk3ODUzYmNfSUQ6NzY1Nzg5NDI2NjcxNDM3NzQxNF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

## 分层定义

|**层级**|**Description**||
|---|---|---|
|驱动层|1. 负责蓝牙物理链路建立及基础通信能力提供，包括：<br>- BLE 广播（Advertising）参数配置<br>- 设备扫描（Scanning）及过滤策略<br>- 连接建立（Connection）与断开（Disconnection）管理<br>- BLE Service / Characteristic 的注册与发现（Discovery）<br>- 基础读写操作接口（Read / Write / Notify / Indicate）|该层向上提供稳定的字节流读写接口，不涉及数据业务语义解析。|
|数据链路层|2. 负责在 BLE 字节流之上建立可靠的数据帧传输机制，包括：<br>- 数据成帧（Frame Encapsulation），定义：<br>- 帧头（Header）<br>- 长度（Length）<br>- 负载（Payload）<br>- 校验（Checksum / CRC）<br>- 分包与组包机制（Fragmentation / Reassembly），适配 BLE MTU 限制<br>- 数据完整性校验与错误帧丢弃机制<br>- 可选重传控制策略（如 ACK / 超时重发）|该层保证应用层获得完整、顺序正确的数据包。|
|应用层|3. 负责业务协议定义及业务数据处理，包括：<br>- 应用层命令字（Command ID）定义<br>- 参数编码格式（TLV / 固定结构体等）<br>- 设备控制指令解析与执行<br>- 设备状态、传感数据及事件上报<br>- 指令响应（Response）与错误码（Error Code）定义|该层面向具体业务功能，实现设备与 APP 的业务交互逻辑。|

# 驱动层服务定义

## UUID定义 

|**BLE GATT连接\&服务发现**|||
|---|---|---|
|**BLE\_Service\_UUID**|9F6B1A20\-3C4D\-4E5F\-A601\-7B8C9D0E1122||
|**BLE\_Write\_UUID**|9F6B1A21\-3C4D\-4E5F\-A601\-7B8C9D0E1122|Write Without Response|
|**BLE\_Notification\_UUID**|9F6B1A22\-3C4D\-4E5F\-A601\-7B8C9D0E1122|Notify|
||||

# 数据链路层定义

数据链路层的传输单元是帧（Frame），每个Frame的最大传输能力由MFS（Max Frame Size）决定。MFS通过应用层命令进行协商。当一个应用层数据包的长度超过了MFS时，数据链路层需要以MFS为单位对数据包进行拆分。由于驱动层（标准蓝牙协议栈）承载能力的限制（比如BLE GATT MTU默认为20字节），Frame在驱动层传输时可能被以MTU为单位进一步拆分。

## 数据链路层帧封装格式

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=Yzg2ZGQ5N2YxNGNkZjNiYjc2MTdhMTUyY2IyMGU5ZjJfMWUxNzA2ZjE1ZWRiZjU2ZjVlZTQ2MTRjNGRmMTYxNzVfSUQ6NzY1Nzg5NDI2NjIxMTExMDA3OF8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

详细字段的描述如下表所示：

|**区域**|**字段名称**|**字节数**|**M/O**|**字段描述**|
|---|---|---|---|---|
|Header<br>|SOF|1|Mandatory|Start Of Frame，标识数据帧的开始，详细定义参考4\.1\.1|
||Length|2|Mandatory|标识从Control字段到Payload字段的长度，不覆盖CRC字段|
||Control|1|Mandatory|数据链路层控制选项，详细定义参考4\.1\.2|
||FSN|1|Optional|Frame Sequence Number。如果Control字段中设置了分帧标志，那么在Header中必须携带FSN字段，用以标识帧序列号。FSN取值范围为\[0,255\]。|
|Payload|应用层数据载荷||||
|Footer|Checksum|2|Mandatory|校验和字段，详细定义参考4\.1\.3|

表 1 数据帧格式

### 帧起始SOF

SOF用于标识数据帧的起始位置。

|**SOF**|**Description**|
|---|---|
|0x5A|控制命令和文件传输通用|

### 控制域Control

|**Bit**|**Name**|**Description**|
|---|---|---|
|Bit 7|Reserved|预留后续扩展使用，默认置0|
|Bit 6|Reserved|预留后续扩展使用，默认置0|
|Bit 5|Reserved|预留后续扩展使用，默认置0|
|Bit 4|Reserved|预留后续扩展使用，默认置0|
|Bit 3|RSP|数据链路层响应帧标志位<br>置1，表示该帧是一个数据链路层响应帧，用于向对端进行数据链路层的确认或者告知CRC校验错误<br>置0，表示该帧是一个数据链路层普通帧<br>举例见|
|Bit 2|ACK|数据链路层请求对端确认标志位<br>置1，表示对端需要回复一个数据链路层的响应对该帧进行确认<br>置0，表示对端不需要回复数据链路层的响应对该帧进行确认<br>注：如没有特殊理由不建议在数据链路层进行确认，因此默认置0|
|Bit1Bit0|FSN|分帧标志位域<br>00，表示本数据帧对应一个完整的应用层数据包，Header中不携带FSN字段。<br>01，表示本数据帧对应一个应用层数据包的起始分帧，Header中携带FSN字段对该帧进行编号。<br>10，表示本数据帧对应一个应用层数据包的过程分帧，Header中携带FSN字段对该帧进行编号。<br>11，表示本数据帧对应一个应用层数据包的结束分帧，Header中携带FSN字段对该帧进行编号。|

### 校验和Checksum

校验和覆盖Header域中Length\+Control\+FSN字段和Payload全域，采用 CRC\-16/CCITT\-FALSE 进行计算。

**参考接口**

```C
uint16_t CRC16_Calc(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0)
        return 0;
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

## 数据包分层封装实例

|假设应用层发送的净载荷内容如下（本例中净载荷长度为18字节）|![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzQ0ZGRiOTU3MDFmZTc1NTVjOTRiMTU3MjQ5YmIxMzFfYWFkODJkYmI3YjkxMGUyMjNmODQyMDk2NTVjOTA0MTFfSUQ6NzY1Nzg5NDI2NDIwNjU5Mjk5M18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)|
|---|---|
|则数据链路层封装后的数据帧内容如下|![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDQ0MTNlYWYzZWQ2Y2M3NDg4NjAyNGUwNzJmZTk4MDZfMTc2NWU1NzM3Y2JkOTQ5YjE5NzQ5MmE1NzdkZjQyYThfSUQ6NzY1Nzg5NDI2NzI3MjIxOTYwMV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)|
|假设在BLE环境中进行传输，驱动层MTU为20字节，则数据包在空口被分为两个分片传输。|![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NDgzOGMxODhhY2QzOGE5ZGVjZGU2MGIzZjg4MmIyZjRfZWIzNmY4OTJiZTU2NDZlN2JmNmE3NmYyMzdkOTNmNzZfSUQ6NzY1Nzg5NDI2NjIxOTY3ODkyMV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)|

## 数据链路层的响应

|当发送端将Control字段中的ACK标志位置1时，接收端在收到该帧后，需要向发送端回复一个数据链路层的响应帧进行确认。响应帧封装格式如下。|![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NmZkN2U3N2U2NTFhM2I3ZThhMzg2MDkxZWZmYWNjZGVfZTgxNDc0NzQ3NmNmMGMzYTM2NjFiNzc1NGNlNzFiZWFfSUQ6NzY1Nzg5NDI2Mzg0NzYzNjE4OV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)|
|---|---|

Errno定义如下。

|**Errno**|**Description**|
|---|---|
|0|CRC校验成功|
|1|CRC校验失败|
|2|待定|

注意，我们在这里设计数据链路层响应的目的是处理异常场景：

- 当接收端发现链路层数据帧的CRC校验和错误时，能够通过链路层NACK机制让发送端进行快速重发，从而防止发送端由于死等应用层响应定时器超时而导致的传输效率低下问题。

- 绝大部分命令在应用层都会设计有响应报文，正常情况下命令的响应通过应用层即可完成，链路层不应再单独做一次冗余响应。

- 因此没有特殊理由时，Control字段中的ACK bit（数据链路层请求对端确认标志位）需要置为0。若ACK bit置为1，则每个响应都会在链路层单独做一次响应，会影响传输效率。

数据链路层CRC校验失败，回复NACK响应的交互流程如下所示。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZWRjOWYwN2Q3ZDEwZDUwYmQ1YTZmNjdjZGZiN2Y1MmNfMTQ4OWQ2ZDc4ZDY5M2E1NWExMjQ3ODdkYzIwODg5MWJfSUQ6NzY1Nzg5NDI2NDA1OTgwODk4MV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

**数据链路层强制显式确认交互流程如下。如前所述没有特殊理由不允许这样做。**

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTNmNGE3YjJlOTk5ZWNjZWE3MzhmZjk0ZTY1YzA1MjNfNzM2NjQ0YjhmZmM3YTBlNjJkNjNkMDNkNWJmZTg2MDVfSUQ6NzY1Nzg5NDI2NTQ4MTU3OTcwN18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

## 数据链路层的交互方式

### 单帧数据包传输场景

当应用层数据包长度在数据链路层可以承载的Payload长度范围内，即应用层净载荷长度 \<= 数据链路层MFS \- 6\(SOF, Length, Control, Checksum\)时，该数据包可在单个链路层数据帧中完成传输。

假设如下场景：

- 应用层传输的数据包长度为60字节

- 数据链路层的MFS为100字节

- 驱动层的MTU为20字节

分层封装过程中，由于净载荷长度小于数据链路层的MFS，因此首先形成一个长度为66字节的Frame

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDk3ZTg2ZjFiY2NmMWY1OTdkYTY3ZGQwNDNjNmUwZThfNmFiYWNmYTc0YzU5YzY1MTAyYzFjZTM4MjE2MzQ0NmFfSUQ6NzY1Nzg5NDI2NTkwOTI4NDAxOV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

在驱动层以20字节为单位分为4个分片，如下：

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MWYxNzc4NDdlZjhkOWU2ZTcxNjA5YjE2YmMxOTU5MWFfNzYwNTExMjdlNWU0M2Q0OGMwNzIxMzFlMDkxOWMyY2VfSUQ6NzY1Nzg5NDI2NzA0NTcxMDgxMl8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

本小节后续描述的所有正常或异常交互流程均以上述场景为例

当驱动层没有出现丢失分片或分片内容错误等问题时，正常交互流程如下：

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YTM0MzRhZjg5OTI5OTM4MDI5ODhhZjAxOTkwZDdhNzVfODM3Nzg5ZGZlMDQ3YTljMTk2MTY0NjRlZjU3ODc0NGJfSUQ6NzY1Nzg5NDI2NjEwNjM5OTcxM18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

当驱动层出现丢失分片或分片内容错误等问题时需要作容错处理，下面针对不同的异常场景进行描述

1. 丢失分片1的场景

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDY2NzYyYjRmNGNjN2VmNWNiZjdiYjE3YzJjY2U5NGRfOWE4ZjU2OTE3Y2YxMTM4NDM4OWVmMzM3YjQ4M2YxYWVfSUQ6NzY1Nzg5NDI2NjkwMzA4ODA4NF8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

2. 丢失分片2、3、4中一个或多个的场景。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTEyMmRlNjc1ZTU0NjhiYWIzZTdlZjUzY2Y4NDhlOGJfNzg2MDI0NjQ5NjNhMWZiNjEzMzc2YTRkNTJjZmM2NmFfSUQ6NzY1Nzg5NDI2NDk5MDc2NDIxN18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

3. 4个分片都没有丢失，但1个或多个分片的内容有错误的场景

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzNhZmZiMTM5ODU1YzVkNGUwZTFmODQyNDU3MTYzMDBfODRkOThmZDZmMTU3MjBjZWQxZTQ3ZWIzMjQzYzBhZjVfSUQ6NzY1Nzg5NDI2Njg1NzExNDg1OV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

### 多帧数据包传输场景

当应用层数据包长度超过了数据链路层可以承载的Payload长度范围，即应用层净载荷长度 \> 数据链路层MFS \- 6\(SOF, Length, Control, Checksum\)时，该数据包需要拆分为多个链路层数据帧传输。

假设如下场景：

- 应用层传输的数据包长度为100字节

- 数据链路层的MFS为50字节

- 驱动层的MTU为20字节

分层封装过程中，由于净载荷长度大于数据链路层的MFS，因此首先形成三个链路层Frame

第一个Frame（起始帧）长度为50字节，其中应用层净载荷43字节，帧序列号为0

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MjZkNDY4MWQ5YTIyMzc0N2Q0MjBhZDBhNTNiYjQ2ZGZfMmQ2ZDJjZjg2YzA2YjM2ODZlNjE2M2RjMmIzMWRiM2VfSUQ6NzY1Nzg5NDI2NzU2NTY1NzAxOF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

第二个Frame（过程帧）长度为50字节，其中应用层净载荷43字节，帧序列号为1

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZTEwYTQ2ZGM1ZDQ1NWFkMGYwOTE2MjZjYzE4M2MyNjZfNzAzNDA0ZmNlZjEwNzQyODMyYjg2MjM2ZmFlZjA5NzhfSUQ6NzY1Nzg5NDI2Njk5NTQyODU5MV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

第三个Frame（结束帧）长度为21字节，其中应用层净载荷14字节，帧序列号为2

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MzU5MzkyMjI4MDc3NDg2YTg2MDBiZTc3ZGMzYTcyNjBfOThkMDVkNDU1NDQ4OTQ1NDMwMDJhYzNjMzMxMmQzNzhfSUQ6NzY1Nzg5NDI2NzE2NzM5NTA1NV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

在驱动层以20字节为单位分为8个分片，如下：

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NGY4Yzk3OGY1ZThiMWY3OTQwYTNjZjRlM2I3MjIwZDRfMzZkNDQ1ODYzODZkNGQwNjNmMzhmNjRiMGI3MWQxZGJfSUQ6NzY1Nzg5NDI2NzU1MzIwNTQyN18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

本小节后续描述的所有正常或异常交互流程均以上述场景为例

接收端依次检测Control\[Bit1Bit0\]标志和FSN字段，直到该数据包对应的所有数据分帧和驱动分片接收完全。

其中单帧传输中的容错处理参考4\.4\.1单帧数据包传输过程处理。

对于多帧传输过程中可能出现的异常，作如下的容错处理：

- 如果没有收到起始帧或者起始帧的FSN不是从0开始，则直接丢弃该帧，直到收到起始帧。

- 如果收到的过程帧的FSN编号不连续，则直接丢弃收到数据包，直到新的起始帧。

- 如果没有收到结束帧，接收端不做处理，等待发送端重发，当收到新的命令开始时，丢弃原先收到数据。

- 如果收到数据帧CRC校验错误，则直接丢弃收到的数据包，

-   如果是命令交互过程，则回复CRC错误码响应。

-   如果是数据传输过程，则由数据传输过程应答机制决定是否响应。

### 限制和约束

为了提升空口传输效率减少冗余字段，本协议没有对链路层到驱动层传输的数据分片进行标识和编号。为了防止不同Frame的驱动层分片混插在一起，需要对APP侧对链路层的Frame传输过程增加如下约束：

在单个Frame传输完之前不允许其他Frame数据同时下发

# 应用层定义

## Frame格式定义

1. 所有应用层PDU的都是以Service ID/CommandId作为起始。

2. Service ID是每个报文所属的业务类型，比如：设备管理，消息通知，闹钟等。

3. Command ID是每个业务对应的命令类型，比如：设置日期时间,获取设备版本相关信息等。

4. TLVs封装消息中的具体信息。

**~~注意：对于BLE设备，单帧的长度尽量控制在20个字节以内，总长度不得超过512字节。TBD~~** 

|**Service ID**|- 占1字节。<br>- 用于标识服务能力类型。 Service ID定义参考5\.1\.1|||
|---|---|---|---|
|**Command ID**|- 占1字节。<br>- 用于标识具体命令类型，每个Service ID对应的Command ID都从1开始，表明真实的逻辑行为。<br>- 具体的Command ID说明在服务接口定义中详细介绍，其中是否单板主动上报指的是单板主动向APP发起的命令，N代表不是，Y代表是。|||
|**TLVs**|- Type/Length/Value 对应每条报文的具体内容。<br>- 内容仅允许为TLV和TL形式，其中TL可在下挂子节点时使用，TL的L可以为下挂所有节点的总长度。|Type类型说明|- Type占用1个字节，bit7表示该类型是否有子节点，0表示无子节点，1表示有子节点，其中有子节点的Type不能包含Value，仅为TL格式，Length表示其子节点的总长度；<br>- 对于可扩展子节点的类型，如果Length字段是0，Type字段的bit7位取值为0或1均可；<br>- bit6\-0表示Type的值，取值范围0\-127，每个Command ID对应的Type都从1开始代表不同的内容类型，Type类型是否包含子节点在对应类型的说明中体现。<br>- 其中127为通用错误码类型，126为主动上报通知获取数据类型。<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NjA1MzQ0MjUzYjA0YzRiNzNjNDUxOWFlZjRhMjM4NGFfYWNiNGQwODhiZmNhZGQ5YjQ5Y2YzNzZjNmI1M2JkNTVfSUQ6NzY1Nzg5NDI2ODEyOTM5ODAwNV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br><br>- 含有子节点的Type格式说明：<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MjlhZDViMGYwZDI4NGIxYjA2NTNhYTE5NDM0N2Y3OWZfZTgzY2M1ZjI0NjE4MTAwODRjMzA5ZDhkODZlODI3ZjZfSUQ6NzY1Nzg5NDI2NzU2NTcwNjE3MF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)|
|||Length类型说明|- Length表示Value的长度，Length占用的字节数可变，目前使用到了占用1\-2个字节两种情况，用前一个字节的bit7表示它后面还需要再解析一个字节，bit7为0时，表示length在此字节结束。如：<br>          0\-127 :0b0xxxxxxx<br>          128\-16383:  0b1xxxxxxx 0b0xxxxxxx<br>- Length占用1个字节可以表示0\-127的长度，占用2个字节可以表示128\-16383的长度，示意图如下：<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NjM2MDg4MDEwYjRlMmRlNTQwYzJiNTU0NGNkYTQ3YTZfYTk4NDBlOGRlYTIxNTQ2Yjg1MmE3NzJlOWE1N2U1ODhfSUQ6NzY1Nzg5NDI2NjQ1NDU0MzM0MF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)|

### Service ID定义

为了避免各个领域和产品型态的Service ID冲突，将Service ID进行统一划分：

|**Service ID**|**Service Name**|**Description**|**必选/可选（M/O）**|**Version**|
|---|---|---|---|---|
|0x01|Device Management Service|设备管理|M|1\.0\.0|
|0x07|Data Service|数据服务|M|1\.0\.0|

### Command ID

Command ID：占1字节，用于标识具体命令类型，每个Service ID对应的Command ID都从1开始，表明真实的逻辑行为，具体的Command ID说明在服务接口定义中详细介绍。

### 通用错误码

|**字段名称**|**Type ID**|**Length Size\(Oct\)**|**Value 数据类型**|**说明**|**必选/可选\(M/O\)**|
|---|---|---|---|---|---|
|error\_code|127|1|uint32|通用错误类型，具体值参见“Error Code表”。|M|

**Error Code表：**

|**Error Code**|**类别**|**描述**|**协议版本**|
|---|---|---|---|
|100000|通用错误|成功|1\.0\.0|
|100001||未知error类型|1\.0\.0|
|100002||不支持该Service的请求|1\.0\.0|
|100003||不支持该Command的请求|1\.0\.0|
|100004||无权限|1\.0\.0|
|100005||系统忙|1\.0\.0|
|100006||请求格式错误|1\.0\.0|
|100007||参数错误|1\.0\.0|
|100009||响应超时|1\.0\.0|
|101001|Device Management Service|入参为空/数据非法|1\.0\.0|
|101002||设备电池电量低（禁止恢复出厂设置）|1\.0\.0|
|107001|Data Service|非法查询|1\.0\.0|
|通用错误码Type值为127，格式如下：<br>-  6位十进制数（为了和json错误码类型兼容，不采用16进制表述）<br>- 最高1位为1，保留后续扩展位。<br>- 次高2位表示模块。<br>- 最低3位表示模块中的错误类型。<br>如：101001，表示01模块中的第001个错误码。<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDk0NTkwMGYxNjdkMWRhYjcyNWVlYWI5NzhhYmUyNjJfM2M1MGE1ZjgxNjJkZGUyM2Y4MjljMTUxOWMyM2U1NjhfSUQ6NzY1Nzg5NDI2NTU1MzAzMDM2Ml8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br><br>**注意事项说明：**<br>（1）对于单板不支持的Service ID，单板返回100002（不支持该Service请求），对于单板不支持的Command ID，单板返回100003（不支持该Command请求）||||

## Data域的定义格式约束

各业务内的定义通用要求如下：

1\. 业务数据定义考虑扩展性，预留扩展能力。

2\. 业务内部按照功能进行合理的子类划分。

## 字节大小头约束

对于多字节传输的字段类型，均按照大头在前的组包格式传输。

比如对于uin32的四字节数据类型，传输格式如下：

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTRlYzg3NTk5YzI4YWEyZWFhN2MzMDAzNWZhYzhlZjdfNWFmMjk4ZjMwYTkzMGE1MGEzMjQxYjNkMjIwNWI2YzBfSUQ6NzY1Nzg5NDI2NzU1NzQ2NTMxOV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

## 典型使用场景举例

**a\. 需要获取信息命令，在下发请求时，仅带有命令值，应答报文中携带查询信息对应的TLV，**

如：

获取服务端电池电量百分比

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length Size\(Oct\)**|**Value数据类型**|**说明**|**必选/可选\(M/O\)**|**协议版本**|
|---|---|---|---|---|---|---|
|battery\_percent|0x01|1|uint8|服务端电池电量百分比。取值范围\[0,100\]|M|1\.0\.0|

**请求帧结构：**

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MGI5NGFmNTQzOGQ1YzQwMjU1MTk2N2VkNTNkYTFjZjZfNWE4NTg3MzVmYzI5ZWQ4MDIwNzA1NDNhNjY4ZTU0YjZfSUQ6NzY1Nzg5NDI2NDI4MjIyMTUxMF8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

**应答帧结构：**

**返回成功：**

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MGRiOGJhMmRmZTRkM2VjM2U0MTc2M2FmZjc2NmU1NDhfMzgwMzNjNjM4ZTg5MWJkNDZlNDY3YTZjYmQxNGJhYjBfSUQ6NzY1Nzg5NDI2NDk5MDgxMzEzN18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

**返回失败：**

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=Y2ViMjZiNzMxNTA2Y2QxMWJhZjU1ZDI3NDg2MWQ4ZWZfOGEzMTA3YmNiYTc5NTU1NDJiNWIyMjhmY2UwZTU5YzhfSUQ6NzY1Nzg5NDI2NzIzMDI2MDQyM18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

**b\. 设置信息命令，在下发请求时，需要携带配置信息对应的TLV，应答报文只确认成功和失败，如：**

设置设备时间信息

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|协议版本|
|---|---|---|---|---|---|---|
|time|0x01|1|uint32|日期时间，使用秒表示从起始时间1970年1月1日0时0分0秒到当前时间的秒数。<br>说明：使用格林尼治时间。|M|1\.0\.0|
|time\_offset|0x02|1|int16|和格林尼治时间的时间差，高位表示小时，低位表示分钟，如：<br>0x10 1E, 表示\+8:30。<br>0x82 00, 表示\-2:00。<br>说明：由于存在一些非整点的时区，因此增加对分钟的表示|M|1\.0\.0|

**请求帧结构：**

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NDZlZGM3ZGM4Mjc5YzVhNTkxMjlkZmMzZjA4MDU5OGVfM2ZhYjU3MWQ4NGVkZTQ1NjQ2YTBiOGYyZGViMzdhODhfSUQ6NzY1Nzg5NDI2NzEyNTQwMjgzMV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)

**应答帧结构：**

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YjBlM2RjNDMyOTUwY2VkMTdmNGM2NDIyMjMxNzQzYmZfZTA5NTY0NmFmMzM3YmJkYWJjYTJlYTE4YzhkNWQ0YzBfSUQ6NzY1Nzg5NDI2NDQyNDc2MjM0Ml8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)

## 索引字段说明

对于本文档中描述到的所有索引，如果没有特殊说明，均是从1开始，并且进行递增。如果有特殊场景，需明确描述在接口字段定义中。

## 边界条件限制

1、APP和穿戴设备间应用层协议最大包长定为2048字节。

2、新增APP和穿戴设备间的TLV命令嵌套深度不超过5层。

# Device Management Service

## 连接参数协商 \(CID=0x01\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length Size\(Oct\)**|**Value数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|protocol\_version|0x01|1|uint8|0x01:协议版本1\.0\.0\.0|M|1\.0\.0|
|max\_frame\_size|0x02|2|uint16|MFS值（链路层最大帧长度 ）|M|1\.0\.0|
|maximum\_tranimission\_unit|0x03|2|uint16|MTU值（物理层最大传输单元，不能超过蓝牙版本支持的最大MTU，且设备系统支持调整）|M|1\.0\.0|
|transmit\_interval|0x04|2|uint16|数据包发送间隔时间，单位毫秒|M|1\.0\.0|
|arq\_mode|0x05|1|uint8|ARQ模式，由设备返回请求设置。<br>00：不启用重传，数据丢失由上层处理<br>01：停等协议，发送一帧等待 ACK 后再发下一帧<br>02：选择重传，仅重传出错帧|M|1\.0\.0|
|ack\_timeout\_ms|0x06|4|uint32|ACK超时时间 ms，若arq\_mode不为0，则需携带该字段|O|1\.0\.0|
|max\_retransmit|0x07|1|uint8|最大重传次数，若arq\_mode不为0，则需携带该字段|O|1\.0\.0|
|random\_value<br>|0x08|1|uint8|携带该字段表明设备要鉴权APP的合法性：<br>加密版本\+随机数。<br>最高两个字节表示加密版本，从1开始。<br>新增取值2, 表示设备支持通用加密TLV|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MjUzYmYxYTA3ZTA3ZWYyNDJlNjViZWMxZTBkZDA1ZDZfMjc5ZWQwZTFmZGQ0MTg4NTQ5MzVkYjE0NzI0NmQwNDdfSUQ6NzY1OTM1OTIyODQwNjI1NDU3OV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>**返回成功**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MGU4MzY0NjMzZmEzMzE3NjZlNmYyYWJkZTNiNDZmNzRfMWI5NTMxMDIxZTMyYTAwMzA4NWY4MTRlYzkwMjk1NzRfSUQ6NzY1OTM1OTIyNTY0NzA3NDI2MV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br><br>**返回失败**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGVmMDExY2JiNDE1YjYyNTk2M2YxMGRjOTg1NDBhMjhfZDI1YmFjMmQzYmZlN2E0Mjc1OTZmYjNkZGJiNzFmMThfSUQ6NzY1OTM1OTIyNTkxOTY3MTI2Nl8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**<br>- 连接参数可根据实际需求增加TLVs，实际导入时根据Type需求下发协商参数。|||||||

---

## 服务能力协商 \(CID=0x02\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|support\_service\_request|0x01|1|uint8|支持的服务类型列表，每个字节是对应服务的Service ID值|O|1\.0\.0|
|support\_service\_response|0x02|1|uint8|对应服务列表的支持能力，1：支持，0：不支持|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NmFjYjZhODFlZmNkMDM0ZTY0OGUyNDkxYWNlN2M5MzJfZGI3ZDNkY2MyZjY5YTRmYzdlMGFlOThmNWUwM2JiMDdfSUQ6NzY1OTM1OTMzMDk5MDYwNzM0N18xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MjQ5NWVjMDEzODAzNjI3MjU4OGU0MDdiMzBiMWYxMmRfZjIxMmQ3ZmFkZGM0YTY2MTBlMjMwZWE3NDk4NmJhZTlfSUQ6NzY1OTM1OTMzMTc5MTUyMjk5NV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- Value根据具体Service ID来下发查询。|||||||

---

## 命令能力协商 \(CID=0x03\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|support\_command\_request\_struct|0x01|1\~2|uint8|命令能力协商结构体，包含：<br>service\_id,<br>command\_list,<br>command\_support\_result|M|1\.0\.0|
|service\_id|0x02|1|uint8|协商命令所属的service   id|M|1\.0\.0|
|command\_list|0x03|1|uint8|协商的命令字列表|O|1\.0\.0|
|command\_support\_result|0x04|1|uint8|对应字节表示是否支持该能力集合1：支持；0：不支持|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZWVmZjU5ZGViNDEwOTkyNTMxYTljODhiZmM0MTVmNTJfMjcwMWQwMTlhMWJhZGUyMWUxYzI1OTI1NjQ5MWI0MWJfSUQ6NzY1OTM1OTM5NjE2NTkyOTk0Ml8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NmFjZjY3ZWFkMGQwNWEyNjE0MmQ5ZTVmZjkwZjlhYWRfMDMxZWIwY2I3ZTI0ZWIzNzU2MTdhMWEyOTAxNTNhNWRfSUQ6NzY1OTM1OTM5MzU5OTYwNjAwNV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- 查询发送command list，对应type为0x03。<br>- 返回结果command support list，对应type为0x04。|||||||



---

## 获取设备版本相关信息 \(CID=0x04\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|bt\_version|0x01|1|uint8|蓝牙接口版本号，传输内容为字符串。字符串内容格式详见：附录6\.4\.1蓝牙版本号约束规则|M|1\.0\.0|
|device\_type|0x02|1|uint8|设备类型<br>00：pump吸奶器，<br>01：watch<br>02：按摩仪|M|1\.0\.0|
|device\_version|0x03|1|uint8|设备硬件版本号，传输内容为字符串。|O|1\.0\.0|
|device\_fw\_version|0x04|1|uint8|设备软件版本号<br>字符串内容格式详见：6\.4\.2软件版本号约束规则|M|1\.0\.0|
|device\_bt\_mac|0x05|1|uint8|设备蓝牙mac地址|O|1\.0\.0|
|device\_sn|0x06|1|uint8|设备SN号，传输内容为字符串。|O|1\.0\.0|
|device\_model|0x07|1|uint8|设备型号，传输内容为字符串。|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZWI3ZDYxZWE3Y2E2NWQ0ZDcwYzMwNjM3MTk0ZDZmYmZfMWIzM2VlZDA1YmFjMWZkMjU3ZmM0ODA5NWI2NjdiMGFfSUQ6NzY1OTM1OTQ0MDk4MjU3NjA4MV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZTc4MWJmM2QyYjRkOWYzMGY3ZmZlZWJlMzhjNjBkMzNfNGU0ZDcxZDk0M2E1MWNkYjU5ZTQ2YTU4NDg0MDkxNjZfSUQ6NzY1OTM1OTQ0MjQ2Njk4Mjg2Nl8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**|||||||



---

## 蓝牙绑定请求 \(CID=0x05\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|require\_bt\_bond|0x01|1|uint8|发送蓝牙绑定请求|M|1\.0\.0|
|bt\_bond\_answer|0x02|1|uint8|设备确认绑定请求，取值范围：\[0,1\]<br>0：代表设备不允许绑定<br>1：代表设备允许绑定|M|1\.0\.0|
|bt\_bond\_os|0x03|1|uint8|发送绑定请求的手机操作系统，取值范围：\[0,1\]<br>0：Android<br>1：iOS|M|1\.0\.0|
|bt\_bond\_feature|0x04|1|uint8|发起绑定请求的特征码，值为4个字节的随机数，如”1234”,   ”0205”等。|M|1\.0\.0|
|bt\_bond\_id|0x05|1|uint8|发起绑定请求的设备标识，6字节值，同一个发起方值相同，不同发起方之间不同。|O|1\.0\.0|
|bt\_bond\_key|0x06|1|uint8|在bt\_bond\_verions存在并且为1时，用于消息通知的加密的密钥。|O|1\.0\.0|
|bt\_bond\_iv|0x07|1|uint8|用于参与加密生成bt\_bond\_Key时的IV|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ODIxMWYxOWMwOTNlMTUzZmU0ZjBmYTE4ZGU3MTJlODNfOTZkZjQ4ZGZjZDA2MGQxMzQyMDY5MTFhNWJmMGM2YWZfSUQ6NzY1OTM1OTQ4NjkxODQ0NjAyMF8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MDg5YWZiMGRmMmJlZWQxYTE4OWU5NDgwNTI5NTNkZWZfN2FjN2ViM2RmMmI4ODExMmUyNmUwM2Y1YjY4NGQ2NGNfSUQ6NzY1OTM1OTQ4NjIwMTIwMzg5MF8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**|||||||

---

## 查询蓝牙绑定状态 \(CID=0x06\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|bt\_bond\_status|0x01|1|uint8|蓝牙绑定状态，取值范围：\[0,1\]<br>0：代表设备和手机未绑定<br>1：代表设备和手机已绑定|M|1\.0\.0|
|bt\_bond\_status\_info|0x02|1|uint8|蓝牙绑定状态信息，取值范围\[1,255\]，该参数配合bt\_bind\_status使用。<br>1：正常绑定；bt\_bind\_status为1。 <br>2：MissKey；bt\_bind\_status为0。<br>3：Unpaired；bt\_bind\_status为0。<br>4：BondInfoAvailable；bt\_bond\_status为0。 有绑定信息，但是没有加密过程。|O|1\.0\.0|
|bt\_bond\_id|0x03|2|uint8|发起绑定请求的设备标识，2字节值，同一个发起方值相同，不同发起方之间不同。|O|1\.0\.0|
|bt\_bond\_verison|0x04|1|uint8|1：版本1，不做BT层面的配对，不做APP加密。<br>2：版本2，通过APP层对消息提醒加密，不做BT层面的配对。<br>其他：保留|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=M2Y1ZWM4ODQ2NjBmZjcyZDE4MjNkNzYyNjZjOWFlMjZfODk5M2RmMmRjNzRkZjJkOGU4NDQ1MjBiZmY0NDUxZmJfSUQ6NzY1OTM1OTUzODk5MDI1NTM1MV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGE2ODcyMmFhZWY2NzQ5NGY3Mzg5Y2Q5MGRmOGQ5NzVfMDE2OWVmMmQ0YzM4NGQzNWViNWM0ZDA3NjNhMDEwNzVfSUQ6NzY1OTM1OTUzNjEzMDM2MjU3OV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**|||||||

---

## 穿戴设备请求手持设备信息 \(CID=0x07\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length Size\(Oct\)**|**Value 数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|device\_board|0x01|1|uint8|手持设备主板信息<br>编码格式：UTF\-8|O|1\.0\.0|
|device\_brand|0x02|1|uint8|手持设备厂商信息<br>编码格式：UTF\-8|O|1\.0\.0|
|device\_name|0x03|1|uint8|手持设备名称<br>编码格式：UTF\-8|O|1\.0\.0|
|device\_model|0x04|1|uint8|手持设备型号信息<br>编码格式：UTF\-8|O|1\.0\.0|
|device\_hardware|0x05|1|uint8|手持设备硬件信息<br>编码格式：UTF\-8|O|1\.0\.0|
|device\_product|0x06|1|uint8|手持设备产品名称<br>编码格式：UTF\-8|O|1\.0\.0|
|device\_manufacturer|0x07|1|uint8|手持设备制造商信息<br>编码格式：UTF\-8|O|1\.0\.0|
|device\_release\_version|0x08|1|uint8|手持设备系统版本信息编码格式：UTF\-8|O|1\.0\.0|
|device\_sdk\_version|0x09|1|uint8|手持设备SDK   API版本信息<br>编码格式：UTF\-8|O|1\.0\.0|
|**发送帧结构：**<br>单板发送帧结构不定，自由组合：<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZWU5NGY3YmUyOGM3MWU0NDJkZTFiMWQ0NDkwMGYxNDVfMDZjNjhkMDY0NGIyNTdlZDY5NDI1OTJjM2ZhYzdjOThfSUQ6NzY1OTM1OTYwNTUyNDgxMDk2Ml8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>发送帧结构按照主动上报结构组合：<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGZkMjM2YzllZDYyN2MxODcyMjc3MTc2ZmJmMzMxZTdfYzYwMDgxODQ3N2MyYzNkOWQ4ZDU0MDQzNmI4YjhmNzJfSUQ6NzY1OTM1OTYwNzIyNzUzNDUyMl8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>|||||||

---

## 设备支持数据类型 \(CID=0x08\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length Size\(Oct\)**|**Value 数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|supported\_data\_list|0x01|1|byte|设备数据类型列表|M|1\.0\.0|
|supported\_activity\_type|0x02|1|uint8|表示设备数据类型下当前支持的数据服务类型，取值参考6\.1|M|1\.0\.0|
|avtivity\_data\_type|0x03<br>|1|bitmap|表示设备数据类型下当前支持的数据服务类型具体数据项目，取值以bitmap形式表示。参考6\.1<br>每一bit具体定义：<br>0：PUMP<br>1：<br>2：<br>3：<br>4：<br>5：<br>目前，数据类型bitmap使用1个字节够用，对于暂未定义的bit，单板返回时默认为0；后续可根据需要扩展bit位；|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NmYzOWU5NDY4YzhiYzhkZDgzMDc5MDQ0MTFhN2MxZmFfYjI4NmMyOGJjMTQzMjliOGUyZDJhNmIwNGMzNmVkOWNfSUQ6NzY1OTM1OTY2OTU4MDI1NDQyOV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDkxYTJkNzU2YmI2ODE3NWFjMTVkZGY3ZjhmOTFkMjlfMTY5OTMxZmQxZDIxYTU2NzE5ODAzMDRlNTBiMWEzOWVfSUQ6NzY1OTM1OTY3MDkxODAwNzk5NF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**使用场景：**<br>- APP会下发此指令来查询单板支持的活动类型以及每一类型需要采集的具体数据格式，APP会根据单板侧返回结果来采集数据，及初始化UI界面。|||||||

---

## 三元组接入合法性认证 \(CID=0x09\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length Size\(Oct\)**|**Value 数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|triplet\_number\_id|0x01|1|uint8|0x01：测试环境 （default）<br>0x02：正式环境|M|1\.0\.0|
|device\_triplet\_struct|0x02|1|uint8||M|1\.0\.0|
|product\_id\_number|0x03|1|uint8||M|1\.0\.0|
|device\_id\_number|0x04|1|uint8||M|1\.0\.0|
|device\_secret\_key|0x05|1|uint8||M|1\.0\.0|
|**发送帧结构：**<br>**接收帧结构：**<br>**说明：**|||||||

---

## 设置设备日期时间 \(CID=0x0A\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|time|0x01|1|uint32|日期时间，使用秒表示从起始时间1970年1月1日0时0分0秒到当前时间的秒数。<br>说明：使用格林尼治时间。|M|1\.0\.0|
|time\_offset|0x02|1|int16|和格林尼治时间的时间差，高位表示小时，低位表示分钟，如：<br>0x10 1E, 表示\+8:30。<br>0x82 00, 表示\-2:00。<br>说明：由于存在一些非整点的时区，因此增加对分钟的表示|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTEzZjZhZDlmZjM3ZDc3MmQ5MTA5MTljZmI3YjFjYmFfNDJjOTg3ZjA0MWZhYzhkN2M1MDQ1M2YxMjY4NmQzMWJfSUQ6NzY1OTM1OTc1ODYwNDAyODg3OF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MDYzMmQzZDk0ZjQ4MDBkODQ5MDA1ZWQ3OGJhNTdiZTdfZDdlYjFhZTcyYmMxYTgzYTE1MWY3MTE2ZmMzZWMxMzhfSUQ6NzY1OTM1OTc1NjkzOTQzMDg4N18xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**|||||||

---

## 获取设备电池电量百分比 \(CID=0x0B\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length Size\(Oct\)**|**Value 数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|battery\_percent|0x01|1|uint8|设备电池电量百分比。取值范围\[0,100\]|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGRmYzI1ZTBkZTg2YmRlMWUxZDhmYjVhYTM2OTJkN2JfYjFiMDc2ZWJhOWY1MjdlMjI1Y2I5NjRmMTE0YWY5MWFfSUQ6NzY1OTM1OTgwMTUyMDE5NjU4Ml8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTQzZjllY2VmMmVmYzcwYWRhOGY4N2VkMDAzNDdkZWJfOTcxZWYwNGY5YWIzZjcxYjg3NTgwNjcwMmRiNDRlNGFfSUQ6NzY1OTM1OTgwMDczNTk5Mjc5Nl8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

---

## 设置设备广播状态 \(CID=0x0C\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length Size\(Oct\)**|**Value 数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|device\_pairing\_enable|0x01|1|uint8|广播使能<br>0x00：disable<br>0x01：enable|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NWYwZDY2YzExZTUxODRhNTM0ZjFlZWNkODQ4NGM4ZTBfODM2YzgwM2Y1Y2Q0MTk1MDg2MmNlNjU1MTZiODg3NTZfSUQ6NzY1OTM1OTgzOTI4MTY5NTk2N18xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=Y2E2NDJkYWFmMmFjZTRjMTkwYjAxZWY2NTUzMDQ1OGVfYjllMTllNDFiYzQ2NjA3OTNiMmRjNjA2ZjMwNDdjZGNfSUQ6NzY1OTM1OTg0MTI1MjcyMzY3Ml8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**|||||||

---

# Notification Service

## 设备消息通知 \(CID=0x01\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|message\_id|0x01|1|uint16|消息索引。APP消息传输时，从0开始累加，相同的消息id相同，用于支持单条消息的分包传输。取值方式65535取模。|O|1\.0\.0|
|message\_type|0x02|2|uint16|2个字节，第1个字节代表消息类型，第2个字节代表消息定义<br>0x01：通用状态消息 <br>        01：低电提醒<br>        02：<br>        03：<br>0x10：pump类消息<br>        01：溢奶提醒<br>        02：满奶提醒<br>        03：姿态提醒<br>        04：佩戴提醒<br>0xFF：通用错误码消息<br>错误码索引<br>[《Momcozy 智能设备通用错误码标准定义v1\.0\.1》](https://alidocs.dingtalk.com/i/nodes/lyQod3RxJKvzLLnbfdzMADb2Vkb4Mw9r?utm_scene=person_space)|M|1\.0\.0|
|message\_content\_data|0x03|variable|uint8|消息数据|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=Yzc1OGYyOGI1MDkwOTVkZTQxMDJiZmE1YzJiMzE4MzhfZDgwMmViOTU5NDRiMDM2ZGVhYWQyMGJlNTU1ZWQxMWJfSUQ6NzY1OTM1OTkzMDY5MTk2Mzg2NV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGY5MzU4ZWJlZGY3NGMzYmQxYmJiZTFmYmYzYzZlZTNfNTVlYTAwYzMxMjQ4ZTc1NmVmYzY2ZTIyYTM5NmVhMWVfSUQ6NzY1OTM1OTkyODkwNTYzMjczNl8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- 自动上报|||||||

---

## 设置消息通知开关 \(CID=0x02\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|message\_notify\_list|0x01|NA|NA|消息开关设置数据列表||1\.0\.0|
|message\_notify\_struct|0x02|1|NA|消息开关设置结构体，子节点包含|M|1\.0\.0|
|message\_type|0x03|2|uint16|2个字节，第1个字节代表消息类型，<br>0x01：通用状态消息<br>0x10：pump类<br>0xFF：通用错误码消息<br>第2个字节代表消息定义，<br>01：低电提醒<br>02：溢奶提醒<br>03：满奶提醒<br>04：姿态提醒<br>05：佩戴提醒<br>06：状态提醒<br>错误码索引[《Momcozy 智能设备通用错误码标准定义v1\.0\.1》](https://alidocs.dingtalk.com/i/nodes/lyQod3RxJKvzLLnbfdzMADb2Vkb4Mw9r?utm_scene=person_space)|M|1\.0\.0|
|message\_notify\_enable<br>|0x04|variable|uint8|设置message\_type对应消息开关状态，<br>0x00：disable<br>0x01：enable|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NDI5YjFhNTRkYzQ3N2Q2NjQ0NDJjNjFiMDA3MThiM2VfNjgzNmNlNDcyMjM4OWFlMDg5ZDNlMDEyZThhMzA4NjNfSUQ6NzY1OTM1OTk3MzcwODgyNzYwNF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YmQ4ZjE4MmI2MzA5MzMxOGRjNzdmNDUxZmExYmI2Y2FfOTRiN2ZlODQyNDBhYzUxMmIzNTc0ODhjMDFiNTZjOTZfSUQ6NzY1OTM1OTk3NDA3Nzg0NDcwMV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- 远端主动查询获取。|||||||

---

## 查询支持消息通知类型 \(CID=0x03\)

**字段描述：**

|**字段名称**|**Type\(bit0\~bit6\)**|**Length** **Size\(Oct\)**|**Value** **数据类型**|**说明**|**必选/可选\(M/O\)**|**版本**|
|---|---|---|---|---|---|---|
|supported\_notify\_message\_list|0x01|NA|NA|消息开关设置数据列表||1\.0\.0|
|supported\_notify\_message\_struct|0x02|1|NA|消息开关设置结构体，子节点包含|M|1\.0\.0|
|supported\_notify\_message\_type|0x03|2|uint16|2个字节，第1个字节代表消息类型，<br>0x01：通用状态消息<br>0x10：pump设备类<br>0xFF：通用错误码消息|M|1\.0\.0|
|message\_notify\_enable|0x04|variable|uint8|设置message\_type对应消息开关状态，<br>0x00：disable<br>0x01：enable|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZmIyYjM5YTYxYTdlNjVkZTU1MzBiYWE1MDI2MTRhODVfMTUyNTZlYmI0ZDY3NjJmNjMxYmJlMmM4ODVjYWZjZTdfSUQ6NzY1OTM2MDAyNDU5NDM2OTUwNl8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NTM3MmRiNmZkMzAxMzU0OTg4YWY0YWEzODQ5MzcyYTlfZjA5NmQ2YTI3YjZhNzliYmMyMDhlNmZkNjM3ZmY1NjNfSUQ6NzY1OTM2MDAyNjYxOTk3MjU1NV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**|||||||

# FCT\&ATE Service

## 获取设备产测状态（CID=0x01）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)<br>11   11 1111|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)<br>|版本|
|---|---|---|---|---|---|---|
|device\_name|0x01|1|uint8\[\]|待测设备名称|O|1\.0\.0|
|device\_fw\_version|0x02|1|uint8\[\]|待测设备的固件版本|O|1\.0\.0|
|device\_mac\_address|0x03|1|uint8|待测设备的mac地址|O|1\.0\.0|
|factory\_complete\_status|0x04|1|uint8|产测通过标识，用于标注是否经过产测（掉电不消失）|O|1\.0\.0|
|hw\_version|0x05|1|uint8\[\]|硬件版本号|O|1\.0\.0|
|res\_version|0x06|1|uint8\[\]|资源版本号|O|1\.0\.0|
|factory\_mode\_list|0x20|NA|NA|工厂模式数据列表|O|1\.0\.0|
|factory\_mode\_struct|0x21|NA|NA|工厂模式数据结构体|O|1\.0\.0|
|factory\_mode\_type|0x22|1<br>|uint8|工厂模式类型<br>00：idle\_mode<br>01：factory\_test\_mode<br>02：aging\_test\_mode<br>03：suckion\_test\_mode<br>04：sucktion\_compensate\_mode<br>05：ate\_test\_mode|O|1\.0\.0<br>|
|factory\_mode\_status|0x23|1<br>|uint8|模式使能状态<br>0x00: disabled<br>0x01: enabled|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDIwMDY0MGYxMmNjOTc0N2Q1M2JiMWZlYTFhOWY1YjBfNDYyYWRiMTc2ZmIzOTk0MTM1Yzk2YjFjNmFmZjYzZWFfSUQ6NzY1Nzg5NzAwOTQ1MzU1MDc5Nl8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YmE1YjBhODMwY2UyNzNjMGJjMDk4NjQ3YTZhM2M1MTdfOGI3NDgwN2JmOWYyYWNhYWRlNjg3MjA3MzkxNzI3N2JfSUQ6NzY1Nzg5NzAxNjA1MTU2NzU5OV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- 远端主动查询获取。|||||||

## 设置设备产测状态（CID=0x02）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|factory\_complete\_status|0x01|1|uint8|产测通过标识，用于标注是否经过产测（掉电不消失）|O|1\.0\.0|
|factory\_mode\_list|0x02|NA|NA|工厂模式数据列表|O|1\.0\.0|
|factory\_mode\_struct|0x03|NA|NA|工厂模式数据结构体|O|1\.0\.0|
|factory\_mode\_type|0x04|1|uint8|工厂模式类型<br>00：idle\_mode<br>01：factory\_test\_mode<br>02：aging\_test\_mode<br>03：suckion\_test\_mode<br>04：sucktion\_compensate\_mode<br>05：ate\_test\_mode|O|1\.0\.0|
|factory\_mode\_enable|0x05|1|uint8|模式使能状态<br>0x00: disabled<br>0x01: enabled|O|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZjUwZWRhOTVjZjdiOGMyNDIyZTYzZDdjYTBhZDM3MGFfMjc3MjU4NGUxMTUzZWNmNTM1ODE4OTFjN2M3Nzg3ZjBfSUQ6NzY1Nzg5NzE4NTcyNzY2MzA1Nl8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MmJlZmU1OTQ4M2IwZTRkOTEwZWJiZjJlMTQzZjhhOWFfNTRmYjg5ODZhYjdhOTJiODE2MmMzNDRlMDc2OGI0ZDNfSUQ6NzY1Nzg5NzE4MjM5MzI4OTY3M18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- |||||||

## 获取通用设备数据（CID=0x03）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|device\_side\_id|0x01|1|uint8|0x00：Left <br>0x01：Right<br>0x02：Independent|M|1\.0\.0|
|device\_data\_timestap|0x02|1|uint8\[\]|数据的UTC写入时间戳|M|1\.0\.0|
|device\_data\_list|0x03|1|uint8|数据列表|M|1\.0\.0|
|device\_data\_struct|0x04|1|uint8\[\]|通用数据结构体，包含<br>device\_data\_type<br>device\_data|M|1\.0\.0|
|device\_data\_type|0x05|1|uint8\[\]|数据类型<br>0x01：device\_sn\_number<br>0x02：product\_id\_number<br>0x03：device\_id\_number<br>0x04：device\_secret\_key<br>0x05：<br>0x06：|M|1\.0\.0|
|device\_data|0x06|variable|uint8\[\]|读出数据|M|1\.0\.0|
|**发送帧结构：**<br>**接收帧结构：**<br>**说明：**<br>- 远端主动查询获取。|||||||

## 设置通用设备数据（CID=0x04）

**字段描述：****（掉电不消失）**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|device\_side\_id|0x01|1|uint8|0x00：Left <br>0x01：Right<br>0x02：Independent|M|1\.0\.0|
|device\_data\_timestap|0x02|4|uint32|UTC时间戳|M|1\.0\.0|
|device\_data\_list|0x03||uint8|写入数据列表|M|1\.0\.0|
|device\_data\_struct|0x04||uint8|通用数据结构体，包含<br>device\_data\_type<br>device\_data|M|1\.0\.0|
|device\_data\_type|0x05||uint8|写入的数据类型<br>0x01：device\_sn\_number<br>0x02：product\_id\_number<br>0x03：device\_id\_number<br>0x04：device\_secret\_key<br>0x05：<br>0x06：|M|1\.0\.0|
|device\_data|0x06|variable|uint8\[\]|写入数据信息|M|1\.0\.0|
|**发送帧结构：**<br>**接收帧结构：**<br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 设置射频测试（CID=0x05）无获取

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|rf\_test\_struct|0x01|NA|NA|射频测试指令数据结构体，包含：<br>rf\_test\_type<br>rf\_test\_data|M|1\.0\.0|
|rf\_test\_type|0x02|1|uint8|信令模式 0x00：signaling\_test   <br>非信令模式 0x01：non\-signaling<br>频率校准模式 0x02：frequency\_calibrate|M|1\.0\.0|
|rf\_test\_data|0x03|1|uint8\[\]|使能和关闭信令测试功能<br>0x00: disable<br>0x01: enable|M|1\.0\.0|
|**发送帧结构：**<br>**接收帧结构：**<br>**说明：**<br>- 使能信令模式后，设备蓝牙会断开，所以无Disable的能力，所以只有设置不读<br>|||||||

## 获取设备射频数据（CID=0x06）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|rf\_rssi\_value|0x01|2|uint8|设备返回实时rssi值|M|1\.0\.0|
|rf\_trim\_value|0x02|2|uint8|设备返回频偏校准值|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YWRkODBlOWQwM2I3YWU3YWY2MDQwMjE3MjNjNGI3MzRfMWE1OWNmZThlZWFlNTMxYWE3ZmRjZTE1MWZiOGU3NTZfSUQ6NzY1Nzg5NzUyMTgxMzE3OTU4MF8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MmY0MmYzMmU0MDY4ZWM3NmY3MjU4NGM3MjM0ZDJhOGFfYzg4MjdkY2RkYmU3MTgwYTY3ZDk0OTA1MzRiNDlhMjlfSUQ6NzY1Nzg5NzUyMjYzMTM2MzUxMl8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 设置设备射频数据（CID=0x07）

**字段描述：****（掉电不消失）**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|rf\_data\_timestap|0x01|4|uint32|射频数据写入UTC时间戳|M|1\.0\.0|
|rf\_data\_struct|0x02|||射频设置数据结构体，包含<br>rf\_tx\_power<br>rf\_trim\_value|M|1\.0\.0|
|rf\_tx\_power|0x03|1|uint8|发射功率|M|1\.0\.0|
|rf\_trim\_value|0x04|1|uint8|频偏校准trim|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YTU3Yzc1ZDA3MjI0ZTJmOWY4MjcxNjZhNjUyNjk0NGFfMDdiNjg1NjFhNmU4NGQ0MzMxODg1ZTU5MDI3MjVhNWZfSUQ6NzY1Nzg5NzU5MjU3NTEwMjE1M18xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NTgwZjg2OGFlNzgyMjI0NDVhOGVlNzRkMTNjMTZmZDJfY2RkMGYwMGYwNmEzYzlkMGVkOTg2OTBlMTFiYjAxZjlfSUQ6NzY1Nzg5NzU5MjY1NDcxMjAwOV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**<br>- |||||||

## 获取设备传感器数据（CID=0x08）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|dut\_sensor\_list|0x01|NA|NA|待测传感器列表|M|1\.0\.0|
|dut\_sensor\_struct|0x02|NA|NA|待测传感器数据|M|1\.0\.0|
|dut\_sensor\_type|0x03|1|uint8|表示数据类型下当前支持的数据类型，<br>00：IMU Sensor <br>01：Pressure Sensor <br>02：Airflow Sensor <br>03：TOF Sensor <br>04：Capacitive Sensor <br>05：Infrared Sensor <br>06：Bio\-impedance Sensor <br>07：Liquid Level Sensor <br>08：Temperature Sensor 温度<br>09：Humidity Sensor 湿度<br>0A：Proximity Sensor 接近传感<br>0B：Current Sensor 电流<br>0C：Hall Sensor 霍尔<br>0D：Encoder 编码器|M|1\.0\.0|
|dut\_sensor\_data|0x04|variable|uint8\[\]|待测传感器返回数据|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ODRlNTI5MDFjODUyY2Y3ZDM1NzJkZTU2MGYyZTA0ZmVfODg3Y2E3ZmU0OTQ4ZTk5OTI1YWQ2MDdmMDJlZTI4NjlfSUQ6NzY1Nzg5NzY2NjIzNTcxNDUwMl8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NjEwNzJmZWFmNzBiODdjNjhmNWQ2ZTZhNzE0OGZhYmJfNTIyZWYxZDEzM2YwZGI3ZTM4OWEyYTI2ZDQ4ZDRmMDdfSUQ6NzY1Nzg5NzY2NDEwOTM2NjUwM18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 设置设备传感器数据（CID=0x09）

**字段描述：****（掉电不消失）**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|dut\_sensor\_list|0x01|NA|NA|传感器参数设置数据列表|M|1\.0\.0|
|dut\_sensor\_config\_struct|0x02|NA|NA|传感器参数结构体，包含：<br>dut\_sensor\_type，<br>dut\_sensor\_config\_data|M|1\.0\.0|
|dut\_sensor\_type|0x03|1|uint8|表示数据类型下当前支持的数据类型，<br>00：IMU Sensor <br>01：Pressure Sensor <br>02：Airflow Sensor <br>03：TOF Sensor <br>04：Capacitive Sensor <br>05：Infrared Sensor <br>06：Bio\-impedance Sensor <br>07：Liquid Level Sensor <br>08：Temperature Sensor 温度<br>09：Humidity Sensor 湿度<br>0A：Proximity Sensor 接近传感<br>0B：Current Sensor 电流<br>0C：Hall Sensor 霍尔<br>0D：Encoder 编码器|M|1\.0\.0|
|dut\_sensor\_config\_data|0x04|variable|uint8\[\]|包含校准数据，及其他数据|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzUwODEwODIzMTQzMTc3ODMxNmI3MTkxODRiMWVhYjdfNDZmYzQxZjQ4YTY4NTgwNzVjMDkwYmJmZGYwOWJkMjRfSUQ6NzY1Nzg5Nzc0NDY5MDMwMjEzOV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YWJjOTQ5YmJiNzFkOWViNzNkOGI2MjIxMDY5YWEzNjZfMTA4MzViNzg3NTMxZGFhMTIyMDNlODA1YTMzMjRlN2JfSUQ6NzY1Nzg5Nzc0NjI0MTk0ODg5NV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- |||||||

## 获取设备阈值数据（CID=0x0A）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|exception\_threshold\_list|0x01|NA|NA|获取异常阈值数据列表|M|1\.0\.0|
|exception\_struct|0x02|NA|NA|异常阈值数据结构体，包含<br>exception\_type<br>exception\_judgment\_threshold|M|1\.0\.0|
|exception\_type|0x03|1|uint8|异常类型|M|1\.0\.0|
|exception\_judgment\_threshold|0x04|variable|uint8\[\]|获取的异常阈值|M|1\.0\.0|
|**发送帧结构：**<br>**接收帧结构：**<br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 设置设备阈值数据（CID=0x0B）

**字段描述：****（掉电不消失）**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|exception\_threshold\_list|0x01|NA|NA|设置异常阈值数据列表|M|1\.0\.0|
|exception\_struct|0x02|NA|NA|异常阈值数据结构体，包含<br>exception\_type<br>exception\_judgment\_threshold|M|1\.0\.0|
|exception\_type|0x03|1|uint8|异常类型|M|1\.0\.0|
|exception\_judgment\_threshold|0x04|variable|uint8\[\]|设置异常阈值|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OTY3YzZlNGIxZGFjNDQ2OTY5YTMzNTA3NGE0M2Q2MWJfN2M3ZmNhMDU0Y2MzNzMxNTAzZTZlNGMxOTExN2NmYjVfSUQ6NzY1Nzg5NzgyNzE1MDA3MzAyOV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTg0NzMwYzcwMDUxZjU5OTdmZjg4Yzk4NWExOTllZmNfZjUzMmNhNzg2YzU5N2Y5Yjc4MGEwYzc3NTMwYjliMGZfSUQ6NzY1Nzg5NzgyMjgwNDkzNzcwMl8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 设备控制命令（CID=0x0C）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|device\_side\_id|0x01|1||标识符<br>0x00：Left <br>0x01：Right<br>0x02：Sync，independence|M|1\.0\.0|
|dut\_control\_data\_type|0x02|1|uint8|控制指令<br>0x01：device\_factory\_reset<br>0x02：device\_power\_off<br>0x03：device\_travel\_lock|M|1\.0\.0|
|dut\_control\_data|0x03|variable|uint8\[\]|控制指令附加数据|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZmY2MDczZTkzYTJlNzBkYTYzMTExNzdiMWY1NzE5NTNfNzQyN2IxNTE0OGJkNTUzOTkyYzdmMTc5MDhhZjJjMzNfSUQ6NzY1Nzg5Nzk4NDEyNjAxMjYwOV8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YjY3MDAxMzI1MjA3ZWEwMTUyODVhYTgyN2I1MTU3NThfZjgzM2ZmNWU5MTUwMDQzNmQ2YzgzYWM5ZTQxMmM3MzdfSUQ6NzY1Nzg5Nzk4Mzc1MjgxNzYxMV8xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- 必须回包完再执行|||||||

## 自定义测试指令（CID=0x0D）比较特殊，暂时考虑不到

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|custom\_cmd\_type|0x01|1|uint8|0x00：<br>0x01：<br>0x02：|M|1\.0\.0|
|custom\_cmd\_data|0x02|variable|uint8\[\]|自定义指令附加数据|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 获取设备电池信息（CID=0x0E）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|battery\_data\_struct|0x01|NA|NA|电池数据结构体，包含：<br>battery\_percent<br>battery\_voltage<br>battery\_current<br>battery\_temperature|M|1\.0\.0|
|battery\_percent|0x02|1|uint8|设备电池电量百分比。取值范围\[0,100\]|M|1\.0\.0|
|battery\_voltage|0x03|2|uint16|设备电池电压mV|M|1\.0\.0|
|battery\_current|0x04|2|uint16|设备电池充放电电流mA|M|1\.0\.0|
|battery\_temperature|0x05|1|uint8|设备电池当前温度°C|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 设置泵阀运行参数（CID=0x0F）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|pump\_circle\_num|0x01|2|uint16|设置测试循环次数|||
|pump\_param\_struct|0x02|NA|NA|pump\_param\_info结构体，包含<br>pump\_duration\_time，pump\_interval\_time，<br>value\_enable\_time，<br>value\_disable\_time，<br>pump\_pwm\_value，<br>value\_pwm\_value，|M|1\.0\.0|
|pump\_duration\_time|0x03|2|uint16|泵工作时长（0\~65535）|M|1\.0\.0|
|pump\_interval\_time|0x04|2|uint16|泵工作间隔时长（0\~65535）|M|1\.0\.0|
|value\_enable\_time|0x05|2|uint16|阀使能时长（0\~65535）|M|1\.0\.0|
|value\_disable\_time|0x06|2|uint16|阀关闭时长（0\~65535）|M|1\.0\.0|
|pump\_pwm\_value|0x07|1|uint8|泵工作pwm（0\~100%）|M|1\.0\.0|
|value\_pwm\_value|0x08|1|uint8|阀工作pwm（0\~100%）|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 获取泵阀运行参数（CID=0x10）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|pump\_run\_time|0x01|2|uint16|当前已测试循环次数|M|1\.0\.0|
|pump\_param\_info|0x02|NA|NA|pump\_param\_info结构体，包含<br>pump\_duration\_time，pump\_interval\_time，<br>value\_enable\_time，<br>value\_disable\_time，<br>pump\_pwm\_value，<br>value\_pwm\_value|M|1\.0\.0|
|pump\_duration\_time|0x03|2|uint16|泵工作时长（0\~65535）|M|1\.0\.0|
|pump\_interval\_time|0x04|2|uint16|泵工作间隔时长（0\~65535）|M|1\.0\.0|
|value\_enable\_time|0x05|2|uint16|阀使能时长（0\~65535）|M|1\.0\.0|
|value\_disable\_time|0x06|2|uint16|阀关闭时长（0\~65535）|M|1\.0\.0|
|pump\_pwm\_value|0x07|1|uint8|泵工作pwm（0\~100%）|M|1\.0\.0|
|value\_pwm\_value|0x08|1|uint8|阀工作pwm（0\~100%）|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 按键模拟测试接口（CID=0x11）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|virtual\_key\_value|0x01|1|uint8|按键枚举索引值<br>0x01        KEY\_CODE\_POWER        电源按键<br>0x02        KEY\_CODE\_START        开始按键<br>0x03        KEY\_CODE\_MODE        模式按键<br>0x04        KEY\_CODE\_FREQUENCY        频率按键<br>0x05        KEY\_CODE\_BREASTFEEDING        母乳按键<br>0x06        KEY\_CODE\_LEFT\_CONTROL        左控制按键<br>0x07        KEY\_CODE\_RIGHT\_CONTROL        右控制按键<br>0x08        KEY\_CODE\_FACTORY\_RESET        恢复出厂按键<br>0x09        KEY\_CODE\_TRAVEL\_LOCK        旅行锁按键<br>0x0a        KEY\_CODE\_KNOB\_LEFT        旋钮左转<br>0x0b        KEY\_CODE\_KNOB\_RIGHT        旋钮右转|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 异常模拟测试接口（CID=0x12）

**字段描述：****待上层提供接口**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|virtual\_exception\_value|0x01|1|uint8|异常索引值[Momcozy 智能设备通用错误码标准定义v1\.0\.1](https://alidocs.dingtalk.com/i/nodes/lyQod3RxJKvzLLnbfdzMADb2Vkb4Mw9r?doc_type=wiki_doc&utm_medium=dingdoc_doc_plugin_card&utm_scene=person_space&utm_source=dingdoc_doc)|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 自动定期上报，或者远端主动查询获取。|||||||

## 电量模拟测试接口（CID=0x13）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|virtual\_battery\_value|0x01|1|uint16|设置模拟电池电量，单位mV。|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 设置成0就是真实的电量，设置成别的数值，就会以别的数值为准|||||||

## 自定义加热测试接口（CID=0x14）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|heat\_status\_struct|0x01|NA|NA|加热设置数据结构体|M|1\.0\.0|
|heat\_enable|0x02|1|uint8|加热使能<br>0x00: disable<br>0x01: enable|M|1\.0\.0|
|heat\_drive\_strength|0x03|1|uint8|加热强度，如pwm，或其他配置值|M|1\.0\.0|
|heat\_duration\_time|0x04|2|uint16|加热时长|O|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 回包做结果反馈|||||||

## 自定义振动测试接口（CID=0x15）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|vibration\_status\_struct|0x01|NA|NA||M|1\.0\.0|
|vibration\_enable|0x02|1|uint8||M|1\.0\.0|
|vibration\_drive\_strength|0x03|1|uint8|强度，如pwm，或其他配置值|M|1\.0\.0|
|vibration\_freq|0x04|1|uint8|振动频率|M|1\.0\.0|
|vibration\_duration\_time|0x05|2|uint16|振动时长|M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 回包做结果反馈|||||||

## 自定义姿态测试接口（CID=0x16）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
||0x01||||M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 回包做结果反馈|||||||

## 自定义液位测试接口（CID=0x17）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
||0x01||||M|1\.0\.0|
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 回包做结果反馈|||||||

## 设置数据采集上报（CID=0x18）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|cycle\_report\_enable|0x02|1|uint8|使能和关闭循环上报<br>0x00: disabled<br>0x01: enabled|M|1\.0\.0|
|report\_data\_type\_list|0x03||||||
|report\_data\_config\_struct|||||||
|report\_data\_type|0x04||uint8|表示数据类型下当前支持的数据类型，<br>00：IMU Sensor <br>01：Pressure Sensor <br>02：Airflow Sensor <br>03：TOF Sensor <br>04：Capacitive Sensor <br>05：Infrared Sensor <br>06：Bio\-impedance Sensor <br>07：Liquid Level Sensor <br>08：Temperature Sensor 温度<br>09：Humidity Sensor 湿度<br>0A：Proximity Sensor 接近传感<br>0B：Current Sensor 电流<br>0C：Hall Sensor 霍尔<br>0D：Encoder 编码器|M|1\.0\.0|
|report\_interval\_time|0x05||||||
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- 允许同时下发使能多个传感器数据类型|||||||

## 数据采集被动上报（CID=0x19）

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|report\_data\_list|0x01|NA|NA||||
|report\_data\_struct|0x02|NA|NA|pump\_param\_info结构体，包含：<br>cycle\_report\_enable<br>report\_data\_type|M|1\.0\.0|
|report\_data\_type|0x03|N|uint8|表示当前上报的数据类型:<br>00：IMU Sensor <br>01：Pressure Sensor <br>02：Airflow Sensor <br>03：TOF Sensor <br>04：Capacitive Sensor <br>05：Infrared Sensor <br>06：Bio\-impedance Sensor <br>07：Liquid Level Sensor <br>08：Temperature Sensor 温度<br>09：Humidity Sensor 湿度<br>0A：Proximity Sensor 接近传感<br>0B：Current Sensor 电流<br>0C：Hall Sensor 霍尔<br>0D：Encoder 编码器|M|1\.0\.0|
|report\_data|0x04|variable|uint8\[\]|自定义数据，如IMU sensor数据采集可能存在角度，或加速度，或角速度等多种数据。需根据实际需求定义。|||
|**发送帧结构：**<br><br>**接收帧结构：**<br><br>**说明：**<br>- |||||||

## 测试数据主动上报（CID=0x1A）限制产测模式

**字段描述：**

|字段名称|Type\(bit0\~bit6\)|Length<br>Size\(Oct\)|Value<br>数据类型|说明|必选/可选\(M/O\)|版本|
|---|---|---|---|---|---|---|
|dut\_notify\_data\_list|0x01|NA|NA|测试上报数据列表|M|1\.0\.0|
|dut\_notify\_data\_struct|0x02|NA|NA|测试数据结构体，包含<br>dut\_notify\_data\_type<br>dut\_notify\_data\_value|M|1\.0\.0|
|dut\_notify\_data\_type|0x03|1|uint8|测试上报数据类型|M|1\.0\.0|
|dut\_notify\_data\_value|0x04|variable|uint8\[\]|测试上报数据|M|1\.0\.0|
|**发送帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NGExNGZiZmFjOWY5Zjc3ZDU5NWIwYjQ5MmE3YzZiOWJfY2Y1MWZlZmY4NWRiYWQwZmQ5MDllMTBlMjVlMzVkMjFfSUQ6NzY2ODI0Njg5OTIxNzAyNjI0OF8xNzg1ODM0NTUxOjE3ODU5MjA5NTFfVjM)<br>**接收帧结构：**<br>![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=M2EyYmQ1ODA4OWJhMDhmOThlNWE5NDNiMzJhMzkwNmJfOTI3YmE2YzY4NzE4YjlmYjEyODU5ZDYwZmU0ZjAwNzlfSUQ6NzY2ODI0Njg5OTEyODkxMzE0N18xNzg1ODM0NTUwOjE3ODU5MjA5NTBfVjM)<br>**说明：**<br>- 自动上报|||||||



# TLV编码规范说明

## Type字段编码

```Plain Text
Type字段为1个字节(8 bits)：
  
Bit 7: 子节点标志 (0=无子节点, 1=有子节点)
Bit 6-0: Type值 (0-127)

例：
  0x01 = 00000001 (无子节点, Type=1)
  0x81 = 10000001 (有子节点, Type=1)
  0xFF = 11111111 (有子节点, Type=127，通用结构)
```

## Length字段编码

```Plain Text
Length表示Value的长度，采用固定长度编码
```

## Value字段编码

**基本数据类型：**

|类型|字节数|字节序|示例|
|---|---|---|---|
|uint8\_t|1|N/A|0x55 = 85|
|uint16\_t|2|Big\-Endian|0x00 0x55 = 85|
|uint32\_t|4|Big\-Endian|0x00 0x00 0x00 0x55 = 85|
|int8\_t|1|补码|0xFB = \-5|
|float|4|IEEE 754 Big\-Endian|0x42 0x4C 0x00 0x00 = 50\.0|
|string|可变|UTF\-8|“Hello” = 0x48 0x65 0x6C 0x6C 0x6F|
|boolean|1|N/A|0x00=false, 0x01=true|

---

## 泌乳会话数据包示例：



---

## 编码与解码规则

**编码示例（float类型）：**

```Plain Text
字段：pressure_kpa = -25.3 kPa
编码步骤：
1. 转换为IEEE 754格式: -25.3 = 0xC1CA0000
2. TLV封装: 
   [Type: 0x40] [Length: 0x04] [Value: 0xC1CA0000]
```

**解码示例（uint32\_t类型）：**

```Plain Text
接收的TLV数据: 03 04 67405A3F
解码步骤：
1. 获取Type: 0x03
2. 获取Length: 0x04 (4字节)
3. 获取Value: 0x67405A3F = 1766948383 (Unix时间戳)
4. 转换为可读格式: 2025-12-28T19:09:13Z
```

---

**Type字段分配策略** \- 每个事件内部的字段都按顺序分配唯一的Type值\(0x01至0x0D\)，这样便于二进制协议编码和解码

**Length字段标准** \- 基于数据类型的实际字节长度进行分配，如uint8\_t为1个字节，uint16\_t为2个字节，uint32\_t为4个字节

**必选性标记** \- M表示必需字段\(Mandatory\)，O表示可选字段\(Optional\)，这样便于协议的向前/向后兼容

**版本控制** \- 统一标注为1\.0\.0，表示在协议版本2\.0中引入和维护、或版本支持。

