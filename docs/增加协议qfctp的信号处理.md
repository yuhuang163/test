# 增加协议 qfctp 的信号处理

## 1. 目标

在保持上层业务逻辑基本不变的前提下，为 `qfctp` 补齐与 `qpb` 对齐的公共信号输出能力，并通过 `QProtocolManager` 统一对外转发，减少上层按协议分支连接信号的改造成本。

与《协议解耦迁移说明》保持一致的原则：

- 上层优先依赖 `Protocol*Data` 公共结构
- 协议私有解析逻辑留在协议层（`qpb` / `qfctp`）
- 上层尽量只和 `QProtocolManager` 交互

---

## 2. 当前现状

### 2.1 `qfctp` 已具备的接收与分发框架

`qfctp` 内部已具备如下关键处理能力，可作为信号补齐基础：

- 帧级收包：`handleFullFrame(...)`
- Response 分发：`handleResponseService(...)` / `handleResponseByType(...)`
- Notify 分发：`handleNotifyService(...)`
- 按 TLV 处理器：`handleRspSnRead(...)`、`handleRspTupleRead(...)`、`handleRspBatteryRead(...)`、`handleRspKeySignalRead(...)` 等

说明：这些函数已经覆盖了大部分产测/系统配置类读写场景，主要缺口在“解析后如何统一对外发信号”。

### 2.2 `qpb` 已有公共信号能力（参考基线）

`qpb` 已对外提供完整的公共信号集合（节选）：

- `send_sn_data(ProtocolSnData)`
- `send_battary(ProtocolBatteryData)`
- `send_button_state(ProtocolButtonStateData)`
- `send_photosensitive_info(ProtocolPhotosensitiveData)`
- `send_periph_data(ProtocolPeriphStateData)`
- `send_pb_date(QString)`
- `sendGetProductResponse(int)`

### 2.3 `QProtocolManager` 当前转发能力

`QProtocolManager::syncActivePointer()` 当前仅统一转发：

- `send_pb_date(...)`
- `sendGetProductResponse(...)`

其余公共信号尚未在管理器层统一转发。

---

## 3. 改造策略

### 3.1 先在 `qfctp` 对齐公共信号，再补 `QProtocolManager` 转发

建议顺序：

1. 在 `qfctp.h` 增加与 `qpb.h` 对齐的公共信号声明（优先高频用到的）
2. 在 `qfctp.cpp` 对应 `handleRsp*` / `handleNotify*` 中完成 `Protocol*Data` 组包并 `emit`
3. 在 `QProtocolManager` 增加对应信号，并在 `syncActivePointer()` 中统一 connect/disconnect
4. 上层连接统一收敛到 `QProtocolManager`

### 3.2 对齐原则

- **信号名优先保持与 `qpb` 一致**，减少上层重复分支
- **参数类型统一为 `Protocol*Data`**，避免上层再次依赖协议私有结构
- **字段缺失时在文档中明确默认值策略**（如 `-1` / 空串 / `false`）

---

## 4. 首批建议对齐的信号清单（高优先级）

> 目标：覆盖当前 `qfctp` 已有解析函数对应的上层常用数据路径

1. `send_sn_data(ProtocolSnData)`  
   来源：`handleRspSnRead(...)`

2. `send_battary(ProtocolBatteryData)`  
   来源：`handleRspBatteryRead(...)`

3. `send_button_state(ProtocolButtonStateData)` 或等价按键信号  
   来源：`handleRspKeySignalRead(...)`

4. `send_periph_data(ProtocolPeriphStateData)`  
   来源：`handleRspSensorState(...)`

5. `send_pb_date(QString)`  
   来源：通用响应日志/流程结果

6. `sendGetProductResponse(int)`  
   来源：请求结果主状态码

7. `send_fw_version(QString)`（若上层已依赖）  
   来源：`handleRspFwVersionRead(...)`

---

## 5. 代码改造点

### 5.1 `agreement/qProtocol/qfctp/qfctp.h`

- 新增与 `qpb` 对齐的公共信号声明（按优先级分批）
- 保留 `qfctp` 特有信号，但建议后续收敛为公共信号

### 5.2 `agreement/qProtocol/qfctp/qfctp.cpp`

- 在 `registerResponseHandlers()` 保证 TLV 到 `handleRsp*` 的映射完整
- 每个 `handleRsp*` 内完成：
  - TLV 数据合法性校验
  - `Protocol*Data` 组包
  - `emit` 公共信号
- `handleNotifyService(...)` 中增加 Notify 类公共信号分发（如按键/光感）

### 5.3 `agreement/qProtocol/qprotocolmanager.h/.cpp`

- 在 `QProtocolManager` 增加需要统一对外的信号声明
- `syncActivePointer()` 中对 `qpb_` / `qfctp_` 执行对称 connect/disconnect
- 保证切换协议类型后，上层连接不需要重新按协议分支处理

---

## 6. 验收标准

满足以下条件即视为本项改造完成：

- `QProtocolManager` 作为上层唯一连接点时，`Qpb` 与 `Qfctp` 均能输出一致信号
- 协议切换（`Qpb` <-> `Qfctp`）后，上层槽函数无需修改
- 首批信号链路（SN / Battery / Sensor / Button / 通用结果）均已通过串口实测
- 未引入 Qt 连接类型不匹配或元类型未注册问题

---

## 7. 实施步骤（执行清单）

1. 盘点 `qfctp` 已实现 `handleRsp*` 与其应输出的公共信号
2. 在 `qfctp` 中补齐首批公共信号发射
3. 在 `QProtocolManager` 中补齐信号桥接
4. 将上层连接点逐步收敛到 `QProtocolManager`
5. 逐条回归 TLV 场景（按《协议适配》中的 8.2.x 清单）

---

## 8. 风险与注意事项

- `qfctp` 部分 TLV 的返回字段可能与 `qpb` 历史字段不完全一致，需在映射时定义兼容策略
- 公共结构缺字段时，不要在上层临时拼协议私有数据，应先扩展 `qprotocol_types.h`
- 先补转发再删旧连接，避免出现“信号已发但上层未接到”的迁移空窗