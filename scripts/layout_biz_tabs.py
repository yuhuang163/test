# -*- coding: utf-8 -*-
"""把业务控制各页内容写进 mainwindow.ui，Designer 与运行时一致。"""
from pathlib import Path

UI = Path(r"D:\code\new_product_test\mainwindow.ui")
text = UI.read_text(encoding="utf-8")


def find_widget(src: str, name: str):
    needle = f'name="{name}"'
    pos = 0
    while True:
        idx = src.find(needle, pos)
        if idx < 0:
            raise SystemExit(f"widget not found: {name}")
        start = src.rfind("<widget", 0, idx)
        if start < 0:
            pos = idx + 1
            continue
        gt = src.find(">", start)
        if gt < 0:
            raise SystemExit(f"bad widget tag: {name}")
        tag = src[start : gt + 1]
        if f'name="{name}"' not in tag:
            pos = idx + 1
            continue
        stripped = tag.rstrip()
        if stripped.endswith("/>"):
            return start, gt + 1, src[start : gt + 1]
        i = gt + 1
        depth = 1
        while i < len(src):
            if src.startswith("<widget", i):
                depth += 1
                i = src.find(">", i) + 1
                continue
            if src.startswith("</widget>", i):
                depth -= 1
                i += len("</widget>")
                if depth == 0:
                    return start, i, src[start:i]
                continue
            i += 1
        raise SystemExit(f"unclosed widget: {name}")


def extract_item(src: str, name: str):
    w0, w1, wxml = find_widget(src, name)
    i = w0
    while i > 0 and src[i - 1] in " \t\r\n":
        i -= 1
    item_open = src.rfind("<item", 0, i)
    if item_open < 0:
        raise SystemExit(f"no <item> before {name}")
    j = w1
    while j < len(src) and src[j] in " \t\r\n":
        j += 1
    if not src.startswith("</item>", j):
        raise SystemExit(f"no </item> after {name}")
    item_end = j + len("</item>")
    k = item_end
    while k < len(src) and src[k] in " \t\r\n":
        k += 1
    return item_open, k, wxml


def take_widget(src: str, name: str):
    a, b, wxml = extract_item(src, name)
    src = src[:a] + src[b:]
    return src, wxml.strip()


def set_btn_text(wxml: str, title: str) -> str:
    old = wxml.find("<string>")
    end = wxml.find("</string>", old)
    if old < 0 or end < 0:
        return wxml
    return wxml[:old] + f"<string>{title}</string>" + wxml[end + len("</string>") :]


def add_visible_false(src: str, name: str) -> str:
    w0, w1, wxml = find_widget(src, name)
    if "name=\"visible\"" in wxml:
        return src
    if wxml.rstrip().endswith("/>"):
        inner = wxml[:-2].rstrip()
        if inner.endswith("/"):
            inner = inner[:-1].rstrip()
        inner = inner + ">"
        wxml = (
            inner
            + "\n                             <property name=\"visible\">\n"
            + "                              <bool>false</bool>\n"
            + "                             </property>\n"
            + "                            </widget>"
        )
        return src[:w0] + wxml + src[w1:]
    gt = src.find(">", w0)
    insert = (
        "\n                             <property name=\"visible\">\n"
        "                              <bool>false</bool>\n"
        "                             </property>"
    )
    return src[: gt + 1] + insert + src[gt + 1 :]


def find_layout_close(src: str, layout_name: str) -> int:
    marker = f'name="{layout_name}">'
    pos = src.find(marker)
    if pos < 0:
        raise SystemExit(f"layout not found: {layout_name}")
    lay_start = src.rfind("<layout", 0, pos)
    k = lay_start
    depth = 0
    while k < len(src):
        if src.startswith("<layout", k):
            depth += 1
            k = src.find(">", k) + 1
            continue
        if src.startswith("</layout>", k):
            depth -= 1
            if depth == 0:
                return k
            k += len("</layout>")
            continue
        k += 1
    raise SystemExit(f"layout close not found: {layout_name}")


def item_wrap(inner: str, indent: str, row=None, col=None) -> str:
    attr = ""
    if row is not None and col is not None:
        attr = f' row="{row}" column="{col}"'
    body = inner.replace("\n", "\n" + indent)
    return f"{indent}<item{attr}>\n{indent} {body}\n{indent}</item>\n"


# --- take widgets ---
factory_names = [
    "label_qaiot_fac_mode",
    "qaiot_fac_mode_type",
    "qaiot_enter_fac_mode",
    "qaiot_exit_fac_mode",
    "qaiot_read_aging_status",
    "qaiot_travel_lock",
    "qaiot_read_soft_ver",
    "qaiot_read_device_name",
    "enterShipModeButton",
    "rebot",
    "reset_factory",
    "factory_flag",
    "factory_flag_undone",
    "factory_flag_read",
]
mode_btns = [
    "enterwhitemode",
    "clean_mode",
    "entersleep",
    "forbidsleep",
    "start_brush",
]
meta_btns = [
    "get_device_sn",
    "write_device_sn",
    "get_board_sn",
    "write_board_sn",
    "get_device_mac",
    "set_device_mac",
    "get_device_subpid",
    "write_device_subpid",
    "get_device_skuid",
    "write_device_skuid",
    "kTlvKeyread",
    "kTlvKeyWrite",
    "kTlvKey_data",
]

taken = {}
for name in factory_names + mode_btns + meta_btns + ["groupBox_root_suction_test"]:
    text, taken[name] = take_widget(text, name)
    print("took", name)

combo = taken["qaiot_fac_mode_type"]
if combo.rstrip().endswith("/>"):
    taken["qaiot_fac_mode_type"] = """<widget class="QComboBox" name="qaiot_fac_mode_type">
                             <item>
                              <property name="text">
                               <string>工厂模式</string>
                              </property>
                             </item>
                             <item>
                              <property name="text">
                               <string>老化模式</string>
                              </property>
                             </item>
                             <item>
                              <property name="text">
                               <string>吸力模式</string>
                              </property>
                             </item>
                             <item>
                              <property name="text">
                               <string>吸力补偿</string>
                              </property>
                             </item>
                             <item>
                              <property name="text">
                               <string>ATE模式</string>
                              </property>
                             </item>
                             <item>
                              <property name="text">
                               <string>单机模式</string>
                              </property>
                             </item>
                             <item>
                              <property name="text">
                               <string>空闲模式</string>
                              </property>
                             </item>
                            </widget>"""

taken["qaiot_enter_fac_mode"] = set_btn_text(taken["qaiot_enter_fac_mode"], "进入")
taken["qaiot_exit_fac_mode"] = set_btn_text(taken["qaiot_exit_fac_mode"], "退出")
taken["qaiot_read_aging_status"] = set_btn_text(taken["qaiot_read_aging_status"], "读取")
for n, title in [
    ("get_device_sn", "读取"),
    ("write_device_sn", "写入"),
    ("get_board_sn", "读取"),
    ("write_board_sn", "写入"),
    ("get_device_mac", "读取"),
    ("set_device_mac", "写入"),
    ("get_device_subpid", "读取"),
    ("write_device_subpid", "写入"),
    ("get_device_skuid", "读取"),
    ("write_device_skuid", "写入"),
    ("kTlvKeyread", "读取"),
    ("kTlvKeyWrite", "写入"),
]:
    taken[n] = set_btn_text(taken[n], title)

factory_xml = f"""                      <item>
                       <widget class="QGroupBox" name="groupBox_business_factory">
                        <property name="title">
                         <string>工厂与电源</string>
                        </property>
                        <property name="sizePolicy">
                         <sizepolicy hsizetype="Expanding" vsizetype="Minimum">
                          <horstretch>0</horstretch>
                          <verstretch>0</verstretch>
                         </sizepolicy>
                        </property>
                        <layout class="QVBoxLayout" name="verticalLayout_business_factory">
                         <property name="spacing">
                          <number>10</number>
                         </property>
                         <property name="leftMargin">
                          <number>12</number>
                         </property>
                         <property name="topMargin">
                          <number>10</number>
                         </property>
                         <property name="rightMargin">
                          <number>12</number>
                         </property>
                         <property name="bottomMargin">
                          <number>12</number>
                         </property>
                         <item>
                          <layout class="QHBoxLayout" name="horizontalLayout_factory_mode">
                           <item>
                            {taken["label_qaiot_fac_mode"]}
                           </item>
                           <item>
                            {taken["qaiot_fac_mode_type"]}
                           </item>
                           <item>
                            {taken["qaiot_enter_fac_mode"]}
                           </item>
                           <item>
                            {taken["qaiot_exit_fac_mode"]}
                           </item>
                           <item>
                            {taken["qaiot_read_aging_status"]}
                           </item>
                           <item>
                            <spacer name="horizontalSpacer_factory_mode">
                             <property name="orientation">
                              <enum>Qt::Horizontal</enum>
                             </property>
                             <property name="sizeHint" stdset="0">
                              <size>
                               <width>40</width>
                               <height>20</height>
                              </size>
                             </property>
                            </spacer>
                           </item>
                          </layout>
                         </item>
                         <item>
                          <layout class="QGridLayout" name="gridLayout_factory_power">
                           <property name="horizontalSpacing">
                            <number>8</number>
                           </property>
                           <property name="verticalSpacing">
                            <number>8</number>
                           </property>
                           <item row="0" column="0">
                            {taken["qaiot_travel_lock"]}
                           </item>
                           <item row="0" column="1">
                            {taken["qaiot_read_soft_ver"]}
                           </item>
                           <item row="0" column="2">
                            {taken["qaiot_read_device_name"]}
                           </item>
                           <item row="1" column="0">
                            {taken["enterShipModeButton"]}
                           </item>
                           <item row="1" column="1">
                            {taken["rebot"]}
                           </item>
                           <item row="1" column="2">
                            {taken["reset_factory"]}
                           </item>
                           <item row="2" column="0">
                            {taken["factory_flag"]}
                           </item>
                           <item row="2" column="1">
                            {taken["factory_flag_undone"]}
                           </item>
                           <item row="2" column="2">
                            {taken["factory_flag_read"]}
                           </item>
                          </layout>
                         </item>
                         <item>
                          <spacer name="verticalSpacer_factory">
                           <property name="orientation">
                            <enum>Qt::Vertical</enum>
                           </property>
                           <property name="sizeHint" stdset="0">
                            <size>
                             <width>20</width>
                             <height>40</height>
                            </size>
                           </property>
                          </spacer>
                         </item>
                        </layout>
                       </widget>
                      </item>
"""

mode_xml = f"""                      <item>
                       {taken["groupBox_root_suction_test"]}
                      </item>
                      <item>
                       <widget class="QGroupBox" name="groupBox_business_mode">
                        <property name="title">
                         <string>其它模式</string>
                        </property>
                        <property name="sizePolicy">
                         <sizepolicy hsizetype="Expanding" vsizetype="Minimum">
                          <horstretch>0</horstretch>
                          <verstretch>0</verstretch>
                         </sizepolicy>
                        </property>
                        <layout class="QGridLayout" name="gridLayout_business_mode">
                         <property name="horizontalSpacing">
                          <number>8</number>
                         </property>
                         <property name="verticalSpacing">
                          <number>8</number>
                         </property>
                         <item row="0" column="0">
                          {taken["enterwhitemode"]}
                         </item>
                         <item row="0" column="1">
                          {taken["clean_mode"]}
                         </item>
                         <item row="0" column="2">
                          {taken["entersleep"]}
                         </item>
                         <item row="1" column="0">
                          {taken["forbidsleep"]}
                         </item>
                         <item row="1" column="1">
                          {taken["start_brush"]}
                         </item>
                        </layout>
                       </widget>
                      </item>
                      <item>
                       <spacer name="verticalSpacer_biz_mode">
                        <property name="orientation">
                         <enum>Qt::Vertical</enum>
                        </property>
                        <property name="sizeHint" stdset="0">
                         <size>
                          <width>20</width>
                          <height>40</height>
                         </size>
                        </property>
                       </spacer>
                      </item>
"""


def meta_row(obj, label, ph, read_xml, write_xml):
    return f"""                        <widget class="QWidget" name="consoleMetaIo_{obj}">
                         <layout class="QHBoxLayout" name="consoleMetaIoLay_{obj}">
                          <property name="spacing">
                           <number>6</number>
                          </property>
                          <property name="leftMargin">
                           <number>0</number>
                          </property>
                          <property name="topMargin">
                           <number>0</number>
                          </property>
                          <property name="rightMargin">
                           <number>0</number>
                          </property>
                          <property name="bottomMargin">
                           <number>0</number>
                          </property>
                          <item>
                           <widget class="QLabel" name="consoleMetaLab_{obj}">
                            <property name="minimumSize">
                             <size>
                              <width>52</width>
                              <height>0</height>
                             </size>
                            </property>
                            <property name="text">
                             <string>{label}</string>
                            </property>
                            <property name="alignment">
                             <set>Qt::AlignRight|Qt::AlignVCenter</set>
                            </property>
                           </widget>
                          </item>
                          <item>
                           <widget class="QLineEdit" name="{obj}">
                            <property name="placeholderText">
                             <string>{ph}</string>
                            </property>
                            <property name="clearButtonEnabled">
                             <bool>true</bool>
                            </property>
                           </widget>
                          </item>
                          <item>
                           {read_xml}
                          </item>
                          <item>
                           {write_xml}
                          </item>
                         </layout>
                        </widget>"""


key_edit = taken["kTlvKey_data"]
if "placeholderText" not in key_edit:
    gt = key_edit.find(">")
    key_edit = (
        key_edit[: gt + 1]
        + "\n                             <property name=\"placeholderText\">\n"
        + "                              <string>三元密钥</string>\n"
        + "                             </property>"
        + key_edit[gt + 1 :]
    )

meta_xml = f"""                      <item>
                       <widget class="QGroupBox" name="groupBox_business_meta">
                        <property name="title">
                         <string>序列号与设备元数据</string>
                        </property>
                        <property name="sizePolicy">
                         <sizepolicy hsizetype="Expanding" vsizetype="Minimum">
                          <horstretch>0</horstretch>
                          <verstretch>0</verstretch>
                         </sizepolicy>
                        </property>
                        <layout class="QGridLayout" name="gridLayout_business_meta">
                         <property name="horizontalSpacing">
                          <number>12</number>
                         </property>
                         <property name="verticalSpacing">
                          <number>8</number>
                         </property>
                         <item row="0" column="0">
{meta_row("sn_whole", "整机 SN", "整机 SN", taken["get_device_sn"], taken["write_device_sn"])}
                         </item>
                         <item row="0" column="1">
{meta_row("sn_board", "板子 SN", "板子 SN", taken["get_board_sn"], taken["write_board_sn"])}
                         </item>
                         <item row="1" column="0">
{meta_row("meta_mac", "MAC", "设备 MAC", taken["get_device_mac"], taken["set_device_mac"])}
                         </item>
                         <item row="1" column="1">
{meta_row("meta_subpid", "设备 ID", "设备 ID", taken["get_device_subpid"], taken["write_device_subpid"])}
                         </item>
                         <item row="2" column="0">
{meta_row("meta_skuid", "产品 ID", "产品 ID", taken["get_device_skuid"], taken["write_device_skuid"])}
                         </item>
                         <item row="2" column="1">
                        <widget class="QWidget" name="consoleMetaIo_tuple">
                         <layout class="QHBoxLayout" name="consoleMetaIoLay_tuple">
                          <property name="spacing">
                           <number>6</number>
                          </property>
                          <property name="leftMargin">
                           <number>0</number>
                          </property>
                          <property name="topMargin">
                           <number>0</number>
                          </property>
                          <property name="rightMargin">
                           <number>0</number>
                          </property>
                          <property name="bottomMargin">
                           <number>0</number>
                          </property>
                          <item>
                           <widget class="QLabel" name="consoleMetaLab_tuple">
                            <property name="minimumSize">
                             <size>
                              <width>52</width>
                              <height>0</height>
                             </size>
                            </property>
                            <property name="text">
                             <string>三元组</string>
                            </property>
                            <property name="alignment">
                             <set>Qt::AlignRight|Qt::AlignVCenter</set>
                            </property>
                           </widget>
                          </item>
                          <item>
                           {key_edit}
                          </item>
                          <item>
                           {taken["kTlvKeyread"]}
                          </item>
                          <item>
                           {taken["kTlvKeyWrite"]}
                          </item>
                         </layout>
                        </widget>
                         </item>
                        </layout>
                       </widget>
                      </item>
                      <item>
                       <spacer name="verticalSpacer_biz_meta">
                        <property name="orientation">
                         <enum>Qt::Vertical</enum>
                        </property>
                        <property name="sizeHint" stdset="0">
                         <size>
                          <width>20</width>
                          <height>40</height>
                         </size>
                        </property>
                       </spacer>
                      </item>
"""

fc = find_layout_close(text, "verticalLayout_tab_biz_factory")
text = text[:fc] + factory_xml + text[fc:]
mc = find_layout_close(text, "verticalLayout_tab_biz_mode")
text = text[:mc] + mode_xml + text[mc:]
xc = find_layout_close(text, "verticalLayout_tab_biz_meta")
text = text[:xc] + meta_xml + text[xc:]

# 删除空的吸力测试页
suction_start = text.find('<widget class="QWidget" name="tab_biz_suction">')
if suction_start < 0:
    raise SystemExit("tab_biz_suction missing")
i = suction_start
depth = 0
while i < len(text):
    if text.startswith("<widget", i):
        depth += 1
        i = text.find(">", i) + 1
        continue
    if text.startswith("</widget>", i):
        depth -= 1
        i += len("</widget>")
        if depth == 0:
            suction_end = i
            break
        continue
    i += 1
else:
    raise SystemExit("tab_biz_suction unclosed")
# 吃掉前后空白
a = suction_start
while a > 0 and text[a - 1] in " \t":
    a -= 1
if text[a - 2 : a] == "\r\n":
    a -= 2
elif text[a - 1] == "\n":
    a -= 1
b = suction_end
while b < len(text) and text[b] in " \t":
    b += 1
if text.startswith("\r\n", b):
    b += 2
elif text.startswith("\n", b):
    b += 1
text = text[:a] + text[b:]
print("removed tab_biz_suction")

for n in [
    "fac_mode",
    "exit_fac_mode",
    "enterSuctionMode",
    "exitSuctionMode",
    "openCompensationSet",
    "closeCompensationSet",
    "label_qaiot_aging_mode",
    "qaiot_aging_mode",
    "label_109",
]:
    try:
        text = add_visible_false(text, n)
        print("hide", n)
    except SystemExit as e:
        print("skip hide", e)

# Designer 默认停在业务控制
text = text.replace(
    '            <property name="currentIndex">\n             <number>0</number>\n            </property>\n            <widget class="QGroupBox" name="groupBox_5">',
    '            <property name="currentIndex">\n             <number>1</number>\n            </property>\n            <widget class="QGroupBox" name="groupBox_5">',
    1,
)
text = text.replace(
    '                <property name="currentIndex">\n                 <number>2</number>\n                </property>\n                <property name="tabBarAutoHide">',
    '                <property name="currentIndex">\n                 <number>0</number>\n                </property>\n                <property name="tabBarAutoHide">',
    1,
)
text = text.replace(
    '                    <property name="currentIndex">\n                     <number>0</number>\n                    </property>\n                    <property name="usesScrollButtons">',
    '                    <property name="currentIndex">\n                     <number>1</number>\n                    </property>\n                    <property name="usesScrollButtons">',
    1,
)

if text.count('name="groupBox_business_factory"') != 1:
    raise SystemExit("factory card count")
if text.count('name="groupBox_root_suction_test"') != 1:
    raise SystemExit("suction box count")
if text.count('name="sn_whole"') != 1:
    raise SystemExit("sn_whole count")

UI.write_text(text, encoding="utf-8")
print("wrote", UI, "len", len(text))
