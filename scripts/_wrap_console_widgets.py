# -*- coding: utf-8 -*-
import xml.etree.ElementTree as ET
from pathlib import Path

ui_path = Path(__file__).resolve().parents[1] / "mainwindow.ui"
wraps = [
    ("consoleStatusDeviceLay", "consoleStatusDevice"),
    ("consoleStatusMotorLay", "consoleStatusMotor"),
    ("consoleStatusPumpLay", "consoleStatusPump"),
    ("consoleStatusValveLay", "consoleStatusValve"),
    ("consoleMetricBatteryLay", "consoleMetricBattery"),
    ("consoleMetricTempLay", "consoleMetricTemp"),
    ("consoleMetricCurrentLay", "consoleMetricCurrent"),
    ("consoleFieldResetLay", "consoleFieldReset"),
    ("consoleHeaderLay", "consoleHeader"),
    ("consoleTitleLay", "consoleTitleBox"),
    ("consoleConnectPageLay", "consoleConnectPage"),
    ("consoleSideBodyLay", "consoleSideBody"),
    ("consoleSideGrid", "consoleSidebar"),
    ("consoleMidLay", "consoleMid"),
    ("consoleRailLay", "consoleRail"),
    ("consoleShellLay", "consoleShell"),
]


def wrap_layout(root, lay_name, widget_name):
    for parent in root.iter():
        children = list(parent)
        for i, child in enumerate(children):
            if child.tag != "layout" or child.get("name") != lay_name:
                continue
            if parent.tag == "widget" and parent.get("name") == widget_name:
                print("SKIP", widget_name)
                return True
            w = ET.Element("widget")
            w.set("class", "QWidget")
            w.set("name", widget_name)
            parent.remove(child)
            w.append(child)
            parent.insert(i, w)
            print("WRAP", lay_name, "->", widget_name)
            return True
    print("MISSING", lay_name)
    return False


tree = ET.parse(ui_path)
root = tree.getroot()
for lay, name in wraps:
    wrap_layout(root, lay, name)

tree.write(ui_path, encoding="UTF-8", xml_declaration=True, short_empty_elements=False)
raw = ui_path.read_bytes()
if raw.startswith(b"\xef\xbb\xbf"):
    raw = raw[3:]
raw = raw.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
ui_path.write_bytes(raw)
print("wrote", ui_path)
