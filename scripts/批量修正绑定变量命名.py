# -*- coding: utf-8 -*-
"""批量修正工站源码中 bandingmac/bandingsn 等绑定相关局部变量拼写。"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"Python39", ".git", "build", "lib", "agreement"}

LOCAL_REPLACEMENTS = [
    (re.compile(r"\bbandingmac\b"), "bindingMac"),
    (re.compile(r"\bbandingsn\b"), "bindingSn"),
    (re.compile(r"\bbandingresult\b"), "bindingResult"),
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
        for pat, repl in LOCAL_REPLACEMENTS:
            text = pat.sub(repl, text)
        if text != orig:
            path.write_text(text, encoding="utf-8-sig")
            changed.append(str(path.relative_to(ROOT)))
    print(f"updated {len(changed)} files")
    for p in sorted(changed):
        print(p)


if __name__ == "__main__":
    main()
