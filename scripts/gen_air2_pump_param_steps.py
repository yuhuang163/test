# -*- coding: utf-8 -*-
"""Air2：泵/阀运行参数 CID=0x0F/0x10 — 泵与阀分步写入读取；默认 Disabled。"""
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

LEGACY = ("写入泵阀运行参数", "读取泵阀运行参数")
STEPS_DEF = [
    (
        "写入泵运行参数",
        "PUMP_PARAM_WRITE",
        "Set",
        "PumpParamWrite",
        {"circleNum": 10, "durationTime": 1000, "intervalTime": 500, "pumpPwm": 50},
        False,
        "",
        "",
    ),
    (
        "读取泵运行参数",
        "PUMP_PARAM_READ",
        "Get",
        "PumpParamRead",
        {},
        True,
        "durationTime",
        "1000",
    ),
    (
        "写入阀运行参数",
        "VALVE_PARAM_WRITE",
        "Set",
        "ValveParamWrite",
        {"valveEnableTime": 200, "valveDisableTime": 200, "valvePwm": 50},
        False,
        "",
        "",
    ),
    (
        "读取阀运行参数",
        "VALVE_PARAM_READ",
        "Get",
        "ValveParamRead",
        {},
        True,
        "valveEnableTime",
        "200",
    ),
]


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def write_step(path: Path, text: str) -> None:
    path.write_bytes(crlf(text))


def make_step(
    name: str,
    mes: str,
    action: str,
    cmd: str,
    params: dict,
    gate: bool,
    field: str,
    expected: str,
) -> str:
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
        f"Action={action}",
        f"DeviceCmd={cmd}",
        "Protocol=Qaiot",
    ]
    for k, v in params.items():
        lines.append(f"Param_{k}={v}")
    lines += [
        "",
        "[Timing]",
        "DelayBeforeMs=0",
        f"DelayAfterMs={'200' if action == 'Set' else '0'}",
        "CommandTimeoutMs=2000",
        "",
        "[Gate]",
        f"Enabled={'true' if gate else 'false'}",
        f"ReportType={'ProtocolAiotPumpParamData' if gate else ''}",
        f"Field={field}",
        f"Op={'eq' if gate else 'range'}",
        "Low=0",
        "High=0",
        f"Expected={expected}",
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


def patch_flow(names: list[str]) -> None:
    text = FLOW.read_bytes().decode("utf-8").replace("\r\n", "\n")
    insert = ",".join(names)
    marker = "写入IMU传感器校准"

    # 去掉旧合并步骤名
    for old in LEGACY:
        text = text.replace(f",{old}", "").replace(f"{old},", "")

    if names[0] not in text:
        if marker not in text:
            raise SystemExit(f"flow marker not found: {marker}")
        text = text.replace(marker, f"{insert},{marker}", 1)

    def patch_disabled(m: re.Match[str]) -> str:
        content = m.group(1)
        for old in LEGACY:
            content = content.replace(f",{old}", "").replace(f"{old},", "")
            if content == old:
                content = ""
        for n in names:
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
    for old in LEGACY:
        p = STEPS / f"{old}.ini"
        if p.exists():
            p.unlink()
            print(f"removed {old}.ini")
    names: list[str] = []
    for args in STEPS_DEF:
        name = args[0]
        write_step(STEPS / f"{name}.ini", make_step(*args))
        names.append(name)
    patch_flow(names)
    print("ok", names)


if __name__ == "__main__":
    main()
