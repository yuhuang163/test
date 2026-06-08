# -*- coding: utf-8 -*-
"""统一工站/MES 相关历史拼写命名（仅 .h/.cpp）。"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"Python39", ".git", "build"}

REPLACEMENTS = [
    ("bandingMacSn_mes", "bindingMacSnMes"),
    ("bandingMacSn", "bindingMacSn"),
    ("initDate", "initData"),
    ("getmacadress", "getMacAddress"),
    ("QString bandingmac", "QString bindingMac"),
    ("QString bandingsn", "QString bindingSn"),
]


def should_skip(path: Path) -> bool:
    return any(part in SKIP_DIRS for part in path.parts)


def main():
    changed = []
    for path in ROOT.rglob("*"):
        if path.suffix not in {".h", ".cpp"} or should_skip(path):
            continue
        text = path.read_text(encoding="utf-8-sig")
        orig = text
        for old, new in REPLACEMENTS:
            text = text.replace(old, new)
        if text != orig:
            path.write_text(text, encoding="utf-8-sig")
            changed.append(str(path.relative_to(ROOT)))
    print(f"updated {len(changed)} files")
    for p in sorted(changed):
        print(p)


if __name__ == "__main__":
    main()
