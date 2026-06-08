# -*- coding: utf-8 -*-
"""将仓库内文本文件换行统一为 CRLF（仓库 * -text 原样入库）。仅改写需要变更的文件。"""
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


def normalize_to_crlf(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    return text.replace("\n", "\r\n")


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
            if suffix in KEEP_LF_SUFFIXES:
                continue
            out.append(p)
    return out


def convert_file(path: Path, dry_run: bool) -> str:
    raw = path.read_bytes()
    if not raw:
        return "empty"
    try:
        text = decode_text(raw)
    except UnicodeDecodeError:
        return "error:decode"

    crlf_text = normalize_to_crlf(text)
    encoded = crlf_text.encode("utf-8")
    new_raw = b"\xef\xbb\xbf" + encoded if raw.startswith(b"\xef\xbb\xbf") else encoded

    if new_raw == raw:
        return "skip"
    if not dry_run:
        path.write_bytes(new_raw)
    return "would-convert" if dry_run else "convert"


def main() -> int:
    dry_run = "--dry-run" in sys.argv
    stats: dict[str, int] = {}
    changed: list[str] = []
    for p in sorted(iter_files()):
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
