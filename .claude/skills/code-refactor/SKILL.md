---
name: code-refactor
description: 代码重构。当用户说"重构 XX""把 XX 改名/重命名""批量重命名/修正命名""去匿名 namespace""抽个公共工具/进 CommonUtils""整理代码""迁移/删除/屏蔽模块""统一命名"等时使用。指导按本项目规范安全重构：先判断重构类型，再按类型走（批量重命名脚本 / 去匿名 namespace / 抽 CommonUtils / 删迁移模块），改完编译验证 + 编码换行 + 按工站宏提交。
---

# 代码重构

本项目重构有强约定（见 CLAUDE.md「改代码的原则 / 代码最小改动 / 禁止匿名 namespace / 测试卡控」等节），核心是**最小改动 + 规则落位 + 编译验证**。先判断类型，再动手，不要一上来就改。

## 0. 环境与读取注意

- **跑 `.py` 用 `py`，不是 `python`**：本机 `python`/`python3` 是 Windows Store 占位符（exit 49），`py` 是 Python 3.10.11（唯一可用）。`py scripts/convert_to_crlf.py <文件>` 能正常跑。
- **读源码优先 Bash `cat` / `sed -n '行号p'` / `grep`**：代码被公司透明加密，`Read` 工具对部分文件（如 `.ps1`）会返回乱码；git-bash 进程被加密软件放行，能读到明文（含中文注释）。
- 写 `.cpp/.h` 后执行 `py scripts/convert_to_crlf.py <文件>` 转 **CRLF + UTF-8 无 BOM**，并自动折叠多余连续空行。

## 1. 先判断重构类型（决定走哪条路）

| 重构类型 | 判据 | 落点 |
|---|---|---|
| ① 批量机械重命名/修正命名 | 统一命名（如 `refresh*` 虚槽）、修正历史拼写 | §2.1 一次性 `.py` 脚本 |
| ② 去匿名 namespace | `namespace { }` 包工具函数/常量 | §2.2 按 CLAUDE.md 落位 |
| ③ 抽公共工具 | 多处复用的纯函数 | §2.3 进 `CommonUtils` |
| ④ 删除/迁移/屏蔽模块 | 删文件、移库、屏蔽工站编译 | §2.4 改 `.pro` + `AbIni.h` 宏 |

> 原则：重构**只做被要求的**，不顺带重构无关代码（最小改动）；未碰到的存量问题（如没动到的匿名 namespace）不动。

## 2. 各类做法

### 2.1 批量机械重命名/修正命名

照 `scripts/批量重命名协议刷新接口.py`、`scripts/批量修正API拼写命名.py` 的既有模式写一次性脚本：

- 头部 `# -*- coding: utf-8 -*-`；`ROOT = Path(__file__).resolve().parents[1]`
- `SKIP_DIRS = {"Python39", ".git", "build", "lib", "agreement"}`（跳过第三方/生成/协议目录）
- `read_text(encoding="utf-8-sig")` / `write_text(encoding="utf-8-sig")`
- 遍历 `ROOT.rglob("*")`，只处理 `.h/.cpp`
- 替换：简单拼写用 `str.replace`；需上下文用 `re.sub`（参考「批量重命名协议刷新接口.py」里 `processInspection` 函数体那段正则，避免误伤）
- 跑完打印 changed 文件列表

**安全要点**：

- 替换前先 `grep -rn "旧名" work_station platform agreement common business --include=*.cpp --include=*.h` 数影响面，确认无误伤（尤其缩略名如 `getmacadress` 会撞子串）。
- 先小范围验证（只跑一个目录 / 先 dry-run）再全量。
- 用 `py scripts/xxx.py` 执行（不是 `python`）。
- 一次性脚本用完留在 `scripts/`（与现有批量脚本一致），文件头注明用途。

### 2.2 去匿名 namespace

按 CLAUDE.md「禁止匿名 namespace」的落位表：

| 性质 | 落位 |
|---|---|
| 多处可复用纯工具 | `CommonUtils::` |
| 仅某一类用 | 该类 `private` / `private static constexpr` |
| 仅本 `.cpp` 用、不可复用 | 文件作用域 `static` 函数/常量（**不要再包匿名 namespace**） |

- 具名 namespace（如 `DongleCmdManifest`）**保留**，不要为「去 namespace」整仓拆。
- 禁止：为躲冲突再套 `namespace detail` / `helpers` 等无业务语义命名空间。
- 禁止：用脚本全仓「去壳 + 盲加 static」（会误伤构造函数初始化列表 `: Base(...)`）。
- 存量匿名 namespace（全仓约 60+ 处）：未改到的文件不动，本次改动碰到时才改。

### 2.3 抽公共工具进 CommonUtils

- 文件：`common/common_utils.h` + `common/common_utils.cpp`
- 保持分区注释：`// --- 字节 ---`、`// --- 时间 ---`、`// --- 文件 ---`、`// --- 字符串 ---`
- 全 `static` 方法，声明与实现同步
- 写后 `py scripts/convert_to_crlf.py common/common_utils.h common/common_utils.cpp`

### 2.4 删除/迁移/屏蔽模块

- 删源文件 / 移库 → 改 `new_production.pro`（增删 `SOURCES`/`HEADERS`）→ 需**完整编译**（去掉 `-SkipQmake`）
- 屏蔽工站编译 → 用 `my_set/AbIni.h` 的宏开关，**不直接删代码**（参考历史提交「进一步屏蔽旧的工站的编译…在 abini 里用宏定义做开关」）

## 3. 安全网（改完必做）

1. **编码换行**：`py scripts/convert_to_crlf.py <本次改动文件>`（CRLF + UTF-8 无 BOM + 折叠多余空行）
2. **空行检查**：`py scripts/convert_to_crlf.py --check-blank-lines <文件>`（exit 1 表示仍有多余空行）
3. **编译验证**：`scripts\编译Release版本.ps1 -SkipQmake`（仅 `.cpp/.h/.ui` 改动时；改了 `.pro` / 新增删除源文件时去掉 `-SkipQmake` 完整跑）
4. **格式化**（改动量大时）：`scripts\格式化代码.ps1`（基于 clang-format + 根目录 `.clang-format`；`.ps1` 用 Read 会乱码，直接 PowerShell 跑即可）

## 4. 提交信息

按 CLAUDE.md「Git 提交信息」节：

- 先看变更文件路径：全部在单个 `work_station/<子目录>/` 且未混改公共模块 → 只写该工站一个宏。
- 工站 + 公共模块同一次提交 → 分别写宏（如 `[XXX_VER]` 与 `[FREE_VER]`，可两行或同行走 `；` 分隔）。
- 宏名与 `my_set/AbIni.h` 里 `#define XXX_VER` 完全一致；说明须简体中文、与 diff 一致。
- `[AGREEMENT_VER]` 禁用（AbIni.h 无此宏）。
- 注意：`.copilot-commit-message-instructions.md` 已不在仓库，以 CLAUDE.md 该节 + `AbIni.h` 为准。

## 5. 输出前自查

- 类型判断对；只改了被要求的，没顺带重构无关代码。
- 批量重命名：影响面 `grep` 过、无误伤、用 `py` 跑、脚本留 `scripts/` 且注明用途。
- 去匿名 namespace：落位对（CommonUtils / static / 类成员），没套新 namespace，没误伤构造初始化列表。
- 抽 CommonUtils：分区注释对、声明实现同步、UTF-8 无 BOM + CRLF。
- 删迁移：`.pro` 改对、用宏开关而非删代码。
- 编译 `-SkipQmake` 通过；编码换行 + 空行检查跑过。
- 提交信息宏名与 `AbIni.h` 一致、简体中文、与 diff 一致。
