# -*- coding: utf-8 -*-
"""Air2：电量模拟 CID=0x13 — 每通道独立设置/置0恢复 → 读测。"""
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

VIRTUAL_PERCENT = 50
VIRTUAL_MV = 3700
VIRTUAL_MA = 200
VIRTUAL_TEMP_C = 25

LEGACY_NAMES = ("设置模拟电量", "恢复真实电量")


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def make_set(step_id: str, mes: str, params: dict[str, str | int]) -> str:
    lines = [
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
        "DeviceCmd=SetBattery",
        "Protocol=Qaiot",
    ]
    for k, v in params.items():
        lines.append(f"Param_{k}={v}")
    lines += [
        "",
        "[Timing]",
        "DelayBeforeMs=0",
        "DelayAfterMs=300",
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
    return "\n".join(lines)


def make_get(step_id: str, field: str, gate_field: str, mes: str, gate: bool = False, low=0, high=0) -> str:
    lines = [
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
        "Action=Get",
        "DeviceCmd=GetBattery",
        "Protocol=Qaiot",
        f"Param_field={field}",
        "",
        "[Timing]",
        "DelayBeforeMs=0",
        "DelayAfterMs=0",
        "CommandTimeoutMs=2000",
        "",
        "[Gate]",
    ]
    if gate:
        lines += [
            "Enabled=true",
            "ReportType=ProtocolBatteryData",
            f"Field={gate_field}",
            "Op=range",
            f"Low={low}",
            f"High={high}",
            "Expected=",
            "ExpectedSettingsKey=",
            "LowSettingsKey=",
            "HighSettingsKey=",
        ]
    else:
        lines += [
            "Enabled=false",
            "ReportType=ProtocolBatteryData",
            f"Field={gate_field}",
            "Op=range",
            "Low=0",
            "High=0",
            "Expected=",
            "ExpectedSettingsKey=",
            "LowSettingsKey=",
            "HighSettingsKey=",
        ]
    lines += ["", "[Hook]", "Enabled=false", "HookId=", ""]
    return "\n".join(lines)


def main() -> None:
    STEPS.mkdir(parents=True, exist_ok=True)

    files = {
        "设置模拟电池百分比": make_set(
            "设置模拟电池百分比", "VIRTUAL_BATTERY_PERCENT", {"percent": VIRTUAL_PERCENT}
        ),
        "设置模拟电池电压": make_set(
            "设置模拟电池电压", "VIRTUAL_BATTERY_VOLTAGE", {"voltageMv": VIRTUAL_MV}
        ),
        "设置模拟电池电流": make_set(
            "设置模拟电池电流", "VIRTUAL_BATTERY_CURRENT", {"currentMa": VIRTUAL_MA}
        ),
        "设置模拟电池温度": make_set(
            "设置模拟电池温度", "VIRTUAL_BATTERY_TEMP", {"temperatureC": VIRTUAL_TEMP_C}
        ),
        "恢复真实电池百分比": make_set(
            "恢复真实电池百分比", "VIRTUAL_BATTERY_PERCENT_CLEAR", {"percent": 0}
        ),
        "恢复真实电池电压": make_set(
            "恢复真实电池电压", "VIRTUAL_BATTERY_VOLTAGE_CLEAR", {"voltageMv": 0}
        ),
        "恢复真实电池电流": make_set(
            "恢复真实电池电流", "VIRTUAL_BATTERY_CURRENT_CLEAR", {"currentMa": 0}
        ),
        "恢复真实电池温度": make_set(
            "恢复真实电池温度", "VIRTUAL_BATTERY_TEMP_CLEAR", {"temperatureC": 0}
        ),
        "模拟后读取电池百分比": make_get(
            "模拟后读取电池百分比",
            "percent",
            "percent",
            "BATTERY_PERCENT_VIRTUAL",
            True,
            VIRTUAL_PERCENT - 2,
            VIRTUAL_PERCENT + 2,
        ),
        "模拟后读取电池电压": make_get(
            "模拟后读取电池电压",
            "voltage",
            "voltageMv",
            "BATTERY_VOLTAGE_VIRTUAL",
            True,
            VIRTUAL_MV - 50,
            VIRTUAL_MV + 50,
        ),
        "模拟后读取电池电流": make_get(
            "模拟后读取电池电流",
            "current",
            "currentMa",
            "BATTERY_CURRENT_VIRTUAL",
            True,
            VIRTUAL_MA - 20,
            VIRTUAL_MA + 20,
        ),
        "模拟后读取电池温度": make_get(
            "模拟后读取电池温度",
            "temperature",
            "temperatureC",
            "BATTERY_TEMP_VIRTUAL",
            True,
            VIRTUAL_TEMP_C - 2,
            VIRTUAL_TEMP_C + 2,
        ),
        "恢复后读取电池百分比": make_get(
            "恢复后读取电池百分比", "percent", "percent", "BATTERY_PERCENT_REAL", True, 0, 100
        ),
        "恢复后读取电池电压": make_get(
            "恢复后读取电池电压", "voltage", "voltageMv", "BATTERY_VOLTAGE_REAL"
        ),
        "恢复后读取电池电流": make_get(
            "恢复后读取电池电流", "current", "currentMa", "BATTERY_CURRENT_REAL"
        ),
        "恢复后读取电池温度": make_get(
            "恢复后读取电池温度", "temperature", "temperatureC", "BATTERY_TEMP_REAL"
        ),
    }
    for name, body in files.items():
        (STEPS / f"{name}.ini").write_bytes(crlf(body))
        print(f"wrote {name}.ini")

    for legacy in LEGACY_NAMES:
        p = STEPS / f"{legacy}.ini"
        if p.exists():
            p.unlink()
            print(f"removed {legacy}.ini")

    new_block = (
        "设置模拟电池百分比,模拟后读取电池百分比,"
        "设置模拟电池电压,模拟后读取电池电压,"
        "设置模拟电池电流,模拟后读取电池电流,"
        "设置模拟电池温度,模拟后读取电池温度,"
        "恢复真实电池百分比,恢复真实电池电压,恢复真实电池电流,恢复真实电池温度,"
        "恢复后读取电池百分比,恢复后读取电池电压,恢复后读取电池电流,恢复后读取电池温度"
    )
    old_blocks = [
        (
            "设置模拟电池百分比,模拟后读取电池百分比,"
            "设置模拟电池电压,模拟后读取电池电压,"
            "设置模拟电池电流,模拟后读取电池电流,"
            "设置模拟电池温度,模拟后读取电池温度,"
            "恢复真实电量,"
            "恢复后读取电池百分比,恢复后读取电池电压,恢复后读取电池电流,恢复后读取电池温度"
        ),
        (
            "设置模拟电量,"
            "模拟后读取电池百分比,模拟后读取电池电压,模拟后读取电池电流,模拟后读取电池温度,"
            "恢复真实电量,"
            "恢复后读取电池百分比,恢复后读取电池电压,恢复后读取电池电流,恢复后读取电池温度"
        ),
    ]
    raw = FLOW.read_bytes().decode("utf-8")
    changed = False
    for old in old_blocks:
        if old in raw:
            raw = raw.replace(old, new_block)
            changed = True
            break
    if "恢复真实电量," in raw:
        raw = raw.replace(
            "恢复真实电量,",
            "恢复真实电池百分比,恢复真实电池电压,恢复真实电池电流,恢复真实电池温度,",
        )
        changed = True
    if changed:
        FLOW.write_bytes(crlf(raw))
        print("updated flow.ini")
    else:
        print("flow.ini already up to date")


if __name__ == "__main__":
    main()
