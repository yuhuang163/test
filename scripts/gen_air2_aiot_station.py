# -*- coding: utf-8 -*-
"""生成 Air2协议全量测试工站 profile / flow / steps（UTF-8 无 BOM，CRLF）。"""
from __future__ import annotations

from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATION = "Air2协议全量测试工站"
KEY = "FLOW_ST_0025"
TARGETS = [
    ROOT
    / "build"
    / "Desktop_Qt_5_15_2_MSVC2019_64bit-Release"
    / "bin"
    / "test_case"
    / "profiles"
    / STATION,
]
FLOW_INI = (
    ROOT
    / "build"
    / "Desktop_Qt_5_15_2_MSVC2019_64bit-Release"
    / "bin"
    / "test_case"
    / "总的测试流程.ini"
)


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def make_step(
    step_id: str,
    *,
    channel: str = "Product",
    action: str = "Get",
    cmd: str = "",
    protocol: str = "Qaiot",
    params: dict | None = None,
    timeout: int = 1500,
    gate: dict | None = None,
    prompt: str = "",
    delay_after: int = 0,
    mes: str = "",
) -> str:
    params = params or {}
    lines = [
        "[Meta]",
        f"StepId={step_id}",
        f"Name={step_id}",
        f"DisplayName={step_id}",
        f"MesTag={mes or step_id}",
        f"PromptEnabled={'true' if prompt else 'false'}",
        "PromptOnly=false",
        f"PromptText={prompt}",
        "",
        "[Send]",
        f"Channel={channel}",
        f"Action={action}",
        f"DeviceCmd={cmd}",
    ]
    if channel == "Product":
        lines.append(f"Protocol={protocol}")
    for k, v in params.items():
        lines.append(f"Param_{k}={v}")
    lines += [
        "",
        "[Timing]",
        "DelayBeforeMs=0",
        f"DelayAfterMs={delay_after}",
        f"CommandTimeoutMs={timeout}",
        "",
        "[Gate]",
    ]
    if gate:
        lines += [
            "Enabled=true",
            f"ReportType={gate.get('type', '')}",
            f"Field={gate.get('field', '')}",
            f"Op={gate.get('op', 'range')}",
            f"Low={gate.get('low', 0)}",
            f"High={gate.get('high', 0)}",
            f"Expected={gate.get('expected', '')}",
            "ExpectedSettingsKey=",
            "LowSettingsKey=",
            "HighSettingsKey=",
        ]
    else:
        lines += [
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
        ]
    lines += ["", "[Hook]", "Enabled=false", "HookId=", ""]
    return "\n".join(lines)


def main() -> None:
    steps_spec: list[tuple[str, dict]] = [
        (
            "扫描连接蓝牙",
            dict(
                channel="Dongle",
                action="Set",
                cmd="BleScanConnect",
                params={"string": "$MAC"},
                timeout=30000,
            ),
        ),
        (
            "进入工厂模式",
            dict(action="Set", cmd="FacMode", params={"value": "1"}, timeout=2000, delay_after=300),
        ),
        (
            "读取版本号",
            dict(action="Get", cmd="SoftVersionRead", timeout=2000),
        ),
        (
            "读取设备名称",
            dict(action="Get", cmd="DeviceInfo", timeout=2000),
        ),
        (
            "读取MAC地址",
            dict(action="Get", cmd="MacRead", timeout=2000),
        ),
        (
            "读取产测完成标识",
            dict(action="Get", cmd="FactoryDoneRead", timeout=2000),
        ),
        (
            "获取电量信息",
            dict(
                action="Get",
                cmd="GetBattery",
                timeout=2000,
                gate={
                    "type": "ProtocolBatteryData",
                    "field": "percent",
                    "op": "range",
                    "low": 0,
                    "high": 100,
                },
            ),
        ),
        ("获取整机SN码", dict(action="Get", cmd="Sn", timeout=2000)),
        ("读取设备三元组", dict(action="Get", cmd="TupleRead", timeout=2000)),
        (
            "写入SN码",
            dict(
                action="Set",
                cmd="Sn",
                params={"sn": "$SN"},
                timeout=2000,
                prompt="将写入 SN（Param_sn=$SN），确认继续？",
            ),
        ),
        (
            "设置老化模式开",
            dict(action="Set", cmd="BurningMode", params={"switch": "1"}, timeout=2000, delay_after=300),
        ),
        ("读取老化状态", dict(action="Get", cmd="AgingStatusRead", timeout=2000)),
        (
            "设置老化模式关",
            dict(action="Set", cmd="BurningMode", params={"switch": "0"}, timeout=2000, delay_after=300),
        ),
        (
            "进入吸力模式",
            dict(action="Set", cmd="SuctionMode", params={"on": "1"}, timeout=2000, delay_after=300),
        ),
        (
            "退出吸力模式",
            dict(action="Set", cmd="SuctionMode", params={"on": "0"}, timeout=2000, delay_after=300),
        ),
        (
            "开启蓝牙信号模式",
            dict(action="Set", cmd="BtSignalMode", params={"on": "1"}, timeout=2000, delay_after=200),
        ),
        (
            "关闭蓝牙信号模式",
            dict(action="Set", cmd="BtSignalMode", params={"on": "0"}, timeout=2000, delay_after=200),
        ),
        (
            "开启蓝牙无信号模式",
            dict(action="Set", cmd="BtNoSignalMode", params={"on": "1"}, timeout=2000, delay_after=200),
        ),
        (
            "关闭蓝牙无信号模式",
            dict(action="Set", cmd="BtNoSignalMode", params={"on": "0"}, timeout=2000, delay_after=200),
        ),
        (
            "开启蓝牙定频模式",
            dict(action="Set", cmd="BtFreqMode", params={"on": "1"}, timeout=2000, delay_after=200),
        ),
        (
            "关闭蓝牙定频模式",
            dict(action="Set", cmd="BtFreqMode", params={"on": "0"}, timeout=2000, delay_after=200),
        ),
        (
            "写入射频Trim",
            dict(
                action="Set",
                cmd="TrimSet",
                params={"trim": "0", "power": "0"},
                timeout=2000,
                prompt="将写入射频 Trim=0/Power=0，确认继续？",
            ),
        ),
        (
            "读取RSSI",
            dict(
                action="Get",
                cmd="RssiRead",
                timeout=2000,
                gate={
                    "type": "ProtocolRssiData",
                    "field": "dbm",
                    "op": "range",
                    "low": -120,
                    "high": 0,
                },
            ),
        ),
        ("读取Trim", dict(action="Get", cmd="TrimRead", timeout=2000)),
        ("读取传感器", dict(action="Get", cmd="PeriphState", timeout=2000)),
        ("模拟按键", dict(action="Set", cmd="ButtonState", params={"int": "1"}, timeout=2000)),
        (
            "写产测完成标识",
            dict(
                action="Set",
                cmd="FacResult",
                params={"done": "1"},
                timeout=2000,
                prompt="将写产测完成标识=1，确认继续？",
            ),
        ),
        (
            "退出工厂模式",
            dict(action="Set", cmd="FacMode", params={"value": "0"}, timeout=2000, delay_after=300),
        ),
        (
            "设备复位",
            dict(action="Set", cmd="DevReset", timeout=3000, prompt="设备复位（CID=0x0C），确认执行？"),
        ),
        (
            "恢复出厂",
            dict(action="Set", cmd="FactoryReset", timeout=3000, prompt="恢复出厂（破坏性），确认执行？"),
        ),
        (
            "关机船运模式",
            dict(action="Set", cmd="ShipMode", timeout=3000, prompt="关机/船运模式（流程末尾），确认执行？"),
        ),
    ]

    flow_items = ",".join(sid for sid, _ in steps_spec)
    profile_text = (
        f"[Profile]\n"
        f"StationKey={KEY}\n"
        f"DisplayName={STATION}\n"
        f"CreatedAt={datetime.now().strftime('%Y-%m-%dT%H:%M:%S')}\n"
        f"ProfileVersion=2\n"
        f"StepsLibraryVersion=1\n"
    )
    flow_text = (
        f"[Flow]\n"
        f'Items="{flow_items}"\n'
        f"StopFlowOnTestFail=true\n"
        f"FailItems=\n"
        f"\n"
        f"[SerialUi]\n"
        f"JigVisible=false\n"
        f"JigLabel=治具串口\n"
        f"ProductVisible=false\n"
        f"ProductLabel=产品串口(仪器)\n"
        f"UsbVisible=false\n"
        f"UsbLabel=万用表串口\n"
    )

    for base in TARGETS:
        steps_dir = base / "steps"
        steps_dir.mkdir(parents=True, exist_ok=True)
        (base / "profile.ini").write_bytes(crlf(profile_text))
        (base / "flow.ini").write_bytes(crlf(flow_text))
        for sid, kw in steps_spec:
            (steps_dir / f"{sid}.ini").write_bytes(crlf(make_step(sid, **kw)))
        print(f"created {base} ({len(steps_spec)} steps)")

    if FLOW_INI.exists():
        raw = FLOW_INI.read_bytes().decode("utf-8")
        line = f"{KEY}={STATION}"
        if KEY not in raw:
            raw = raw.rstrip("\r\n") + "\r\n" + line + "\r\n"
            FLOW_INI.write_bytes(crlf(raw))
            print(f"registered {line}")
        else:
            print("already registered in 总的测试流程.ini")
    else:
        print(f"skip register: missing {FLOW_INI}")


if __name__ == "__main__":
    main()
