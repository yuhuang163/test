# -*- coding: utf-8 -*-
from pathlib import Path

p = Path(r"D:\code\new_product_test\mainwindow.ui")
t = p.read_text(encoding="utf-8")

start = t.find('<widget class="QGroupBox" name="groupBox_dongle_at_bomb">')
end = t.find('<widget class="QGroupBox" name="groupBox_dongle_at_query">')
if start < 0 or end < 0:
    raise SystemExit(f"markers {start} {end}")

# keep the item row="2" column="0" wrapper: start from groupBox open to before query group
# Find the <item row="2" column="0"> that contains bomb
item_start = t.rfind('<item row="2" column="0">', 0, start)
# end at the </item> before query's item - query is also row=2 col=1
# Replace from groupBox bomb through its closing </widget></item>
bomb_end = t.find("</widget>", end - 80)  # not reliable
# Better: find closing of bomb group: after start, match widgets
# Simple: replace from start to the </item>\n                    <item row="2" column="1">
marker = '\n                    <item row="2" column="1">\n                     <widget class="QGroupBox" name="groupBox_dongle_at_query">'
idx = t.find(marker)
if idx < 0:
    raise SystemExit("end marker missing")
# find bomb item start
bomb_item = t.rfind('<item row="2" column="0">', 0, start)
new_item = '''                    <item row="2" column="0">
                     <widget class="QGroupBox" name="groupBox_dongle_at_bomb">
                      <property name="title">
                       <string>船运模式</string>
                      </property>
                      <layout class="QVBoxLayout" name="verticalLayout_dongle_at_bomb">
                       <property name="spacing">
                        <number>6</number>
                       </property>
                       <item>
                        <layout class="QHBoxLayout" name="horizontalLayout_dongle_at_bomb_row1">
                         <property name="spacing">
                          <number>8</number>
                         </property>
                         <item>
                          <widget class="QPushButton" name="ship_bomb">
                           <property name="toolTip">
                            <string>底层：AT+BOMB</string>
                           </property>
                           <property name="text">
                            <string>进入船运</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <widget class="QLabel" name="label_86">
                           <property name="text">
                            <string>信号</string>
                           </property>
                           <property name="toolTip">
                            <string>信号强度 RSSI</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <widget class="QLineEdit" name="bombrssi">
                           <property name="maximumSize">
                            <size>
                             <width>64</width>
                             <height>16777215</height>
                            </size>
                           </property>
                           <property name="text">
                            <string>-80</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <widget class="QLabel" name="label_85">
                           <property name="text">
                            <string>名字</string>
                           </property>
                           <property name="toolTip">
                            <string>设备名字</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <widget class="QLineEdit" name="bombname">
                           <property name="maximumSize">
                            <size>
                             <width>88</width>
                             <height>16777215</height>
                            </size>
                           </property>
                           <property name="text">
                            <string>P20P</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <widget class="QLabel" name="label_87">
                           <property name="text">
                            <string>间隔</string>
                           </property>
                           <property name="toolTip">
                            <string>连接间隔</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <widget class="QLineEdit" name="bombinterval">
                           <property name="maximumSize">
                            <size>
                             <width>56</width>
                             <height>16777215</height>
                            </size>
                           </property>
                           <property name="text">
                            <string>10</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <spacer name="horizontalSpacer_dongle_at_bomb_r1">
                           <property name="orientation">
                            <enum>Qt::Horizontal</enum>
                           </property>
                           <property name="sizeHint" stdset="0">
                            <size>
                             <width>8</width>
                             <height>20</height>
                            </size>
                           </property>
                          </spacer>
                         </item>
                        </layout>
                       </item>
                       <item>
                        <layout class="QHBoxLayout" name="horizontalLayout_dongle_at_bomb_row2">
                         <property name="spacing">
                          <number>8</number>
                         </property>
                         <item>
                          <widget class="QLabel" name="label_bomb_command">
                           <property name="text">
                            <string>指令</string>
                           </property>
                          </widget>
                         </item>
                         <item>
                          <widget class="QLineEdit" name="bombcommand">
                           <property name="sizePolicy">
                            <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
                             <horstretch>1</horstretch>
                             <verstretch>0</verstretch>
                            </sizepolicy>
                           </property>
                           <property name="toolTip">
                            <string>AT+BOMB 第4参数 command（十六进制载荷）</string>
                           </property>
                           <property name="text">
                            <string>0008021a0408051001e6</string>
                           </property>
                          </widget>
                         </item>
                        </layout>
                       </item>
                      </layout>
                     </widget>
                    </item>'''

t = t[:bomb_item] + new_item + t[idx:]
p.write_text(t, encoding="utf-8", newline="\n")
print("rewrote bomb")
