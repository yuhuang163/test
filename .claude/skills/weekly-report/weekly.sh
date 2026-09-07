#!/usr/bin/env bash
# 周报辅助脚本：拉取时间段内多个仓库的 git 提交，按改动路径粗归类，输出紧凑结果，供 weekly-report skill 快速成文。
# 依赖：git、GNU date（git-bash 自带）、awk。

set -u

EXTRA_AUTHORS=()           # --author 追加的作者
AUTHORS=()                 # 最终作者列表（默认本人 + 追加）
USE_AUTHOR=1               # 1=按作者过滤（默认只本人）
DAYS=7
MODE="days"
SINCE=""
UNTIL=""
REPOS=()                   # 仓库路径列表
REPO_GIVEN=0

usage() {
  cat <<'EOF'
用法: bash weekly.sh [选项]

  --author NAME      追加作者 NAME（可多次；子串匹配，默认已含本人）
  --all-authors      包含所有人（不按作者过滤）
  --repo PATH        只扫描指定仓库（可多次；默认扫描当前仓库 + ../fwq）
  --days N           最近 N 天（默认 7）
  --this-week        本周一 00:00 至今
  --since YYYY-MM-DD 指定起始日期
  --until YYYY-MM-DD 指定结束日期
  -h, --help         显示本帮助

示例:
  bash weekly.sh --this-week                       # 本周、本人、上位机 + 数据平台
  bash weekly.sh --this-week --author 黄雨豪        # 本人 + 黄雨豪
  bash weekly.sh --this-week --all-authors          # 全部人
  bash weekly.sh --since 2026-08-27 --repo d:/code/fwq
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --author) EXTRA_AUTHORS+=("$2"); shift 2 ;;
    --all-authors) USE_AUTHOR=0; shift ;;
    --repo) REPOS+=("$2"); REPO_GIVEN=1; shift 2 ;;
    --days) DAYS="$2"; MODE="days"; shift 2 ;;
    --this-week) MODE="this-week"; shift ;;
    --since) SINCE="$2"; shift 2 ;;
    --until) UNTIL="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "未知参数: $1" >&2; usage >&2; exit 2 ;;
  esac
done

# ---- 默认仓库：当前仓库 + ../fwq（若存在且是 git 仓库） ----
if [[ "$REPO_GIVEN" == "0" ]]; then
  REPOS+=(".")
  if git -C "../fwq" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    REPOS+=("../fwq")
  fi
fi

# ---- 作者列表：默认本人（去掉常见机器后缀以匹配同人多机身份）+ --author 追加 ----
if [[ "$USE_AUTHOR" == "1" ]]; then
  me="$(git config user.name 2>/dev/null || echo '')"
  me="${me%台式机}"; me="${me%笔记本}"; me="${me%电脑}"
  [[ -n "$me" ]] && AUTHORS+=("$me")
  AUTHORS+=("${EXTRA_AUTHORS[@]}")
fi

# 组装 --author 正则：多个作者用 BRE 的 \| 取并集
AUTHOR_REGEX=""
for a in "${AUTHORS[@]}"; do
  [[ -n "$AUTHOR_REGEX" ]] && AUTHOR_REGEX+="\\|"
  AUTHOR_REGEX+="$a"
done

# ---- 计算时间范围 ----
if [[ "$MODE" == "this-week" ]]; then
  dow=$(date +%u)                      # 1=周一 ... 7=周日
  SINCE="$(date -d "$((dow - 1)) days ago" +%Y-%m-%d)"
  UNTIL=""
elif [[ "$MODE" == "days" ]]; then
  SINCE="$(date -d "$DAYS days ago" +%Y-%m-%d)"
  UNTIL=""
fi

echo "范围: ${SINCE:-?} ~ ${UNTIL:-今天} | 作者: $([ "$USE_AUTHOR" = 1 ] && echo "${AUTHOR_REGEX:-?}" || echo '全部') | 仓库: ${REPOS[*]}" >&2
echo "----------------------------------------" >&2

for repo in "${REPOS[@]}"; do
  [[ -d "$repo" ]] || { echo "警告: 仓库 $repo 不存在，跳过" >&2; continue; }
  git -C "$repo" rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "警告: $repo 不是 git 仓库，跳过" >&2; continue; }

  label="$(basename "$(cd "$repo" && pwd)")"

  # 仓库 → 周报归类：fwq 是数据平台；上位机仓库交给路径细分类
  case "$label" in
    fwq|fwq-deploy*) repocat="数据平台/服务器" ;;
    new_product_test) repocat="" ;;
    *) repocat="$label" ;;
  esac

  ARGS=(git -C "$repo" -c core.quotepath=false log --all --no-merges
        "--pretty=format:===%x09%h%x09%ad%x09%an%x09%s"
        "--date=format:%Y-%m-%d" --name-only)
  [[ -n "$SINCE" ]] && ARGS+=(--since="$SINCE")
  [[ -n "$UNTIL" ]] && ARGS+=(--until="$UNTIL")
  [[ -n "$AUTHOR_REGEX" ]] && ARGS+=(--author="$AUTHOR_REGEX")

  "${ARGS[@]}" | awk -v REPO="$label" -v REPOCAT="$repocat" '
function classify(p) {
  gsub(/^[ \t"]+/, "", p)
  gsub(/[ \t"]+$/, "", p)
  if      (p ~ /freework|tuple|三元组/)                 return "三元组/自由工站"
  else if (p ~ /screen_inspect|work_station\/screen/)   return "屏幕测试"
  else if (p ~ /work_station\/ageing/)                  return "老化"
  else if (p ~ /work_station\/camera/)                  return "摄像"
  else if (p ~ /work_station\/imu/)                     return "IMU校准"
  else if (p ~ /work_station\/key/)                     return "按键"
  else if (p ~ /work_station\/motor/)                   return "电机校准"
  else if (p ~ /work_station\/pcba/)                    return "PCBA板测"
  else if (p ~ /work_station\/pressure/)                return "压感"
  else if (p ~ /work_station\/quiescent_current/)       return "静态电流"
  else if (p ~ /work_station\/suction/)                 return "吸力"
  else if (p ~ /work_station\/wifi_ble/)                return "信号/蓝牙"
  else if (p ~ /platform\/cloud\/|mes_protocol|ota/)    return "云服务器/上报"
  else if (p ~ /agreement\//)                           return "协议层"
  else if (p ~ /qsetting|my_set\//)                     return "设置/配置"
  else if (p ~ /mainwindow/)                            return "主窗口"
  else if (p ~ /test_case\/profiles/)                   return "测试用例"
  else if (p ~ /test_base|box_base/)                    return "工站基类"
  return ""
}

function addagg(cat, s) {
  key = cat SUBSEP s
  if (key in aggkey) return
  aggkey[key] = 1
  if (!(cat in agg)) { agg[cat] = ""; ncat++ }
  agg[cat] = agg[cat] (agg[cat] ? "；" : "") s
}

function flush() {
  printf "[%s] %s | %s | %s | %s", REPO, date, author, subj, hash
  if (cats != "") printf "  [%s]", cats
  printf "\n"
  if (cats == "") addagg("其他", subj)
  else {
    n = split(cats, arr, ",")
    for (i = 1; i <= n; i++) addagg(arr[i], subj)
  }
}

BEGIN { subj = "" }
/^===/ {
  if (subj != "") flush()
  split($0, f, "\t")
  hash = f[2]; date = f[3]; author = f[4]; subj = f[5]
  cats = ""; delete seen
  next
}
NF > 0 {
  c = classify($0)
  if (c == "" && REPOCAT != "") c = REPOCAT
  if (c != "" && !(c in seen)) { seen[c] = 1; cats = cats (cats ? "," : "") c }
}
END {
  if (subj != "") flush()
  if (ncat > 0) {
    printf "\n===== 按类别聚合 =====\n"
    for (cat in agg) printf "[%s] %s\n", cat, agg[cat]
  }
}
'
done
