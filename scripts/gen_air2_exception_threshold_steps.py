# -*- coding: utf-8 -*-
"""Air2：异常阈值 CID=0x0A/0x0B — 读全部 + 各类型写/读（示例值取自规范）。"""
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

# (中文名, type, 写参 dict, 读 Gate Field, Expected)
ITEMS = [
    ("电池低电告警阈值", 0x01, {"value": 20}, "value", "20"),
    ("电池低电关机阈值", 0x02, {"value": 5}, "value", "5"),
    ("充电过压阈值", 0x03, {"value": 3000}, "value", "3000"),
    ("充电超时阈值", 0x04, {"value": 1800}, "value", "1800"),
    ("电池温度异常阈值", 0x05, {"low": 0, "high": 55}, "value", "0"),
    ("电机堵转过流阈值", 0x11, {"value": 500}, "value", "500"),
    ("电机开路阈值", 0x12, {"value": 10}, "value", "10"),
    ("负压过高阈值", 0x22, {"value": 100}, "value", "100"),
]


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def write_step(path: Path, text: str) -> None:
    path.write_bytes(crlf(text))


def make_read_all() -> str:
    return "\n".join(
        [
            "[Meta]",
            "StepId=读取全部异常阈值",
            "Name=读取全部异常阈值",
            "DisplayName=读取全部异常阈值",
            "MesTag=EXCEPTION_THRESHOLD_READ_ALL",
            "PromptEnabled=false",
            "PromptOnly=false",
            "PromptText=",
            "",
            "[Send]",
            "Channel=Product",
            "Action=Get",
            "DeviceCmd=ExceptionThresholdRead",
            "Protocol=Qaiot",
            "",
            "[Timing]",
            "DelayBeforeMs=0",
            "DelayAfterMs=0",
            "CommandTimeoutMs=3000",
            "",
            "[Gate]",
            "Enabled=false",
            "ReportType=ProtocolAiotExceptionThresholdData",
            "Field=value",
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


def make_write(name: str, typ: int, params: dict) -> str:
    lines = [
        "[Meta]",
        f"StepId=写入{name}",
        f"Name=写入{name}",
        f"DisplayName=写入{name}",
        f"MesTag=EXCEPTION_THRESHOLD_WRITE_{typ:02X}",
        "PromptEnabled=false",
        "PromptOnly=false",
        "PromptText=",
        "",
        "[Send]",
        "Channel=Product",
        "Action=Set",
        "DeviceCmd=ExceptionThresholdWrite",
        "Protocol=Qaiot",
        f"Param_type={typ}",
    ]
    for k, v in params.items():
        lines.append(f"Param_{k}={v}")
    lines += [
        "",
        "[Timing]",
        "DelayBeforeMs=0",
        "DelayAfterMs=200",
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


def make_read(name: str, typ: int, field: str, expected: str) -> str:
    return "\n".join(
        [
            "[Meta]",
            f"StepId=读取{name}",
            f"Name=读取{name}",
            f"DisplayName=读取{name}",
            f"MesTag=EXCEPTION_THRESHOLD_READ_{typ:02X}",
            "PromptEnabled=false",
            "PromptOnly=false",
            "PromptText=",
            "",
            "[Send]",
            "Channel=Product",
            "Action=Get",
            "DeviceCmd=ExceptionThresholdRead",
            "Protocol=Qaiot",
            f"Param_type={typ}",
            "",
            "[Timing]",
            "DelayBeforeMs=0",
            "DelayAfterMs=0",
            "CommandTimeoutMs=2000",
            "",
            "[Gate]",
            "Enabled=true",
            "ReportType=ProtocolAiotExceptionThresholdData",
            f"Field={field}",
            "Op=eq",
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
    )


def patch_flow(names: list[str]) -> None:
    text = FLOW.read_text(encoding="utf-8")
    # 统一换行便于处理
    text = text.replace("\r\n", "\n")
    marker = "写入IMU传感器校准"
    insert = ",".join(names)
    if "读取全部异常阈值" in text:
        print("flow.ini already has exception threshold steps, skip Items patch")
        return
    if marker not in text:
        raise SystemExit(f"flow marker not found: {marker}")
    text = text.replace(marker, f"{insert},{marker}", 1)
    FLOW.write_bytes(crlf(text))
    print(f"patched flow.ini before {marker}")


def main() -> None:
    STEPS.mkdir(parents=True, exist_ok=True)
    write_step(STEPS / "读取全部异常阈值.ini", make_read_all())
    names: list[str] = ["读取全部异常阈值"]
    for name, typ, params, field, expected in ITEMS:
        write_step(STEPS / f"写入{name}.ini", make_write(name, typ, params))
        write_step(STEPS / f"读取{name}.ini", make_read(name, typ, field, expected))
        names.append(f"写入{name}")
        names.append(f"读取{name}")
        # 温度异常再卡控上限
        if typ == 0x05:
            high_step = make_read(name, typ, "valueHigh", "55").replace(
                f"StepId=读取{name}",
                f"StepId=读取{name}上限",
            ).replace(
                f"Name=读取{name}",
                f"Name=读取{name}上限",
            ).replace(
                f"DisplayName=读取{name}",
                f"DisplayName=读取{name}上限",
            ).replace(
                f"MesTag=EXCEPTION_THRESHOLD_READ_{typ:02X}",
                f"MesTag=EXCEPTION_THRESHOLD_READ_{typ:02X}_HIGH",
            )
            write_step(STEPS / f"读取{name}上限.ini", high_step)
            names.append(f"读取{name}上限")
    patch_flow(names)
    print(f"wrote {len(names)} steps")


if __name__ == "__main__":
    main()
