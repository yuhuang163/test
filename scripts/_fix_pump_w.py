# -*- coding: utf-8 -*-
from pathlib import Path
p = Path(r"d:\code\new_product_test\mainwindow.ui")
t = p.read_text(encoding="utf-8")
old = """                      <property name="minimumSize">
                       <size>
                        <width>280</width>
                        <height>0</height>
                       </size>
                      </property>
                      <property name="maximumSize">
                       <size>
                        <width>480</width>
                        <height>16777215</height>
                       </size>
                      </property>
                      <property name="title">
                       <string>泵/阀参数设置</string>"""
new = """                      <property name="minimumSize">
                       <size>
                        <width>300</width>
                        <height>0</height>
                       </size>
                      </property>
                      <property name="maximumSize">
                       <size>
                        <width>520</width>
                        <height>16777215</height>
                       </size>
                      </property>
                      <property name="title">
                       <string>泵/阀参数设置</string>"""
if old not in t:
    raise SystemExit("pump block not found")
p.write_text(t.replace(old, new, 1), encoding="utf-8", newline="\n")
print("ok")
