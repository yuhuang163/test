# -*- coding: utf-8 -*-
from pathlib import Path

ROOT = Path(r"d:/C/new_production_h_develop")

files = [
    ROOT / "agreement/fixture_protocol/asd9026a/device/asd9026a_device/asd9026a_device.cpp",
    ROOT / "agreement/fixture_protocol/asd9026a/device/asd9026a_device/asd9026a_device.h",
    ROOT / "agreement/fixture_protocol/asd9026a/codec/asd9026a_codec.cpp",
    ROOT / "agreement/fixture_protocol/xwd_fixture/codec/xwd_suction_uart_codec.cpp",
    ROOT / "agreement/fixture_protocol/xwd_fixture/codec/xwd_suction_uart_codec.h",
    ROOT / "platform/test_case/manifest/xwd_suction_fixture_cmd_manifest.cpp",
    ROOT / "work_station/freework/qfreework_test_case.cpp",
    ROOT / "work_station/freework/qfreework_data.cpp",
    ROOT / "work_station/freework/qfreework.cpp",
    ROOT / "common/app_help_menu.cpp",
    ROOT / "common/app_help_menu.h",
    ROOT / "build/Desktop_Qt_5_15_2_MSVC2019_64bit-Release/bin/test_case/profiles/M8组装电流测试工站/flow.ini",
    ROOT / "build/Desktop_Qt_5_15_2_MSVC2019_64bit-Release/bin/test_case/profiles/M8组装吸力测试工站/steps/读取工作电流.ini",
    ROOT / "build/Desktop_Qt_5_15_2_MSVC2019_64bit-Release/bin/test_case/profiles/自由工站/flow.ini",
]

# also walk source dirs for BOM / undecodable utf-8
scan_roots = [
    ROOT / "agreement",
    ROOT / "work_station/freework",
    ROOT / "platform/test_case",
    ROOT / "common",
]

bad = []
ok_files = 0

def check(path: Path, note: str = ""):
    global ok_files
    if not path.is_file():
        bad.append((str(path), "missing", note))
        return
    b = path.read_bytes()
    issues = []
    if b.startswith(b"\xef\xbb\xbf"):
        issues.append("BOM")
    try:
        t = b.decode("utf-8")
    except UnicodeDecodeError as e:
        issues.append(f"not-utf8:{e}")
        bad.append((str(path.relative_to(ROOT)), issues, note))
        return
    if "\ufeff" in t[:80]:
        issues.append("U+FEFF")
    if "\ufffd" in t:
        issues.append("replacement-char")
    # common mojibake fragments when UTF-8 Chinese corrupted
    for frag in ("锟斤拷", "烫烫烫", "屯屯屯", "Ã¤", "Ã©", "å·", "æ˜"):
        if frag in t:
            issues.append(f"mojibake:{frag}")
            break
    cjk = sum(1 for c in t if "\u4e00" <= c <= "\u9fff")
    if issues:
        bad.append((str(path.relative_to(ROOT)), issues, f"cjk={cjk}", note))
    else:
        ok_files += 1

for p in files:
    check(p, "sample")

exts = {".cpp", ".h", ".hpp", ".ini", ".md", ".json"}
for root in scan_roots:
    if not root.exists():
        continue
    for p in root.rglob("*"):
        if not p.is_file() or p.suffix.lower() not in exts:
            continue
        # skip huge / third party
        if "Python39" in p.parts or "qpb" in p.parts and "Python" in str(p):
            continue
        check(p)

print("ok_files", ok_files)
print("bad_count", len(bad))
for item in bad[:50]:
    print("BAD", item)

print("\n--- chinese snippets ---")
for p in files:
    if not p.exists():
        print("MISSING", p.name)
        continue
    t = p.read_text(encoding="utf-8")
    line = next((ln for ln in t.splitlines() if any("\u4e00" <= c <= "\u9fff" for c in ln)), None)
    rel = p.relative_to(ROOT).as_posix()
    print(rel)
    print(" ", (line or "(no CJK)").strip()[:140])
