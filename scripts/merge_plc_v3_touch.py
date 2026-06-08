# -*- coding: utf-8 -*-
"""从 git 导出的 touch_key/touch_switch 合并为 plc_v3_touch，输出 UTF-8 BOM。"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_H = ROOT / "agreement/qplc/plc_v3_touch.h"
OUT_CPP = ROOT / "agreement/qplc/plc_v3_touch.cpp"


def git_show(path: str) -> str:
    result = subprocess.run(
        ["git", "show", f":{path}"],
        cwd=ROOT,
        capture_output=True,
        check=True,
    )
    # 统一 LF，避免 CRLF 再经 write_text 写出后出现空行
    return result.stdout.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")


def main() -> int:
    key_h = git_show("agreement/qplc/plc_v3_touch_key.h")
    key_cpp = git_show("agreement/qplc/plc_v3_touch_key.cpp")
    switch_cpp = git_show("agreement/qplc/plc_v3_touch_switch.cpp")

    out_h = key_h.replace("PLC_V3_TOUCH_KEY_H", "PLC_V3_TOUCH_H")
    if "runPlcV3TouchSwitch" not in out_h:
        out_h = out_h.replace(
            "#endif  // PLC_V3_TOUCH_H",
            "/** V3 治具旋钮整步（前推 + 按压 Modbus 握手）。 */\n"
            "PlcV3TouchResult runPlcV3TouchSwitch(PlcModbusSession& session);\n\n"
            "#endif  // PLC_V3_TOUCH_H",
        )

    key_part = key_cpp.replace('#include "plc_v3_touch_key.h"', '#include "plc_v3_touch.h"')
    marker = "PlcV3TouchResult runPlcV3TouchSwitch"
    idx = switch_cpp.index(marker)
    out_cpp = key_part.rstrip() + "\n\n" + switch_cpp[idx:].rstrip() + "\n"

    OUT_H.write_text(out_h, encoding="utf-8-sig", newline="\r\n")
    OUT_CPP.write_text(out_cpp, encoding="utf-8-sig", newline="\r\n")
    print(f"wrote {OUT_H.name} ({len(out_h.splitlines())} lines)")
    print(f"wrote {OUT_CPP.name} ({len(out_cpp.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
