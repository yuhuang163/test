# -*- coding: utf-8 -*-
"""
校验提交说明是否满足 docs/生成版本说明.py 的解析约定（与 .copilot-commit-message-instructions.md 对齐）。

用法：
  Git commit-msg 钩子：传入 Git 写入的提交说明文件路径
    py docs/validate_commit_message.py .git/COMMIT_EDITMSG

  手动检查暂存区与某段说明：
    py docs/validate_commit_message.py --message-file FILE

  跳过（紧急）：环境变量 SKIP_COMMIT_MSG_VALIDATE=1

退出码：0 通过；1 不通过（stderr 打印原因）
"""
from __future__ import annotations

import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def _repo_root() -> str:
    try:
        r = subprocess.run(
            ["git", "-C", SCRIPT_DIR, "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
            encoding="utf-8",
        )
        return r.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return os.path.dirname(SCRIPT_DIR)


def parse_abini_ver_tags(header_path: str) -> set[str]:
    """与 docs/生成版本说明.py 中 parse_header_file 一致：只认 #define XXX_VER \"…\" """
    tags: set[str] = set()
    pat = re.compile(r'^#define\s+(\w+VER)\s+"')
    with open(header_path, "r", encoding="utf-8") as f:
        for line in f:
            m = pat.match(line.strip())
            if m:
                tags.add(m.group(1))
    return tags


def staged_paths(repo: str) -> list[str]:
    try:
        r = subprocess.run(
            ["git", "-C", repo, "diff", "--cached", "--name-only", "-z"],
            capture_output=True,
            check=True,
        )
        if not r.stdout:
            return []
        # -z：路径以 NUL 分隔，末尾可能多一个空串
        return [p.decode("utf-8", errors="replace") for p in r.stdout.split(b"\0") if p]
    except (subprocess.CalledProcessError, FileNotFoundError):
        return []


# 路径片段 -> 提交中应出现的宏（与 .copilot-commit-message-instructions.md 一致）
# 匹配时按片段长度降序，只取第一个命中（避免 agreement/ 误伤 qtuple）
_PATH_TO_TAG: tuple[tuple[str, str], ...] = (
    ("agreement/qtuple/", "FREE_VER"),
    ("work_station/quiescent_current/", "QC_VER"),
    ("work_station/wifi_ble/", "SINGLE_VER"),
    ("work_station/freework/", "FREE_VER"),
    ("work_station/pressure/", "PRESSURE_VER"),
    ("work_station/pcba/", "PCBA_VER"),
    ("work_station/ageing/", "AGE_VER"),
    ("work_station/screen/", "SCREEN_VER"),
    ("work_station/camera/", "CAMERA_VER"),
    ("work_station/motor/", "MOTOR_VER"),
    ("work_station/suction/", "SUCTION_VER"),
    ("work_station/imu/", "IMU_VER"),
    ("work_station/key/", "KEY_VER"),
    ("djitestfunction", "DEBUG_VER"),
    ("new_production.pro", "DEBUG_VER"),
    ("mainwindow", "DEBUG_VER"),
)

# 仅文档/元信息：不强制 [XXX_VER]
_DOCISH_PREFIXES = (
    "docs/",
    ".github/",
    ".cursor/",
    ".copilot",
    ".githooks/",
)


def _normalize(p: str) -> str:
    return p.replace("\\", "/")


def _is_docish_path(p: str) -> bool:
    q = p.lstrip("./").replace("\\", "/")
    return any(q.startswith(x) for x in _DOCISH_PREFIXES)


def _all_staged_paths_docish(paths: list[str]) -> bool:
    """暂存文件是否全部为文档/元信息路径（无工站与协议等业务代码）。"""
    norms = [_normalize(raw).lstrip("./") for raw in paths if raw.strip()]
    if not norms:
        return False
    return all(_is_docish_path(p) for p in norms)


def _is_code_touch_path(p: str) -> bool:
    q = p.lstrip("./").replace("\\", "/")
    return bool(
        q.endswith((".cpp", ".h", ".hpp", ".c", ".ui", ".pro", ".qrc"))
        or q.startswith("agreement/")
        or q.startswith("work_station/")
        or q.startswith("my_set/")
    )


def infer_required_tags(paths: list[str]) -> set[str]:
    """根据暂存路径推断提交说明中必须出现的 [XXX_VER]（多路径取并集）。"""
    rules = sorted(_PATH_TO_TAG, key=lambda x: len(x[0]), reverse=True)
    required: set[str] = set()
    has_code_touch = False
    for raw in paths:
        p = _normalize(raw).lstrip("./")
        if not p:
            continue
        if _is_docish_path(p):
            continue
        tag_for_file: str | None = None
        for frag, tag in rules:
            if frag in p:
                tag_for_file = tag
                break
        if tag_for_file:
            has_code_touch = True
            required.add(tag_for_file)
        elif _is_code_touch_path(p):
            has_code_touch = True
    if paths and not has_code_touch:
        return set()
    return required


def tags_in_message(text: str) -> set[str]:
    return set(re.findall(r"\[(\w+_VER)\]", text))


def _bracket_tokens(text: str) -> list[str]:
    """方括号内标识（笔误检测）；仅匹配以字母开头的标识，降低误伤 [refs/1] 等。"""
    return re.findall(r"\[([A-Za-z][A-Za-z0-9_]*)\]", text)


def validate_message(
    text: str,
    valid_tags: set[str],
    required: set[str],
    *,
    staged_paths: list[str] | None = None,
) -> list[str]:
    errors: list[str] = []
    # 常见笔误：写成 _VR 少 E（不会被 tags_in_message 识别，会导致钩子与生成脚本都漏掉）
    for inner in _bracket_tokens(text):
        if inner.endswith("_VR") and not inner.endswith("_VER"):
            fixed = inner[:-2] + "VER"
            hint = f"（应为 [{fixed}]）" if fixed in valid_tags else "（宏名须以 _VER 结尾，与 AbIni.h 一致）"
            errors.append(f"疑似宏名笔误：[{inner}] {hint}")
    # 未知宏（严格 _VER 结尾）
    used = tags_in_message(text)
    for t in used:
        if t not in valid_tags:
            errors.append(f"提交说明中出现未在 AbIni.h 定义的 [{t}]（禁止臆造宏名）")
    # 允许多个 [XXX_VER] 在同一行，用 ； 或 ; 分隔（与 .copilot-commit-message-instructions.md 一致）
    # 禁止 AGREEMENT_VER
    if "AGREEMENT_VER" in used:
        errors.append("禁止使用 [AGREEMENT_VER]（AbIni.h 无此宏）")
    # 三元组却写了 DEBUG_VER（常见误用）
    if re.search(r"\[DEBUG_VER\].*三元组|三元组.*\[DEBUG_VER\]", text):
        errors.append("三元组相关说明请使用 [FREE_VER]，不要用 [DEBUG_VER]（产测主窗口专用）")
    # 路径要求的标签
    for tag in required:
        if tag not in used:
            errors.append(f"本次暂存变更需要包含 [{tag}] 行（与变更路径对应）")
    # 仅 docs/、.githooks 等时禁止任何 [XXX_VER]（避免把脚本/校验误标为三元组）
    if staged_paths is not None and _all_staged_paths_docish(staged_paths) and used:
        errors.append(
            "本次暂存均为 docs/、.githooks、.copilot、.cursor、.github 等文档或元信息路径时，不要写任何 [XXX_VER]（勿用 [FREE_VER] 或「三元组服务」描述生成脚本、校验或钩子，见 .copilot-commit-message-instructions.md「仅改工具脚本 / 校验 / 钩子」）。"
        )
    return errors


def main() -> int:
    if os.environ.get("SKIP_COMMIT_MSG_VALIDATE", "").strip() in ("1", "true", "yes"):
        return 0

    repo = _repo_root()
    header = os.path.join(repo, "my_set", "AbIni.h")
    if not os.path.isfile(header):
        print("[validate_commit_message] 跳过：未找到 my_set/AbIni.h", file=sys.stderr)
        return 0

    valid_tags = parse_abini_ver_tags(header)
    paths = staged_paths(repo)

    argv = sys.argv[1:]
    msg_path: str | None = None
    if "--message-file" in argv:
        i = argv.index("--message-file")
        if i + 1 >= len(argv):
            print("用法: --message-file 后须跟文件路径", file=sys.stderr)
            return 1
        msg_path = argv[i + 1]
    elif argv and not argv[0].startswith("-"):
        msg_path = argv[0]

    if not msg_path:
        print("用法: py docs/validate_commit_message.py <commit-msg文件>", file=sys.stderr)
        print("      py docs/validate_commit_message.py --message-file PATH", file=sys.stderr)
        return 1

    if not os.path.isfile(msg_path):
        print(f"[validate_commit_message] 文件不存在: {msg_path}", file=sys.stderr)
        return 1

    with open(msg_path, "r", encoding="utf-8") as f:
        text = f.read()

    # 去掉注释行（# 开头）常见编辑器模板
    lines = [ln for ln in text.splitlines() if not ln.strip().startswith("#")]
    body = "\n".join(lines).strip()
    if not body:
        return 0

    required = infer_required_tags(paths)
    errs = validate_message(body, valid_tags, required, staged_paths=paths)
    if errs:
        print("[validate_commit_message] 提交说明未通过校验：", file=sys.stderr)
        for e in errs:
            print(f"  - {e}", file=sys.stderr)
        print("  参考：.copilot-commit-message-instructions.md；生成脚本：docs/生成版本说明.py", file=sys.stderr)
        print("  跳过本检查：SKIP_COMMIT_MSG_VALIDATE=1 或 git commit --no-verify", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
