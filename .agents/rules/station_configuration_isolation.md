# 工站配置隔离与单步参数规范 (Step Parameter Isolation Rule)

## 核心原则 (Mandatory Rule)

1. **严禁将具体业务/工站的配置、开关或阈值放入全局配置文件（如 `上位机设置.ini` / `SETTINGS`）。**
2. **所有与具体测试项目、仪器仪表卡控、显示开关相关的配置，必须严格存放在具体用例步骤参数（`Param_xxx`）中。**
3. **不得依赖 `flow.ini` 或跨文件全局读取。**

---

## 规范细节

- **参数位置**：对应工站的步骤 INI 文件（例如 `profiles/{工站}/steps/{步骤名}.ini`）的 `[Send]` 节中；
- **参数格式**：`Param_{参数名}={参数值}`（例如 `Param_showCurveInTable=1`、`Param_deadDiff=35` 等）；
- **代码读取规范**：
  - 仅从步骤参数表 `map`（即解析后的 `Param_xxx`）中读取；
  - 若步骤中未配置该参数，**直接回退至安全的硬编码默认值**（如 `false`、`0`、`""`、`35` 等）；
  - **严禁**回退至 `SETTINGS.value(...)` 或其他全局 INI 文件。
