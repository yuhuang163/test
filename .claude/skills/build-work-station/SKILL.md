---
name: build-work-station
description: 在自由工站里搭建 / 新建测试用例工站（FlowStation）。当用户说"搭建工站""新建工站""在自由工站里加一个 XX 测试工站""新建 XX 测试用例 / 测试流程""写个 XX 的测试流程"等时使用。指导在自由工站（FREE_WORK）新建一个测试用例工站：先判断纯编排还是需要新命令 / 钩子，再走「测试流程编排」UI 或补 manifest / hook 代码，产出能跑通的简体中文测试流程。
---

# 在自由工站搭建测试用例工站

自由工站（FREE_WORK）里的「测试用例工站」（FlowStation）是**数据驱动的流程编排**：一个工站 = 一组有序的功能块（TestCase），每块 = 一条 Send 命令 + 时序 + 卡控（Gate）＋ 可选钩子（Hook）。大部分工站**纯编排即可建成，不用写 C++**；只有当工站要用到现有命令清单里没有的命令、或需要 send+gate 表达不了的自定义逻辑时，才补代码。

## 0. 读取与编码注意

- 源文件是 UTF-8 无 BOM + CRLF，`Read` 工具可直接读中文。
- 写 `.cpp/.h` 后用 `perl -pi -e 's/\r?\n/\r\n/g' <文件>` 转 CRLF（UTF-8 无 BOM）。本机 `python` 是 Windows Store 占位符，跑不了 `scripts/convert_to_crlf.py`。

## 1. 架构：自由工站里的「工站」是什么

| 概念 | 结构体 | 落地位置 | 谁创建 |
|------|--------|---------|--------|
| **工站** FlowStation | `TestFlowStationEntry{key, displayName}` | `测试流程.ini [FlowStations]` + `profiles/{中文名}/` 目录 | 编排 UI（`TestCaseStore::addFlowStation` / `copyFlowStation`） |
| **流程项** FlowItems | `TestFlowItemEntry{caseName, enabled}` | `profiles/{中文名}/flow.ini [FlowItems]`（失败区 `[FailItems]`） | 编排 UI（`saveStationFlowItems`） |
| **功能块** TestCase | `TestCaseDefinition`（Meta/Send/Timing/Gate/Hook） | `test_case/steps/{stepId}.ini`（总库）+ `profiles/{中文名}/steps/{stepId}.ini`（覆盖层） | 编排 UI（`saveCase` / `saveCaseForStation`） |
| **命令清单** Manifest | C++ 表 + catalog | `platform/test_case/manifest/` + `catalog/` | 写代码（仅新命令时） |
| **执行器** Runner | 通用 C++ | `platform/test_case/runner/` | 已通用，不改 |
| **钩子** Hook | C++ 函数 | `platform/test_case/hooks/qfreework_case_hooks.cpp` | 写代码（仅自定义逻辑时） |

一个**功能块**（`TestCaseDefinition`）落地为一个 ini，结构（键名与 `writeCaseIniFile` 一致）：

```ini
[Meta]   StepId / Name / DisplayName / MesTag / PromptEnabled / PromptOnly / PromptText
[Send]   Channel / Protocol / Device / DeviceCmd / Action(Set|Get) / Param...
[Timing] DelayBeforeMs / DelayAfterMs / CommandTimeoutMs / WaitReply
[Gate]   Enabled / ReportType / Field / Op / Low / High / Expected
         / ExpectedSettingsKey / LowSettingsKey / HighSettingsKey
[Gate/N] 多卡控 gates（可选）
[Hook]   Enabled / HookId
```

**Send 通道**（`TestCaseSendChannel`）与协议：

| Channel | 用途 / 可选 Protocol |
|---------|---------------------|
| `Product` | 产测协议 `Qfctp` / `Qpb` / `Qroot` / `Qaiot`（走 `DeviceCmdCatalog`） |
| `ProductSerial` | 产品串口仪器（`ProductSerialCmd`） |
| `Dongle` | Dongle AT 透传（`DongleCmdCatalog`） |
| `Cloud` | 三元组 / 云端（`TupleCmdCatalog`） |
| `Fixture` | 治具 `Pcba` / `Asd9026a` / `Xwd` / `JieliBtBox` / `UsbCamera` / `VesLight` |
| `Modbus` | 外设（`ModbusPeriphCmdCatalog`，按设备 route 分命令） |
| `Scpi` | 外设（`ScpiPeriphCmdCatalog`，按设备 route 分命令） |

## 2. 先判断路径（搭一个工站要不要写代码）

先确认工站每个功能块要发的命令，**命令是否已在现有 manifest 里**（§4 列了全部现有清单）：

- **都在现有 manifest 里** → 走 **§3 纯编排**，零代码。
- **有新命令**（新设备 / 现有设备新指令，manifest 表里没有） → §3 编排 + **§4 加 manifest**。
- **有 send+gate 表达不了的自定义逻辑**（如写入 SN 尾缀、PLC 专用时序、CMW 射频脚本） → §3 编排 + **§5 加 Hook**。

> 原则：能用现有命令 + 卡控拼出来的，绝不写代码；写代码只补「缺的命令」或「缺的步骤逻辑」，不要重复造轮子。

## 3. 纯编排路径（主路径，零代码）

入口：**设置页 → 测试流程编排**（`TestFlowEditor`，`platform/settings/test_flow/`，由 `qsetting::initTestFlowEditorUi` 绑定）。

1. **新建工站**：工站下拉右键菜单「新建工站」→ 输入中文名（`promptAddFlowStation` → `addFlowStation` 生成 `profiles/{中文名}/`）。已有类似工站可用「复制工站」起底（`copyFlowStation`）。
   > ⚠️ **工站编号从服务器拿，避免重复**：`addFlowStation` 生成 key 用的 `allocateCustomFlowStationKey` 只在**本机** catalog 内自增查重（`FLOW_ST_0001` 起），跨上位机不唯一——两台机器各自新建都会撞号，上传云端后 `station_key` 冲突。工站编号应从服务器（fwq）统一分配/获取，不要靠本地自增。
2. **配功能块列表**：在流程区按顺序加入功能块（每个块引用一个步骤 id；顺序即 `flow.ini [FlowItems]`）。
3. **配每个功能块**：选 `Send` 通道 + 协议 + 设备 + 命令（下拉来自 manifest，命令带中文说明与参数提示）+ `Timing` + `Gate` 卡控（`ReportType`+`Field`、算子、阈值或 `*SettingsKey`）。
4. **配串口显隐**：`flow.ini [SerialUi]` 治具/产品/万用表三路串口的显隐与标签（`TestCaseSerialUiConfig`）。
5. **保存**：写 `profiles/{中文名}/flow.ini` + `steps/*.ini`（覆盖层）。新工站无需重编译。

纯编排下的 API（如需脚本化，`platform/test_case/store/test_case_store.h`）：`addFlowStation` / `copyFlowStation` / `saveStationFlowItems` / `saveCaseForStation` / `saveStationSerialUiConfig`。

## 4. 需要新命令：加 Manifest（写代码，4 处）

以「给 SCPI 新增一个设备命令」或「新治具协议」为例，沿现有 `ScpiCmdManifest` / `XwdRawFixtureCmd` 照抄。注册链四处：

1. **命令表** `platform/test_case/manifest/<xxx>_cmd_manifest.h/.cpp`：`namespace XxxCmdManifest { struct Row { device; enumName; uiLabel; paramHint; sendActions; gateReportType; gateDefaultField; }; const Row* rows(); }`——`enumName` 是功能块 ini 里 `Send/DeviceCmd` 存的值，`uiLabel` 是编排页中文名，`gateReportType/gateDefaultField` 是该命令默认卡控上报字段。
2. **catalog 类** `platform/test_case/catalog/cmd_manifest_catalogs.h/.cpp`：`class XxxCmdManifestCatalog : public CmdManifestCatalog`，构造函数里用 `CmdManifestRegistry{Policy, rows}` 生成；按需 override `paramFromIniGroup / paramToIniGroup`。
3. **门面** `platform/test_case/catalog/test_case_catalog.h/.cpp`：`class XxxCmdCatalog { static CmdManifestCatalog& catalog(); }`（返回静态单例）。
4. **分发绑定** `platform/test_case/catalog/test_case_send_dispatch.cpp`：加 `catalogFn()` 并接入 `CatalogBinding` 表（按 channel/protocol 命中）。

> Modbus / Scpi 是「按设备 route 再分命令」的两级清单（`ModbusPeriphCmdCatalog` / `ScpiPeriphCmdCatalog`），新设备按设备 route 扩表，不新开 manifest 文件。

## 5. 需要自定义逻辑：加 Hook（写代码，2 处）

功能块 ini 里 `[Hook] Enabled=1 HookId=XXX` 引用，执行到该步调 `TestCaseHookRegistry::invoke`。

1. **实现** `platform/test_case/hooks/qfreework_case_hooks.cpp`：在 `QFreeWorkTestCaseHookRegistrar::dispatch(QFreeWork* fw, const QString& hookId)` 里加 `if (hookId == QStringLiteral("XXX")) { ... }`（函数签名 `void(QFreeWork*)`，通过 `fw` 取界面/串口/协议上下文）。
2. **注册** 同文件 `registerAll()` 里加一行 `registerDispatchHook(QStringLiteral("XXX"));`。

## 6. 卡控与异步判定（写对的关键）

- **卡控阈值不硬编码**：`Gate` 优先用 `LowSettingsKey / HighSettingsKey / ExpectedSettingsKey` 指向 `SETTINGS` 键（与 `qsetting` 同源）；`Op` ∈ `Range / Gt / Lt / Eq / CompareVersions`，版本比对用 `CompareVersions`（别用裸 `==`）。
- **`ReportType` + `Field`** 对应协议上报数据（如 `ProtocolMeasureData` + `value`），与 manifest 里 `gateReportType/gateDefaultField` 及 test_base 的 `refresh*` 槽对上。
- **`TestCaseRunner::needAsyncDone(def)`** 决定本步是否必须等异步 `markDone` 才过步（有 Gate / 连接 / 采样的步骤为 true）；与 `sendCommandWithRetry` 的 `allowResend`（仅控制窗口内重发）**是两回事**，勿混用。
- **异步回调守卫**：自由工站在 `refresh*` 回调里判定时用 `isCurrentStep("步骤中文名")` 守卫，避免串步骤误判定。
- **结论**：卡控通过/失败写 `showlog`（带「卡控通过/失败、当前值、允许范围」），结果落到工站结果表 / MES 分项（`MesTag`）。

## 7. 验证

- 纯编排：进自由工站（FREE_WORK），在工站下拉选中新工站，跑一遍功能块列表，确认每步命令下发、回包、卡控判定与失败区（FailItems）符合预期。
- 加了 manifest / hook 代码：`scripts\编译Release版本.ps1 -SkipQmake` 增量编译通过后，再到自由工站跑流程；命令的中文名与参数提示应在编排页下拉里可见、可保存。

## 8. 输出前自查

- 路径判断对：新工站命令都在现有 manifest 里（纯编排）还是需要补 manifest / hook。
- 每个功能块 ini 结构完整（Meta/Send/Timing/Gate/Hook），`DeviceCmd` 与 manifest `enumName` 一致，`Channel/Protocol` 合法。
- 卡控 `ReportType/Field/Op` 对上协议上报字段，阈值走 `*SettingsKey` 而非硬编码；版本比对用 `CompareVersions`。
- 需要异步结案的步骤 `needAsyncDone` 语义对，回调带 `isCurrentStep` 守卫。
- 代码 UTF-8 无 BOM + CRLF，简体中文注释；新增 manifest / hook 按 §4/§5 的注册链无遗漏。
- 工站编号从服务器统一分配，未依赖本地自增（跨机撞号）。
- 在自由工站里实际跑通，而非只保存了 ini。
