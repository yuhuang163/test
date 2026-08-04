# -*- coding: utf-8 -*-
"""换行处理：默认 CRLF；仅仓库根目录 new_production.pro 与 *.sh 保持 LF。

无参数时只检查/修正 new_production.pro，不批量改动其它文件。
传入路径时仅处理所列文件（Agent 改动的文件）。
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
    if should_keep_lf(rel, path.suffix):
        normalized = normalize_to_lf(text)
    else:
        normalized = normalize_to_crlf(text)
    encoded = normalized.encode("utf-8")

    if encoded == raw:
        return "skip"
    if not dry_run:
        path.write_bytes(encoded)
    return "would-convert" if dry_run else "convert"


def main() -> int:
    dry_run = "--dry-run" in sys.argv
    cli_paths = resolve_cli_paths(sys.argv[1:])
    if cli_paths:
        targets = cli_paths
    else:
        # 无参数：只保证 new_production.pro，不批量动其它文件
        targets = [ROOT / "new_production.pro"]

    stats: dict[str, int] = {}
    changed: list[str] = []
    for p in sorted(set(targets)):
        if not p.is_file():
            stats["missing"] = stats.get("missing", 0) + 1
            continue
        rel = p.relative_to(ROOT).as_posix()
        try:
            action = convert_file(p, dry_run)
        except Exception as exc:
            action = f"error:{exc}"
        stats[action] = stats.get(action, 0) + 1
        if action in ("convert", "would-convert"):
            changed.append(rel)

    tag = "DRY-RUN" if dry_run else "APPLY"
    print(f"[{tag}] stats:", dict(sorted(stats.items())))
    if changed:
        print(f"[{tag}] changed ({len(changed)}):")
        for rel in changed[:50]:
            print(" ", rel)
        if len(changed) > 50:
            print(f"  ... and {len(changed) - 50} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
