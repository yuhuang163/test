# -*- coding: utf-8 -*-
"""Air2：自定义加热 CID=0x14 — 开/关；默认 Disabled。"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATION = (
    ROOT
    / "build"
    / "Desktop_Qt_5_15_2_MSVC2019_64bit-Release"
    / "bin"
    / "test_case"
    / "profiles"
    / "Air2协议全量测试工站"
)
STEPS = STATION / "steps"
FLOW = STATION / "flow.ini"

ON_NAME = "开启自定义加热"
OFF_NAME = "关闭自定义加热"
NAMES = [ON_NAME, OFF_NAME]


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def write_step(path: Path, text: str) -> None:
    path.write_bytes(crlf(text))


def make_heat(name: str, mes: str, enable: int, strength: int, duration: int | None) -> str:
    lines = [
        "[Meta]",
        f"StepId={name}",
        f"Name={name}",
        f"DisplayName={name}",
        f"MesTag={mes}",
        "PromptEnabled=false",
        "PromptOnly=false",
        "PromptText=",
        "",
        "[Send]",
        "Channel=Product",
        "Action=Set",
        "DeviceCmd=HeatTestWrite",
        "Protocol=Qaiot",
        f"Param_enable={enable}",
        f"Param_driveStrength={strength}",
    ]
    if duration is not None:
        lines.append(f"Param_durationTime={duration}")
    lines += [
        "",
        "[Timing]",
        "DelayBeforeMs=0",
        "DelayAfterMs=200",
        "CommandTimeoutMs=2000",
        "",
        "[Gate]",
        "Enabled=true",
        "ReportType=ProtocolAiotHeatTestData",
        "Field=enable",
        "Op=eq",
        "Low=0",
        "High=0",
        f"Expected={enable}",
        "ExpectedSettingsKey=",
        "LowSettingsKey=",
        "HighSettingsKey=",
        "",
        "[Hook]",
        "Enabled=false",
        "HookId=",
        "",
    ]
    return "\n".join(lines)


def patch_flow() -> None:
    text = FLOW.read_bytes().decode("utf-8").replace("\r\n", "\n")
    insert = ",".join(NAMES)
    marker = "写入IMU传感器校准"
    if ON_NAME not in text:
        # 优先插在阀步骤后；否则 IMU 前
        if "读取阀运行参数," in text:
            text = text.replace("读取阀运行参数,", f"读取阀运行参数,{insert},", 1)
        elif marker in text:
            text = text.replace(marker, f"{insert},{marker}", 1)
        else:
            raise SystemExit("flow insert marker not found")

    def patch_disabled(m: re.Match[str]) -> str:
        content = m.group(1)
        for n in NAMES:
            if n not in content:
                content = f"{content},{n}" if content else n
        return f'DisabledItems="{content}"'

    text2, n = re.subn(r'DisabledItems="([^"]*)"', patch_disabled, text, count=1)
    if n != 1:
        raise SystemExit("DisabledItems patch failed")
    FLOW.write_bytes(crlf(text2))
    print("patched flow.ini")


def main() -> None:
    STEPS.mkdir(parents=True, exist_ok=True)
    write_step(STEPS / f"{ON_NAME}.ini", make_heat(ON_NAME, "HEAT_TEST_ON", 1, 50, 1000))
    write_step(STEPS / f"{OFF_NAME}.ini", make_heat(OFF_NAME, "HEAT_TEST_OFF", 0, 0, None))
    patch_flow()
    print("ok")


if __name__ == "__main__":
    main()
