# -*- coding: utf-8 -*-
"""将仓库内文本源码统一为 UTF-8（默认无 BOM；项目规范见 .cursor/rules/qt-cpp-project.mdc）。仅改写需要变更的文件。"""
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
    "agreement/factory_protocol/protocol/qpb/Python39",
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


def encode_text(text: str, use_bom: bool) -> bytes:
    data = text.encode("utf-8")
    if use_bom:
        return b"\xef\xbb\xbf" + data
    return data


def iter_files(ini_only: bool, roots: list[Path]) -> list[Path]:
    out: list[Path] = []
    walk_roots = roots if roots else [ROOT]
    for walk_root in walk_roots:
        if not walk_root.exists():
            continue
        if walk_root.is_file():
            try:
                rel = walk_root.relative_to(ROOT).as_posix()
            except ValueError:
                rel = walk_root.as_posix()
            if path_excluded(rel):
                continue
            if ini_only and walk_root.suffix.lower() != ".ini":
                continue
            if not ini_only and walk_root.suffix.lower() not in TEXT_EXTENSIONS:
                continue
            if walk_root.name in SKIP_FILE_NAMES:
                continue
            out.append(walk_root)
            continue
        for dirpath, dirnames, filenames in os.walk(walk_root):
            dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIR_NAMES]
            for fn in filenames:
                p = Path(dirpath) / fn
                try:
                    rel = p.relative_to(ROOT).as_posix()
                except ValueError:
                    rel = p.as_posix()
                if path_excluded(rel):
                    continue
                suffix = p.suffix.lower()
                if ini_only:
                    if suffix != ".ini":
                        continue
                elif suffix not in TEXT_EXTENSIONS and fn not in ("CMakeLists.txt",):
                    continue
                if fn in SKIP_FILE_NAMES:
                    continue
                out.append(p)
    return out


def convert_file(path: Path, dry_run: bool, use_bom: bool, reencode: bool) -> str:
    raw = path.read_bytes()
    if not raw:
        return "empty"
    try:
        text = decode_text(raw)
    except UnicodeDecodeError:
        return "error:decode"

    if reencode:
        new_raw = encode_text(text, use_bom)
        if new_raw == raw:
            return "skip"
        action = "would-reencode" if dry_run else "reencode"
    elif use_bom:
        if has_utf8_bom(raw):
            return "skip"
        action = "would-add-bom" if dry_run else "add-bom"
        new_raw = encode_text(text, True)
    else:
        if not has_utf8_bom(raw):
            return "skip"
        action = "would-strip-bom" if dry_run else "strip-bom"
        new_raw = encode_text(text, False)

    if not dry_run:
        path.write_bytes(new_raw)
    return action


def parse_roots(argv: list[str]) -> list[Path]:
    roots: list[Path] = []
    for arg in argv:
        if arg.startswith("-"):
            continue
        p = Path(arg)
        if not p.is_absolute():
            p = ROOT / p
        roots.append(p)
    return roots


def main() -> int:
    dry_run = "--dry-run" in sys.argv
    reencode = "--reencode" in sys.argv
    ini_only = "--ini-only" in sys.argv
    if "--bom" in sys.argv:
        use_bom = True
        mode_label = "UTF-8+BOM"
    elif "--no-bom" in sys.argv:
        use_bom = False
        mode_label = "UTF-8(no BOM)"
    else:
        print("用法: 转换文本为UTF8编码.py --no-bom|--bom [--reencode] [--ini-only] [--dry-run] [目录或文件...]")
        print("  --reencode  将 GBK/UTF-8 等解码后统一写为 UTF-8（推荐处理 .ini 配置）")
        print("  --ini-only  仅处理 .ini 文件")
        print("  示例: python scripts/转换文本为UTF8编码.py --reencode --no-bom --ini-only 工厂工站配置文件")
        return 2

    roots = parse_roots(sys.argv[1:])
    if reencode:
        mode_label += "+reencode"

    stats: dict[str, int] = {}
    changed_paths: list[str] = []
    for p in sorted(set(iter_files(ini_only, roots))):
        try:
            rel = p.relative_to(ROOT).as_posix()
        except ValueError:
            rel = p.as_posix()
        try:
            action = convert_file(p, dry_run, use_bom, reencode)
        except Exception as e:
            action = f"error:{e}"
        stats[action] = stats.get(action, 0) + 1
        if action.startswith("would-") or action in ("add-bom", "strip-bom", "reencode"):
            changed_paths.append(rel)

    tag = "DRY-RUN" if dry_run else "APPLY"
    print(f"[{tag} {mode_label}] stats:", dict(sorted(stats.items())))
    if changed_paths:
        print(f"[{tag}] changed ({len(changed_paths)}):")
        for rel in changed_paths[:80]:
            print(" ", rel)
        if len(changed_paths) > 80:
            print(f"  ... and {len(changed_paths) - 80} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
