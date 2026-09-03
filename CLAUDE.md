# new_product_test — 工程约定与 AI 协作规则

> 由 Cursor 规则（`.cursor/rules/*.mdc`）迁移而来，供 Claude Code 使用。原 `.cursor/rules/` 保留不变。

## 环境注意事项（Claude Code 专用）

- **`python` 是 Windows Store 占位符**（`python -c ...` 退出码 49、无输出），跑不了 `scripts/convert_to_crlf.py`；需要转 CRLF 时改用 `perl -pi -e 's/\r?\n/\r\n/g' <文件>`。

## 技术栈

## 上下文防遗忘与长对话机制（强制执行）
- **长上下文检索**：因为系统可能折叠早期的对话历史，当涉及**以前修改过的文件配置、网络 IP/端口设置、或者早期的用户明确要求**时，**严禁**单凭摘要猜测。
- **强制使用日志**：遇到疑问或用户反馈“联系错前后文”时，必须主动使用工具读取 `.system_generated/logs/transcript.jsonl`（包含全部完整的历史对话日志），精准还原上文细节和用户最初贴出的完整内容。

- **Qt 5.15**，**MSVC**，**C++17**，工程入口：`new_production.pro`（`CONFIG += c++17`，编译选项含 `/utf-8`）。
- **文本编码**：除第三方或未改动的存量文件外，本项目新建或修改的 **`.cpp` / `.h` / `.hpp` / `.ui` / `.pro` / `.qrc` / `.md` / `.ini` 配置片段等** 一律使用 **UTF-8（无 BOM）** 保存；工程已配置 `/utf-8` 编译选项，无需 BOM。已有文件若非任务要求不要随意改编码。
- **换行**：`.gitattributes` 为 `* -text`，**仓库内文本默认 CRLF 原样入库**；**仅根目录 `new_production.pro` 固定 LF**（qmake 入口）；`*.sh` 保持 LF。`python scripts/convert_to_crlf.py` **无参数时只检查 `new_production.pro`**，不批量改其它文件；Agent 改动其它文本后对该文件执行 `python scripts/convert_to_crlf.py <路径>` 转 CRLF。
- **Agent 写文件后必做**：改动 **`.cpp` / `.h` / `.ui` / `.ini` 等** 后对**本次改动的文件**执行 `python scripts/convert_to_crlf.py <路径>`（CRLF + UTF-8 无 BOM，并**自动折叠 C/C++ 中连续空行 >1**）。改动 **`new_production.pro`** 时执行 `python scripts/convert_to_crlf.py new_production.pro`（LF + UTF-8 无 BOM）。**禁止**批量改编码/换行导致中文乱码。
- **空行**：`.cpp` / `.h` 等与邻文件一致——**禁止「每行代码后多空一行」**；块与块之间最多 **1** 行空行。检查：`python scripts/convert_to_crlf.py --check-blank-lines <路径>`（exit 1 表示仍有多余空行）；`convert_to_crlf.py <路径>` 写入时会自动修正。
- 配置多用 **`SETTINGS`（Ini）**；协议与界面分层：`agreement/`、`work_station/` 等，改动前先对照同目录现有写法。

## 代码风格（与仓库一致）

- **配置**：统一通过 **`SETTINGS`（`Abini.h` / Ini）** 读写；键名已有惯例如 `BLE/LowRssi`、`BLE/HighRssi`、`Current/LowCharCurrent`、`Current/HighCharCurrent`、`BATTARY/standbattary`、`TestOrderMeta/SelectedStation` 等，新增项沿用 **`分类/键名`**，并在 **`platform/settings/qsetting`** 做加载/保存/UI（避免「只写代码、设置页缺项」）。
- **测试结果**：继承 **`test_base`** 的工站使用 **`passValue` / `failValue`**（中文「通过」「失败」）、**`TestResult`** 与 **`showlog`**；表格分项用 **`TestItem` + `testResultTableUpdate`**。与用户可见结论用 **`showlog`**，调试细节可用 **`qDebug()`**。
- **字符串比对**：期望值与实测比较优先 **`compareVersions`**（与现有关键键测等逻辑一致），不要随意换成裸 `==` 除非周边代码如此。
- **头文件 / Qt**：保持与邻文件一致的包含风格（工程内多为 **`#include "..."`**，Qt 模块多为小写 **`#include <qxxx.h>`** 形式）。
- **代码整齐性**：对齐同一类语句的换行与缩进风格（尤其是 `if / else if` 条件链）；尽量使用单行条件，必要多行时保证括号配对且 `&& / ||` 的换行位置与邻近代码一致，避免把 `&&` 留到括号外导致语法错误。
- **避免无意义混用**：除非周边代码/性能/语义明确需要，否则不要引入与文件风格不一致的包装（例如 `QLatin1String`）。这类“没必要的转换/换写”应删除并保持同文件一致写法。
- **禁止为单调用点拆函数**：一段逻辑若**仅在一处**使用（如 `switch` 某分支收满后的分发、`if/else if` 链的收尾），**直接写在调用处**，**不要**再抽 `onFrameComplete()`、`handleXxx()` 等只为「看起来整齐」的私有成员或文件内函数；`resetState()` 等多处复用的状态重置、确有**多处调用**或能明显缩短大函数/去重复时再封装（与下文「改代码的原则」一致）。
- **界面样式（QSS）**：可复用的 `setStyleSheet` / 大段 Qt 样式规则一律写在 **`stytle/qss/`**（主入口 **`Ubuntu.qss`**，按 `#objectName` 或控件类型选择器组织）；通过 **`applyWidgetStyleSheet` / `updateMainStyle`** 加载。**禁止**在 **`.cpp`** 里硬编码长串 QSS（仅允许单行、与运行时数据强绑定的动态片段，且须注释说明原因）。`.ui` 里尽量只设 **`objectName`**，样式交给 QSS。
- **批量格式化**：全仓 C/C++ 统一用 **`scripts\格式化代码.ps1`**（基于 Qt Creator 自带 `clang-format` + 根目录 `.clang-format`）；脚本会自动排除 `advance/xlsx`、`lib/qcustomplot`、生成 pb 等第三方/生成目录，并在末尾确保文本为 **UTF-8（无 BOM）**。
- **查找表/常量矩阵**：CRC 表、256 项查表数组等**手工对齐的常量块**须用 `// clang-format off` … `// clang-format on` 包裹，避免被格式化成逐行一项；新增同类表沿用此约定。
- **test_base 控件桥接**：工站头文件里 `override { return ui->xxx; }` 的单行桥接块同样用 `// clang-format off` … `on` 包裹，放在 `public:` 区，避免全仓格式化拆成多行。

## 测试卡控（阈值与判定）

- **阈值来源**：卡控上下限应从 **`SETTINGS`** 读取（构造函数或步骤开始前缓存到成员变量），与 **`qsetting::loadConfig` / `saveConfig`** 同源；不要硬编码生产阈值。
- **异步步骤**：在 **`refresh*` / 协议回调**里判定卡控时，必须 **`isCurrentStep("步骤中文名")`** 守卫，避免串步骤误判定。
- **回填状态机**：判定结束后设置 **`stepRuntime_.done = true`**、**`stepRuntime_.pass`**，失败时 **`TestResult = failValue`**；日志建议带「卡控通过/失败、当前值、允许范围」，与现有 RSSI/电流等句式一致。
- **边界含义**：新建卡控时与同类步骤保持一致（例如 RSSI 现为 **`>` / `<` 非 `>=`**）；若改边界语义须在注释与设置说明里写清。
- **`sendCommandWithRetry`**：仅表示「发指令并等到设备回包」；第三参 **`allowResend`** 只控制超时窗口内是否重发，**不是**结案开关。步骤是否必须异步结案看 **`TestCaseRunner::needAsyncDone`**（流程里常叫 `needCaseDone`/`needAsyncDone`）：为 false 时默认不会因协议 FAIL 自动 **`pass = false`**；卡控类步骤应为 true，并在回调里写入 **`stepRuntime_.pass`**，或用显式预检/单独判定（参见自由工站三元组写入）。

## 编译验证

- 修改 **`.cpp` / `.h` / `.pro` / `.ui`** 等需编译项后，Agent 应在终端执行 **`scripts\编译Release版本.ps1`**（或 `scripts\编译Release版本.bat`）做 **Release 编译**；失败则根据输出中的 `error C` / `LNK` 修复后重跑，直至通过或确认环境缺失。
- **默认增量、禁止无谓全量**：日常验证**一律优先** `scripts\编译Release版本.ps1 -SkipQmake`（跳过 qmake，只增量编译改动目标）。**不要**每次默认跑完整 qmake 全量编译。
- **与 Qt Creator 共用同一 Release 目录**：`build/Desktop_Qt_5_15_2_MSVC2019_64bit-Release`。脚本 qmake 参数须与 Creator Effective qmake 一致（当前为 `-spec win32-msvc "CONFIG+=qtquickcompiler"`），避免 Creator 点运行因 Makefile 参数不匹配再全量编一次。
- **仅在必要时去掉 `-SkipQmake`（完整跑）**：改了 **`.pro`**、**新增/删除源文件或资源**、Makefile 缺失/损坏、或 `-SkipQmake` 后出现与工程文件列表/生成规则明显相关的编译失败。仅改 `.cpp` / `.h` / `.ui`（已有对应 Makefile 规则时，jom 仍会按依赖跑 `uic`/`moc`）→ **只用 `-SkipQmake`**。
- 完整日志：`build/logs/build_*.log`；可通过环境变量覆盖工具路径：`NEW_PRODUCT_QT_DIR`、`NEW_PRODUCT_JOM`、`NEW_PRODUCT_VCVARS`。

## 改代码的原则

- **只改与任务直接相关的文件与逻辑**，避免顺带重构、删注释或扩大范围；细则见下文「代码最小改动」节。
- **命名、类型、错误处理、日志风格与周边代码保持一致**；优先复用已有函数与模式。
- **调用点少则不单独抽函数**：一段逻辑若**仅在一处**使用（或极少处且无复用/单测收益），**不要**仅为「整洁」再封装一层独立函数；写在调用处附近即可。确有**多处调用**、或抽离能明显缩短大函数、降低重复时再封装。
- UI（`.ui`）改完后需在本地用 Qt Creator / `uic` 重新生成并编译验证。
- **HTTPS（QSslSocket）**：Windows 部署需注意 OpenSSL DLL；与三元组/网络相关的改动要考虑 TLS 与 `Tuple/BaseUrl` 配置。

## 代码最小改动

先定位再改；能改 1 处就不要改 3 处。用户没要求的列、字段、日志、开关、文档一律不加。

### 必须

- 只改与当前问题直接相关的文件与语句；不顺带重构、不改无关注释、不扩大 API。
- 用户要「只留一个」时：CSV / 曲线 / Label 只保留那一个数据源，不要同时写出对照列（如 `host_us` + `dongle_ms`）。
- 两种协议（如 AT / 十六进制）分时互斥时：用已有字段分支（如 `dongleTimestampMs >= 0`），不要为对照再加一套时间轴。
- 调用点少则不抽函数；临时 `qDebug` 定位完即删，不要留进正式路径。

### 禁止

- 为「方便以后排查」默认加调试 CSV 列、双时间戳、双包间隔显示。
- 用户未要求时改 `docs/`、协议说明、Tooltip 长文。
- 修 A 时顺手改 B 的封装/命名/格式（除非不改无法编译）。

### 反例（吸力曲线）

用户只要曲线不竖切 → 改横轴用哪个时间即可。不要先加 `host_us,dongle_ms,dongle_dt_ms` 三列再让用户删回去。

## 禁止匿名 namespace；工具进 CommonUtils

### 禁止

- 新建 **`namespace { ... }`（匿名命名空间）** 包工具函数、常量、小助手。
- 为「躲冲突」再套无业务语义的命名空间（如 `namespace detail` / `helpers`）。
- 在业务 `.cpp` 里堆可复用静态小函数却不进公共工具。

### 允许 / 应保留

- **具名**命名空间（如 `namespace DongleCmdManifest`、`namespace Asd9026aCodec`）——与现有清单/编解码模块一致，**不要**为「去 namespace」整仓拆掉。
- Qt / 第三方已有命名空间。

### 应该怎么放

| 性质 | 放哪里 |
|------|--------|
| 多处可复用纯工具 | **`CommonUtils::`**（`common/common_utils.h` / `.cpp`） |
| 仅某一类用 | 该类 `private` / `private static constexpr` |
| 仅本 .cpp 用、不可复用 | 文件作用域 **`static`** 函数/常量（**不要**再包匿名 namespace） |

### 注意

- **存量**匿名命名空间：未改到的文件不必为合规整文件搬迁；**本次改动碰到**时删掉匿名命名空间并按上表落位。
- 往 `CommonUtils` 加方法时保持现有分区注释，声明与实现同步，UTF-8 无 BOM + CRLF。
- **禁止**用脚本对全仓匿名命名空间做「去壳 + 盲加 static」（易误伤构造函数初始化列表 `: Base(...)`）。

## 问题反馈与日志排查

- 用户反馈“报错 / 失败 / 异常 / 卡住 / 不通过 / 日志里有”等运行问题时，优先查看最新上位机日志：`build/Desktop_Qt_5_15_2_MSVC2019_64bit-Release/bin/所有log/上位机log/`。
- 若用户指定了具体日志文件，先读该文件；否则按修改时间读取该目录下最新的 1-3 个 `*.txt`，结合用户描述判断当前执行到的步骤、工位、报错栈和最近一次 `showlog/qDebug` 内容。
- 日志排查要先复述关键日志事实，再定位相关代码；不要只凭报错文字猜测。读取日志时避免一次性展开大量历史日志，必要时再扩大范围到设备 log、dongle log 或对应工站代码。

## 语言与注释

- **必须使用简体中文**回复用户（说明、总结、提交描述、聊天中的解释与步骤）。
- **关键处补充中文注释**：非一目了然的逻辑、状态机/异步边界、协议与 UI 的隐含约定、容易误判的分支（例如「为何跳过发送」「为何阻塞信号」）等，用**简短中文**写在代码里。
- 注释风格与当前文件一致；**不写**赘述型注释（重复代码字面含义）或大块教案式注释。

## Git 提交信息（按工站标签）

编写或生成 **Git 提交说明**时，**以仓库根目录 `.copilot-commit-message-instructions.md` 为准**（与 Copilot 提交生成共用）；本节只列要点，避免与上文冲突。

**必遵守的判定顺序**：

1. 先看变更文件路径：若**全部**只在**某一个** `work_station/<子目录>/` 下，且**未**混改 `agreement/`、`test_base.cpp`、`box_base.cpp`、`mainwindow` 等 → 提交信息只写该子目录对应的一个宏标签。具体路径与宏标签不要在本规则维护，提交前以 `.copilot-commit-message-instructions.md` 和 `my_set/AbIni.h` 为准。
2. **说明文字必须与 diff 一致**：不得写未改动模块（见 `.copilot-commit-message-instructions.md`「必须与 diff 一致」节）。
3. **工站 + 公共模块同一次提交**：不得把公共改动只写在工站宏的说明文字里却不出现另一宏；须按 `.copilot-commit-message-instructions.md` **分别写出**对应块（如工站 + 三元组 → **`[XXX_VER]`** 与 **`[FREE_VER]`**；可两行，或**同一行**用 **`；`** 分隔；主窗口相关 → **`[DEBUG_VER]`** 等）。
4. 公共模块、多工站、仅主窗口/文档等情形，见 **`.copilot-commit-message-instructions.md`** 全文。
5. **`[AGREEMENT_VER]` 禁止使用**：`AbIni.h` 无此宏；`agreement/qtuple` 类改动用 **`[FREE_VER]`**（勿用 `DEBUG_VER` 概括三元组），或按公共模块多行规则（详见该文档专节）。

**格式**（可多行时：每行一个本次实际涉及的工站或规则要求的行）：

```text
[XXX_VER] 改动要点1，改动要点2，改动要点3
```

- **`XXX_VER`** 与 `my_set/AbIni.h` 中 **`#define XXX_VER`** 完全一致；描述**须简体中文**（禁止整段英文提交说明），逗号分隔。

**GitHub Copilot**：`github.copilot.chat.commitMessageGeneration.instructions` 指向 **`.copilot-commit-message-instructions.md`**。

## 文档、映射表与输出

- **不要**未经任务明确要求就**新建文件**（含单独 `.md`、`.ts`、`.h` 等）来存放「参数大全」「操作码与中文对照」等；**本规则文件不收录**具体键值清单全文，清单以**代码里单一出处**为准。
- **三元组上报 / MES / 日志**等场景下的 **操作码或字段 → 人可读中文名**：在**已有**协议、上报或工站模块内就地维护（与枚举、`switch`、`QMap`/常量表等现有写法一致），**禁止**为对照表再拆一个「大全」文件。
- **不要**未经要求新建或大范围改写 `docs/`、`README` 等说明文档。

## 协作提示

- **提交说明**：按「Git 提交信息」节与 **`.copilot-commit-message-instructions.md`**；单工站改动通常 **一行** `[XXX_VER]`；**工站 + 三元组**同提交须**分别出现** **`[XXX_VER]`** 与 **`[FREE_VER]`**（可两行或同一行用 **`；`** 分隔）；**工站 + 主窗口**用 **`[DEBUG_VER]`**；勿把不同模块要点塞进**同一个** `[XXX_VER]` 说明却不写应有宏。
- 涉及工站流程（自由工站、设置页 `qsetting`、协议 `qfctp`）时，注意步骤状态机、`stepRuntime_`、`needAsyncDone`（勿与 `allowResend` 混淆）与异步回调的一致性，避免「有回包即判通过」类误判。
- **检索**：全仓 **`grep` / 语义搜索** 时避开 **`agreement/factory_protocol/protocol/qpb/Python39/`**（内置 Python，与业务无关）；仓库根目录 **`.cursorignore`** 已忽略该路径（供 Cursor 索引，Claude Code 侧同样应在搜索时手动排除）。Agent 搜索优先限定 **`work_station/`**、**`platform/settings/`**、**`agreement/factory_protocol/`**、**`agreement/modbus/`**（不含 Python39）等。
