# -*- coding: utf-8 -*-
"""根据 work_station/freework/testFunction.cpp FREEWORK_TEST_LIST 生成 test_case ini。"""
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
        f"Field={gate.get('field', '')}",
        f"Op={gate.get('op', 'range')}",
        f"Low={gate.get('low', 0)}",
        f"High={gate.get('high', 0)}",
        f"Expected={gate.get('expected', '')}",
        f"LowSettingsKey={gate.get('low_key', '')}",
        f"HighSettingsKey={gate.get('high_key', '')}",
        "",
        "[Hook]",
        f"Enabled={'true' if hook.get('enabled') else 'false'}",
        f"HookId={hook.get('id', '')}",
        "",
    ]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8-sig") as f:
        f.write("\n".join(lines))


def proto(name, mes, action, cmd, params=None, gate=None, timing=None, prompt=None):
    return {
        "name": name,
        "mes": mes,
        "send": {"action": action, "cmd": cmd, "params": params or {}},
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
            "params": params or {"Send/Param/string": "$MAC"},
        },
        "gate": {"enabled": False},
        "hook": {"enabled": False},
        "timing": timing or {"command_timeout": 6000},
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
        "prompt_enabled": bool(prompt_text),
    }


def rssi_gate():
    return {
        "enabled": True,
        "report": "ProtocolRssiData",
        "field": "dbm",
        "op": "range",
        "low": 0,
        "high": 0,
        "low_key": "BLE/LowRssi",
        "high_key": "BLE/HighRssi",
    }


def charge_gate():
    return {
        "enabled": True,
        "report": "ProtocolUInt32ValueData",
        "field": "value",
        "op": "range",
        "low": 0,
        "high": 0,
        "low_key": "Current/LowCharCurrent",
        "high_key": "Current/HighCharCurrent",
    }


def battery_gate():
    return {
        "enabled": True,
        "report": "ProtocolBatteryData",
        "field": "percent",
        "op": "gt",
        "low": 0,
        "high": 100,
        "low_key": "BATTARY/standbattary",
        "high_key": "",
    }


CASES = [
    proto("禁止休眠", "FORBID_SLEEP", "Set", "ForbidSleep", {"Param/int": 1}),
    proto("获取整机SN码", "TAIL_SN_READ", "Get", "Sn", {"Param/int": 1}),
    proto("获取基本信息", "BASE_INFO", "Get", "BaseInfo"),
    proto("获取电量信息", "BATTERY_INFO", "Get", "GetBattery", gate=battery_gate()),
    proto("关机", "SHIP_MODE", "Set", "ShipMode", {"Param/int": 1}),
    proto("产测完成写入", "FAC_RESULT_WRITE", "Set", "FacResult", {"Param/int": 1}),
    proto(
        "设置老化测试模式",
        "AGING_MODE_SET",
        "Set",
        "BurningMode",
        {"Param/mode": 1, "Param/seconds": 14400, "Param/switch": 1},
    ),
    proto("设置休眠状态", "SLEEP_CMD", "Set", "Sleep", {"Param/int": 1}),
    proto("设置工厂模式", "FAC_MODE_SET", "Set", "FacMode", {"Param/int": 1}),
    # Sn 载荷由 Hook 从 MES expectedTailSnFromMes 填入；Param/int=1 即 FacDevInfoType_TAIL_SN
    hook("写入SN码", "SN_WRITE_TAIL", send=("Set", "Sn", {"Param/int": 1})),
    proto("获取外围设备状态", "PERIPH_STATE", "Get", "PeriphState"),
    proto("获取WiFi信息", "WIFI_INFO", "Get", "WifiInfo"),
    hook("读取治具电流测量值", "JIG_CURRENT_READ"),
    dongle("直连接蓝牙", "BT_DIRECT_DCON", "BleDirectConnect"),
    proto(
        "获取BT RSSI",
        "BT_RSSI",
        "Get",
        "RssiRead",
        {"Param/mode": 1},
        gate=rssi_gate(),
    ),
    proto(
        "获取BLE RSSI",
        "BLE_RSSI",
        "Get",
        "RssiRead",
        {"Param/mode": 0},
        gate=rssi_gate(),
    ),
    proto("读取充电电流", "CHARGE_CURRENT_READ", "Get", "ChargeCurrentRead", gate=charge_gate()),
    hook("获取云端三元组", "CLOUD_TUPLE_FETCH"),
    hook("写入productKey", "WRITE_PRODUCT_KEY", send=("Set", "Sn", {"Param/int": 7})),
    hook("写入deviceName", "WRITE_DEVICE_NAME", send=("Set", "Sn", {"Param/int": 6})),
    hook("写入deviceSecret", "WRITE_DEVICE_SECRET", send=("Set", "WriteKey", {})),
    proto("读取设备三元组并比较", "READ_TUPLE_COMPARE", "Get", "TupleRead"),
    hook("上报三元组写入记录", "TUPLE_WRITE_REPORT"),
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
    proto("进入吸力测试模式", "SUCTION_MODE_ENTER", "Set", "SuctionMode", {"Param/enter": 1}),
    proto("退出吸力测试模式", "SUCTION_MODE_EXIT", "Set", "SuctionMode", {"Param/enter": 0}),
    hook("PLC_Modbus连接", "PLC_MODBUS_CONN"),
    hook("PLC_V3_mode触摸整步", "PLC_V3_KEY_MODE"),
    hook("PLC_V3_program触摸整步", "PLC_V3_KEY_PROGRAM"),
    hook("PLC_V3_speed触摸整步", "PLC_V3_KEY_SPEED"),
    hook("PLC_V3_right触摸整步", "PLC_V3_KEY_RIGHT"),
    hook("PLC_V3_start_pause触摸整步", "PLC_V3_KEY_START_PAUSE"),
    hook("PLC_V3_left触摸整步", "PLC_V3_KEY_LEFT"),
    hook("PLC_V3_power触摸整步", "PLC_V3_KEY_POWER"),
    hook("PLC_V3_switch旋钮整步右旋", "PLC_V3_SWITCH_RIGHT_WHOLE"),
    hook("产品串口仪器复位应答1", "PROD_INST_RESET_ACK_1"),
    hook("产品串口仪器复位应答2", "PROD_INST_RESET_ACK_2"),
    hook("产品串口仪器复位应答3", "PROD_INST_RESET_ACK_3"),
    hook("产品串口仪器复位应答4", "PROD_INST_RESET_ACK_4"),
    hook("产品串口仪器复位应答5", "PROD_INST_RESET_ACK_5"),
    hook("产品串口仪器复位应答6", "PROD_INST_RESET_ACK_6"),
    hook("产品串口开始接收2402_BLE1M", "PROD_INST_START_RX_2402_1M"),
    hook("产品串口开始接收2440_BLE1M", "PROD_INST_START_RX_2440_1M"),
    hook("产品串口开始接收2480_BLE1M", "PROD_INST_START_RX_2480_1M"),
    hook("产品串口开始接收2402_BLE2M", "PROD_INST_START_RX_2402_2M"),
    hook("产品串口开始接收2440_BLE2M", "PROD_INST_START_RX_2440_2M"),
    hook("产品串口开始接收2480_BLE2M", "PROD_INST_START_RX_2480_2M"),
    hook("并联CMW播放Profile0", "FREE_INSTR_CMW_GPRF_P0"),
    hook("并联CMW播放Profile1", "FREE_INSTR_CMW_GPRF_P1"),
    hook("并联CMW播放Profile2", "FREE_INSTR_CMW_GPRF_P2"),
    hook("并联CMW播放Profile3", "FREE_INSTR_CMW_GPRF_P3"),
    hook("并联CMW播放Profile4", "FREE_INSTR_CMW_GPRF_P4"),
    hook("并联CMW播放Profile5", "FREE_INSTR_CMW_GPRF_P5"),
    hook("产品串口停止接收与PER1", "PROD_INST_STOP_RX_PER_1"),
    hook("产品串口停止接收与PER2", "PROD_INST_STOP_RX_PER_2"),
    hook("产品串口停止接收与PER3", "PROD_INST_STOP_RX_PER_3"),
    hook("产品串口停止接收与PER4", "PROD_INST_STOP_RX_PER_4"),
    hook("产品串口停止接收与PER5", "PROD_INST_STOP_RX_PER_5"),
    hook("产品串口停止接收与PER6", "PROD_INST_STOP_RX_PER_6"),
    proto(
        "进入蓝牙非信令模式",
        "BT_NO_SIGNAL_ENTER",
        "Set",
        "BtNoSignalMode",
        {"Param/enter": 1},
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
