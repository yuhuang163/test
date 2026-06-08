# -*- coding: utf-8 -*-
"""协议虚槽统一 refresh* 命名；processInspection 形参 stringsn → inputSnText。"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"Python39", ".git", "build", "lib", "agreement"}


def should_skip(path: Path) -> bool:
    return any(part in SKIP_DIRS for part in path.parts)


def fix_process_inspection_bodies(text: str) -> str:
    pat = re.compile(
        r"(void\s+(?:\w+::)?processInspection\(QString\s+inputSnText\)\s*\{)(.*?)(^\})",
        re.MULTILINE | re.DOTALL,
    )

    def repl(m):
        body = re.sub(r"\bstringsn\b", "inputSnText", m.group(2))
        return m.group(1) + body + m.group(3)

    return pat.sub(repl, text)


def process_file(path: Path) -> bool:
    text = path.read_text(encoding="utf-8-sig")
    orig = text
    renames = {
        "checkbutton": "refreshButton",
        "checkButton": "refreshButton",
        "checkLedControlState": "refreshLedControlState",
        "checkBrushControlState": "refreshBrushControlState",
        "getPressSensorData": "refreshPressSensorData",
        "getPresscalidata": "refreshPressCalibData",
        "getimuData": "refreshImuData",
        "getPictureSendOver": "refreshPictureSendOver",
        "getDongleWifi": "refreshDongleWifi",
        "getWifiMsg": "refreshWifiMsg",
    }
    for old, new in renames.items():
        text = text.replace(old, new)
    text = text.replace("processInspection(QString stringsn)", "processInspection(QString inputSnText)")
    if path.suffix == ".cpp" and "processInspection(QString inputSnText)" in text:
        text = fix_process_inspection_bodies(text)
    if text != orig:
        path.write_text(text, encoding="utf-8-sig")
        return True
    return False


def main():
    changed = []
    for path in ROOT.rglob("*"):
        if path.suffix not in {".h", ".cpp"} or should_skip(path):
            continue
        if process_file(path):
            changed.append(str(path.relative_to(ROOT)))
    print(f"updated {len(changed)} files")
    for p in sorted(changed):
        print(p)


if __name__ == "__main__":
    main()
