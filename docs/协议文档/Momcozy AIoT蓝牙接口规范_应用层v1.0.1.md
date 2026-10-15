| **文档编号：**   | MC-AIOT                                                                       |
| ----------- | ----------------------------------------------------------------------------- |
| **版本号：**    | V 1.0.1                                                                       |
| **发布日期：**   |                                                                               |
| **密级：？？待定** | 内部公开 / 合作伙伴受控                                                                 |
| **归口部门：**   | Momcozy AIOT                                                                  |
| **变更**      | v1.0.1<br />1.增加cmd\_timeout\_ms作为接收端分包超时<br />2.增加max\_fsn\_count作为发送端最大分包限制 |

# 1 文档介绍

## 1.1 目的

文档是接口系列文档的应用层部分的规范定义文档。

## 1.2 范围

文档定义了应用层数据格式的通用定义和要求。

# 2 接口分层介绍

## 2.1 分层架构

![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-50f42def-b683-4a22-b61e-c95934ea0b82.png)

## 2.2 分层定义

<table><colgroup><col width="244"><col width="489"><col width="525"></colgroup>
<thead>
<tr>
<th><strong>应用层</strong></th>
<th>负责业务协议定义及业务数据处理，包括：<ul>
<li>应用层命令字（Command ID）定义</li>
<li>参数编码格式（TLV / 固定结构体等）</li>
<li>设备控制指令解析与执行</li>
<li>设备状态、传感数据及事件上报</li>
<li>指令响应（Response）与错误码（Error Code）定义</li>
</ul></th>
<th>该层面向具体业务功能，实现设备与 APP 的业务交互逻辑。
应用层承载各业务特性的功能，可以通过扩展业务类型加入新的业务。<br /></th>
</tr>
</thead>
</table>

# 3 应用层帧格式定义

## 3.1 Frame格式定义

![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-80d7114c-2a46-4a29-a11e-cd55d7520f8f.png)

1. 所有应用层PDU的都是以Service ID/CommandId作为起始。

2. Service ID是每个报文所属的业务类型，比如：设备管理，消息通知，闹钟等。

3) Command ID是每个业务对应的命令类型，比如：设置日期时间,获取设备版本相关信息等。

4) TLVs封装消息中的具体信息。

* `bit7 = 1`：容器节点（有子节点），不含 Value，Length = 所有子节点 TLV 总长。wire 值 = `Type | 0x80`。

* `bit7 = 0`：子节点（无子节点），Value 为实际数据。

* 字段定义表中列出的 Type 均为 **bit6-0 的逻辑值**，容器节点实际组包时需自行置 bit7。

| **Service ID** | \* 占1字节。&#xA;&#xA;\* 用于标识服务能力类型。 Service ID定义参考3.1.1                                                               |                |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| -------------- | ------------------------------------------------------------------------------------------------------------------ | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Command ID** | \* 占1字节。&#xA;&#xA;\* 用于标识具体命令类型，每个Service ID对应的Command ID都从1开始，表明真实的逻辑行为。&#xA;&#xA;\* 具体的Command ID说明在服务接口定义中详细介绍。 |                |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **TLVs**       | \* Type/Length/Value 对应每条报文的具体内容。&#xA;&#xA;\* 内容仅允许为TLV和TL形式，其中TL可在下挂子节点时使用，TL的L可以为下挂所有节点的总长度。                     | Type类型说明<br /> | \* Type占用1个字节，bit7表示该类型是否有子节点，0表示无子节点，1表示有子节点，其中有子节点的Type不能包含Value，仅为TL格式，Length表示其子节点的总长度；&#xA;&#xA;\* 对于可扩展子节点的类型，如果Length字段是0，Type字段的bit7位取值为0或1均可；&#xA;&#xA;\* bit6-0表示Type的值，取值范围0-127，每个Command ID对应的Type都从1开始代表不同的内容类型，Type类型是否包含子节点在对应类型的说明中体现。&#xA;&#xA;\* 其中127为通用错误码类型。&#xA;&#xA;\*  ![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-eb356198-79b4-4f84-82f6-2c7c9bfb1a5e.png)&#xA;&#xA;\* 含有子节点的Type格式说明：&#xA;&#xA;\*  ![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-e197b12d-bc16-45a2-a750-8baeaf2c544d.png) |
|                |                                                                                                                    | Length类型说明     | \* Length表示Value的长度，Length占用的字节数可变，目前使用到了占用1-2个字节两种情况，用前一个字节的bit7表示它后面还需要再解析一个字节，bit7为0时，表示length在此字节结束。如：&#xA;&#xA;          0-127 :0b0xxxxxxx&#xA;          128-16383:  0b1xxxxxxx 0b0xxxxxxx&#xA;\* Length占用1个字节可以表示0-127的长度，占用2个字节可以表示128-16383的长度，示意图如下：&#xA;&#xA;\* ![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-a8b9b92f-f5ba-4afc-93ef-ce818e4e603a.png)                                                                                                                                                  |

### 3.1.1 Service ID定义

为了避免各个领域和产品型态的Service ID冲突，将Service ID进行统一划分：

| **Service ID** | **Service Name**          | **Description** | **必选/可选（M/O）** | **Version** |
| -------------- | ------------------------- | --------------- | -------------- | ----------- |
| 0x01           | Device Management Service | 设备管理            | M              | 1.0.0       |
| 0x02           | Notification Service      | 通知              | M              | 1.0.0       |
| 0x03           | Fitness Service           | 健康              | O              | 1.0.0       |
| 0x04           | FCT\&ATE Service          | 测试              | O              | 1.0.0       |
| 0x05           | OTA Service               | 升级              | O              | 1.0.0       |
| 0x07           | Data Service              | 数据              | O              | 1.0.0       |
| 0x10           | Pump Service              | Pump设备类         | O              | 1.0.0       |

### 3.1.2 Command ID

Command ID：占1字节，用于标识具体命令类型，每个Service ID对应的Command ID都从1开始，表明真实的逻辑行为，具体的Command ID说明在服务接口定义中详细介绍。

### 3.1.3 通用错误码

| **字段名称**    | **Type ID** | **Length Size(Oct)** | **Value 数据类型** | **说明**                     | **必选/可选(M/O)** |
| ----------- | ----------- | -------------------- | -------------- | -------------------------- | -------------- |
| error\_code | 127         | 1                    | uint32         | 通用错误类型，具体值参见“Error Code表”。 | M              |

**Error Code表：**

| **Error Code**                                                                                                                                                                                                                                                          | **类别**                                                                                          | **描述**                                                                                   | **版本**                                                                             |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| 100000                                                                                                                                                                                                                                                                  | 通用错误<br />                                                                                      | 成功                                                                                       | 1.0.0                                                                              |
| 100001                                                                                                                                                                                                                                                                  |                                                                                                 | 未知error类型                                                                                | 1.0.0                                                                              |
| 100002                                                                                                                                                                                                                                                                  |                                                                                                 | 不支持该Service的请求                                                                           | 1.0.0                                                                              |
| 100003                                                                                                                                                                                                                                                                  |                                                                                                 | 不支持该Command的请求                                                                           | 1.0.0                                                                              |
| 100004                                                                                                                                                                                                                                                                  |                                                                                                 | 无权限                                                                                      | 1.0.0                                                                              |
| 100005                                                                                                                                                                                                                                                                  |                                                                                                 | 系统忙                                                                                      | 1.0.0                                                                              |
| 100006                                                                                                                                                                                                                                                                  |                                                                                                 | 请求格式错误                                                                                   | 1.0.0                                                                              |
| 100007                                                                                                                                                                                                                                                                  |                                                                                                 | 参数错误                                                                                     | 1.0.0                                                                              |
| 100008                                                                                                                                                                                                                                                                  |                                                                                                 | 申请内存失败                                                                                   | 1.0.0                                                                              |
| 100009                                                                                                                                                                                                                                                                  |                                                                                                 | 响应超时                                                                                     | 1.0.0                                                                              |
| 100010                                                                                                                                                                                                                                                                  |                                                                                                 | 此命令需要加密传输，明文下发拒绝处理                                                                       | 1.0.0                                                                              |
| 100011                                                                                                                                                                                                                                                                  |                                                                                                 | 解密数据格式错误                                                                                 | 1.0.0                                                                              |
| 100012                                                                                                                                                                                                                                                                  |                                                                                                 | 加密数据包序值非法                                                                                | 1.0.0                                                                              |
| 100013                                                                                                                                                                                                                                                                  |                                                                                                 | 加密数据损坏，解密失败                                                                              | 1.0.0                                                                              |
| 101001                                                                                                                                                                                                                                                                  | Device Management                                                                               | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 101002                                                                                                                                                                                                                                                                  |                                                                                                 | 设备电池电量低（禁止恢复出厂设置）                                                                        | 1.0.0                                                                              |
| 101003                                                                                                                                                                                                                                                                  |                                                                                                 | 设备不可用                                                                                    | 1.0.0                                                                              |
| 102001                                                                                                                                                                                                                                                                  | Notification                                                                                    | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 103001                                                                                                                                                                                                                                                                  |                                                                                                 | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 104001                                                                                                                                                                                                                                                                  |                                                                                                 | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 105001                                                                                                                                                                                                                                                                  | OTA<br />                                                                                       | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 105002                                                                                                                                                                                                                                                                  |                                                                                                 | 设备电池电量低（禁止升级）                                                                            | 1.0.0                                                                              |
| 105003                                                                                                                                                                                                                                                                  |                                                                                                 | 设备固件异常（禁止升级）                                                                             | 1.0.0                                                                              |
| 105004                                                                                                                                                                                                                                                                  |                                                                                                 | 设备Flash擦写失败（禁止升级）                                                                        | 1.0.0                                                                              |
| 105005                                                                                                                                                                                                                                                                  |                                                                                                 | 设备不允许升级（禁止升级）                                                                            | 1.0.0                                                                              |
| 105006                                                                                                                                                                                                                                                                  |                                                                                                 | 文件传输过程中失败                                                                                | 1.0.0                                                                              |
| 105007                                                                                                                                                                                                                                                                  |                                                                                                 | 蓝牙下载失败                                                                                   | 1.0.0                                                                              |
| 105008                                                                                                                                                                                                                                                                  |                                                                                                 | 蓝牙升级失败                                                                                   | 1.0.0                                                                              |
| 105009                                                                                                                                                                                                                                                                  |                                                                                                 | 设备写数据失败                                                                                  | 1.0.0                                                                              |
| 105010                                                                                                                                                                                                                                                                  |                                                                                                 | 设备数据校验失败                                                                                 | 1.0.0                                                                              |
| 105011                                                                                                                                                                                                                                                                  |                                                                                                 | 设备检测升级包大小异常                                                                              | 1.0.0                                                                              |
| 107001                                                                                                                                                                                                                                                                  | Data Service                                                                                    | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 107002                                                                                                                                                                                                                                                                  |                                                                                                 | 查询数据过大                                                                                   | 1.0.0                                                                              |
| 108001                                                                                                                                                                                                                                                                  |                                                                                                 | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| <span style="color: inherit; background-color: rgba(255,246,122,0.8)">110001</span>                                                                                                                                                                                     | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">Pump Service</span><br /> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">对端未连接</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |
| <span style="color: inherit; background-color: rgba(255,246,122,0.8)">110002</span>                                                                                                                                                                                     |                                                                                                 | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">低电禁止开奶碗灯</span>    | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |
| <span style="color: inherit; background-color: rgba(255,246,122,0.8)">110003</span>                                                                                                                                                                                     |                                                                                                 | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">旅行锁使能禁止启动泵阀</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |
| <span style="color: inherit; background-color: rgba(255,246,122,0.8)">110004</span>                                                                                                                                                                                     |                                                                                                 | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">当前模式不支持</span>     | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |
| 112001                                                                                                                                                                                                                                                                  | Font                                                                                            | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 119001                                                                                                                                                                                                                                                                  | BT factory Test                                                                                 | 入参为空/数据非法                                                                                | 1.0.0                                                                              |
| 通用错误码Type值为127，格式如下：&#xA;\*   6位十进制数（为了和json错误码类型兼容，不采用16进制表述）&#xA;&#xA;\*   最高1位为1，保留后续扩展位。&#xA;&#xA;\*   次高2位表示模块。&#xA;&#xA;\*   最低3位表示模块中的错误类型。&#xA;&#xA;如：101001，表示01模块中的第001个错误码。![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-20379b83-1f71-4ad2-9b16-7bf72719a281.png) |                                                                                                 |                                                                                          |                                                                                    |

**注意事项说明：**

（1）对于单板不支持的Service ID，单板返回100002（不支持该Service请求），对于单板不支持的Command ID，单板返回100003（不支持该Command请求）

### 3.1.4 通用加密格式定义

**字段描述：**

| **字段名称**                                                                                                                                                                                                                                                                                                                                                                          | **Type(bit0\~bit6)** | **Length Size(Oct)** | **Value数据类型** | **说明**                                       | **必选/可选(M/O)** | **版本** |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------- | -------------------- | ------------- | -------------------------------------------- | -------------- | ------ |
| encrypt\_type                                                                                                                                                                                                                                                                                                                                                                     | 0x7C                 | 1                    | uint8         | 是否加密，&#xA;0：代表数据不加密，默认该场景不使用&#xA;1：代表AES加密方式 | O              | 1.0.0  |
| encrypt\_iv                                                                                                                                                                                                                                                                                                                                                                       | 0x7D                 | 1                    | uint8         | 加密动态IV，默认使用16字节                              | O              | 1.0.0  |
| encrypt\_data                                                                                                                                                                                                                                                                                                                                                                     | 0x7E                 | 1\~2                 | uint8         | 加密后的数据, 对应原来的所有TLV字段                         | O              | 1.0.0  |
| 说明：该三个字段必须同时存在，或者同时不存在。对于需要加密的命令，按照如下步骤进行处理（以设置设备时间举例）：&#xA;1\.   按照对应的业务接口组包成原始TLV信息（time对应的TLV和time\_offset对应的TLV）；&#xA;&#xA;2\.   将原始TLV信息进行整体加密并作为encrypt\_data的Value进行7E字段TLV的封装；&#xA;&#xA;3\.   封装encrypt\_enable以及encrypt\_iv的TLV信息并拼接encrypt\_data的TLV信息；&#xA;&#xA;按照上述步骤封装后的TLV命令包结构如下：![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-c64c658b-2def-4a52-9273-124a0c2d14d4.png) |                      |                      |               |                                              |                |        |

## 3.2 Data域的定义格式约束

各业务内的定义通用要求如下：

1. 业务数据定义考虑扩展性，预留扩展能力。

2. 业务内部按照功能进行合理的子类划分。

## 3.3 字节大小头约束

对于多字节传输的字段类型，均按照大头在前的组包格式传输。

比如对于uin32的四字节数据类型，传输格式如下：

![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-21207aaf-a3ad-44f5-a284-5666370daf58.png)

## 3.4 典型使用场景举例

### a. 设置类命令

APP 下发配置参数，设备执行后回复成功或错误码。请求帧携带配置 TLV，接收帧为空或仅含 error\_code。

如：设置设备时间信息

**发送帧：** `[SID] [CID] [time(0x01)] [Len=4] [0x5E4A3C00] [time_offset(0x02)] [Len=2] [0x100A]`

**接收帧（成功）：** `[SID] [CID] [error_code(127)] [Len=4] [0x000186A0]`
**接收帧（失败）：** `[SID] [CID] [error_code(127)] [Len=4] [0x000186A7]`

### b. 查询类命令 — 请求为空

请求帧不带任何 TLV，设备返回该命令的全部数据。适用于数据量小、无过滤需求的简单查询。

如：获取设备版本信息（5.1.4）不带过滤字段时，返回全部版本 TLV。

**发送帧：** `[SID] [CID]`

**接收帧：** `[SID] [CID]` + 该命令定义的全部 leaf TLV（见对应章节）。

### c. 查询类命令 — 请求携带可选过滤条件

请求帧可携带 leaf TLV 作为 AND 过滤条件。过滤字段均标 O（可选），不携带时该维度不做过滤，空请求返回全部。

如：获取设备电池电量百分比（5.1.11）按 side 过滤；获取设备吸乳工作状态（5.7.4）按 pump\_side 过滤。

**发送帧：**

* 查全部：`[SID] [CID]`

* 查 Left 侧：`[SID] [CID] [pump_side(0x03)] [Len=1] [0x00]`

**接收帧（Left 侧 pumping 示例）：**

```plaintext
[SID] [CID]
  [pump_status_list(0x81 = 0x01 | 0x80)] [Len=0x15]
    [pump_status_struct(0x82 = 0x02 | 0x80)] [Len=0x12]
      [pump_side(0x03)]         [Len=1] [0x00]
      [pump_status(0x04)]       [Len=1] [0x00]
      [pump_mode(0x05)]         [Len=1] [0x01]
      [pump_level(0x06)]        [Len=1] [0x05]
      [pump_freq(0x07)]         [Len=2] [0x0001]
      [pump_duration_time(0x08)] [Len=2] [0x0258]
```

### d. 查询类命令 — 请求携带必选过滤条件

请求帧必须携带标记为 M（必选）的过滤字段，缺失则返回 100007（参数错误）。O（可选）字段可附加缩小范围。适用于数据量大、不允许全量返回的场景。

如：获取设备韵律参数（需携带 rhythm\_id）

**发送帧：**

* 合法：`[SID] [CID] [rhythm_id(0x04)] [Len=1] [0x03]`

* 合法（带可选步号）：`[SID] [CID] [rhythm_id(0x04)] [Len=1] [0x03] [step_index(0x07)] [Len=1] [0x02]`

* 非法：`[SID] [CID]` → 设备返回 `[error_code(127)] [100007]`

**接收帧：** 与对应 SET 命令结构一致，返回匹配到的嵌套 TLV 子树。

### 查询类命令通用约定

1. **请求帧**：可携带 leaf TLV 作为过滤条件，多个条件为 AND 关系。未出现的字段不做过滤。

2. **M/O 语义**：M = 必选（缺失返回 `100007`），O = 可选（不携带则不限制该维度）。

3) **Type 字段**：字段定义表中 `Type(bit0~bit6)` 列出的是逻辑编号（0\~127），实际组包时：

   * 容器节点（`_list` / `_struct`）需置 bit7：`wire_type = type | 0x80`（如 `0x01 → 0x81`）

   * 子节点不置 bit7：`wire_type = type`

   * 详见 3.1 节 Type 字节结构说明

4) **响应帧数据结构**：

   * 状态查询（GET STATUS）：响应结构为独立定义，仅返回运行时关键字段，通常比 SET 结构精简

   * 配置查询（GET SETTING）：响应结构与对应 SET 命令一致，返回完整的嵌套 TLV

5. **示例表示法**：

   * `[name(0x8X = 0x0X | 0x80)] [Len=N]` — 容器节点，Type 已含 bit7

   * `[name(0x0X)] [Len=N] [Value]` — 子节点，Value 为 hex 字面值

## 3.5 索引字段说明

对于本文档中描述到的所有索引，如果没有特殊说明，均是从1开始，并且进行递增。如果有特殊场景，需明确描述在接口字段定义中。

## 3.6 边界条件限制

1、APP和穿戴设备间应用层协议最大包长定为2048字节。

2、新增APP和穿戴设备间的TLV命令嵌套深度不超过5层。

# 4 能力协商说明

## 4.1 连接参数协商

参数协商属于设备和APP之间的一条命令，用于确定设备和APP之间使用的协议版本号，最大帧长度和MTU的等信息，协商后的参数信息用于后续数据交互。具体的参数和报文格式在5.1.1中详细介绍。

## 4.2 服务能力协商

设备和APP之间可以通过服务能力协商确定支持的能力，协商的报文格式在5.1.2中详细介绍。

## 4.3 命令能力协商

命令能力协商用于协商每个服务支持的命令，同一个服务下的命令按照功能集合进行分组，功能集合为能力协商的最小单位，所以，同版本的同一个功能集合只需协商第一个Command ID。详细的报文格式在5.1.3中详细介绍。

# 5 服务接口定义

## 5.1 Device Management Service

### 5.1.1 连接参数协商 (CID=0x01)

**字段描述：**

<table><colgroup><col width="218"><col width="146"><col width="146"><col width="133"><col width="447"><col width="131"><col width="131"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>protocol_version</td>
<td>0x01</td>
<td>1</td>
<td>uint8</td>
<td>0x01:协议版本1.0.0.0</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td>1.0.0</td>
</tr>
<tr>
<td>max_frame_size</td>
<td>0x02</td>
<td>2</td>
<td>uint16</td>
<td>MFS值（链路层最大帧长度 ）</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td>1.0.0</td>
</tr>
<tr>
<td>maximum_tranimission_unit</td>
<td>0x03</td>
<td>2</td>
<td>uint16</td>
<td>MTU值（物理层最大传输单元，不能超过蓝牙版本支持的最大MTU，且设备系统支持调整）</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td>1.0.0</td>
</tr>
<tr>
<td>transmit_interval</td>
<td>0x04</td>
<td>2</td>
<td>uint16</td>
<td>数据包发送间隔时间，单位毫秒</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td>1.0.0</td>
</tr>
<tr>
<td>arq_mode</td>
<td>0x05</td>
<td>1</td>
<td>uint8<br /></td>
<td>ARQ模式，由设备返回请求设置。
00：不启用重传，数据丢失由上层处理
01：停等协议，发送一帧等待 ACK 后再发下一帧
02：选择重传，仅重传出错帧</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td>1.0.0</td>
</tr>
<tr>
<td>ack_timeout_ms</td>
<td>0x06</td>
<td>4</td>
<td>uint32</td>
<td>ACK超时时间 >0 ms，若arq_mode不为0，则需携带该字段</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>max_retransmit</td>
<td>0x07</td>
<td>1</td>
<td>uint8</td>
<td>最大重传次数 >0，若arq_mode不为0，则需携带该字段</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>random_value<br /></td>
<td>0x08</td>
<td>1</td>
<td>uint8</td>
<td>携带该字段表明设备要鉴权APP的合法性：
加密版本+随机数。
最高两个字节表示加密版本，从1开始。
新增取值2, 表示设备支持通用加密TLV</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">cmd_timeout_ms</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x09</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">4</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint32</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">接收端分包</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">CMD</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">超时时间 ms，从接收到最新的一个分包开始计时，若未设置则用默认值12000 ms</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.1</span><br /></td>
</tr>
<tr>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">max_fsn_count</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0A</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">接收侧最多可缓存的分片数，取值1 -</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">255</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">，发送端根据max_fsn_count规划拆包，避免超出 RX 缓存，若未设置则用默认值16</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span><br /></td>
</tr>
<tr>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">link_layer_version</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0B</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">只要本命令（CID=0x01）携带该 TLV，协商阶段请求与响应帧都按 </span><code>Version=0</code><span style="color: inherit; background-color: rgba(255,246,122,0.8)"> 版本发出，避免双方版本不同导致解析失败。若双方均不携带该 TLV，则默认按 </span><code>0x00</code><span style="color: inherit; background-color: rgba(255,246,122,0.8)"> 处理</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td colspan="7"><strong>发送帧结构：</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-84a283db-c1a0-4645-9640-4a375435daa3.png" alt=""><strong>接收帧结构：</strong>
<strong>返回成功</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-bc305970-3e05-46e3-8f09-ec6c9032a59e.png" alt="">
<strong>返回失败</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-750916a1-4963-47d7-9eca-c684613bb1c2.png" alt=""><strong>说明：</strong><ul>
<li>连接参数可根据实际需求增加TLVs，实际导入时根据Type需求下发协商参数。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### ~~5.1.2 服务能力协商 (CID=0x02)~~

**字段描述：**

<table><colgroup><col width="216"><col width="147"><col width="147"><col width="139"><col width="441"><col width="131"><col width="131"></colgroup>
<thead>
<tr>
<th><del><strong>字段名称</strong></del></th>
<th><del><strong>Type(bit0~bit6)</strong></del></th>
<th><del><strong>Length</strong> <strong>Size(Oct)</strong></del></th>
<th><del><strong>Value</strong> <strong>数据类型</strong></del></th>
<th><del><strong>说明</strong></del></th>
<th><del><strong>必选/可选(M/O)</strong></del></th>
<th><del><strong>版本</strong></del></th>
</tr>
</thead>
<tbody>
<tr>
<td><del>support_service_request</del></td>
<td><del>0x01</del></td>
<td><del>1</del></td>
<td><del>uint8</del></td>
<td><del>支持的服务类型列表，每个字节是对应服务的Service ID值，<span style="color: inherit; background-color: rgba(255,246,122,0.8)">请求帧必带</span></del></td>
<td><del>O</del></td>
<td><del>1.0.0</del></td>
</tr>
<tr>
<td><del>support_service_response</del></td>
<td><del>0x02</del></td>
<td><del>1</del></td>
<td><del>uint8</del></td>
<td><del>对应服务列表的支持能力，1：支持，0：不支持，<span style="color: inherit; background-color: rgba(255,246,122,0.8)">响应帧必带</span></del></td>
<td><del>O</del></td>
<td><del>1.0.0</del></td>
</tr>
<tr>
<td colspan="7"><del><strong>发送帧结构：</strong></del><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-ace4be17-2609-4ae8-bdc1-080e350bf3c8.png" alt=""><del><strong>接收帧结构：</strong></del><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-0300546a-5aba-4f9c-963c-4a1802706d94.png" alt=""><del><strong>说明：</strong></del><ul>
<li><del>Value根据具体Service ID来下发查询。</del></li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.1.3 命令能力协商 (CID=0x03)

**字段描述：**

| **列表名称**                                                                                                  | **结构体名称**                                                                                                   | **字段名称**                                                                                  | **Type(bit0\~bit6)**                                                                                                                                                  | **Length** **Size(Oct)**                                                                         | **Value** **数据类型**                                                                    | **说明**                                                                                                                                                                                                                                                                                                                                                                                                               | **必选/可选(M/O)**                                                                       | **版本**                                                                                                                                                                 |
| --------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| <span style="color: inherit; background-color: rgba(255,246,122,0.8)">support_command_request_list</span> |                                                                                                             |                                                                                           | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01</span>                                                                                     | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span>                  | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">命令能力协商列表，包含：</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">support_command_request_struct1,</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">support_command_request_struct2,</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">...</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span>                                                                                     |
|                                                                                                           | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">support_command_request_struct</span> | <br />                                                                                    | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span><br /> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span>                  | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span><br /> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">命令能力协商结构体，包含：</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">service_id,</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">command_list,</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">command_support_result</span>                       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span><br /> |
|                                                                                                           |                                                                                                             | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">service_id</span>   | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">3</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span>                   | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span>    | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">协商命令所属的service   id</span>                                                                                                                                                                                                                                                                                                                     | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span>       |
|                                                                                                           |                                                                                                             | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">command_list</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">4</span>       | <br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">&gt;= 1</span><br /> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span>    | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">协商的命令字列表</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">{cid1，cid2，cid3...}，其中每个cid是uint8，对端也带自己支持的命令字列表{cid1，cid2，cid3...}，两边取合集</span>                                                                                                                                                                         | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span><br /> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span>       |

**发送帧结构：**

```plaintext
[SID] [CID]
[support_command_request_list(0x81 = 0x01|0x80)] [Len=0x09]
  [support_command_request_struct(0x82 = 0x02|0x80)] [Len=0x07]
    [service_id(0x03)]       [Len=1] [0x01]          ← DevMgmt
    [command_list(0x04)]     [Len=2] [0x04] [0x0B]  ← 协商 GetVersion / GetBattery
```

**接收帧结构：**

```plaintext
[SID] [CID]
[support_command_request_list(0x81 = 0x01|0x80)]    [Len=0x09]
  [support_command_request_struct(0x82 = 0x02|0x80)] [Len=0x07]
    [service_id(0x03)]       [Len=1] [0x01]          ← DevMgmt
    [command_list(0x04)]     [Len=2] [0x04] [0x0B]   ← 协商 GetVersion / GetBattery
```

**说明：**

* 查询跟返回结果command support list，对应type为0x04。

***

### 5.1.4 获取设备版本相关信息 (CID=0x04)

**字段描述：**

| **字段名称**                                                                                                                                                                                      | **Type(bit0\~bit6)** | **Length** **Size(Oct)** | **Value** **数据类型** | **说明**                                                                                                       | **必选/可选(M/O)** | **版本** |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------- | ------------------------ | ------------------ | ------------------------------------------------------------------------------------------------------------ | -------------- | ------ |
| bt\_version<br />                                                                                                                                                                             | 0x01                 | NA                       | uint8              | 蓝牙接口版本号，传输内容为字符串。字符串内容格式详见：附录6.1蓝牙版本号约束规则                                                                    | M              | 1.0.0  |
| device\_type                                                                                                                                                                                  | 0x02                 | 1                        | uint8              | 设备类型&#xA;00：pump吸奶器，&#xA;01：watch&#xA;02：按摩仪                                                                 | M              | 1.0.0  |
| device\_version                                                                                                                                                                               | 0x03                 | NA                       | uint8              | 设备硬件版本号，传输内容为字符串。                                                                                            | O              | 1.0.0  |
| device\_fw\_version                                                                                                                                                                           | 0x04                 | NA                       | uint8              | 设备软件版本号&#xA;字符串内容格式详见：[ Momcozy FW版本释放规范v1.2](https://momcozy-in.feishu.cn/wiki/UVz4wh4GDiTn9rkkQbJcRUzGngc) | M              | 1.0.0  |
| device\_bt\_mac                                                                                                                                                                               | 0x05                 | NA                       | uint8              | 设备蓝牙mac地址                                                                                                    | O              | 1.0.0  |
| device\_sn                                                                                                                                                                                    | 0x06                 | NA                       | uint8              | 设备SN号，传输内容为字符串。                                                                                              | O              | 1.0.0  |
| device\_model                                                                                                                                                                                 | 0x07                 | NA                       | uint8              | 设备型号，传输内容为字符串。                                                                                               | O              | 1.0.0  |
| **发送帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-6d0d97b0-82c5-490c-9bb8-76b114d3860f.png)**接收帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-85cf71e1-8b0a-43c8-b033-299bfaf88942.png)**说明：** |                      |                          |                    |                                                                                                              |                |        |

***

### 5.1.5 蓝牙绑定请求 (CID=0x05)

**字段描述：**

| **字段名称**                                                                                                                                                                                      | **Type(bit0\~bit6)** | **Length** **Size(Oct)** | **Value** **数据类型** | **说明**                                              | **必选/可选(M/O)** | **版本** |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------- | ------------------------ | ------------------ | --------------------------------------------------- | -------------- | ------ |
| require\_bt\_bond                                                                                                                                                                             | 0x01                 | 1                        | uint8              | 发送蓝牙绑定请求                                            | M              | 1.0.0  |
| bt\_bond\_answer                                                                                                                                                                              | 0x02                 | 1                        | uint8              | 设备确认绑定请求，取值范围：\[0,1]&#xA;0：代表设备不允许绑定&#xA;1：代表设备允许绑定 | M              | 1.0.0  |
| bt\_bond\_os                                                                                                                                                                                  | 0x03                 | 1                        | uint8              | 发送绑定请求的手机操作系统，取值范围：\[0,1]&#xA;0：Android&#xA;1：iOS   | M              | 1.0.0  |
| bt\_bond\_feature                                                                                                                                                                             | 0x04                 | 1                        | uint8              | 发起绑定请求的特征码，值为4个字节的随机数，如”1234”,   ”0205”等。           | M              | 1.0.0  |
| bt\_bond\_id                                                                                                                                                                                  | 0x05<br />           | 1                        | uint8              | 发起绑定请求的设备标识，6字节值，同一个发起方值相同，不同发起方之间不同。               | O              | 1.0.0  |
| bt\_bond\_key                                                                                                                                                                                 | 0x06                 | 1                        | uint8              | 在bt\_bond\_verions存在并且为1时，用于消息通知的加密的密钥。             | O              | 1.0.0  |
| bt\_bond\_iv                                                                                                                                                                                  | 0x07                 | 1                        | uint8              | 用于参与加密生成bt\_bond\_Key时的IV                           | O              | 1.0.0  |
| **发送帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-1cb34339-4f5b-4057-a448-86a3fc855929.png)**接收帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-d79e1d62-2eef-4b56-8993-ea815f15af28.png)**说明：** |                      |                          |                    |                                                     |                |        |

***

### 5.1.6 查询蓝牙绑定状态 (CID=0x06)

**字段描述：**

| **字段名称**                                                                                                                                                                                      | **Type(bit0\~bit6)** | **Length** **Size(Oct)** | **Value** **数据类型** | **说明**                                                                                                                                                                                                          | **必选/可选(M/O)** | **版本** |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------- | ------------------------ | ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------- | ------ |
| bt\_bond\_status                                                                                                                                                                              | 0x01                 | 1                        | uint8              | 蓝牙绑定状态，取值范围：\[0,1]&#xA;0：代表设备和手机未绑定&#xA;1：代表设备和手机已绑定                                                                                                                                                            | M              | 1.0.0  |
| bt\_bond\_status\_info                                                                                                                                                                        | 0x02                 | 1                        | uint8              | 蓝牙绑定状态信息，取值范围\[1,255]，该参数配合bt\_bind\_status使用。&#xA;1：正常绑定；bt\_bind\_status为1。 &#xA;2：MissKey；bt\_bind\_status为0。&#xA;3：Unpaired；bt\_bind\_status为0。&#xA;4：BondInfoAvailable；bt\_bond\_status为0。 有绑定信息，但是没有加密过程。 | O              | 1.0.0  |
| bt\_bond\_id                                                                                                                                                                                  | 0x03                 | 2                        | uint8              | 发起绑定请求的设备标识，2字节值，同一个发起方值相同，不同发起方之间不同。                                                                                                                                                                           | O              | 1.0.0  |
| bt\_bond\_verison<br />                                                                                                                                                                       | 0x04                 | 1                        | uint8              | 1：版本1，不做BT层面的配对，不做APP加密。&#xA;2：版本2，通过APP层对消息提醒加密，不做BT层面的配对。&#xA;其他：保留                                                                                                                                           | O              | 1.0.0  |
| **发送帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-b986f3d5-f352-48f1-a2f8-3be1eb935072.png)**接收帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-2d11e714-1814-40a6-b05e-9300860d8b3f.png)**说明：** |                      |                          |                    |                                                                                                                                                                                                                 |                |        |

***

### 5.1.7 穿戴设备请求手持设备信息 (CID=0x07)

**字段描述：**

| **字段名称**                                                                                                                                                                                                          | **Type(bit0\~bit6)** | **Length Size(Oct)** | **Value 数据类型** | **说明**                           | **必选/可选(M/O)** | **版本** |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------- | -------------------- | -------------- | -------------------------------- | -------------- | ------ |
| device\_board                                                                                                                                                                                                     | 0x01                 | 1                    | uint8          | 手持设备主板信息&#xA;编码格式：UTF-8          | O              | 1.0.0  |
| device\_brand                                                                                                                                                                                                     | 0x02                 | 1                    | uint8          | 手持设备厂商信息&#xA;编码格式：UTF-8          | O              | 1.0.0  |
| device\_name                                                                                                                                                                                                      | 0x03                 | 1                    | uint8          | 手持设备名称&#xA;编码格式：UTF-8            | O              | 1.0.0  |
| device\_model                                                                                                                                                                                                     | 0x04                 | 1                    | uint8          | 手持设备型号信息&#xA;编码格式：UTF-8          | O              | 1.0.0  |
| device\_hardware                                                                                                                                                                                                  | 0x05                 | 1                    | uint8          | 手持设备硬件信息&#xA;编码格式：UTF-8          | O              | 1.0.0  |
| device\_product                                                                                                                                                                                                   | 0x06                 | 1                    | uint8          | 手持设备产品名称&#xA;编码格式：UTF-8          | O              | 1.0.0  |
| device\_manufacturer                                                                                                                                                                                              | 0x07                 | 1                    | uint8          | 手持设备制造商信息&#xA;编码格式：UTF-8         | O              | 1.0.0  |
| device\_release\_version                                                                                                                                                                                          | 0x08                 | 1                    | uint8          | 手持设备系统版本信息编码格式：UTF-8             | O              | 1.0.0  |
| device\_sdk\_version                                                                                                                                                                                              | 0x09                 | 1                    | uint8          | 手持设备SDK   API版本信息&#xA;编码格式：UTF-8 | O              | 1.0.0  |
| **发送帧结构：**<br />单板发送帧结构不定，自由组合：![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-1120f299-3f07-49b5-b8d0-8731c2815b04.png)发送帧结构按照主动上报结构组合：![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-17cf4933-aa5d-417c-8473-b95d52425865.png) |                      |                      |                |                                  |                |        |

***

### 5.1.8 设备支持数据类型 (CID=0x08)

**字段描述：**

<table><colgroup><col width="150"><col width="150"><col width="100"><col width="100"><col width="272"><col width="350"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value 数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>supported_data_list</td>
<td>0x01</td>
<td>1</td>
<td>byte</td>
<td>设备数据类型列表</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td>supported_activity_type</td>
<td>0x02</td>
<td>1</td>
<td>uint8</td>
<td>表示设备数据类型下当前支持的数据服务类型，取值参考6.1</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td>avtivity_data_type<br /></td>
<td>0x03</td>
<td>1</td>
<td>bitmap</td>
<td>表示设备数据类型下当前支持的数据服务类型具体数据项目，取值以bitmap形式表示。参考6.1
每一bit具体定义：
0：PUMP
1：
2：
3：
4：
5：
目前，数据类型bitmap使用1个字节够用，对于暂未定义的bit，单板返回时默认为0；后续可根据需要扩展bit位；</td>
<td>M</td>
<td>1.0.0<br /></td>
</tr>
<tr>
<td colspan="7"><strong>发送帧结构：</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-6337f0cd-3a7c-4b54-87ec-33a20dd01073.png" alt=""><strong>接收帧结构：</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-ede87d6d-131a-4ed9-8789-79eae5e850a5.png" alt=""><strong>使用场景：</strong><ul>
<li>APP会下发此指令来查询单板支持的活动类型以及每一类型需要采集的具体数据格式，APP会根据单板侧返回结果来采集数据，及初始化UI界面。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.1.9 三元组接入合法性认证 (CID=0x09)

**字段描述：**

| **结构体名称**                               | **字段名称**            | **Type(bit0\~bit6)** | **Length Size(Oct)**                                                            | **Value 数据类型** | **说明**                                                                                          | **必选/可选(M/O)** | **版本** |
| --------------------------------------- | ------------------- | -------------------- | ------------------------------------------------------------------------------- | -------------- | ----------------------------------------------------------------------------------------------- | -------------- | ------ |
|                                         | triplet\_number\_id | 0x01                 | 1                                                                               | uint8          | 0x01：测试环境 （default）&#xA;0x02：正式环境                                                               | M              | 1.0.0  |
| device\_triplet\_struct                 |                     | 0x02                 | 1                                                                               | uint8          |                                                                                                 | M              | 1.0.0  |
|                                         | product\_id\_number | 0x03                 | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">6</span>  | uint8          | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">固定最大 6 bytes，超出截断</span>  | M              | 1.0.1  |
|                                         | device\_id\_number  | 0x04                 | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">16</span> | uint8          | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">固定最大 16 bytes，超出截断</span> | M              | 1.0.1  |
|                                         | device\_secret\_key | 0x05                 | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">16</span> | uint8          | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">二进制密钥</span>              | M              | 1.0.1  |
| **发送帧结构：**<br />**接收帧结构：**<br />**说明：** |                     |                      |                                                                                 |                |                                                                                                 |                |        |

***

### 5.1.10 设置设备日期时间 (CID=0x0A)

**字段描述：**

| **字段名称**                                                                                                                                                                                      | **Type(bit0\~bit6)** | **Length** **Size(Oct)** | **Value** **数据类型** | **说明**                                                                                                  | **必选/可选(M/O)** | **版本** |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------- | ------------------------ | ------------------ | ------------------------------------------------------------------------------------------------------- | -------------- | ------ |
| time                                                                                                                                                                                          | 0x01                 | 4                        | uint32             | 日期时间，使用秒表示从起始时间1970年1月1日0时0分0秒到当前时间的秒数。&#xA;说明：使用格林尼治时间。                                                | M              | 1.0.0  |
| time\_offset<br />                                                                                                                                                                            | 0x02                 | 2                        | int16              | 和格林尼治时间的时间差，高位表示小时，低位表示分钟，如：&#xA;0x10 1E, 表示+8:30。&#xA;0x82 00, 表示-2:00。&#xA;说明：由于存在一些非整点的时区，因此增加对分钟的表示 | O              | 1.0.0  |
| **发送帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-3bbf581b-79bc-4b8c-ace2-94624f2310e9.png)**接收帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-dbbce8b0-2914-4af5-948f-e10bdb1208bb.png)**说明：** |                      |                          |                    |                                                                                                         |                |        |

***

### 5.1.11 获取设备电池电量百分比 (CID=0x0B)

查询设备电量。请求可携带 side 过滤；不携带则返回全部已有侧（本机 + TWS 对端若电量有效）。

**请求帧字段：**

| **字段名称** | **Type(bit0\~bit6)** | **Length Size(Oct)** | **Value 数据类型** | **说明**                                                                                                                                                             | **必选/可选(M/O)** | **版本** |
| -------- | -------------------- | -------------------- | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------------- | ------ |
| side     | 0x03                 | 1                    | uint8          | 作用对象标识符<span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>&#xA;0x00：Left&#xA;0x01：Right&#xA;0x02：Sync&#xA;0x03：Independence | O              | 1.0.2  |

请求帧为空时返回全部 side 的电量。

**发送帧结构：**

* 查全部：`[SID] [CID]`

* 查 Left 侧：`[SID] [CID] [side(0x03)] [Len=1] [0x00]`

**接收帧字段：**

| **列表名称**                                                                                  | **结构体名称**                                                                                   | **字段名称**                                                                                     | **Type(bit0\~bit6)**                                                              | **Length Size(Oct)**                                                            | **Value 数据类型**                                                                     | **说明**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | **必选/可选(M/O)**                                                                       | **版本**                                                                             |
| ----------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------- |
| <span style="color: inherit; background-color: rgba(255,246,122,0.8)">battery_list</span> |                                                                                             |                                                                                              | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span>    | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">电量数据列表</span>                                                                                                                                                                                                                                                                                                                                                                                                    | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |
|                                                                                           | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">battery_struct</span> |                                                                                              | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span>    | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">单侧电量结构体，子节点包含 side、battery_percent</span>                                                                                                                                                                                                                                                                                                                                                                        | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |
|                                                                                           |                                                                                             | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">side</span><br />      | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span>  | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span>&#xA;<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：Independence</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |
|                                                                                           |                                                                                             | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">battery_percent</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x04</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span>  | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span> | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">该侧电量百分比，取值范围[0,100]</span>                                                                                                                                                                                                                                                                                                                                                                                       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span>       | <span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span> |

**接收帧结构：**

返回成功（Left 侧示例）：

```plaintext
[SID] [CID]
  [battery_list(0x81 = 0x01 | 0x80)]   [Len=0x08]
    [battery_struct(0x82 = 0x02 | 0x80)] [Len=0x06]
      [side(0x03)]             [Len=1] [0x00]   ← Left
      [battery_percent(0x04)]  [Len=1] [0x50]   ← 80%
```

查全部时按 Left/Right 多组返回示例：

```plaintext
[SID] [CID]
  [battery_list(0x81)] ...
    [battery_struct(0x82)]
      [side(0x03)] [Len=1] [0x00]   ← Left
      [battery_percent(0x04)] [Len=1] [0x50]
    [battery_struct(0x82)]
      [side(0x03)] [Len=1] [0x01]   ← Right
      [battery_percent(0x04)] [Len=1] [0x48]
```

**说明：**

* 自动定期上报，或者远端主动查询获取。

* 接收/上报时每组电量放在 battery\_struct 内，须携带 side，用于区分左右。

***

### 5.1.12 设置设备广播状态 (CID=0x0C)

**字段描述：**

| **字段名称**                                                                                                                                                                                      | **Type(bit0\~bit6)** | **Length Size(Oct)** | **Value 数据类型** | **说明**                                | **必选/可选(M/O)** | **版本** |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------- | -------------------- | -------------- | ------------------------------------- | -------------- | ------ |
| device\_pairing\_enable<br />                                                                                                                                                                 | 0x01                 | 1                    | uint8          | 广播使能&#xA;0x00：disable&#xA;0x01：enable | M              | 1.0.0  |
| **发送帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-7e0e0fab-7fd2-4176-9245-f02e45154f8b.png)**接收帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-9bcd4829-e539-4619-9f12-8cc1467b130f.png)**说明：** |                      |                      |                |                                       |                |        |

***

## 5.2 Notification Service

### 5.2.1 设备消息通知 (CID=0x01)

**字段描述：**

<table><colgroup><col width="168"><col width="143"><col width="143"><col width="143"><col width="449"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong> <strong>Size(Oct)</strong></th>
<th><strong>Value</strong> <strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>message_id</td>
<td>0x01</td>
<td>1</td>
<td>uint16</td>
<td>消息索引。APP消息传输时，从0开始累加，相同的消息id相同，用于支持单条消息的分包传输。取值方式65535取模。</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>message_type<br /></td>
<td>0x02<br /></td>
<td>2<br /></td>
<td>uint16<br /></td>
<td>2个字节，第1个字节代表消息类型，第2个字节代表消息定义
0x01：通用状态消息 
        01：低电提醒
        02：
        03：
0x10：pump类消息
        01：溢奶提醒
        02：满奶提醒
        03：姿态提醒
        04：佩戴提醒
0xFF：通用错误码消息
错误码索引
<a href="https://alidocs.dingtalk.com/i/nodes/lyQod3RxJKvzLLnbfdzMADb2Vkb4Mw9r?utm_scene=person_space">《Momcozy 智能设备通用错误码标准定义v1.0.1》</a></td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td>message_content_data</td>
<td>0x03</td>
<td>variable</td>
<td>uint8</td>
<td>消息数据</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7"><strong>发送帧结构：</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-ddaa6d33-6dc4-46a2-b23d-db4ee71063a8.png" alt=""><strong>接收帧结构：</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-2aee3c55-7200-4c4b-9032-570abc711d86.png" alt=""><strong>说明：</strong><ul>
<li>自动上报</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.2.2 设置消息通知开关 (CID=0x02)

**字段描述：**

<table><colgroup><col width="160"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong> <strong>Size(Oct)</strong></th>
<th><strong>Value</strong> <strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>message_notify_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>消息开关设置数据列表</td>
<td></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>message_notify_struct</td>
<td></td>
<td>0x02</td>
<td>1</td>
<td>NA</td>
<td>消息开关设置结构体，子节点包含</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>message_type</td>
<td>0x03</td>
<td>2</td>
<td>uint16</td>
<td>2个字节，第1个字节代表消息类型，
0x01：通用状态消息
0x10：pump类
0xFF：通用错误码消息
第2个字节代表消息定义，
01：低电提醒
02：溢奶提醒
03：满奶提醒
04：姿态提醒
05：佩戴提醒
06：状态提醒
错误码索引<a href="https://alidocs.dingtalk.com/i/nodes/lyQod3RxJKvzLLnbfdzMADb2Vkb4Mw9r?utm_scene=person_space">《Momcozy 智能设备通用错误码标准定义v1.0.1》</a></td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>message_notify_enable<br /></td>
<td>0x04</td>
<td>variable</td>
<td>uint8</td>
<td>设置message_type对应消息开关状态，
0x00：disable
0x01：enable</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>发送帧结构：</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-6491ea68-6101-436e-beba-cfb599585a23.png" alt=""><strong>接收帧结构：</strong><img src="images/Momcozy%C2%A0AIoT%E8%93%9D%E7%89%99%E6%8E%A5%E5%8F%A3%E8%A7%84%E8%8C%83_%E5%BA%94%E7%94%A8%E5%B1%82v1.0.1-4bae0447-b2db-4bb8-ac35-31f506c622af.png" alt=""><strong>说明：</strong><ul>
<li>远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.2.3 查询支持消息通知类型 (CID=0x03)

**字段描述：**

| **列表名称**                                                                                                                                                                                      | **结构体名称**                          | **字段名称**                         | **Type(bit0\~bit6)** | **Length** **Size(Oct)** | **Value** **数据类型** | **说明**                                                              | **必选/可选(M/O)** | **版本** |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- | -------------------------------- | -------------------- | ------------------------ | ------------------ | ------------------------------------------------------------------- | -------------- | ------ |
| supported\_notify\_message\_list                                                                                                                                                              |                                    |                                  | 0x01                 | NA                       | NA                 | 消息开关设置数据列表                                                          |                | 1.0.0  |
|                                                                                                                                                                                               | supported\_notify\_message\_struct |                                  | 0x02                 | 1                        | NA                 | 消息开关设置结构体，子节点包含                                                     | M              | 1.0.0  |
|                                                                                                                                                                                               |                                    | supported\_notify\_message\_type | 0x03                 | 2                        | uint16             | 2个字节，第1个字节代表消息类型，&#xA;0x01：通用状态消息&#xA;0x10：pump设备类&#xA;0xFF：通用错误码消息 | M              | 1.0.0  |
|                                                                                                                                                                                               |                                    | message\_notify\_enable<br />    | 0x04                 | variable                 | uint8              | 设置message\_type对应消息开关状态，&#xA;0x00：disable&#xA;0x01：enable           | M              | 1.0.0  |
| **发送帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-3189e320-3da3-4288-acd3-788da9299871.png)**接收帧结构：**![](images/Momcozy AIoT蓝牙接口规范_应用层v1.0.1-7019758f-59df-46c4-9f9f-577168b65782.png)**说明：** |                                    |                                  |                      |                          |                    |                                                                     |                |        |

## 5.3 Fitness Service

## 5.4 FCT\&ATE Service

**Follow**[《Momcozy 智能制造专项-产测服务协议规范v1.0.0》](https://alidocs.dingtalk.com/i/nodes/ZgpG2NdyVXayeeqDhGzxAr3EJMwvDqPk?utm_scene=team_space)

***

## 5.5 OTA Service TBA

## 5.7 Data Service

**Follow**[《Momcozy 智能泌乳专项-数据埋点服务协议规范v1.0.2》](https://alidocs.dingtalk.com/i/nodes/oP0MALyR8kbpvvkLu2e95b1aJ3bzYmDO?utm_scene=person_space)

***

## 5.10 Pump Service

### 5.10.1 设置设备通用功能 (CID=0x01)

**字段描述：**

<table><colgroup><col width="167"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value 数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>common_setting_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>通用设置数据列表</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>common_setting_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>通用设置结构体，包含:
pump_side
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">travel_lock_enable</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">wear_detect_enable</span></td>
<td>M</td>
<td>1.0.2</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td>作用对象标识符<span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>
0x00：Left 
0x01：Right
0x02：Sync<br />0x03：independence</td>
<td>O<br /></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">travel_lock_enable</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x04</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">使能和关闭旅行锁</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00: disable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable</span></td>
<td rowspan="2"><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O（至少携带一个）</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">wear_detect_enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x05</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">使能和关闭旅佩戴检测</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00: disable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td colspan="9"><strong>发送帧结构：</strong><pre><code class="language-plaintext">[SID] [CID]
  [common_setting_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [common_setting_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]              [Len=1] [0x00]   ← Left（可选）
      [function_type_enable(0x04)]   [Len=1] [0x01]   ← travel_lock 开
</code></pre><strong>接收帧结构：</strong><br /><strong>说明：</strong></td>
</tr>
</tbody>
</table>

***

### 5.10.2 获取设备通用状态 (CID=0x02)

查询设备当前通用运行状态。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="167"><col width="143"><col width="143"><col width="143"><col width="449"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value 数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td>作用对象标识符
0x00：Left 
0x01：Right
0x02：Sync<br />0x03：independence</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">请求帧为空时返回全部 pump_side 的通用状态。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**

<table><colgroup><col width="167"><col width="167"><col width="212"><col width="143"><col width="143"><col width="143"><col width="449"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value 数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">common_status_list</span></td>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">通用状态列表，包含：</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">common_status_struct1,</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">common_status_struct2,</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">...</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">common_status_struct</span></td>
<td><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">通用状态结构体（容器），包含 </span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_side</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">connection_status</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">travel_lock_status</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)"> flange_type</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_side</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">3</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">connection_status</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">4</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00=single，0x01=peers，0x02=multi</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">travel_lock_status</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">5</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">旅行锁使能状态</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00: disable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">flange_type</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x06</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">法兰类型</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">（未定义）</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span></td>
</tr>
<tr>
<td colspan="9"><strong>接收帧结构：</strong><br />返回成功（Left 侧示例）：<pre><code class="language-plaintext">[SID] [CID]
  [common_status_list(0x81 = 0x01 | 0x80)] [Len=0xNA]           ← 容器, 子节点总长
    [common_status_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]           ← 容器, 子节点总长
      [pump_side(0x03)]           [Len=1] [0x00]      ← Left
      [connection_status(0x04)]   [Len=1] [0x00]      ← single
      [travel_lock_status(0x05)]  [Len=1] [0x00]      ← disabled
      [flange_type(0x06)]         [Len=1] [0x01]
</code></pre><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.3 设置设备吸乳工作参数 (CID=0x03)

**字段描述：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="106"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value 数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_setting_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>泵阀设置参数列表</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>pump_setting_struct<br /></td>
<td></td>
<td>0x02<br /></td>
<td>NA</td>
<td>NA</td>
<td>泵阀设置参数结构体，包含：
pump_side
pump_enable
pump_mode
pump_level
pump_duration_time</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_enable</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x04</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">使能 暂停 关闭泵阀</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable，0x02: pause，0x03：stop</span></td>
<td rowspan="6"><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O（至少携带一个，pump_mode跟pump_type绑定，得一块带/不带）</span><br /><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span><br /></td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_mode</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>模式/韵律ID（0x01~0x07）<br />模式：0x01 按摩/刺激 0x02 吸乳 0x03 混合 0x04 自定义 0x05 PP追奶 0x06 2+8奶量提升 0x07 开奶<br /><br />韵律：1 常规韵律/Cozy Flow · 2 2+8奶量提升 · 3 PP追奶 · 4 开奶 · 5 追加排空 · 6 放松 · 7 自定义</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_level</td>
<td>0x06</td>
<td>1</td>
<td>uint8</td>
<td>强度挡位（1档~15档）</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_freq</span></td>
<td>0x07</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">速度：（0：slow 1：mid 2：high）</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><del>pump_duration_time</del></td>
<td><del>0x08</del></td>
<td><del>2</del><br /></td>
<td><del>uint16</del></td>
<td><del>工作时长 >0s（产品确认，韵律/模式工作时长固定，app不能够下发来改变，故删除）</del></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></del></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_type</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x09</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：模式</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：韵律</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span><br /></td>
</tr>
<tr>
<td colspan="9"><strong>发送帧结构：</strong><pre><code class="language-plaintext">[SID] [CID]
  [pump_setting_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [pump_setting_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]            [Len=1] [0x00]     ← Left（可选）
      [pump_enable(0x04)]          [Len=1] [0x00]     ← enable
      [pump_mode(0x05)]            [Len=1] [0x01]     ← 模式/韵律 ID
      [pump_level(0x06)]           [Len=1] [0x05]     ← 5 档
      [pump_freq(0x07)]            [Len=1] [0x01]     ← mid
      [pump_type(0x09)]            [Len=1] [0x00]     ← 模式
</code></pre><strong>接收帧结构：</strong><br /><strong>说明：</strong></td>
</tr>
</tbody>
</table>

***

### 5.10.4 获取设备吸乳工作状态 (CID=0x04)

查询设备当前吸乳运行状态。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="212"><col width="143"><col width="143"><col width="143"><col width="449"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">请求帧为空时返回全部 pump_side 的吸乳状态。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value 数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_status_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>泵阀状态数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>pump_status_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>泵阀状态结构体（容器），包含 pump_side + pump_status + pump_mode + pump_level + pump_freq + pump_duration_time + <span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_type + pump_elapsed_time</span></td>
<td>M<br /></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_status</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0=idle 0x01=pumping，0x02=paused，0x03=stopped</span></td>
<td rowspan="7"><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O（至少携带一个）</span><br /><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_mode</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>0x01~0x07<br />0x01 按摩/刺激 0x02 吸乳 0x03 混合 0x04 自定义<br />0x05 PP追奶 0x06 2+8奶量提升 0x07 开奶</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_level</td>
<td>0x06</td>
<td>1</td>
<td>uint8</td>
<td>0x01~0x0F（1档~15档）</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_freq</td>
<td>0x07</td>
<td>1</td>
<td>uint8</td>
<td>当前速度 0 （0：slow 1：mid 2：high）</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_duration_time</td>
<td>0x08</td>
<td>2</td>
<td>uint16</td>
<td>对应模式下需要的工作时长（秒）<br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_type</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x09</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：模式</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：韵律</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span><br /></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_elapsed_time</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x0A</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">2</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint16</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">已运行时长</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span><br /></td>
</tr>
<tr>
<td colspan="9"><strong>接收帧结构：</strong><br />返回成功（Left 侧示例）：<pre><code class="language-plaintext">[SID] [CID]
  [pump_status_list(0x81 = 0x01 | 0x80)]   [Len=0xNA]           ← 容器, 子节点总长
    [pump_status_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]         ← 容器, 子节点总长
      [pump_side(0x03)]         [Len=1] [0x00]    ← Left
      [pump_status(0x04)]       [Len=1] [0x00]    ← pumping
      [pump_mode(0x05)]         [Len=1] [0x01]    ← 按摩
      [pump_level(0x06)]        [Len=1] [0x05]    ← 5档
      [pump_freq(0x07)]         [Len=1] [0x01]
      [pump_duration_time(0x08)] [Len=2] [0x0258]
</code></pre><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.5 设置设备振动参数 (CID=0x05，暂时挂起，当前不支持app设置震动参数)

**字段描述：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length Size(Oct)</strong></th>
<th><strong>Value 数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>vibration_setting_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>振动参数数据列表</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>vibration_setting_struct</td>
<td></td>
<td>0x02<br /></td>
<td>NA</td>
<td>NA</td>
<td>振动参数结构体，包含
pump_side
vibration_enable
vibration_mode
vibration_freq
vibration_duration_time</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03<br /></td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td>vibration_enable</td>
<td>0x04<br /></td>
<td>1</td>
<td>uint8</td>
<td>使能和关闭振动
0x00: disable
0x01: enable</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>vibration_mode</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>振动模式</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>vibration_freq</td>
<td>0x06</td>
<td>2</td>
<td>uint16</td>
<td>振动频率 Hz</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>vibration_duration_time</td>
<td>0x07</td>
<td>2</td>
<td>uint16</td>
<td>振动持续时长>0 s</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>发送帧结构：</strong><pre><code class="language-plaintext">[SID] [CID]
  [vibration_setting_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [vibration_setting_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]                 [Len=1] [0x00]     ← Left（可选）
      [vibration_enable(0x04)]          [Len=1] [0x01]     ← enable
      [vibration_mode(0x05)]            [Len=1] [0x01]
      [vibration_freq(0x06)]            [Len=2] [0x0032]   ← 50Hz
      [vibration_duration_time(0x07)]   [Len=2] [0x012C]   ← 300s
</code></pre><strong>接收帧结构：</strong><br /><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.6 获取设备振动状态 (CID=0x06，同上)

查询设备当前振动运行状态。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="212"><col width="143"><col width="143"><col width="143"><col width="449"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">对象标识符</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">请求帧为空时返回全部 pump_side 的振动状态。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>vibration_status_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>振动状态数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>vibration_status_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>振动状态结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">对象标识符</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">vibration_status</span></del></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x04</span></del></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></del></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></del></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00=vibrating，0x01=paused</span></del></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span></del></td>
<td><del><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.0</span></del></td>
</tr>
<tr>
<td></td>
<td></td>
<td>vibration_mode</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>振动工作模式</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>vibration_freq</td>
<td>0x06</td>
<td>2</td>
<td>uint16</td>
<td>振动工作频率</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>vibration_duration_time</td>
<td>0x07</td>
<td>2</td>
<td>uint16</td>
<td>振动工作持续时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>接收帧结构：</strong><br />返回成功（Left 侧示例）：<pre><code class="language-plaintext">[SID] [CID]
  [vibration_status_list(0x81 = 0x01 | 0x80)]   [Len=0xNA]              ← 容器, 子节点总长
    [vibration_status_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]            ← 容器, 子节点总长
      [pump_side(0x03)]             [Len=1] [0x00]        ← Left
      [vibration_status(0x04)]      [Len=1] [0x00]        ← vibrating
      [vibration_mode(0x05)]        [Len=1] [0x01]
      [vibration_freq(0x06)]        [Len=2] [0x0032]
      [vibration_duration_time(0x07)] [Len=2] [0x012C]
</code></pre><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.7 设置设备灯参数 (CID=0x07)

**字段描述：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>light_setting_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>灯参数数据列表</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>light_setting_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>灯参数结构体，包含
pump_side
light_enable
light_adapt_enable
light_brightness_level
light_duration_time</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">对象标识符</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_type</td>
<td>0x04</td>
<td>1</td>
<td>uint8<br /></td>
<td>灯类型
0x00：照明灯
0x01：
0x02：</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_enable</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>使能和关闭灯
0x00: disable
0x01: enable</td>
<td rowspan="4"><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O（至少携带一个）</span></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_adapt_enable</td>
<td>0x06</td>
<td>1</td>
<td>uint8</td>
<td>使能和关闭灯自适应
0x00: disable
0x01: enable</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_brightness_level</td>
<td>0x07</td>
<td>1</td>
<td>uint8</td>
<td>灯亮度挡位0～100</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_duration_time</td>
<td>0x08</td>
<td>2</td>
<td>uint16</td>
<td>灯工作持续时长>0s</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>发送帧结构：</strong><pre><code class="language-plaintext">[SID] [CID]
  [light_setting_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [light_setting_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]                 [Len=1] [0x00]     ← Left（可选）
      [light_type(0x04)]                [Len=1] [0x00]     ← 照明灯（必带）
      [light_enable(0x05)]              [Len=1] [0x01]     ← enable
      [light_adapt_enable(0x06)]        [Len=1] [0x00]     ← 自适应关
      [light_brightness_level(0x07)]    [Len=1] [0x50]     ← 亮度
      [light_duration_time(0x08)]       [Len=2] [0x003C]   ← 60s
</code></pre><strong>接收帧结构：</strong><br /><strong>说明：</strong></td>
</tr>
</tbody>
</table>

***

### 5.10.8 获取设备灯状态 (CID=0x08)

查询设备当前灯运行状态。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="212"><col width="143"><col width="143"><col width="143"><col width="449"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">对象标识符</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>light_type</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>过滤：0x00=照明灯。不携带则返回全部</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">请求帧为空时返回全部 pump_side 及全部 light_type 的灯状态。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧照明灯：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00] [light_type(0x04)] [Len=1] [0x00]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>light_status_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>灯状态数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>light_status_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>灯状态结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">对象标识符</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_type</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>0x00=照明灯</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_status</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>0x00=on，0x01=off</td>
<td rowspan="4"><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O（至少携带一个）</span></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_adapt_status</td>
<td>0x06</td>
<td>1</td>
<td>uint8</td>
<td>灯自适应使能状态<br />0x00: disable
0x01: enable</td>
<td>1.0.0<br /></td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_brightness_level</td>
<td>0x07</td>
<td>1</td>
<td>uint8</td>
<td>灯亮度等级0-100</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>light_duration_time</td>
<td>0x08</td>
<td>2</td>
<td>uint16</td>
<td>灯工作持续时间>0s</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>接收帧结构：</strong><br />返回成功（Left 侧照明灯示例）：<pre><code class="language-plaintext">[SID] [CID]
  [light_status_list(0x81 = 0x01 | 0x80)]   [Len=0xNA]                 ← 容器, 子节点总长
    [light_status_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]               ← 容器
      [pump_side(0x03)]         [Len=1] [0x00]           ← Left
      [light_type(0x04)]        [Len=1] [0x00]           ← 照明灯
      [light_status(0x05)]      [Len=1] [0x00]           ← on
      [light_adapt_status(0x06)] [Len=1] [0x01]
      [light_brightness_level(0x07)] [Len=1] [0x03]
      [light_duration_time(0x08)] [Len=2] [0x012C]
</code></pre><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.9 设置设备加热参数 (CID=0x09)

**字段描述：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>heat_setting_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>加热参数数据列表</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>heat_setting_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>加热参数结构体，包含
pump_side
heat_enable
heat_level
heat_duration_time</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">对象标识符</span> <span style="color: inherit; background-color: rgba(255,246,122,0.8)">（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>heat_enable</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>使能和关闭加热
0x00: disable
0x01: enable</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>heat_level</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>加热挡位(未定义)</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>heat_duration_time</td>
<td>0x06</td>
<td>2</td>
<td>uint16</td>
<td>加热工作持续时长>0s</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>发送帧结构：</strong><pre><code class="language-plaintext">[SID] [CID]
  [heat_setting_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [heat_setting_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]              [Len=1] [0x00]     ← Left（可选）
      [heat_enable(0x04)]            [Len=1] [0x01]     ← enable
      [heat_level(0x05)]             [Len=1] [0x02]     ← 档位
      [heat_duration_time(0x06)]     [Len=2] [0x012C]   ← 300s
</code></pre><strong>接收帧结构：</strong><br /><strong>说明：</strong></td>
</tr>
</tbody>
</table>

***

### 5.10.10 获取设备加热状态 (CID=0x0A)

查询设备当前加热运行状态。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="212"><col width="143"><col width="143"><col width="143"><col width="449"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side<br /></td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">请求帧为空时返回全部 pump_side 的加热状态。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>heat_status_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>加热状态数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>heat_status_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>加热状态结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>heat_status</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>0x00=on，0x01=off</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>heat_level</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>加热挡位(未定义)</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>heat_duration_time</td>
<td>0x06</td>
<td>2</td>
<td>uint16</td>
<td>加热工作持续时间>0s</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>接收帧结构：</strong><br />返回成功（Left 侧示例）：<pre><code class="language-plaintext">[SID] [CID]
  [heat_status_list(0x81 = 0x01 | 0x80)]   [Len=0xNA]               ← 容器, 子节点总长
    [heat_status_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]              ← 容器
      [pump_side(0x03)]        [Len=1] [0x00]          ← Left
      [heat_status(0x04)]      [Len=1] [0x00]          ← on
      [heat_level(0x05)]       [Len=1] [0x03]          ← 3档
      [heat_duration_time(0x06)] [Len=2] [0x0258]
</code></pre><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.11 设置设备状态感知数据 (CID=0x0B)

**字段描述：**

<table><colgroup><col width="151"><col width="212"><col width="193"><col width="193"><col width="143"><col width="143"><col width="247"><col width="449"><col width="129"></colgroup>
<thead>
<tr>
<th>列表名称</th>
<th>结构体名称</th>
<th>字段名称</th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">perception_list</span></td>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">感知数据列表</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span></td>
<td></td>
</tr>
<tr>
<td><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">letdownmilk_config_struct</span><br /></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">奶阵奶量参数结构体，包含</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_side</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">milk_volume notify enable</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">let-down notify enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">M</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_side</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">milk_volume notify enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x04</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">使能和关闭奶量通知</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00: disable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable</span></td>
<td rowspan="2"><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O（至少携带一个）</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">let-down notify enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x05</span></td>
<td>1</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">使能和关闭奶阵通知</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00: disable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td colspan="9"><strong>发送帧结构：</strong><pre><code class="language-plaintext">[SID] [CID]
  [perception_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [letdownmilk_config_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]       [Len=1] [0x00]     ← Left（可选）
      [let-down notify enable(0x05)]   [Len=1] [0x01]          

</code></pre><strong>接收帧结构：</strong></td>
</tr>
</tbody>
</table>



***

### 5.10.12 获取设备感知状态信息 (CID=0x0C)

查询设备当前感知功能运行状态。请求可携带过滤条件缩小范围。

**接收帧字段：**

<table><colgroup><col width="133"><col width="193"><col width="119"><col width="143"><col width="143"><col width="143"><col width="450"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
<th><strong>必选/可选(M/O)</strong></th>
<th><strong>版本</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">perception_list</span></td>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">感知数据列表</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">letdownmilk_config_struct</span><br /></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">NA</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">奶阵奶量参数结构体，包含</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_side</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">notify_enable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">milk_volume</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">let-down</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span><br /></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">pump_side</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">milk_volume notify enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x04</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">使能和关闭奶量通知</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00: disable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable</span></td>
<td><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">let-down notify enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x05</span></td>
<td>1</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">使能和关闭奶阵通知</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00: disable</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01: enable</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">milk_volume</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x06</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">当前奶量（ml）</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td></td>
<td></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">let-down</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x07</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">uint8</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">奶阵 0x00=idle，0x01=start，0x02=active，0x03=end</span><br /></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">1.0.2</span></td>
</tr>
<tr>
<td colspan="8"><strong>接收帧结构：</strong><br />返回成功（示例）：<pre><code class="language-plaintext">[SID] [CID]
  [perception_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [letdownmilk_config_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]       [Len=1] [0x00]     ← Left
      [notify_enable(0x04)]   [Len=1] [0x03]
      [milk_volume(0x06)]     [Len=1] [0x1E]     ← 30ml
      [let-down(0x07)]        [Len=1] [0x00]     ← idle               

</code></pre><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
<td></td>
</tr>
</tbody>
</table>



***

### 5.10.13 设置设备模式单元参数 (CID=0x0D)

**字段描述：**

<table><colgroup><col width="140"><col width="160"><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>子列表名称</strong></th>
<th><strong>子结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>mode_unit_list</td>
<td></td>
<td></td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>模式单元数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x02</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>mode_unit_struct</td>
<td></td>
<td></td>
<td></td>
<td>0x03</td>
<td>NA</td>
<td>NA</td>
<td>模式单元结构体（容器），包含 mode_unit_id + driver_type + mode_unit_data_list</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>模式单元标识：0x00=stimulation，0x01=expression，0x02=Mix</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>driver_type</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>驱动类型：0x00=pump（泵阀），0x01=stepper（步进电机）。不同驱动类型的 mode_unit_data_struct 字段定义不同</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>mode_unit_data_list</td>
<td></td>
<td></td>
<td>0x06</td>
<td>NA</td>
<td>NA</td>
<td>模式单元数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td>mode_unit_data_struct</td>
<td></td>
<td>0x07</td>
<td>NA</td>
<td>NA</td>
<td>模式单元数据结构体（容器），包含以下 6 个 leaf</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_level</td>
<td>0x08</td>
<td>1</td>
<td>uint8</td>
<td>吸乳挡位</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_duration_time</td>
<td>0x09</td>
<td>2</td>
<td>uint16</td>
<td>泵工作时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_delay_time</td>
<td>0x0A</td>
<td>2</td>
<td>uint16</td>
<td>泵停止后延迟时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_pwm</td>
<td>0x0B</td>
<td>1</td>
<td>uint8</td>
<td>泵驱动PWM</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>value_duration_time</td>
<td>0x0C</td>
<td>2</td>
<td>uint16</td>
<td>阀工作时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>value_delay_time</td>
<td>0x0D</td>
<td>2</td>
<td>uint16</td>
<td>阀停止后延迟时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="11"><pre><code class="language-plaintext">[SID] [CID]
  [mode_unit_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [side(0x01)]                 [Len=1] [0x00]     ← Left（可选，list 级）
    [mode_unit_struct(0x83 = 0x03 | 0x80)] [Len=0xNA]
      [mode_unit_id(0x04)]            [Len=1] [0x00]     ← stimulation
      [driver_type(0x05)]             [Len=1] [0x00]     ← pump
      [mode_unit_data_list(0x86 = 0x06 | 0x80)] [Len=0xNA]
        [mode_unit_data_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [pump_level(0x08)]          [Len=1] [0x01]
          [pump_duration_time(0x09)]  [Len=2] [0x03E8]
          [pump_delay_time(0x0A)]     [Len=2] [0x01F4]
          [pump_pwm(0x0B)]            [Len=1] [0x50]
          [value_duration_time(0x0C)] [Len=2] [0x03E8]
          [value_delay_time(0x0D)]    [Len=2] [0x01F4]
</code></pre><strong>发送帧结构：</strong><br /><strong>接收帧结构：</strong><br /><strong>说明：</strong></td>
</tr>
</tbody>
</table>

***

### 5.10.14 获取设备模式单元参数 (CID=0x0E)

查询已存储的模式单元配置。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="210"><col width="145"><col width="145"><col width="145"><col width="445"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th>字段名称</th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x02</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>mode_unit_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>过滤：模式单元标识。不携带则返回全部</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">过滤条件之间为 AND 关系。请求帧为空时返回全部 pump_side 及全部 mode_unit_id 的模式单元数据。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧 stimulation：<code>[SID] [CID] [side(0x01)] [Len=1] [0x00] [mode_unit_id(0x04)] [Len=1] [0x00]</code></li>
<li>查 Left 侧全部模式：<code>[SID] [CID] [side(0x01)] [Len=1] [0x00]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**（与"设置设备模式单元参数"发送帧一致，按过滤条件返回匹配的子集）

<table><colgroup><col width="140"><col width="160"><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>子列表名称</strong></th>
<th><strong>子结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>mode_unit_list</td>
<td></td>
<td></td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>模式单元数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x02</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>mode_unit_struct</td>
<td></td>
<td></td>
<td></td>
<td>0x03</td>
<td>NA</td>
<td>NA</td>
<td>模式单元结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>0x00=stimulation，0x01=expression，0x02=Mix</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>driver_type</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>驱动类型：0x00=pump（泵阀）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>mode_unit_data_list</td>
<td></td>
<td></td>
<td>0x06</td>
<td>NA</td>
<td>NA</td>
<td>模式单元数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td>mode_unit_data_struct</td>
<td></td>
<td>0x07</td>
<td>NA</td>
<td>NA</td>
<td>模式单元数据结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_level</td>
<td>0x08</td>
<td>1</td>
<td>uint8</td>
<td>（driver_type=0x00）吸乳挡位</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_duration_time</td>
<td>0x09</td>
<td>2</td>
<td>uint16</td>
<td>（driver_type=0x00）泵工作时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_delay_time</td>
<td>0x0A</td>
<td>2</td>
<td>uint16</td>
<td>（driver_type=0x00）泵停止后延迟时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_pwm</td>
<td>0x0B</td>
<td>1</td>
<td>uint8</td>
<td>（driver_type=0x00）泵驱动PWM</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>value_duration_time</td>
<td>0x0C</td>
<td>2</td>
<td>uint16</td>
<td>（driver_type=0x00）阀工作时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>value_delay_time</td>
<td>0x0D</td>
<td>2<br /></td>
<td>uint16</td>
<td>（driver_type=0x00）阀停止后延迟时间</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="11"><strong>接收帧结构：</strong><br />返回成功（请求 <code>[pump_side=Left, mode_unit_id=stimulation]</code> 的响应）：<pre><code class="language-plaintext">[SID] [CID]
  [mode_unit_list(0x81 = 0x01 | 0x80)]   [Len=0xNA]                    ← 容器, 子节点总长 
    [side(0x01)]       [Len=1] [0x00]               ← Left
    [mode_unit_struct(0x83 = 0x03 | 0x80)] [Len=0xNA]                  ← 容器
      [mode_unit_id(0x04)] [Len=1] [0x00]              ← stimulation
      [driver_type(0x05)]  [Len=1] [0x00]              ← pump
      [mode_unit_data_list(0x86 = 0x06 | 0x80)] [Len=0xNA]                 ← 容器
        [mode_unit_data_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]             ← 容器
          [pump_level(0x08)]          [Len=1] [0x01]
          [pump_duration_time(0x09)]  [Len=2] [0x03E8]
          [pump_delay_time(0x0A)]     [Len=2] [0x01F4]
          [pump_pwm(0x0B)]            [Len=1] [0x50]
          [value_duration_time(0x0C)] [Len=2] [0x03E8]
          [value_delay_time(0x0D)]    [Len=2] [0x01F4]
        [mode_unit_data_struct(0x87 = 0x07 | 0x80)] [Len=0x10] ...   // level=2
        [mode_unit_data_struct(0x87 = 0x07 | 0x80)] [Len=0x10] ...   // level=3
</code></pre><strong>说明：</strong><ul>
<li>远端主动查询获取，或设置后本地回显。</li>
<li>请求帧不带过滤条件时，设备返回已存储的全部模式单元数据。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.15 设置设备模式参数 (CID=0x0F)

**字段描述：**

<table><colgroup><col width="140"><col width="160"><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>子列表名称</strong></th>
<th><strong>子结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>mode_list</td>
<td></td>
<td></td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>模式配置数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>mode_struct</td>
<td></td>
<td></td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>模式结构体（容器），包含 pump_side + mode_id + mode_name + mode_unit_ref_list</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O<br /></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>模式编号</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_name</td>
<td>0x05</td>
<td>variable</td>
<td>uint8</td>
<td>模式名称，UTF-8 编码</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>mode_unit_ref_list</td>
<td></td>
<td></td>
<td>0x06</td>
<td>NA</td>
<td>NA</td>
<td>模式单元引用列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td>mode_unit_ref_struct</td>
<td></td>
<td>0x07</td>
<td>NA</td>
<td>NA</td>
<td>模式单元引用结构体（容器），包含 mode_unit_seq + mode_unit_id + mode_unit_count</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_seq</td>
<td>0x08</td>
<td>1</td>
<td>uint8</td>
<td>模式单元序号（1 开始递增）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_id</td>
<td>0x09</td>
<td>1</td>
<td>uint8</td>
<td>引用的模式单元 ID</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_count</td>
<td>0x0A</td>
<td>2</td>
<td>uint16</td>
<td>模式单元执行次数</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="11"><strong>发送帧结构：</strong><pre><code class="language-plaintext">[SID] [CID]
  [mode_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [mode_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]           [Len=1] [0x00]       ← Left（可选）
      [mode_id(0x04)]             [Len=1] [0x03]       ← 模式编号
      [mode_name(0x05)]           [Len=N] [UTF-8…]     ← 可选
      [mode_unit_ref_list(0x86 = 0x06 | 0x80)] [Len=0xNA]
        [mode_unit_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_unit_seq(0x08)]   [Len=1] [0x01]
          [mode_unit_id(0x09)]    [Len=1] [0x02]
          [mode_unit_count(0x0A)] [Len=2] [0x0002]     ← 执行 2 次
        [mode_unit_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_unit_seq(0x08)]   [Len=1] [0x02]
          [mode_unit_id(0x09)]    [Len=1] [0x03]
          [mode_unit_count(0x0A)] [Len=2] [0x0003]
        [mode_unit_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_unit_seq(0x08)]   [Len=1] [0x03]
          [mode_unit_id(0x09)]    [Len=1] [0x01]
          [mode_unit_count(0x0A)] [Len=2] [0x0001]
</code></pre><strong>接收帧结构：</strong><br /><strong>说明：</strong></td>
</tr>
</tbody>
</table>

***

### 5.10.16 获取设备模式参数 (CID=0x10)

查询已存储的模式配置。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="210"><col width="145"><col width="145"><col width="145"><col width="445"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th>字段名称</th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>mode_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>过滤：模式编号。不携带则返回全部</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">过滤条件之间为 AND 关系。请求帧为空时返回全部 pump_side 及全部 mode_id 的模式数据。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧指定模式：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00] [mode_id(0x04)] [Len=1] [0x03]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**（与"设置设备模式参数"发送帧一致，按过滤条件返回匹配的子集）

<table><colgroup><col width="140"><col width="160"><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>子列表名称</strong></th>
<th><strong>子结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>mode_list</td>
<td></td>
<td></td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>模式配置数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>mode_struct</td>
<td></td>
<td></td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>模式结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>模式编号</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_name</td>
<td>0x05</td>
<td>variable</td>
<td>uint8</td>
<td>模式名称，UTF-8 编码</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>mode_unit_ref_list</td>
<td></td>
<td></td>
<td>0x06</td>
<td>NA</td>
<td>NA</td>
<td>模式单元引用列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td>mode_unit_ref_struct</td>
<td></td>
<td>0x07</td>
<td>NA</td>
<td>NA</td>
<td>模式单元引用结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_seq</td>
<td>0x08</td>
<td>1</td>
<td>uint8</td>
<td>模式单元序号（1 开始递增）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_id</td>
<td>0x09</td>
<td>1</td>
<td>uint8</td>
<td>引用的模式单元 ID</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_unit_count</td>
<td>0x0A</td>
<td>2</td>
<td>uint16</td>
<td>模式单元执行次数</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="11"><strong>接收帧结构：</strong><br />返回成功（Left 侧、mode_id=3 示例）：<pre><code class="language-plaintext">[SID] [CID]
  [mode_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [mode_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]      [Len=1] [0x00]
      [mode_id(0x04)]        [Len=1] [0x03]
      [mode_name(0x05)]      [Len=7] [0xE6 0x8C 0x89 0xE6 0x91 0xA9 0xE6]   ← "按摩模式"
      [mode_unit_ref_list(0x86 = 0x06 | 0x80)] [Len=0xNA]
        [mode_unit_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_unit_seq(0x08)]   [Len=1] [0x01]
          [mode_unit_id(0x09)]    [Len=1] [0x02]     ← 模式单元 A
          [mode_unit_count(0x0A)] [Len=2] [0x02]     ← 执行 2 次
        [mode_unit_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_unit_seq(0x08)]   [Len=1] [0x02]
          [mode_unit_id(0x09)]    [Len=1] [0x03]     ← 模式单元 B
          [mode_unit_count(0x0A)] [Len=2] [0x03]     ← 执行 3 次
        [mode_unit_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_unit_seq(0x08)]   [Len=1] [0x03]
          [mode_unit_id(0x09)]    [Len=1] [0x01]     ← 模式单元 C
          [mode_unit_count(0x0A)] [Len=2] [0x01]     ← 执行 1 次
</code></pre><strong>说明：</strong><ul>
<li>远端主动查询获取，或设置后本地回显。</li>
<li>请求帧不带过滤条件时，设备返回已存储的全部模式数据。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.17 设置设备韵律参数 (CID=0x11)

**字段描述：**

<table><colgroup><col width="140"><col width="160"><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>子列表名称</strong></th>
<th><strong>子结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>rhythm_list</td>
<td></td>
<td></td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>韵律配置数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>rhythm_struct</td>
<td></td>
<td></td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>韵律结构体（容器），包含 pump_side + rhythm_id + rhythm_name + mode_ref_list</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>rhythm_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>韵律编号</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>rhythm_name</td>
<td>0x05</td>
<td>variable</td>
<td>uint8</td>
<td>韵律名称，UTF-8 编码</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>mode_ref_list</td>
<td></td>
<td></td>
<td>0x06</td>
<td>NA</td>
<td>NA</td>
<td>模式引用列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td>mode_ref_struct</td>
<td></td>
<td>0x07</td>
<td>NA</td>
<td>NA</td>
<td>模式引用结构体（容器），包含 mode_seq + mode_id + mode_time</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_seq</td>
<td>0x08</td>
<td>1</td>
<td>uint8</td>
<td>模式序号（1 开始递增）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_id</td>
<td>0x09</td>
<td>1</td>
<td>uint8</td>
<td>引用的模式 ID</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_time</td>
<td>0x0A</td>
<td>2</td>
<td>uint16</td>
<td>模式执行时长（秒）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="11"><strong>发送帧结构：</strong><br />设置 Left 侧 rhythm_id=1，依次执行 mode 3（2 秒）、mode 1（3 秒）：<pre><code class="language-plaintext">[SID] [CID]
  [rhythm_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [rhythm_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]   [Len=1] [0x00]
      [rhythm_id(0x04)]   [Len=1] [0x01]
      [rhythm_name(0x05)] [Len=9] [0x43 0x6F 0x7A 0x79 0x20 0x46 0x6C 0x6F 0x77]   ← "Cozy Flow"
      [mode_ref_list(0x86 = 0x06 | 0x80)] [Len=0xNA]
        [mode_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_seq(0x08)]  [Len=1] [0x01]
          [mode_id(0x09)]   [Len=1] [0x03]
          [mode_time(0x0A)] [Len=2] [0x0002]
        [mode_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_seq(0x08)]  [Len=1] [0x02]
          [mode_id(0x09)]   [Len=1] [0x01]
          [mode_time(0x0A)] [Len=2] [0x0003]
</code></pre><strong>接收帧结构：</strong> <br /><strong>说明：</strong><ul>
<li>韵律由一个或多个模式按序组成。每个 entry 指定引用哪个模式（mode_id）以及执行时长（mode_time，单位秒）。</li>
<li>韵律的结构与模式一致：模式是"模式单元的序列"，韵律是"模式的序列"。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.18 获取设备韵律参数 (CID=0x12)

查询已存储的韵律配置。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="210"><col width="145"><col width="145"><col width="145"><col width="445"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th>字段名称</th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>rhythm_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>过滤：韵律编号。不携带则返回全部</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">请求帧为空时返回全部 pump_side 及全部 rhythm_id 的韵律配置。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧指定韵律：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00] [rhythm_id(0x04)] [Len=1] [0x03]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**（与"设置设备韵律参数"发送帧一致，按过滤条件返回匹配的子集）

<table><colgroup><col width="140"><col width="160"><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>子列表名称</strong></th>
<th><strong>子结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>rhythm_list</td>
<td></td>
<td></td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>韵律配置数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>rhythm_struct</td>
<td></td>
<td></td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>韵律结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">O</span></td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>rhythm_id</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>韵律编号</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>rhythm_name</td>
<td>0x05</td>
<td>variable</td>
<td>uint8</td>
<td>韵律名称，UTF-8 编码</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>mode_ref_list</td>
<td></td>
<td></td>
<td>0x06</td>
<td>NA</td>
<td>NA</td>
<td>模式引用列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td>mode_ref_struct</td>
<td></td>
<td>0x07</td>
<td>NA</td>
<td>NA</td>
<td>模式引用结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_seq</td>
<td>0x08</td>
<td>1</td>
<td>uint8</td>
<td>模式序号（1 开始递增）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_id</td>
<td>0x09</td>
<td>1</td>
<td>uint8</td>
<td>引用的模式 ID</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td></td>
<td>mode_time</td>
<td>0x0A</td>
<td>2</td>
<td>uint16</td>
<td>模式执行时长（秒）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="11"><strong>接收帧结构：</strong><br />返回成功（Left 侧、rhythm_id=1 示例）：<pre><code class="language-plaintext">[SID] [CID]
  [rhythm_list(0x81 = 0x01 | 0x80)] [Len=0xNA]
    [rhythm_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]
      [pump_side(0x03)]   [Len=1] [0x00]
      [rhythm_id(0x04)]   [Len=1] [0x01]
      [rhythm_name(0x05)] [Len=9] [0x43 0x6F 0x7A 0x79 0x20 0x46 0x6C 0x6F 0x77]   ← "Cozy Flow"
      [mode_ref_list(0x86 = 0x06 | 0x80)] [Len=0xNA]rhythm_name
        [mode_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_seq(0x08)]  [Len=1] [0x01]
          [mode_id(0x09)]   [Len=1] [0x03]
          [mode_time(0x0A)] [Len=2] [0x0002]
        [mode_ref_struct(0x87 = 0x07 | 0x80)] [Len=0xNA]
          [mode_seq(0x08)]  [Len=1] [0x02]
          [mode_id(0x09)]   [Len=1] [0x01]
          [mode_time(0x0A)] [Len=2] [0x0003]
</code></pre><strong>说明：</strong><ul>
<li>远端主动查询获取，或设置后本地回显。</li>
<li>请求帧不带过滤条件时，设备返回已存储的全部韵律数据。</li>
</ul></td>
</tr>
</tbody>
</table>

***

### 5.10.19 获取设备韵律状态 (CID=0x13)

查询设备当前韵律运行状态。请求可携带过滤条件缩小范围。

**请求帧字段：**

<table><colgroup><col width="210"><col width="145"><col width="145"><col width="145"><col width="445"><col width="129"><col width="129"></colgroup>
<thead>
<tr>
<th>字段名称</th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td>rhythm_id</td>
<td>0x05</td>
<td>1</td>
<td>uint8<br /></td>
<td>过滤：韵律编号。不携带则返回全部</td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="7">请求帧为空时返回全部 pump_side 及全部 rhythm_id 的韵律运行状态。<br /><strong>发送帧结构：</strong><ul>
<li>查全部：<code>[SID] [CID]</code></li>
<li>查 Left 侧指定韵律：<code>[SID] [CID] [pump_side(0x03)] [Len=1] [0x00] [rhythm_id(0x05)] [Len=1] [0x03]</code></li>
</ul></td>
</tr>
</tbody>
</table>

**接收帧字段：**

<table><colgroup><col width="140"><col width="160"><col width="150"><col width="100"><col width="100"><col width="100"><col width="350"><col width="80"><col width="80"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong></th>
<th><strong>结构体名称</strong></th>
<th><strong>字段名称</strong></th>
<th>Type(bit0~bit6)</th>
<th>Length
Size(Oct)</th>
<th>Value
数据类型</th>
<th>说明</th>
<th>必选/可选(M/O)</th>
<th>版本</th>
</tr>
</thead>
<tbody>
<tr>
<td>rhythm_status_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>韵律状态数据列表（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td>rhythm_status_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>韵律状态结构体（容器）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td><span style="color: inherit; background-color: rgba(255,246,122,0.8)">作用对象标识符（如果不携带，则默认全部）</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x00：Left </span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x01：Right</span>
<span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x02：Sync</span><br /><span style="color: inherit; background-color: rgba(255,246,122,0.8)">0x03：independence</span></td>
<td>O</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>rhythm_status</td>
<td>0x04</td>
<td>1</td>
<td>uint8</td>
<td>0x00=running，0x01=paused</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>rhythm_id</td>
<td>0x05</td>
<td>1</td>
<td>uint8</td>
<td>当前工作韵律编号</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>mode_seq</td>
<td>0x06</td>
<td>1</td>
<td>uint8</td>
<td>当前运行的模式序号</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td></td>
<td></td>
<td>rhythm_time_left</td>
<td>0x07</td>
<td>2</td>
<td>uint16</td>
<td>当前工作韵律剩余时长（秒）</td>
<td>M</td>
<td>1.0.0</td>
</tr>
<tr>
<td colspan="9"><strong>接收帧结构：</strong><br />返回成功（Left 侧、rhythm_id=3 示例）：<pre><code class="language-plaintext">[SID] [CID]
  [rhythm_status_list(0x81 = 0x01 | 0x80)]   [Len=0xNA]              ← 容器, 子节点总长
    [rhythm_status_struct(0x82 = 0x02 | 0x80)] [Len=0xNA]             ← 容器
      [pump_side(0x03)]          [Len=1] [0x00]         ← Left
      [rhythm_status(0x04)]      [Len=1] [0x00]         ← running
      [rhythm_id(0x05)]          [Len=1] [0x03]
      [mode_seq(0x06)]           [Len=1] [0x02]        ← 当前运行第 2 个模式引用
      [rhythm_time_left(0x07)]   [Len=2] [0x0258]
</code></pre><strong>说明：</strong><ul>
<li>自动上报，或者远端主动查询获取。</li>
</ul></td>
</tr>
</tbody>
</table>

### 5.7.20 上报原始压差 (CID=0x14)（临时）

设备约 **1 秒一次**主动 Notify 原始压差（`raw_diff_press`）。主机侧整理 Left/Right 后上报；从机经 TWS 推给主机，不直连 App。App **无需下发请求**（当前实现不支持 GET）。

**接收帧字段：**（设备→App Notify）

<table><colgroup><col width="140"><col width="100"><col width="100"><col width="160"><col width="150"><col width="100"><col width="449"></colgroup>
<thead>
<tr>
<th><strong>列表名称</strong><br /></th>
<th>结构体名称</th>
<th>字段名称</th>
<th><strong>Type(bit0~bit6)</strong></th>
<th><strong>Length</strong>
<strong>Size(Oct)</strong></th>
<th><strong>Value</strong>
<strong>数据类型</strong></th>
<th><strong>说明</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td>raw_diff_list</td>
<td></td>
<td></td>
<td>0x01</td>
<td>NA</td>
<td>NA</td>
<td>压差数据列表（容器）</td>
</tr>
<tr>
<td></td>
<td>raw_diff_struct</td>
<td></td>
<td>0x02</td>
<td>NA</td>
<td>NA</td>
<td>单侧压差结构体（容器）</td>
</tr>
<tr>
<td></td>
<td></td>
<td>pump_side</td>
<td>0x03</td>
<td>1</td>
<td>uint8</td>
<td>0x00=Left，0x01=Right</td>
</tr>
<tr>
<td></td>
<td></td>
<td>raw_diff_press</td>
<td>0x04</td>
<td>2</td>
<td>uint16</td>
<td>原始压差：截断取整；>65535 钳到 65535；大端</td>
</tr>
<tr>
<td colspan="7"><strong>接收帧结构：</strong><br />返回成功（Left 侧、rhythm_id=3 示例）：<pre><code class="language-plaintext">[SID=0x10] [CID=0x14]
  [raw_diff_list(0x81 = 0x01 | 0x80)]   [Len=0x12]
    [raw_diff_struct(0x82 = 0x02 | 0x80)] [Len=0x07]
      [pump_side(0x03)]        [Len=1] [0x00]         ← Left
      [raw_diff_press(0x04)]   [Len=2] [0x0000]
    [raw_diff_struct(0x82 = 0x02 | 0x80)] [Len=0x07]
      [pump_side(0x03)]        [Len=1] [0x01]         ← Right
      [raw_diff_press(0x04)]   [Len=2] [0x0000]
</code></pre><strong>说明：</strong><ul>
<li>仅设备主动上报；须先 Enable Pump TX CCCD。</li>
<li>单机仅带本机一侧；TWS 已通时主机 Notify 固定 Left + Right（对侧暂无缓存时 value=0）。</li>
<li>数值来源：算法 <code>milk_pressure.raw_diff_press</code>，uint16 截断取整并上限钳位。</li>
</ul></td>
</tr>
</tbody>
</table>

***

# 6 版本约束规则

## 6.1 蓝牙版本规则定义

* 蓝牙版本号命名规则为：蓝牙协议版本号 + “蓝牙接口规范\_应用层”版本号

&#x20;   格式为：X.X.X.XX

&#x20;   如：5.0.2.12，表示使用蓝牙5.0协议，使用“蓝牙接口规范\_应用层”的V2.12版本。
