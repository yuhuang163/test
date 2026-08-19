# -*- coding: utf-8 -*-
"""换行处理：默认 CRLF；仅仓库根目录 new_production.pro 与 *.sh 保持 LF。

无参数时只检查/修正 new_production.pro，不批量改动其它文件。
传入路径时仅处理所列文件（Agent 改动的文件）。

  --check-blank-lines  仅检查 C/C++ 是否有多余连续空行（>1），有问题 exit 1
  --dry-run            不写盘，只报告将转换的文件
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXCLUDE_DIR_NAMES = {
    "build",
    ".git",
    "Python39",
    "node_modules",
    ".cursor",
    "generator",
}

EXCLUDE_PATH_PARTS = (
    "factory_protocol/protocol/qpb/Python39",
    "qProtocol/qpb/Python39",
    "adapter/qProtocol/qpb/Python39",
    "lib/libusb",
)

TEXT_EXTENSIONS = {
    ".cpp",
    ".h",
    ".hpp",
    ".c",
    ".cc",
    ".cxx",
    ".pro",
    ".pri",
    ".ui",
    ".qml",
    ".qrc",
    ".md",
    ".mdc",
    ".ini",
    ".py",
    ".bat",
    ".ps1",
    ".ts",
    ".qss",
    ".rc",
    ".def",
    ".cs",
    ".txt",
    ".xml",
    ".json",
    ".gitattributes",
    ".gitignore",
}

# 仅根工程 new_production.pro 固定 LF；*.sh 避免 shebang 带 \r
KEEP_LF_REL_PATHS = {"new_production.pro"}
KEEP_LF_SUFFIXES = {".sh"}


def path_excluded(rel_posix: str) -> bool:
    rel = rel_posix.replace("\\", "/")
    for part in EXCLUDE_PATH_PARTS:
        if part in rel:
            return True
    return False


def decode_text(raw: bytes) -> str:
    if raw.startswith(b"\xef\xbb\xbf"):
        text = raw[3:].decode("utf-8")
    else:
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            text = raw.decode("gbk")
    if text.startswith("\ufeff"):
        text = text[1:]
    return text


def normalize_to_lf(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def normalize_to_crlf(text: str) -> str:
    return normalize_to_lf(text).replace("\n", "\r\n")


def _should_keep_blank_between(prev: str, nxt: str) -> bool:
    """邻行均为非空时，是否保留中间空行（与仓库 .h/.cpp 常见风格一致）。"""
    p = prev.rstrip()
    n = nxt.lstrip()
    if not p or not n:
        return False
    if n.startswith("/**"):
        return True
    if n.startswith("class ") or n.startswith("struct ") or n.startswith("namespace "):
        return True
    if p.startswith("#include") and not n.startswith("#") and not n.startswith("//"):
        return True
    return False


def normalize_cpp_blank_lines(text: str) -> tuple[str, bool]:
    """折叠连续空行 >1，并去除「每行后多空一行」类 Agent 写入瑕疵。"""
    original = normalize_to_lf(text)
    lines = original.split("\n")
    if not lines:
        return text, False

    # 1) 连续空行 >1 → 1
    step1: list[str] = []
    blank_run = 0
    for line in lines:
        if line.strip() == "":
            blank_run += 1
            if blank_run <= 1:
                step1.append("")
        else:
            blank_run = 0
            step1.append(line)

    empty_count = sum(1 for l in step1 if l.strip() == "")
    nonempty_count = len(step1) - empty_count
    # 2) 空行占比过高：去掉夹在两条非空行之间的多余空行
    if nonempty_count >= 4 and empty_count / max(len(step1), 1) >= 0.28:
        step2: list[str] = []
        changed = step1 != lines
        for i, line in enumerate(step1):
            if line.strip() != "":
                step2.append(line)
                continue
            prev = step2[-1] if step2 else ""
            nxt = ""
            for j in range(i + 1, len(step1)):
                if step1[j].strip() != "":
                    nxt = step1[j]
                    break
            if prev.strip() and nxt.strip() and _should_keep_blank_between(prev, nxt):
                if not step2 or step2[-1].strip() != "":
                    step2.append("")
            else:
                if prev.strip() and nxt.strip():
                    changed = True
        lines = step2
    else:
        lines = step1
        changed = lines != original.split("\n")

    normalized = "\n".join(lines)
    if original.endswith("\n") and not normalized.endswith("\n"):
        normalized += "\n"
    return normalized, changed or normalized != original


def collapse_excessive_blank_lines(text: str, max_consecutive: int = 1) -> tuple[str, bool]:
    """兼容旧名；C/C++ 请用 normalize_cpp_blank_lines。"""
    if max_consecutive != 1:
        # 仅折叠连续空行
        lines = normalize_to_lf(text).split("\n")
        out: list[str] = []
        blank_run = 0
        changed = False
        for line in lines:
            if line.strip() == "":
                blank_run += 1
                if blank_run > max_consecutive:
                    changed = True
                    continue
                out.append("")
            else:
                blank_run = 0
                out.append(line)
        normalized = "\n".join(out)
        if text.endswith("\n") and not normalized.endswith("\n"):
            normalized += "\n"
        return normalized, changed or normalized != normalize_to_lf(text)
    return normalize_cpp_blank_lines(text)


def check_excessive_blank_lines(text: str, max_consecutive: int = 1) -> bool:
    """存在多余空行时返回 True（表示有问题）。"""
    _, changed = collapse_excessive_blank_lines(text, max_consecutive)
    return changed


def should_keep_lf(rel_posix: str, suffix: str) -> bool:
    rel = rel_posix.replace("\\", "/")
    if rel in KEEP_LF_REL_PATHS:
        return True
    return suffix.lower() in KEEP_LF_SUFFIXES


def iter_files() -> list[Path]:
    out: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIR_NAMES]
        for fn in filenames:
            p = Path(dirpath) / fn
            rel = p.relative_to(ROOT).as_posix()
            if path_excluded(rel):
                continue
            suffix = p.suffix.lower()
            if suffix not in TEXT_EXTENSIONS and fn not in ("CMakeLists.txt",):
                continue
            out.append(p)
    return out


def resolve_cli_paths(argv: list[str]) -> list[Path]:
    paths: list[Path] = []
    for arg in argv:
        if arg.startswith("-"):
            continue
        p = Path(arg)
        if not p.is_absolute():
            p = ROOT / p
        try:
            p = p.resolve()
        except OSError:
            continue
        if not p.is_file():
            continue
        try:
            p.relative_to(ROOT)
        except ValueError:
            continue
        paths.append(p)
    return paths


def convert_file(path: Path, dry_run: bool) -> str:
    raw = path.read_bytes()
    if not raw:
        return "empty"
    try:
        text = decode_text(raw)
    except UnicodeDecodeError:
        return "error:decode"

    rel = path.relative_to(ROOT).as_posix()
    suffix = path.suffix.lower()
    # C/C++ 源文件：折叠「逐行空行」等异常空行（Agent Write 偶发）
    if suffix in {".cpp", ".h", ".hpp", ".c", ".cc", ".cxx"}:
        text, blank_fixed = normalize_cpp_blank_lines(text)
    else:
        blank_fixed = False

    if should_keep_lf(rel, path.suffix):
        normalized = normalize_to_lf(text)
    else:
        normalized = normalize_to_crlf(text)
    encoded = normalized.encode("utf-8")

    if encoded == raw:
        if blank_fixed:
            return "blank-skip"
        return "skip"
    if not dry_run:
        path.write_bytes(encoded)
    return "would-convert" if dry_run else ("blank-convert" if blank_fixed else "convert")


def main() -> int:
    dry_run = "--dry-run" in sys.argv
    check_only = "--check-blank-lines" in sys.argv
    cli_paths = resolve_cli_paths(sys.argv[1:])
    if cli_paths:
        targets = cli_paths
    else:
        # 无参数：只保证 new_production.pro，不批量动其它文件
        targets = [ROOT / "new_production.pro"]

    stats: dict[str, int] = {}
    changed: list[str] = []
    blank_issues: list[str] = []
    for p in sorted(set(targets)):
        if not p.is_file():
            stats["missing"] = stats.get("missing", 0) + 1
            continue
        rel = p.relative_to(ROOT).as_posix()
        if check_only:
            try:
                text = decode_text(p.read_bytes())
            except (UnicodeDecodeError, OSError) as exc:
                stats[f"error:{exc}"] = stats.get(f"error:{exc}", 0) + 1
                continue
            if p.suffix.lower() in {".cpp", ".h", ".hpp", ".c", ".cc", ".cxx"} and check_excessive_blank_lines(text):
                blank_issues.append(rel)
                stats["blank-lines"] = stats.get("blank-lines", 0) + 1
            else:
                stats["ok"] = stats.get("ok", 0) + 1
            continue
        try:
            action = convert_file(p, dry_run)
        except Exception as exc:
            action = f"error:{exc}"
        stats[action] = stats.get(action, 0) + 1
        if action in ("convert", "would-convert", "blank-convert"):
            changed.append(rel)

    tag = "CHECK" if check_only else ("DRY-RUN" if dry_run else "APPLY")
    print(f"[{tag}] stats:", dict(sorted(stats.items())))
    if blank_issues:
        print(f"[{tag}] excessive blank lines ({len(blank_issues)}):")
        for rel in blank_issues[:50]:
            print(" ", rel)
        if len(blank_issues) > 50:
            print(f"  ... and {len(blank_issues) - 50} more")
        return 1
    if changed:
        print(f"[{tag}] changed ({len(changed)}):")
        for rel in changed[:50]:
            print(" ", rel)
        if len(changed) > 50:
            print(f"  ... and {len(changed) - 50} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
