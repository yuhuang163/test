# -*- coding: utf-8 -*-
"""Air2：数据采集循环上报 CID=0x18/0x19 — 各传感器开/等待 + 关闭；默认 Disabled。"""
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

# report_data_type 与规范一致
SENSORS = [
    (0x00, "IMU", "IMU"),
    (0x01, "压力", "PRESSURE"),
    (0x02, "气流", "AIRFLOW"),
    (0x03, "TOF", "TOF"),
    (0x04, "电容", "CAPACITIVE"),
    (0x05, "红外", "INFRARED"),
    (0x06, "生物阻抗", "BIOIMPEDANCE"),
    (0x07, "液位", "LIQUID_LEVEL"),
    (0x08, "温度", "TEMPERATURE"),
    (0x09, "湿度", "HUMIDITY"),
    (0x0A, "接近", "PROXIMITY"),
    (0x0B, "电流", "CURRENT"),
    (0x0C, "霍尔", "HALL"),
    (0x0D, "编码器", "ENCODER"),
]

INTERVAL_MS = 200
OFF_NAME = "关闭循环上报"


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def write_step(path: Path, text: str) -> None:
    path.write_bytes(crlf(text))


def on_name(cn: str) -> str:
    return f"开启{cn}循环上报"


def wait_name(cn: str) -> str:
    return f"等待{cn}循环上报"


def all_names() -> list[str]:
    names: list[str] = []
    for _code, cn, _en in SENSORS:
        names.append(on_name(cn))
        names.append(wait_name(cn))
    names.append(OFF_NAME)
    return names


def make_cfg(name: str, mes: str, enable: int, typ: int | None, interval: int | None) -> str:
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
        "DeviceCmd=CycleReportWrite",
        "Protocol=Qaiot",
        f"Param_enable={enable}",
    ]
    if typ is not None:
        lines.append(f"Param_type={typ}")
    if interval is not None:
        lines.append(f"Param_intervalTime={interval}")
    lines += [
        "",
        "[Timing]",
        "DelayBeforeMs=0",
        "DelayAfterMs=200",
        "CommandTimeoutMs=2000",
        "",
        "[Gate]",
        "Enabled=true",
        "ReportType=ProtocolAiotCycleReportConfigData",
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


def make_wait(name: str, mes: str, data_type: int, cn: str) -> str:
    return "\n".join(
        [
            "[Meta]",
            f"StepId={name}",
            f"Name={name}",
            f"DisplayName={name}",
            f"MesTag={mes}",
            "PromptEnabled=true",
            "PromptOnly=true",
            f"PromptText=等待设备上报{cn}循环采集数据（CID=0x19）",
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
            "CommandTimeoutMs=10000",
            "",
            "[Gate]",
            "Enabled=true",
            "ReportType=ProtocolAiotCycleReportData",
            "Field=dataType",
            "Op=eq",
            "Low=0",
            "High=0",
            f"Expected={data_type}",
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
    text = FLOW.read_bytes().decode("utf-8").replace("\r\n", "\n")
    insert = ",".join(names)
    name_set = set(names)

    # Items：若已有任一步骤名，先整段删掉再插入全量；否则插在振动后
    def strip_cycle_names(s: str) -> str:
        parts = [p for p in s.split(",") if p and p not in name_set and p != "开启IMU循环上报" and p != "等待IMU循环上报"]
        # 旧 OFF 也可能单独残留
        parts = [p for p in parts if p != OFF_NAME]
        return ",".join(parts)

    m_items = re.search(r'Items="([^"]*)"', text)
    if not m_items:
        raise SystemExit("Items= not found")
    items = strip_cycle_names(m_items.group(1))
    if "关闭自定义振动" in items:
        items = items.replace("关闭自定义振动", f"关闭自定义振动,{insert}", 1)
    elif "开启自定义振动" in items:
        items = items.replace("开启自定义振动", f"开启自定义振动,{insert}", 1)
    elif "写入IMU传感器校准" in items:
        items = items.replace("写入IMU传感器校准", f"{insert},写入IMU传感器校准", 1)
    else:
        items = f"{items},{insert}" if items else insert
    text = text[: m_items.start(1)] + items + text[m_items.end(1) :]

    def patch_disabled(m: re.Match[str]) -> str:
        content = strip_cycle_names(m.group(1))
        parts = [p for p in content.split(",") if p]
        for n in names:
            if n not in parts:
                parts.append(n)
        return f'DisabledItems="{",".join(parts)}"'

    text2, n = re.subn(r'DisabledItems="([^"]*)"', patch_disabled, text, count=1)
    if n != 1:
        raise SystemExit("DisabledItems patch failed")
    FLOW.write_bytes(crlf(text2))
    print("patched flow.ini")


def main() -> None:
    STEPS.mkdir(parents=True, exist_ok=True)
    names = all_names()
    for code, cn, en in SENSORS:
        on = on_name(cn)
        wait = wait_name(cn)
        write_step(STEPS / f"{on}.ini", make_cfg(on, f"CYCLE_REPORT_{en}_ON", 1, code, INTERVAL_MS))
        write_step(STEPS / f"{wait}.ini", make_wait(wait, f"WAIT_CYCLE_REPORT_{en}", code, cn))
        print(f"wrote {on}.ini / {wait}.ini type=0x{code:02X}")
    write_step(STEPS / f"{OFF_NAME}.ini", make_cfg(OFF_NAME, "CYCLE_REPORT_OFF", 0, None, None))
    print(f"wrote {OFF_NAME}.ini")
    patch_flow(names)
    print(f"ok, steps={len(names)}")


if __name__ == "__main__":
    main()
