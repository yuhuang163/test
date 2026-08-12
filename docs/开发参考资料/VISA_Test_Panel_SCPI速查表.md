# VISA Test Panel SCPI 速查表 — 程控电源

> **一句话**：在 NI MAX「VISA Test Panel」里可直接粘贴的 SCPI 命令表（均带 `\n`）。  
> **读者**：产线调试 / 上位机开发。**前提**：已安装 NI-VISA；仪器在 MAX 中可见；测试前关闭其它占用同一资源的程序。

## 快速参考

| 项 | 值 |
|----|-----|
| 工具 | NI MAX → 设备 → Open VISA Test Panel → Input/Output → Basic I/O |
| 工程默认（安捷伦） | `agreement/scpi_protocol/device/agilent_66319d_scpi/agilent_66319d_profile.cpp` |
| 工程默认（会凌） | `agreement/scpi_protocol/device/huiling_wfp60h_scpi/huiling_wfp60h_profile.cpp` |
| 步骤覆盖 | 自由工站 ini：`Param_scpiSetVoltageCmd` 等 |

## 1. 范围

### 包含

- IEEE 488.2 通用查询/复位
- 工程默认的 **Agilent 66319D**、**会凌 WFP60H** 程控电源短形命令
- NI MAX 中 Write / Query 选用说明

### 不包含（边界）

- CMW100 / 其它仪表完整命令树
- 厂家手册全部长形关键字
- USB 电流表（非 VISA 程控电源通道）的命令

---

## 2. 使用约定

1. 命令末尾必须带 **`\n`（换行，`0x0A`）**，与 `*IDN?\n` 相同；否则仪器可能不解析。
2. 带 **`?`** 的命令用面板 **Query**；无 `?` 的设置/开关用 **Write**。
3. 模板里的数值示例可改；工程代码里用 `%1` 占位，由上位机替换。
4. 步骤 ini 若写了 `Param_scpi*`，以 **ini 为准**，下表仅为代码默认短形。
5. **测上位机前关闭本 Test Panel**（以及 NI MAX 通讯器），避免占线。

---

## 3. IEEE 488.2 通用

| 命令（含换行） | 面板操作 | 含义 | 典型返回 |
|----------------|----------|------|----------|
| `*IDN?\n` | Query | 查询设备身份 | `厂商,型号,序列号,版本` |
| `*RST\n` | Write | 复位到出厂/默认状态 | 无（或随后读错误队列） |
| `*CLS\n` | Write | 清除状态字节/错误队列 | 无 |
| `SYST:ERR?\n` | Query | 读系统错误 | `0,"No error"` 或错误码文本 |

---

## 4. Agilent 66319D（工程默认）

来源：`Agilent66319dScpiProfileUtil` 默认值；自由工站「配置Visa程控电源」常用同类字符串。

| 命令（含换行） | 面板操作 | 含义 | 对应 ini / 设置键（示例） |
|----------------|----------|------|---------------------------|
| `VOLT 5.0\n` | Write | 设输出电压（V） | `scpiSetVoltageCmd=VOLT %1` |
| `CURR 3.0\n` | Write | 设限流（A） | `scpiSetCurrentCmd=CURR %1` |
| `SENS:CURR:RANG 3\n` | Write | 设电流量程 | `scpiSetCurrentRangeCmd=SENS:CURR:RANG %1` |
| `OUTP ON\n` | Write | 打开输出 | `scpiOutputOnCmd=OUTP ON` |
| `OUTP OFF\n` | Write | 关闭输出 | `scpiOutputOffCmd=OUTP OFF` |
| `MEAS:VOLT:DC?\n` | Query | 测量输出电压 | `scpiReadVoltageCmd=MEAS:VOLT:DC?` |
| `MEAS:CURR:DC?\n` | Query | 测量输出电流 | `scpiReadCurrentCmd=MEAS:CURR:DC?` |
| `INST OUT1\n` | Write | 双通道选通（通道号按机型） | `scpiChannelSelectCmd=INST OUT%1` |

建议手测顺序：`*IDN?` → `VOLT` / `CURR` → `OUTP ON` → `MEAS:VOLT:DC?` / `MEAS:CURR:DC?` → `OUTP OFF`。

---

## 5. 会凌 WFP60H（工程默认短形）

来源：`HuilingWfp60hScpiProfile` 默认值。长形说明见同目录 `WFP60H_SCPI与上位机电源配置说明.md`。

| 命令（含换行） | 面板操作 | 含义 | 对应 ini / 设置键（示例） |
|----------------|----------|------|---------------------------|
| `SOUR1:VOLT 5.0\n` | Write | 通道 1 设电压（V） | `scpiSetVoltageCmd=SOUR1:VOLT %1` |
| `SOUR1:CURR 3.0\n` | Write | 通道 1 设限流（A） | `scpiSetCurrentCmd=SOUR1:CURR %1` |
| `OUTP1 ON\n` | Write | 通道 1 打开输出 | `scpiOutputOnCmd=OUTP1 ON` |
| `OUTP1 OFF\n` | Write | 通道 1 关闭输出 | `scpiOutputOffCmd=OUTP1 OFF` |
| `MEAS1:VOLT:DC?\n` | Query | 读通道 1 电压 | `scpiReadVoltageCmd=MEAS1:VOLT:DC?` |
| `MEAS1:CURR:DC?\n` | Query | 读通道 1 电流 | `scpiReadCurrentCmd=MEAS1:CURR:DC?` |

通道 2 时把命令中的 `1` 换成 `2`（如 `SOUR2:VOLT`、`OUTP2 ON`），以厂家手册为准。

---

## 6. 验证

| 步骤 | 预期 |
|------|------|
| 在 MAX 中对正确 VISA 地址打开 Test Panel | 标题栏显示完整 `USB0::…` / `GPIB0::…` 地址 |
| Query `*IDN?\n` | Return Data 为 **No Error**，有身份字符串 |
| Write 设压/设流后 Query 测量命令 | 返回数值；面板无 ABORT / 超时 |
| 关闭 Test Panel 后再跑上位机「配置Visa程控电源」 | 上位机日志出现 `VISA TX:`，无「打开设备失败」 |

失败时优先检查：地址是否与 ini 一致、线缆/电源、是否仍有 MAX 窗口占线。

---

## 7. 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| Write/Query 报错或上位机 `VI_ERROR_ABORT` | Test Panel / 通讯器占着资源 | 关闭面板与 MAX 后再测上位机 |
| 写出成功但仪器无反应 | 漏了 `\n`，或命令与机型不符 | 用表中带 `\n` 的完整串；核对 66319D vs WFP60H |
| `*IDN?` 通，设压失败 | 机型命令集不同或通道未选 | 换对应机型表；双通道先 `INST OUTn` |
| 上位机命令与面板不一致 | 步骤 ini 覆盖了默认 | 打开对应 `配置Visa程控电源.ini` 看 `Param_scpi*` |

---

## 3 行摘要

- **结论**：NI MAX 手测用「命令 + `\n`」；66319D 用 `VOLT`/`OUTP`/`MEAS:`，WFP60H 用 `SOUR1`/`OUTP1`/`MEAS1:`。  
- **操作**：Test Panel → 粘贴上表 → Query/Write；测完关掉面板。  
- **验证**：`*IDN?\n` 无错且有回包；上位机与 ini 地址一致且无占线。
