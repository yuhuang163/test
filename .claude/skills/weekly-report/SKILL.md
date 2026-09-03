---
name: weekly-report
description: 生成周报。当用户说"写周报""生成周报""这周/本周做了什么""总结本周工作""周报""weekly report"等时使用。读取过去一周（默认最近 7 天，或本周一至今）多个仓库的 git 提交（默认含 fwq 数据平台），按工站/功能/作者归类，生成带时间范围的简体中文周报并写入 .md 文件。
---

# 周报生成

根据过去一周的 git 提交，生成简体中文周报要点列表。本目录自带 `weekly.sh` 脚本，把「算日期 + 多仓库拉提交 + 按路径粗归类」一次做完，**优先用它**，读紧凑结果后成文。

## 1. 确定时间范围

- 默认：**最近 7 天**（今天往前推 7 天）。用户说「本周/这周」时改用**本周一 00:00 至今**。
- 用户给出具体起止日期时，用指定范围。
- 时间范围交给脚本算，不要自己手算或默认拉取很久的历史。

## 2. 拉取提交（优先用脚本）

直接跑本目录脚本，输出「每条提交一行（含仓库/作者/类别标签）」+「按类别聚合」两段紧凑结果：

```bash
# 默认：本周、本人、上位机(new_product_test) + 数据平台(../fwq) 双仓库
bash .claude/skills/weekly-report/weekly.sh --this-week

# 本人 + 黄雨豪（--author 是追加，默认已含本人）
bash .claude/skills/weekly-report/weekly.sh --this-week --author 黄雨豪

# 全部人
bash .claude/skills/weekly-report/weekly.sh --this-week --all-authors

# 指定区间 / 只看某仓库
bash .claude/skills/weekly-report/weekly.sh --since 2026-08-27 --until 2026-09-04
bash .claude/skills/weekly-report/weekly.sh --this-week --repo d:/code/fwq
```

脚本关键行为：
- 默认扫描**当前仓库 + `../fwq`**（若存在且是 git 仓库）。`fwq` 是工厂数据平台，其提交统一归为「数据平台/服务器」。
- 用 `--all` 覆盖所有分支：上位机工作常散在 feature 分支（`feature/m9`、`feature/m10-Lite组装厂`、`feature/云服务器功能开发`、`feature/bes蓝牙校准` 等），**可能未合入 develop**，只看当前分支会漏。
- `--no-merges` 去掉合并噪音；`-c core.quotepath=false` 让中文路径直接可读。
- 作者是子串匹配：默认本人取 `user.name` 并去掉机器后缀（「何宇杰台式机」→「何宇杰」，同时命中「何宇杰」）；`--author 黄雨豪` 表示**本人 + 黄雨豪**（多作者用 `\|` 取并集）。
- 每行格式：`[仓库] 日期 | 作者 | 提交说明 | hash [类别...]`。

脚本不可用、或需要看单个提交改了什么时，用手动命令兜底（多作者用 `\|` 分隔）：

```bash
git -c core.quotepath=false log --all --no-merges \
  --since="<起始>" --until="<结束>" --author="<姓名1>\|<姓名2>" \
  --pretty=format:"=== %h | %ad | %an | %s" --date=format:"%Y-%m-%d" --name-only
git show --stat --oneline <hash>   # 看单个提交
```

## 3. 按工站/功能归类

脚本已按路径给出粗归类标签，此处只做**精修与合并**（同一功能的多条提交合成一条要点，不要逐条罗列 commit）。目录 → 工站/功能对照（与脚本一致）：

| 改动路径 | 归类 |
|---|---|
| `work_station/freework`、`business/tuple`、`*tuple*` | 三元组 / 自由工站 |
| `work_station/screen`、`platform/debug/screen_inspect*` | 屏幕测试工站 |
| `work_station/ageing` | 老化测试工站 |
| `work_station/camera` | 摄像测试工站 |
| `work_station/imu` | IMU 校准工站 |
| `work_station/key` | 按键测试工站 |
| `work_station/motor` | 电机校准工站 |
| `work_station/pcba` | 产品板子测试 |
| `work_station/pressure` | 压感校测工站 |
| `work_station/quiescent_current` | 静态电流测试 |
| `work_station/suction` | 吸力测试工站 |
| `work_station/wifi_ble` | 信号 / 蓝牙相关 |
| `agreement/` | 协议层 |
| `platform/cloud/`、`agreement/mes_protocol/`、`*ota*` | 云服务器 / 上报 |
| `platform/settings/qsetting*`、`my_set/` | 设置 / 配置 |
| `mainwindow`、产品名相关 | 产品主窗口 / 整体 |
| `factory-admin`、`factory-api`（fwq 仓库） | 数据平台 / 服务器 |

- 分支名也常直接暗示功能：`feature/m9`、`feature/m10-Lite组装厂`、`feature/云服务器功能开发`、`feature/bes蓝牙校准` 等，归类时优先参考。
- 提交信息与路径都可能误导（例如叫「新增claudecode的规则」的提交实际也改了 freework），最终以**改动文件路径**为准。

## 4. 输出：写 .md 文件 + 写时间范围

- **必须写入 `.md` 文件**，不要只在对话里贴文本。默认文件名 `周报_<起始>_<结束>.md`（如 `周报_2026-08-31_2026-09-03.md`），默认放到 `docs/周报/` 目录（目录不存在则先创建）；用户指定位置则按其放。
- **文件内必须写明时间范围**：标题下首行写 `时间范围：YYYY-MM-DD ~ YYYY-MM-DD`，起止以脚本输出的范围为准（范围显示 `~ 今天` 时，把「今天」替换成当天日期）。
- 文件按仓库 `.md` 约定保存：**UTF-8 无 BOM + CRLF**。
- 正文简体中文，每条一行简短短语，动宾或名词短语即可。风格示例：
  - m9与m10协议自测上位机
  - 三元组sn清空工站
  - 屏幕测试优化
  - w1lite名字替换
  - 完善w1 lite的自动扫描工站
  - 安工泵阀调试功能需求
  - 服务器新增软报错警告
- 只写**实际有提交支撑**的工作项；没有提交的不写、不编造。
- 一条要点可涵盖同功能下多个提交，但要点之间不重叠。
- 若周报含多名作者（本人 + 同事），**按人分节或标注归属**，不要把别人的活写成自己的。
- 如用户需要，可在要点后附一句简短说明（改了哪、达到什么效果），保持精简。

## 5. 输出前自查

- 时间范围对不对（最近 7 天 / 本周 / 指定区间），且**已写进 .md 文件**。
- 是否覆盖了 fwq 数据平台、以及未合入 develop 的 feature 分支（脚本已用 `--all` 处理）。
- 是否按需包含了同事（黄雨豪等）的提交，且归属没写混。
- 每条要点都有提交依据，无凭空捏造。
- 结果是否写入 `.md` 文件（UTF-8 无 BOM + CRLF），而非只贴在对话里。
- 语言为简体中文，无整段英文说明。
