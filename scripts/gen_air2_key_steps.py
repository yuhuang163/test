# -*- coding: utf-8 -*-
"""Air2：按键模拟 0x01~0x0B 拆成独立步骤。"""
from __future__ import annotations

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

KEYS = [
    (0x01, "电源按键", "POWER"),
    (0x02, "开始按键", "START"),
    (0x03, "模式按键", "MODE"),
    (0x04, "频率按键", "FREQUENCY"),
    (0x05, "母乳按键", "BREASTFEEDING"),
    (0x06, "左控制按键", "LEFT_CONTROL"),
    (0x07, "右控制按键", "RIGHT_CONTROL"),
    (0x08, "恢复出厂按键", "FACTORY_RESET"),
    (0x09, "旅行锁按键", "TRAVEL_LOCK"),
    (0x0A, "旋钮左转", "KNOB_LEFT"),
    (0x0B, "旋钮右转", "KNOB_RIGHT"),
]


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def make_step(step_id: str, key: int, mes: str) -> str:
    return "\n".join(
        [
            "[Meta]",
            f"StepId={step_id}",
            f"Name={step_id}",
            f"DisplayName={step_id}",
            f"MesTag={mes}",
            "PromptEnabled=false",
            "PromptOnly=false",
            "PromptText=",
            "",
            "[Send]",
            "Channel=Product",
            "Action=Set",
            "DeviceCmd=ButtonState",
            "Protocol=Qaiot",
            f"Param_int={key}",
            "",
            "[Timing]",
            "DelayBeforeMs=0",
            "DelayAfterMs=0",
            "CommandTimeoutMs=2000",
            "",
            "[Gate]",
            "Enabled=false",
            "ReportType=",
            "Field=",
            "Op=range",
            "Low=0",
            "High=0",
            "Expected=",
            "ExpectedSettingsKey=",
            "LowSettingsKey=",
            "HighSettingsKey=",
            "",
            "[Hook]",
            "Enabled=false",
            "HookId=",
            "",
        ]
    )


def main() -> None:
    STEPS.mkdir(parents=True, exist_ok=True)
    names: list[str] = []
    for code, cn, en in KEYS:
        sid = f"模拟{cn}"
        names.append(sid)
        (STEPS / f"{sid}.ini").write_bytes(crlf(make_step(sid, code, f"SIMULATE_KEY_{en}")))
        print(f"wrote {sid}.ini key=0x{code:02X}")

    old = STEPS / "模拟按键.ini"
    if old.exists():
        old.unlink()
        print("removed 模拟按键.ini")

    raw = FLOW.read_bytes().decode("utf-8")
    joined = ",".join(names)
    if "模拟按键," in raw:
        raw = raw.replace("模拟按键,", joined + ",")
    elif ",模拟按键" in raw:
        raw = raw.replace(",模拟按键", "," + joined)
    else:
        raw = raw.replace("退出工厂模式", joined + ",退出工厂模式")
    FLOW.write_bytes(crlf(raw))
    print(f"updated flow.ini ({len(names)} key steps)")


if __name__ == "__main__":
    main()
