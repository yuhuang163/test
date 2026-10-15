# -*- coding: utf-8 -*-
"""Patch mainwindow.ui: nav icons, connect flex, peripheral tab shells."""
from pathlib import Path
import re

UI = Path(r"D:\code\new_product_test\mainwindow.ui")
text = UI.read_text(encoding="utf-8")

# connect page: remove fixed max width 920
old = (
    '                <property name="maximumSize">\n'
    "                 <size>\n"
    "                  <width>920</width>\n"
    "                  <height>16777215</height>\n"
    "                 </size>\n"
    "                </property>\n"
    '                <layout class="QVBoxLayout" name="consoleConnectPageLay">'
)
new = (
    '                <property name="sizePolicy">\n'
    '                 <sizepolicy hsizetype="Expanding" vsizetype="Preferred">\n'
    "                  <horstretch>1</horstretch>\n"
    "                  <verstretch>0</verstretch>\n"
    "                 </sizepolicy>\n"
    "                </property>\n"
    '                <layout class="QVBoxLayout" name="consoleConnectPageLay">'
)
if old in text:
    text = text.replace(old, new, 1)
    print("connect max removed")
else:
    print("WARN: connect max not found")

# groupBox_2: remove maxHeight 180 so Designer matches expandable state page
old2 = (
    '                    <property name="maximumSize">\n'
    "                     <size>\n"
    "                      <width>16777215</width>\n"
    "                      <height>180</height>\n"
    "                     </size>\n"
    "                    </property>\n"
    "                   </widget>\n"
    "                  </item>\n"
    "                  <item>\n"
    '                   <widget class="QTabWidget" name="tabWidget_debug_peripheral">'
)
new2 = (
    "                   </widget>\n"
    "                  </item>\n"
    "                  <item>\n"
    '                   <widget class="QTabWidget" name="tabWidget_debug_peripheral">'
)
if old2 in text:
    text = text.replace(old2, new2, 1)
    print("groupBox_2 max removed")
else:
    print("WARN: groupBox_2 max not found")

icons = [
    ("consoleNavConnect", "nav-connect"),
    ("consoleNavBiz", "nav-business"),
    ("consoleNavPeriph", "nav-peripheral"),
    ("consoleNavDongle", "nav-dongle"),
    ("consoleNavOta", "nav-ota"),
    ("consoleNavMisc", "nav-misc"),
]
for name, base in icons:
    needle = f'<widget class="QPushButton" name="{name}">'
    if f"nav/{base}.png" in text and text.find(f"nav/{base}.png") < text.find(needle) + 500:
        # crude already-check
        pass
    icon_xml = (
        f'<widget class="QPushButton" name="{name}">\n'
        '               <property name="icon">\n'
        '                <iconset resource="new_production.qrc">\n'
        f"                 <normaloff>:/stytle/picture/nav/{base}.png</normaloff>\n"
        f"                 <normalon>:/stytle/picture/nav/{base}.png</normalon>\n"
        f"                 <activeon>:/stytle/picture/nav/{base}.png</activeon>\n"
        f"                 <selectedon>:/stytle/picture/nav/{base}.png</selectedon>\n"
        f"                 :/stytle/picture/nav/{base}.png\n"
        "                </iconset>\n"
        "               </property>\n"
        '               <property name="iconSize">\n'
        "                <size>\n"
        "                 <width>18</width>\n"
        "                 <height>18</height>\n"
        "                </size>\n"
        "               </property>"
    )
    idx = text.find(needle)
    if idx < 0:
        print("missing", name)
        continue
    window = text[idx : idx + 450]
    if f"nav/{base}.png" in window:
        print("already", name)
        continue
    text = text[:idx] + icon_xml + text[idx + len(needle) :]
    print("icon", name)

# Rename existing peripheral sub-tab titles to match HTML
replacements = [
    (
        'name="screenInspectPage">\n                     <attribute name="title">\n                      <string>屏幕测试</string>',
        'name="screenInspectPage">\n                     <attribute name="title">\n                      <string>屏幕</string>',
    ),
    (
        'name="tab_13">\n                     <attribute name="title">\n                      <string>',
        None,  # handle below
    ),
]
text = text.replace(
    'name="screenInspectPage">\n                     <attribute name="title">\n                      <string>屏幕测试</string>',
    'name="screenInspectPage">\n                     <attribute name="title">\n                      <string>屏幕</string>',
    1,
)

def set_tab_title(widget_name: str, title: str):
    global text
    pat = re.compile(
        rf'(name="{widget_name}">\s*<attribute name="title">\s*<string>)(.*?)(</string>)',
        re.S,
    )
    m = pat.search(text)
    if not m:
        print("title miss", widget_name)
        return
    text = pat.sub(rf"\g<1>{title}\g<3>", text, count=1)
    print("title", widget_name, "->", title)


set_tab_title("tab_13", "LED灯")
set_tab_title("tab_11", "摄像头")
set_tab_title("tab_5", "电机")
# tab_6 was 传感器测试 — keep content but runtime splits; rename for Designer clarity
set_tab_title("tab_6", "传感器(遗留)")

# Insert empty peripheral category tabs before closing of tabWidget_debug_peripheral
# Find the closing of last child tab_5 then tabWidget close: after tab_5 widget ends we have
#                    </widget>\n                   </widget>\n                  </item>
# for peripheral's tab widget.

EMPTY_TABS = [
    ("tab_periph_state", "外设状态"),
    ("tab_periph_vibrator", "振子"),
    ("tab_periph_heat", "加热"),
    ("tab_periph_battery", "电池"),
    ("tab_periph_key", "按键"),
    ("tab_periph_imu", "陀螺仪"),
    ("tab_periph_ble", "蓝牙"),
    ("tab_periph_pump", "泵"),
    ("tab_periph_valve", "阀"),
    ("tab_periph_light", "光感"),
    ("tab_periph_pressure", "压感"),
]

marker = '                   <widget class="QTabWidget" name="tabWidget_debug_peripheral">'
if marker not in text:
    print("WARN: peripheral tabwidget not found")
else:
    # Insert empty pages right after tabWidget opening properties / before screenInspectPage
    insert_at = text.find('<widget class="ScreenInspectWidget" name="screenInspectPage">')
    if insert_at < 0:
        print("WARN: screenInspectPage not found")
    else:
        chunks = []
        for obj, title in EMPTY_TABS:
            if f'name="{obj}"' in text:
                print("tab exists", obj)
                continue
            chunks.append(
                f'                    <widget class="QWidget" name="{obj}">\n'
                f'                     <attribute name="title">\n'
                f"                      <string>{title}</string>\n"
                f"                     </attribute>\n"
                f'                     <layout class="QVBoxLayout" name="verticalLayout_{obj}">\n'
                f'                      <property name="spacing">\n'
                f"                       <number>8</number>\n"
                f"                      </property>\n"
                f'                      <property name="leftMargin">\n'
                f"                       <number>4</number>\n"
                f"                      </property>\n"
                f'                      <property name="topMargin">\n'
                f"                       <number>4</number>\n"
                f"                      </property>\n"
                f'                      <property name="rightMargin">\n'
                f"                       <number>4</number>\n"
                f"                      </property>\n"
                f'                      <property name="bottomMargin">\n'
                f"                       <number>4</number>\n"
                f"                      </property>\n"
                f"                     </layout>\n"
                f"                    </widget>\n"
            )
        if chunks:
            text = text[:insert_at] + "".join(chunks) + text[insert_at:]
            print("inserted", len(chunks), "empty periph tabs")

# Move groupBox_2 from outer layout into tab_periph_state if both exist:
# Too fragile in pure text — leave runtime to fill; Designer at least shows 15 tabs.

# business: rename production group title attribute for clarity
text = text.replace(
    'name="tab_debug_group_production">\n                 <attribute name="title">\n                  <string>产测</string>',
    'name="tab_debug_group_production">\n                 <attribute name="title">\n                  <string>业务控制</string>',
    1,
)

UI.write_text(text, encoding="utf-8", newline="\n")
print("done bytes", len(text.encode("utf-8")))
