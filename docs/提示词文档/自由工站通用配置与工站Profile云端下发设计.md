# 自由工站通用配置与工站 Profile 云端下发 — 需求设计

> 本文档用于 Review 与后续开发对齐。与 [功能测试库与测试流程编排需求.md](./功能测试库与测试流程编排需求.md)、[云端工厂平台功能设计.md](./云端工厂平台功能设计.md) 配套阅读。  
> 实现时须遵守仓库 `.cursor/rules/qt-cpp-project.mdc`（`SETTINGS`、分层、UTF-8 无 BOM 等）。

---

## 一、背景与问题

### 1.1 现状

- 所有测试步骤 ini 平铺在 `exe/test_case/`，**逻辑、参数、卡控**写在同一文件。
- 各工站流程在 `总的测试流程.ini` 的 `[Station/{StationKey}]` 中仅保存 **步骤名称列表**（`Items`）。
- `TestCaseStore::loadCase(caseName)` **无工站上下文**，参数只能复制整份 ini → 出现 `M8_xxx`、同逻辑多文件名，编排与运维混乱。
- 云端 `test_case_sync_service` 当前打包/覆盖 **整个 `test_case/` 目录**，无法按工站精准下发，易误覆盖其它线体配置。

### 1.2 目标架构（已拍板方向）

| 决策 | 说明 |
|------|------|
| **唯一运行时工站** | 后续全部采用 **自由工站（`QFreeWork` / `FREE_WORK`）** 作为通用执行引擎 |
| **具体工站 = 配置实例** | 在设置页「测试流程编排」中 **新建工站** 得到 `StationKey`（如 `FLOW_ST_0007`），非独立 C++ 工站类 |
| **步骤逻辑与参数分离** | 全局 **步骤库** + 每工站 **Profile 目录**（流程 + 参数覆盖） |
| **云端按工站整包替换** | 下发/同步时 **整体替换 `profiles/{StationKey}/` 目录**（含 flow、步骤参数、profile 元数据），保证流程与参数一致 |

### 1.3 非目标（本期不做）

- 不新增专用 C++ 工站类（`suction`、`key_test` 等逐步退役为 Profile 预设）。
- 不要求自动迁移历史 `TestOrder_*`；旧平铺 ini 通过兼容层渐进淘汰。

---

## 二、概念模型

```text
┌─────────────────────────────────────────────────────────────────┐
│  QFreeWork（唯一运行时）                                           │
│  SYSTEM/station = FREE_WORK（固定）                                │
│  TestOrderMeta/SelectedStation = FLOW_ST_0007（当前 Profile）     │
└───────────────────────────────┬─────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
  test_case/steps/      profiles/{Key}/flow.ini    上位机设置.ini
  （全局步骤库）          （本工站步骤顺序）          [Station/{Key}/…] 阈值
        │                       │
        │                       └── profiles/{Key}/steps/
        │                           （本工站参数覆盖）
        └──────── merge ────────► TestCaseDefinition → 执行
```

| 概念 | 键/路径 | 职责 |
|------|---------|------|
| **StepId** | 稳定 ID，如 `采集双通道吸力` | 全厂唯一步骤逻辑标识；≠ 历史 ini 文件名 |
| **步骤库** | `test_case/steps/{StepId}.ini` | Channel、DeviceCmd、Hook、Gate 结构、默认 Timing |
| **工站 Profile** | `test_case/profiles/{StationKey}/` | 某条产线/工艺的完整可运行配置 |
| **工站流程** | `profiles/{Key}/flow.ini` | `Items` 顺序、`StopFlowOnTestFail` |
| **工站参数** | `profiles/{Key}/steps/{StepId}.ini` | Send/Param、Timing/Gate 覆盖 |
| **工站阈值** | `上位机设置.ini` `[Station/{Key}/…]` | 吸力/RSSI/电流等产线级卡控（可选） |

**StationKey** 与现有体系对齐：

- 模板：`FREE_WORK`
- 自定义：`FLOW_ST_0001` …（`TestCaseStore::allocateCustomFlowStationKey`）
- 显示名：`总的测试流程.ini` → `[FlowStations]`（如 `按键测试(xwd)`）

---

## 三、目录结构规范

```text
test_case/
├── steps/                              # 全局步骤库（研发维护，多工站共用）
│   ├── 采集双通道吸力.ini
│   ├── 配置Visa程控电源.ini
│   └── ...
│
├── profiles/                           # 工站 Profile（新建工站 = 新建子目录）
│   ├── FREE_WORK/                      # 模板 Profile；「从 FREE_WORK 复制」的源
│   │   ├── profile.ini
│   │   ├── flow.ini
│   │   └── steps/                      # 相对步骤库的参数覆盖（可为空）
│   │       └── 配置Visa程控电源.ini
│   │
│   ├── FLOW_ST_0007/                   # 实例：按键测试(xwd)
│   │   ├── profile.ini
│   │   ├── flow.ini
│   │   └── steps/
│   │       ├── 采集双通道吸力.ini
│   │       └── ...
│   └── ...
│
└── 总的测试流程.ini                     # 过渡期：工站目录 + 显示名；Items 逐步迁入 flow.ini
```

### 3.1 `steps/{StepId}.ini`（步骤库 · 仅逻辑）

```ini
[Meta]
StepId=采集双通道吸力
DisplayName=采集双通道吸力
MesTag=SUCTION_DUAL_SAMPLE
TemplateVersion=1

[Send]
Channel=Product
Action=Set
DeviceCmd=DevReset
Protocol=Qfctp

[Hook]
Enabled=true
HookId=DONGLE_SUCTION_SAMPLE

[Gate]
Enabled=false

[Timing]
DelayBeforeMs=0
DelayAfterMs=500
CommandTimeoutMs=60000
```

**步骤库禁止写入**工站/产线差异项（VISA 地址、具体阈值、工厂 MES 差异等）→ 一律进 Profile 或 SETTINGS。

### 3.2 `profiles/{StationKey}/profile.ini`

```ini
[Profile]
StationKey=FLOW_ST_0007
DisplayName=按键测试(xwd)
CreatedFrom=FREE_WORK
CreatedAt=2026-07-03T10:00:00
ProfileVersion=3
StepsLibraryVersion=1
```

### 3.3 `profiles/{StationKey}/flow.ini`

```ini
[Flow]
Items=扫描连接蓝牙,M8_开启按键上报,采集双通道吸力,关闭dongle吸力读取
StopFlowOnTestFail=true
```

### 3.4 `profiles/{StationKey}/steps/{StepId}.ini`（仅覆盖）

```ini
[Meta]
StepId=配置Visa程控电源

[Send]
Param/visaAddress=TCPIP::192.168.1.10::5026::SOCKET
Param/voltage=12.0
Param/current=2.5

[Timing]
CommandTimeoutMs=8000
```

---

## 四、运行时加载与合并

### 4.1 API（新增/调整）

```cpp
// TestCasePaths
QString stepsDir();
QString stepLibraryPath(const QString& stepId);
QString profileDir(const QString& stationKey);
QString profileFlowPath(const QString& stationKey);
QString profileStepOverridePath(const QString& stationKey, const QString& stepId);

// TestCaseStore
bool loadCaseForStation(const QString& stationKey, const QString& stepId,
                        TestCaseDefinition& out, QString* errorOut = nullptr);
QVector<TestFlowItemEntry> loadStationFlowItems(const QString& stationKey);
// 优先 profiles/{Key}/flow.ini，fallback 总的测试流程.ini [Station/{Key}]
```

### 4.2 合并顺序（后者覆盖前者）

1. `steps/{StepId}.ini` — 步骤库默认  
2. `profiles/{StationKey}/steps/{StepId}.ini` — 工站参数覆盖  
3. `SETTINGS` — `Gate/*SettingsKey`、`[Station/{StationKey}/…]`、Hook 专用段（如 `[Suction]` 绑定到当前 StationKey）

### 4.3 自由工站执行入口

- `QFreeWork` 启动时读取 `TestOrderMeta/SelectedStation` → 解析为 `StationKey`。
- 流程列表：`loadStationFlowItems(stationKey)`。
- 逐步执行：`loadCaseForStation(stationKey, stepId, def)`。
- **`SYSTEM/station` 固定 `FREE_WORK`**；不再按 `SUCTION_TEST` 等切换工站类。

### 4.4 兼容策略（迁移期）

| 查找顺序 | 说明 |
|----------|------|
| `steps/` + `profiles/.../steps/` | 新结构 |
| `test_case/{StepId}.ini` 平铺 | 旧结构 fallback |
| `总的测试流程.ini` `[Station/.../Items]` | flow fallback |

---

## 五、设置页与「新建工站」行为

### 5.1 测试流程编排（现有 `TestFlowEditor` 扩展）

| 操作 | 磁盘行为 |
|------|----------|
| **新建工站** | 分配 `StationKey` → 创建 `profiles/{Key}/`（`profile.ini` + 空 `flow.ini` + 空 `steps/`） |
| **从 FREE_WORK 复制** | `copyFlowStation` + **递归复制** `profiles/FREE_WORK/` → `profiles/{NewKey}/`（flow + steps） |
| **保存流程** | 写入 `profiles/{Key}/flow.ini`，并同步 `[FlowStations]` 显示名 |
| **打开步骤参数** | 编辑 `profiles/{Key}/steps/{StepId}.ini`（表单化，禁止手改 ini） |

### 5.2 步骤库管理（新入口或子 Tab）

- 列表数据源：`test_case/steps/` 的 StepId（**全局唯一**，编排区只显示一份「采集双通道吸力」）。
- 编辑步骤库：只改逻辑字段；保存到 `steps/{StepId}.ini`。
- **从流程移除** ≠ 删除步骤库文件（与现有需求一致）。

### 5.3 工站步骤参数（新 Tab，P1）

- 左：当前 `StationKey`（`comboBox_testFlowStation`）  
- 中：该 Profile 的 `flow.ini` 中 StepId 列表  
- 右：该 Step 在 **本 Profile** 下的参数覆盖（写入 `profiles/{Key}/steps/`）

### 5.4 上位机工站类型

- 设置页「工站类型」：**仅保留「自由工站」** 为可执行项（其它 Radio 逐步隐藏或映射到 Profile 预设）。
- `FactoryCloud/StationKey`、`TestOrderMeta/SelectedStation` 与 Profile 的 `StationKey` 一致。

---

## 六、云端下发（核心：整包替换工站 Profile 目录）

### 6.1 原则

> **运维下发某工站配置时，必须整体替换 `test_case/profiles/{StationKey}/` 目录**（atomic replace），  
> 使 **流程（flow.ini）与步骤参数（steps/）同步更新**，避免「只更流程不更参数」或反之导致的不一致。

步骤库 `test_case/steps/` 为 **全厂共享**，与 Profile **分包、分版本** 管理。

### 6.2 包类型

| 包类型 | 路径 | 替换范围 | 典型场景 |
|--------|------|----------|----------|
| **Profile 包** | `profiles/{StationKey}/**` | **整目录替换** | 某线调流程、改 VISA 地址、改步骤参数 |
| **Steps 库包** | `steps/**` | 整目录替换 | 发版：新增 Hook、改 DeviceCmd 逻辑 |
| **全量 test_case 包** | `steps/` + `profiles/` + 索引 ini | 整包 | 新机初始化、灾难恢复 |

**Profile 包必须包含**（最少）：

```text
profiles/FLOW_ST_0007/
├── profile.ini          # ProfileVersion 递增
├── flow.ini             # Items + StopFlowOnTestFail
└── steps/               # 本 Profile 全部参数覆盖（可为空目录）
    ├── 配置Visa程控电源.ini
    └── ...
```

云端发布 Profile 时：**以目录为原子单元**；客户端下载 zip 后 **删除本地 `profiles/{StationKey}/` 再解压**，或解压到临时目录后 `rename` 替换（见 §6.4）。

### 6.3 HTTP 接口（在现有 factory-tool 上扩展）

#### 6.3.1 Profile manifest（JSON）

```http
GET /api/factory-tool/test-profiles/{stationKey}/manifest
Station-Key: FLOW_ST_0007
```

```json
{
  "code": 0,
  "data": {
    "stationKey": "FLOW_ST_0007",
    "displayName": "按键测试(xwd)",
    "profileVersion": 12,
    "stepsLibraryVersion": 5,
    "sha256": "...",
    "downloadUrl": "/api/factory-tool/test-profiles/FLOW_ST_0007/bundle",
    "updatedAt": "2026-07-03T09:00:00Z"
  }
}
```

#### 6.3.2 Profile bundle（zip 流）

```http
GET /api/factory-tool/test-profiles/{stationKey}/bundle
Authorization: Bearer ...
Station-Key: FLOW_ST_0007
```

- zip **根目录**即为 `profiles/{StationKey}/` 的内容（或 zip 内单根文件夹名 = `StationKey`）。
- **禁止** Profile bundle 只含部分 `steps/` 文件而不含 `flow.ini`（服务端校验）。

#### 6.3.3 Steps 库 manifest / bundle（JSON + zip）

```http
GET /api/factory-tool/test-steps/manifest
GET /api/factory-tool/test-steps/bundle
```

- 替换 `test_case/steps/` 整目录。
- manifest 带 `stepsLibraryVersion`；Profile 的 `profile.ini` 可记录依赖的最低 `StepsLibraryVersion`（可选校验）。

#### 6.3.4 上传（工程师站 → 云）

```http
POST /api/factory-tool/test-profiles/{stationKey}/upload
Content-Type: multipart/form-data
file: profile_FLOW_ST_0007.zip
```

- 服务端存 Object + 递增 `profileVersion`。
- 上传前客户端打包 **完整** `profiles/{StationKey}/` 目录。

### 6.4 客户端同步流程（`test_case_sync_service` 扩展）

```text
syncProfile(stationKey):
  1. GET manifest → 比对本地 profile.ini ProfileVersion
  2. 若 remote > local：
     a. GET bundle → 临时目录 {temp}/profiles/{stationKey}/
     b. 校验 sha256、必选文件（profile.ini, flow.ini 存在）
     c. 备份现有目录 → test_case/.backup/profiles/{stationKey}_{timestamp}/
     d. 删除 test_case/profiles/{stationKey}/
     e. 移动 temp → test_case/profiles/{stationKey}/
     f. 更新 SETTINGS FactoryCloud/ProfileVersion/{stationKey}
     g. TestCaseStore::invalidateCache()
  3. showlog 成功/失败；失败则保留 backup，不半量覆盖
```

**硬性要求**：

- Profile 同步 **不允许** 逐文件 patch 到旧目录（避免残留废弃 StepId 参数文件）。
- 同步 **进行中** 若工站正在跑测，提示「请停止测试后同步」或排队到空闲（P1）。

### 6.5 与现有「整包 test_case」的关系

| 现有 | 调整后 |
|------|--------|
| `GET /test-cases/bundle` 覆盖整个 `test_case/` | 保留为 **全量包**；日常运维改用 **Profile 包** + **Steps 库包** |
| 上传整个 test_case zip | 工程师全量发布；产线 **仅拉 Profile** |

---

## 七、版本与依赖

| 字段 | 位置 | 含义 |
|------|------|------|
| `TemplateVersion` | `steps/{StepId}.ini` | 单步骤逻辑版本 |
| `StepsLibraryVersion` | Steps 库 manifest | 全局步骤库版本 |
| `ProfileVersion` | `profile.ini` | **工站 Profile 整包版本**（云端下发比对主键） |
| `StepsLibraryVersion` | `profile.ini` | 该 Profile 依赖的步骤库版本（可选） |

**云端发布 Profile 时**：若步骤库逻辑变更，应 **先发布 Steps 库包**，再发布引用新 StepId/Hook 的 Profile 包；或 Profile 包内 manifest 声明 `minStepsLibraryVersion`，客户端不同步则拒绝替换并告警。

---

## 八、参数分层决策表

| 参数类型 | 存放位置 | 随 Profile 包下发 | 示例 |
|----------|----------|-------------------|------|
| 指令/Hook/卡控结构 | `steps/` | 否（Steps 库包） | `HookId=DONGLE_SUCTION_SAMPLE` |
| 工站 Send/Param | `profiles/{Key}/steps/` | **是** | VISA 地址、Modbus 寄存器 |
| 工站 Timing 覆盖 | `profiles/{Key}/steps/` | **是** | `CommandTimeoutMs` |
| 流程顺序 | `profiles/{Key}/flow.ini` | **是** | `Items=...` |
| 产线阈值 | `上位机设置.ini` 或 Profile 附带 `settings.patch.ini` | 可选 second zip | `[Station/{Key}/Suction]` |
| 运行时 SN/MAC | 不写入 ini | — | MES 扫码 |

**吸力卡控**：Hook 逻辑在步骤库；阈值优先 `[Station/{StationKey}/Suction]` 或 SETTINGS `[Suction]`（绑定当前 StationKey 后读取）。

---

## 九、迁移计划

| 阶段 | 内容 | 风险 |
|------|------|------|
| **P0** | `TestCasePaths` + `loadCaseForStation` + Profile 目录约定；`copyFlowStation` 创建/复制 `profiles/` | 低（兼容旧 ini） |
| **P1** | `flow.ini` 迁入 Profile；编排页只列 `steps/`；Profile 整包云端 API + 客户端 atomic replace | 中 |
| **P2** | 工站步骤参数 Tab；`[Station/{Key}/…]` SETTINGS；废弃平铺重复 ini | 中 |
| **P3** | 专用工站类下线；`SYSTEM/station` 仅 FREE_WORK；旧 test_case 全量包改 Steps+Profiles 分包 | 高（需产线验证） |

**迁移脚本（建议）**：

- 按 `MesTag + Send.Channel + Send.DeviceCmd + Hook.HookId` 指纹合并 → `steps/`。
- 各 `[Station/{Key}/Items]` 差异提取 → `profiles/{Key}/steps/`。
- 保留原 ini 至 `test_case/.backup/legacy/` 直至产线确认。

---

## 十、验收标准

1. 两个 Profile（`FLOW_ST_A` / `FLOW_ST_B`）共用同一 StepId「采集双通道吸力」，**参数互不影响**。
2. 编排页步骤池 **不出现** 同逻辑多文件名。
3. 云端下发 Profile 包后，本地 `flow.ini` 与 `steps/` **同时** 更新，无 orphan 旧参数文件。
4. Profile 同步失败时，本地目录可 **回滚 backup**，不出现半套配置。
5. 自由工站切换 `SelectedStation` 后，加载对应 Profile 流程与参数，**无需** 切换工站类型 Radio。
6. Steps 库包与 Profile 包 **可独立版本管理**，Profile manifest 可声明步骤库依赖。

---

## 十一、相关代码与文档索引

| 模块 | 路径 |
|------|------|
| 步骤存储/流程 | `platform/test_case/test_case.cpp` · `TestCaseStore` |
| 流程编排 UI | `platform/settings/test_flow/test_flow_editor.cpp` |
| 自由工站执行 | `work_station/freework/qfreework.cpp` · `qfreework_test_case.cpp` |
| 云端用例同步 | `platform/cloud/sync/test_case_sync_service.cpp` |
| 云端协议总览 | `docs/云端工厂平台功能设计.md` §4.3 |
| 原功能测试库需求 | `docs/功能测试库与测试流程编排需求.md` |

---

## 十二、修订记录

| 日期 | 说明 |
|------|------|
| 2026-07-03 | 初稿：FREE_WORK 唯一运行时、profiles/steps 目录、Profile 整目录云端替换 |
