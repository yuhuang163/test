# -*- coding: utf-8 -*-
"""把 groupBox_2 从外设外层挪进 tab_periph_state，Designer 与运行时一致。"""
from pathlib import Path
import re

UI = Path(r"D:\code\new_product_test\mainwindow.ui")
text = UI.read_text(encoding="utf-8")

# 定位 outer item 包裹的 groupBox_2（外设页顶部）
start = text.find('<widget class="QGroupBox" name="groupBox_2">')
if start < 0:
    print("groupBox_2 not found")
    raise SystemExit(1)

# 向前找到 <item> 起始
item_start = text.rfind("<item>", 0, start)
# 向后匹配该 widget 结束：用简单深度计数
i = start
depth = 0
end = None
while i < len(text):
    if text.startswith("<widget", i):
        depth += 1
        i = text.find(">", i) + 1
        continue
    if text.startswith("</widget>", i):
        depth -= 1
        i += len("</widget>")
        if depth == 0:
            end = i
            break
        continue
    i += 1
if end is None:
    print("failed to find groupBox_2 end")
    raise SystemExit(1)

# item 结束
item_end = text.find("</item>", end)
if item_end < 0:
    print("no </item>")
    raise SystemExit(1)
item_end += len("</item>")

block = text[item_start:item_end]
# 取出纯 widget（不要外层 item）
w_start = block.find('<widget class="QGroupBox" name="groupBox_2">')
w_end = block.rfind("</widget>") + len("</widget>")
widget_xml = block[w_start:w_end]

# 从原文删除整个 item（含换行）
text2 = text[:item_start] + text[item_end:]
# 可能留下多余空行，可接受

# 插入到 tab_periph_state 的 layout 内（</layout> 之前）
marker = 'name="verticalLayout_tab_periph_state">'
pos = text2.find(marker)
if pos < 0:
    print("state layout not found")
    raise SystemExit(1)
# 找到该 layout 的 </layout>
lay_start = text2.rfind("<layout", 0, pos)
# find matching </layout> after properties
search_from = pos
depth = 0
lay_close = None
j = text2.find("<layout", lay_start)
# simpler: first </layout> after marker that closes this layout — count nested layouts
k = pos
depth = 1  # inside layout
while k < len(text2):
    if text2.startswith("<layout", k):
        depth += 1
        k = text2.find(">", k) + 1
        continue
    if text2.startswith("</layout>", k):
        depth -= 1
        if depth == 0:
            lay_close = k
            break
        k += len("</layout>")
        continue
    k += 1

if lay_close is None:
    print("layout close not found")
    raise SystemExit(1)

insert = (
    "\n                      <item>\n"
    + widget_xml
    + "\n                      </item>\n"
)
text3 = text2[:lay_close] + insert + text2[lay_close:]
UI.write_text(text3, encoding="utf-8", newline="\n")
print("moved groupBox_2 into tab_periph_state")
