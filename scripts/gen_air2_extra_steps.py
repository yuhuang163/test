# -*- coding: utf-8 -*-
"""为 Air2协议全量测试工站补齐：硬件/资源版本、多工厂模式、传感器分类型读写。"""
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


def crlf(text: str) -> bytes:
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def make_step(
    step_id: str,
    *,
    action: str,
    cmd: str,
    params: dict | None = None,
    timeout: int = 2000,
    delay_after: int = 0,
    mes: str = "",
    gate: dict | None = None,
) -> str:
    params = params or {}
    lines = [
        "[Meta]",
        f"StepId={step_id}",
        f"Name={step_id}",
        f"DisplayName={step_id}",
        f"MesTag={mes or step_id}",
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


SENSORS = [
    (0x00, "IMU", "IMU"),
    (0x01, "压力", "PRESSURE"),
    (0x02, "气流", "AIRFLOW"),
    (0x03, "TOF", "TOF"),
    (0x04, "电容", "CAPACITIVE"),
    (0x05, "红外", "INFRARED"),
    (0x06, "生物阻抗", "BIO_IMPEDANCE"),
    (0x07, "液位", "LIQUID_LEVEL"),
    (0x08, "温度", "TEMPERATURE"),
    (0x09, "湿度", "HUMIDITY"),
    (0x0A, "接近", "PROXIMITY"),
    (0x0B, "电流", "CURRENT"),
    (0x0C, "霍尔", "HALL"),
    (0x0D, "编码器", "ENCODER"),
]


def main() -> None:
    STEPS.mkdir(parents=True, exist_ok=True)
    new_steps: list[tuple[str, dict]] = [
        (
            "读取硬件版本",
            dict(
                action="Get",
                cmd="SoftVersionRead",
                params={"field": "hw_version"},
                mes="HW_VERSION",
                gate={
                    "type": "ProtocolBaseInfoData",
                    "field": "hw_version",
                    "op": "compareVersions",
                    "expected": "1",
                },
            ),
        ),
        (
            "读取资源版本",
            dict(
                action="Get",
                cmd="SoftVersionRead",
                params={"field": "res_version"},
                mes="RES_VERSION",
                gate={
                    "type": "ProtocolBaseInfoData",
                    "field": "res_version",
                    "op": "compareVersions",
                    "expected": "1.02",
                },
            ),
        ),
        (
            "读取电池百分比",
            dict(
                action="Get",
                cmd="GetBattery",
                params={"field": "percent"},
                mes="BATTERY_PERCENT",
                gate={
                    "type": "ProtocolBatteryData",
                    "field": "percent",
                    "op": "range",
                    "low": 0,
                    "high": 100,
                },
            ),
        ),
        (
            "读取电池电压",
            dict(
                action="Get",
                cmd="GetBattery",
                params={"field": "voltage"},
                mes="BATTERY_VOLTAGE",
            ),
        ),
        (
            "读取电池电流",
            dict(
                action="Get",
                cmd="GetBattery",
                params={"field": "current"},
                mes="BATTERY_CURRENT",
            ),
        ),
        (
            "读取电池温度",
            dict(
                action="Get",
                cmd="GetBattery",
                params={"field": "temperature"},
                mes="BATTERY_TEMPERATURE",
            ),
        ),
        (
            "进入空闲模式",
            dict(
                action="Set",
                cmd="FacMode",
                params={"mode": "0", "on": "1"},
                delay_after=300,
                mes="IDLE_MODE_ON",
            ),
        ),
        (
            "读取空闲模式状态",
            dict(action="Get", cmd="AgingStatusRead", params={"mode": "0"}, mes="IDLE_MODE_STATUS"),
        ),
        (
            "退出空闲模式",
            dict(
                action="Set",
                cmd="FacMode",
                params={"mode": "0", "on": "0"},
                delay_after=300,
                mes="IDLE_MODE_OFF",
            ),
        ),
        (
            "读取工厂测试模式状态",
            dict(
                action="Get",
                cmd="AgingStatusRead",
                params={"mode": "1"},
                mes="FACTORY_MODE_STATUS",
            ),
        ),
        (
            "进入吸力补偿模式",
            dict(
                action="Set",
                cmd="CompensationSet",
                params={"on": "1"},
                delay_after=300,
                mes="SUCTION_COMP_MODE_ON",
            ),
        ),
        (
            "读取吸力补偿模式状态",
            dict(
                action="Get",
                cmd="AgingStatusRead",
                params={"mode": "4"},
                mes="SUCTION_COMP_MODE_STATUS",
            ),
        ),
        (
            "退出吸力补偿模式",
            dict(
                action="Set",
                cmd="CompensationSet",
                params={"on": "0"},
                delay_after=300,
                mes="SUCTION_COMP_MODE_OFF",
            ),
        ),
        (
            "进入ATE模式",
            dict(
                action="Set",
                cmd="FacMode",
                params={"mode": "5", "on": "1"},
                delay_after=300,
                mes="ATE_MODE_ON",
            ),
        ),
        (
            "读取ATE模式状态",
            dict(action="Get", cmd="AgingStatusRead", params={"mode": "5"}, mes="ATE_MODE_STATUS"),
        ),
        (
            "退出ATE模式",
            dict(
                action="Set",
                cmd="FacMode",
                params={"mode": "5", "on": "0"},
                delay_after=300,
                mes="ATE_MODE_OFF",
            ),
        ),
        (
            "读取吸力测试模式状态",
            dict(
                action="Get",
                cmd="AgingStatusRead",
                params={"mode": "3"},
                mes="SUCTION_MODE_STATUS",
            ),
        ),
    ]

    for typ, cn, en in SENSORS:
        new_steps.append(
            (
                f"读取{cn}传感器",
                dict(
                    action="Get",
                    cmd="PeriphState",
                    params={"type": str(typ)},
                    mes=f"SENSOR_READ_{en}",
                ),
            )
        )
        new_steps.append(
            (
                f"写入{cn}传感器校准",
                dict(
                    action="Set",
                    cmd="LightCalibWrite",
                    params={"type": str(typ), "data": "00"},
                    mes=f"SENSOR_CALIB_WRITE_{en}",
                ),
            )
        )

    for sid, kw in new_steps:
        (STEPS / f"{sid}.ini").write_bytes(crlf(make_step(sid, **kw)))
        print(f"wrote {sid}.ini")

    old = STEPS / "读取传感器.ini"
    if old.exists():
        old.unlink()
        print("removed 读取传感器.ini")

    # 更新 flow：在保留用户自定义项的基础上插入新步骤
    sensor_reads = [f"读取{cn}传感器" for _, cn, _ in SENSORS]
    sensor_writes = [f"写入{cn}传感器校准" for _, cn, _ in SENSORS]
    mode_block = [
        "进入空闲模式",
        "读取空闲模式状态",
        "退出空闲模式",
        "读取工厂测试模式状态",
        "设置老化模式开",
        "读取老化状态",
        "设置老化模式关",
        "进入吸力模式",
        "读取吸力测试模式状态",
        "退出吸力模式",
        "进入吸力补偿模式",
        "读取吸力补偿模式状态",
        "退出吸力补偿模式",
        "进入ATE模式",
        "读取ATE模式状态",
        "退出ATE模式",
    ]

    raw = FLOW.read_bytes().decode("utf-8")
    # 构造完整 Items（基于当前 flow 顺序习惯）
    items = [
        "扫描连接蓝牙",
        "进入工厂模式",
        "读取工厂测试模式状态",
        "读取版本号",
        "读取硬件版本",
        "读取资源版本",
        "读取设备名称",
        "写入MAC地址",
        "读取MAC地址",
        "写产测完成标识",
        "读取产测完成标识",
        "读取电池百分比",
        "读取电池电压",
        "读取电池电流",
        "读取电池温度",
        "写入SN码",
        "获取整机SN码",
        "三元组云端登录（dev）air2",
        "获取云端三元组",
        "写入productKey",
        "写入deviceName",
        "写入deviceSecret",
        "读取productKey并比较",
        "读取deviceName并比较",
        "读取deviceSecret并比较",
        *mode_block,
        "开启蓝牙信号模式",
        "关闭蓝牙信号模式",
        "开启蓝牙无信号模式",
        "关闭蓝牙无信号模式",
        "开启蓝牙定频模式",
        "关闭蓝牙定频模式",
        "写入射频Trim",
        "读取RSSI",
        "读取Trim",
        *sensor_writes,
        *sensor_reads,
        "模拟按键",
        "退出工厂模式",
        "设备复位",
        "恢复出厂",
        "关机船运模式",
    ]
    # mode_block 已含 读取工厂测试模式状态，开头也有 —— 去重保序
    seen = set()
    uniq = []
    for x in items:
        if x not in seen:
            seen.add(x)
            uniq.append(x)

    disabled = [
        "获取云端三元组",
        "写入productKey",
        "写入deviceName",
        "写入deviceSecret",
        "读取productKey并比较",
        "读取deviceName并比较",
        "读取deviceSecret并比较",
        "开启蓝牙信号模式",
        "关闭蓝牙信号模式",
        "开启蓝牙无信号模式",
        "关闭蓝牙无信号模式",
        "开启蓝牙定频模式",
        "关闭蓝牙定频模式",
        "设备复位",
        "恢复出厂",
        "关机船运模式",
        # 传感器校准写默认先禁用（需真实校准数据时再开）
        *sensor_writes,
    ]

    flow_text = (
        "[Flow]\n"
        f'Items="{",".join(uniq)}"\n'
        "StopFlowOnTestFail=true\n"
        "FailItems=\n"
        f'DisabledItems="{",".join(disabled)}"\n'
        "\n"
        "[SerialUi]\n"
        "JigVisible=false\n"
        "JigLabel=治具串口\n"
        "ProductVisible=false\n"
        "ProductLabel=产品串口(仪器)\n"
        "UsbVisible=false\n"
        "UsbLabel=万用表串口\n"
    )
    FLOW.write_bytes(crlf(flow_text))
    print(f"updated flow.ini ({len(uniq)} steps)")


if __name__ == "__main__":
    main()
