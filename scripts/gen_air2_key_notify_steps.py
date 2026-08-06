# -*- coding: utf-8 -*-
"""Air2：等待 CID=0x1A 按键主动上报（PromptOnly + Gate keyButtonId）。"""
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


def make_wait(step_id: str, key: int, cn: str, mes: str) -> str:
    return "\n".join(
        [
            "[Meta]",
            f"StepId={step_id}",
            f"Name={step_id}",
            f"DisplayName={step_id}",
            f"MesTag={mes}",
            "PromptEnabled=true",
            "PromptOnly=true",
            f"PromptText=请按下设备{cn}（等待 CID=0x1A 上报）",
            "",
            "[Send]",
            "Channel=Product",
            "Action=",
            "DeviceCmd=",
            "Protocol=Qaiot",
            "",
            "[Timing]",
            "DelayBeforeMs=0",
            "DelayAfterMs=0",
            "CommandTimeoutMs=30000",
            "",
            "[Gate]",
            "Enabled=true",
            "ReportType=ProtocolButtonStateData",
            "Field=keyButtonId",
            "Op=eq",
            "Low=0",
            "High=0",
            f"Expected={key}",
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
        sid = f"等待{cn}上报"
        names.append(sid)
        (STEPS / f"{sid}.ini").write_bytes(crlf(make_wait(sid, code, cn, f"WAIT_KEY_{en}")))
        print(f"wrote {sid}.ini key={code}")

    joined = ",".join(names)
    raw = FLOW.read_bytes().decode("utf-8")
    changed = False
    if "等待电源按键上报" not in raw:
        # 插在模拟按键之后、退出工厂模式之前
        if "模拟旋钮右转," in raw:
            raw = raw.replace("模拟旋钮右转,", "模拟旋钮右转," + joined + ",")
        else:
            raw = raw.replace("退出工厂模式", joined + ",退出工厂模式")
        changed = True
    # 默认禁用：需人工按键，避免全自动跑卡死
    if 'DisabledItems="' in raw and "等待电源按键上报" not in raw.split("DisabledItems=", 1)[-1].split("\n", 1)[0]:
        raw = raw.replace('DisabledItems="', f'DisabledItems="{joined},', 1)
        changed = True
    if changed:
        FLOW.write_bytes(crlf(raw))
        print(f"updated flow.ini (+{len(names)} wait-key, disabled by default)")
    else:
        print("flow.ini already has wait-key steps (incl. DisabledItems)")


if __name__ == "__main__":
    main()
