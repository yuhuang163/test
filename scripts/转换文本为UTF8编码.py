# -*- coding: utf-8 -*-
"""将仓库内文本源码统一为 UTF-8（可选 with BOM / 无 BOM）。仅改写需要变更的文件。"""
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
}

EXCLUDE_PATH_PARTS = (
    "agreement/qProtocol/qpb/Python39",
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
}

SKIP_FILE_NAMES = {
    ".gitignore",
    "settings.json",
}


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


def has_utf8_bom(raw: bytes) -> bool:
    return raw.startswith(b"\xef\xbb\xbf")


def iter_files() -> list[Path]:
    out: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIR_NAMES]
        for fn in filenames:
            p = Path(dirpath) / fn
            rel = p.relative_to(ROOT).as_posix()
            if path_excluded(rel):
                continue
            if p.suffix.lower() not in TEXT_EXTENSIONS and fn not in (
                "CMakeLists.txt",
            ):
                continue
            if fn in SKIP_FILE_NAMES:
                continue
            out.append(p)
    return out


def convert_file(path: Path, dry_run: bool, use_bom: bool) -> str:
    raw = path.read_bytes()
    if not raw:
        return "empty"
    try:
        text = decode_text(raw)
    except UnicodeDecodeError:
        return "error:decode"

    if use_bom:
        if has_utf8_bom(raw):
            return "skip"
        action = "would-add-bom" if dry_run else "add-bom"
    else:
        if not has_utf8_bom(raw):
            return "skip"
        action = "would-strip-bom" if dry_run else "strip-bom"

    if not dry_run:
        data = text.encode("utf-8")
        if use_bom:
            data = b"\xef\xbb\xbf" + data
        path.write_bytes(data)
    return action


def main() -> int:
    dry_run = "--dry-run" in sys.argv
    if "--bom" in sys.argv:
        use_bom = True
        mode_label = "UTF-8+BOM"
    elif "--no-bom" in sys.argv:
        use_bom = False
        mode_label = "UTF-8(no BOM)"
    else:
        print("用法: 转换文本为UTF8编码.py --no-bom|--bom [--dry-run]")
        return 2

    stats: dict[str, int] = {}
    changed_paths: list[str] = []
    for p in sorted(iter_files()):
        rel = p.relative_to(ROOT).as_posix()
        try:
            action = convert_file(p, dry_run, use_bom)
        except Exception as e:
            action = f"error:{e}"
        stats[action] = stats.get(action, 0) + 1
        if action.startswith("would-") or action in ("add-bom", "strip-bom"):
            changed_paths.append(rel)

    tag = "DRY-RUN" if dry_run else "APPLY"
    print(f"[{tag} {mode_label}] stats:", dict(sorted(stats.items())))
    if changed_paths:
        print(f"[{tag}] changed ({len(changed_paths)}):")
        for rel in changed_paths:
            print(" ", rel)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
