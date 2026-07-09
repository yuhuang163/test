# -*- coding: utf-8 -*-
"""根据脚本内步骤目录生成 test_case/*.ini（原 testFunction.cpp 已移除）。"""
import os

OUT_DIR = os.path.join(
    os.path.dirname(__file__),
    "..",
    "build",
    "Desktop_Qt_5_15_2_MSVC2019_64bit-Release",
    "bin",
    "test_case",
)


def sanitize_name(name: str) -> str:
    for c in '\\/:*?"<>|':
        name = name.replace(c, "")
    return name.strip()


def write_ini(path: str, meta: dict, send: dict, timing: dict, gate: dict, hook: dict) -> None:
    lines = [
        "[Meta]",
        f"Name={meta['name']}",
        f"DisplayName={meta['display']}",
        f"MesTag={meta['mes']}",
        f"PromptEnabled={'true' if meta.get('prompt') else 'false'}",
        f"PromptText={meta.get('prompt_text', '')}",
        "",
        "[Send]",
        f"Channel={send.get('channel', 'Product')}",
        f"Action={send['action']}",
        f"DeviceCmd={send['cmd']}",
    ]
    channel = send.get("channel", "Product")
    if channel == "Product":
        lines.append(f"Protocol={send.get('protocol', 'Qfctp')}")
    for k, v in send.get("params", {}).items():
        lines.append(f"{k}={v}")
    lines += [
        "",
        "[Timing]",
        f"DelayBeforeMs={timing.get('before', 0)}",
        f"DelayAfterMs={timing.get('after', 0)}",
        f"CommandTimeoutMs={timing.get('command_timeout', 300)}",
        "",
        "[Gate]",
        f"Enabled={'true' if gate.get('enabled') else 'false'}",
        f"ReportType={gate.get('report', '')}",
    ]
    multi = gate.get("multi_gates")
    if multi:
        lines += [
            "Field=multi",
            f"Count={len(multi)}",
            "",
        ]
        for idx, item in enumerate(multi, start=1):
            field, expected = item[0], item[1]
            lines += [
                f"[Gate/{idx}]",
                f"Field={field}",
                "Op=eq",
                f"Expected={expected}",
                f"Low={expected}",
                f"High={expected}",
                "",
            ]
    else:
        lines += [
            f"Field={gate.get('field', '')}",
            f"Op={gate.get('op', 'range')}",
            f"Low={gate.get('low', 0)}",
            f"High={gate.get('high', 0)}",
            f"Expected={gate.get('expected', '')}",
            f"ExpectedSettingsKey={gate.get('expected_key', '')}",
            f"LowSettingsKey={gate.get('low_key', '')}",
            f"HighSettingsKey={gate.get('high_key', '')}",
            "",
        ]
    lines += [
        "[Hook]",
        f"Enabled={'true' if hook.get('enabled') else 'false'}",
        f"HookId={hook.get('id', '')}",
        "",
    ]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8-sig") as f:
        f.write("\n".join(lines))


def proto(name, mes, action, cmd, params=None, gate=None, timing=None, prompt=None, protocol="Qfctp"):
    return {
        "name": name,
        "mes": mes,
        "send": {
            "action": action,
            "channel": "Product",
            "cmd": cmd,
            "protocol": protocol,
            "params": params or {},
        },
        "gate": gate or {"enabled": False},
        "hook": {"enabled": False},
        "timing": timing or {},
        "prompt": prompt,
    }


def dongle(name, mes, cmd, params=None, timing=None):
    return {
        "name": name,
        "mes": mes,
        "send": {
            "action": "Set",
            "channel": "Dongle",
            "cmd": cmd,
            "params": params or {"Param_string": "$MAC"},
        },
        "gate": {"enabled": False},
        "hook": {"enabled": False},
        "timing": timing or {"command_timeout": 6000},
    }


def cloud(name, mes, action, cmd, params=None, timing=None):
    return {
        "name": name,
        "mes": mes,
        "send": {
            "action": action,
            "channel": "Cloud",
            "cmd": cmd,
            "params": params or {},
        },
        "gate": {"enabled": False},
        "hook": {"enabled": False},
        "timing": timing or {},
    }


def product_serial(name, mes, cmd, timing=None):
    """产品串口仪器步骤：Channel=ProductSerial，不再使用 Hook。"""
    return {
        "name": name,
        "mes": mes,
        "send": {
            "action": "Set",
            "channel": "ProductSerial",
            "cmd": cmd,
            "params": {},
        },
        "gate": {"enabled": False},
        "hook": {"enabled": False},
        "timing": timing or {"command_timeout": 30000},
    }


def hook(name, mes, hook_id=None, timing=None, prompt_text=None, send=None):
    """仅 Hook 执行的步骤：Send 供设置页校验与展示，须与 Hook 实际协议一致；未指定时用 DevReset 占位。"""
    if send is None:
        send_dict = {"action": "Set", "cmd": "DevReset", "params": {}}
    else:
        action, cmd, params = send
        send_dict = {"action": action, "cmd": cmd, "params": params or {}}
    return {
        "name": name,
        "mes": mes,
        "send": send_dict,
        "gate": {"enabled": False},
        "hook": {"enabled": True, "id": hook_id or mes},
        "timing": timing or {},
        "prompt_text": prompt_text or "",
        # Hook 内自带弹窗；勿开 Meta/Prompt，否则 test_case 流程会叠两个 QMessageBox
        "prompt_enabled": False,
    }


def base_info_gate(expected="0.3.30"):
    return {
        "enabled": True,
        "report": "ProtocolBaseInfoData",
        "field": "soft_version",
        "op": "compareVersions",
        "expected": expected,
        "expected_key": "",
    }


def rssi_gate():
    return {
        "enabled": True,
        "report": "ProtocolRssiData",
        "field": "dbm",
        "op": "range",
        "low": -65,
        "high": -10,
        "low_key": "",
        "high_key": "",
    }


def charge_gate():
    return {
        "enabled": True,
        "report": "ProtocolChargeCurrentData",
        "field": "currentMa",
        "op": "range",
        "low": 0,
        "high": 0,
        "low_key": "Current/LowCharCurrent",
        "high_key": "Current/HighCharCurrent",
    }


TUPLE_ENV_PRESETS = [
    ("dev", "http://192.168.200.140:8080"),
    ("prod", "https://lute-aiot-mac.luteos.com"),
    ("build-dev-inner", "http://192.168.200.140:8080"),
    ("build-dev", "https://983ug2va5885.vicp.fun"),
    ("build-prod", "https://lute-aiot-mac.luteos.com"),
]


def tuple_cloud_login_cases():
    cases = []
    for env_key, base_url in TUPLE_ENV_PRESETS:
        display = f"三元组云端登录（{env_key}）"
        mes = f"CLOUD_TUPLE_LOGIN_{env_key.upper().replace('-', '_')}"
        cases.append(
            cloud(
                display,
                mes,
                "Set",
                "Login",
                {
                    "Param_baseUrl": base_url,
                    "Param_userName": "INTER_QC",
                    "Param_password": "Aa123456",
                },
            )
        )
    return cases


def battery_gate():
    return {
        "enabled": True,
        "report": "ProtocolBatteryData",
        "field": "percent",
        "op": "range",
        "low": 0,
        "high": 100,
        "low_key": "",
        "high_key": "",
    }


def periph_gate():
    """外设状态：每项独立期望值（等于）。"""
    return {
        "enabled": True,
        "report": "ProtocolPeriphStateData",
        "multi_gates": [
            ("press0_state", "0"),
            ("press1_state", "0"),
            ("battery_ic_state", "1"),
            ("touch_ic_state", "1"),
            ("led_ic_state", "0"),
            ("pd_ic_state", "-1"),
        ],
    }


CASES = [
    proto("禁止休眠", "FORBID_SLEEP", "Set", "ForbidSleep", {"Param_int": 1}),
    proto("获取整机SN码", "TAIL_SN_READ", "Get", "Sn", {"Param_int": 1}),
    proto("读取版本号", "BASE_INFO", "Get", "SoftVersionRead", gate=base_info_gate(), timing={"command_timeout": 8000}),
    proto("获取电量信息", "BATTERY_INFO", "Get", "GetBattery", gate=battery_gate()),
    proto("关机", "SHIP_MODE", "Set", "ShipMode", {"Param_int": 1}),
    proto("产测完成写入", "FAC_RESULT_WRITE", "Set", "FacResult", {"Param_int": 1}),
    proto(
        "设置老化测试模式",
        "AGING_MODE_SET",
        "Set",
        "BurningMode",
        {"Param_mode": 1, "Param_seconds": 14400, "Param_switch": 1},
    ),
    proto("设置休眠状态", "SLEEP_CMD", "Set", "Sleep", {"Param_int": 1}),
    proto("进入工厂模式", "FAC_MODE_SET", "Set", "FacMode", {"Param_value": 1}),
    proto("退出工厂模式", "FAC_MODE_EXIT", "Set", "FacMode", {"Param_value": 0}),
    # Sn 载荷由 Hook 从 MES expectedTailSnFromMes 填入；Param_int=1 即 FacDevInfoType_TAIL_SN
    hook("写入SN码", "SN_WRITE_TAIL", send=("Set", "Sn", {"Param_int": 1})),
    proto("获取外围设备状态", "PERIPH_STATE", "Get", "PeriphState", gate=periph_gate()),
    proto("获取WiFi信息", "WIFI_INFO", "Get", "WifiInfo"),
    hook("读取治具电流测量值", "JIG_CURRENT_READ"),
    dongle("直连接蓝牙", "BT_DIRECT_DCON", "BleDirectConnect"),
    proto(
        "获取BT RSSI",
        "BT_RSSI",
        "Get",
        "RssiRead",
        {"Param_mode": 1},
        gate=rssi_gate(),
    ),
    proto(
        "获取BLE RSSI",
        "BLE_RSSI",
        "Get",
        "RssiRead",
        {"Param_mode": 0},
        gate=rssi_gate(),
    ),
    proto("读取充电电流", "CHARGE_CURRENT_READ", "Get", "ChargeCurrentRead", gate=charge_gate()),
    *tuple_cloud_login_cases(),
    cloud(
        "获取云端三元组",
        "CLOUD_TUPLE_FETCH",
        "Get",
        "ApplyTupleByMac",
        {"Param_mac": "$MAC", "Param_sku": "PH9", "Param_position": "L"},
    ),
    proto(
        "写入productKey",
        "WRITE_PRODUCT_KEY",
        "Set",
        "Sn",
        {"Param_which_sn": 7, "Param_sn": "$TUPLE_PRODUCT_KEY"},
    ),
    proto(
        "写入deviceName",
        "WRITE_DEVICE_NAME",
        "Set",
        "Sn",
        {"Param_which_sn": 6, "Param_sn": "$TUPLE_DEVICE_NAME"},
    ),
    proto(
        "写入deviceSecret",
        "WRITE_DEVICE_SECRET",
        "Set",
        "WriteKey",
        {"Param_value": "$TUPLE_DEVICE_SECRET"},
    ),
    proto("读取设备三元组并比较", "READ_TUPLE_COMPARE", "Get", "TupleRead"),
    cloud("上报三元组写入记录", "TUPLE_WRITE_REPORT", "Set", "ReportWriteRecord"),
    dongle("扫描连接蓝牙", "BT_SCAN_MAC", "BleScanConnect"),
    hook("电源键测试", "KEY_POWER", prompt_text="请短按下旋钮"),
    hook("开始/暂停键测试", "KEY_START_PAUSE", prompt_text="请短按下开始/暂停按钮"),
    hook("模式键测试", "KEY_MODE", prompt_text="请短按下模式按钮"),
    hook("速度键测试", "KEY_SPEED", prompt_text="请短按下速度按钮"),
    hook("程序键测试", "KEY_PROGRAM", prompt_text="请短按下程序按钮"),
    hook("左键测试", "KEY_LEFT", prompt_text="请短按下左按钮"),
    hook("右键测试", "KEY_RIGHT", prompt_text="请短按下右按钮"),
    hook("左旋键测试", "KEY_ROT_LEFT", prompt_text="请左旋按钮"),
    hook("右旋键测试", "KEY_ROT_RIGHT", prompt_text="请右旋按钮"),
    proto("进入吸力测试模式", "SUCTION_MODE_ENTER", "Set", "SuctionMode", {"Param_enter": 1}),
    proto("退出吸力测试模式", "SUCTION_MODE_EXIT", "Set", "SuctionMode", {"Param_enter": 0}),
    hook("PLC_Modbus连接", "PLC_MODBUS_CONN"),
    hook("PLC_V3_mode触摸整步", "PLC_V3_KEY_MODE"),
    hook("PLC_V3_program触摸整步", "PLC_V3_KEY_PROGRAM"),
    hook("PLC_V3_speed触摸整步", "PLC_V3_KEY_SPEED"),
    hook("PLC_V3_right触摸整步", "PLC_V3_KEY_RIGHT"),
    hook("PLC_V3_start_pause触摸整步", "PLC_V3_KEY_START_PAUSE"),
    hook("PLC_V3_left触摸整步", "PLC_V3_KEY_LEFT"),
    hook("PLC_V3_power触摸整步", "PLC_V3_KEY_POWER"),
    hook("PLC_V3_switch旋钮整步右旋", "PLC_V3_SWITCH_RIGHT_WHOLE"),
    product_serial("产品串口仪器复位应答", "PROD_INST_RESET_ACK", "InstrumentReset"),
    product_serial("产品串口开始接收2402_BLE1M", "PROD_INST_START_RX_2402_1M", "StartRx2402Ble1M"),
    product_serial("产品串口开始接收2440_BLE1M", "PROD_INST_START_RX_2440_1M", "StartRx2440Ble1M"),
    product_serial("产品串口开始接收2480_BLE1M", "PROD_INST_START_RX_2480_1M", "StartRx2480Ble1M"),
    product_serial("产品串口开始接收2402_BLE2M", "PROD_INST_START_RX_2402_2M", "StartRx2402Ble2M"),
    product_serial("产品串口开始接收2440_BLE2M", "PROD_INST_START_RX_2440_2M", "StartRx2440Ble2M"),
    product_serial("产品串口开始接收2480_BLE2M", "PROD_INST_START_RX_2480_2M", "StartRx2480Ble2M"),
    hook("并联CMW播放2402_BLE1M", "FREE_INSTR_CMW_GPRF_2402_1M"),
    hook("并联CMW播放2440_BLE1M", "FREE_INSTR_CMW_GPRF_2440_1M"),
    hook("并联CMW播放2480_BLE1M", "FREE_INSTR_CMW_GPRF_2480_1M"),
    hook("并联CMW播放2402_BLE2M", "FREE_INSTR_CMW_GPRF_2402_2M"),
    hook("并联CMW播放2440_BLE2M", "FREE_INSTR_CMW_GPRF_2440_2M"),
    hook("并联CMW播放2480_BLE2M", "FREE_INSTR_CMW_GPRF_2480_2M"),
    product_serial("产品串口停止接收与PER", "PROD_INST_STOP_RX_PER", "StopRxAndPer"),
    proto(
        "进入蓝牙非信令模式",
        "BT_NO_SIGNAL_ENTER",
        "Set",
        "BtNoSignalMode",
        {"Param_enter": 1},
        timing={"after": 3000},
    ),
    hook("PLC_V3_switch测试完成M复位", "PLC_V3_SWITCH_DONE_RESET_M"),
]


def main():
    out = os.path.normpath(OUT_DIR)
    written = []
    for raw in CASES:
        c = dict(raw)
        name = c["name"]
        file_base = sanitize_name(name)
        meta = {
            "name": name,
            "display": name,
            "mes": c["mes"],
            "prompt": c.get("prompt_enabled", False),
            "prompt_text": c.get("prompt_text", ""),
        }
        path = os.path.join(out, file_base + ".ini")
        write_ini(path, meta, c["send"], c.get("timing", {}), c["gate"], c["hook"])
        written.append(file_base + ".ini")
    print(f"Wrote {len(written)} ini files to {out}")
    for w in written:
        print(" ", w)


if __name__ == "__main__":
    main()
