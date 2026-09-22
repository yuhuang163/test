# -*- coding: utf-8 -*-
from pathlib import Path
import re

t = Path(r"d:\code\new_product_test\mainwindow.ui").read_text(encoding="utf-8")
m = re.search(
    r'<layout class="QGridLayout" name="gridLayout_dongle_suction_peak_params">(.*?)</layout>\s*</widget>\s*</item>\s*<item row="0" column="1"',
    t,
    re.S,
)
g = m.group(1)
rows = re.findall(r'row="(\d+)" column="(\d+)"(?: colspan="(\d+)")?', g)
print("cells:", sorted(rows, key=lambda x: (int(x[0]), int(x[1]))))
print("export", t.count("btnDongleExportSuctionPng"))
# ensure no row1 param spins besides export
for name in [
    "dongleSuctionPeakTargetSpin",
    "dongleSuctionPeakToleranceSpin",
    "dongleSuctionXWindowSpin",
    "btnDongleExportSuctionPng",
]:
    i = t.find(f'name="{name}"')
    snippet = t[max(0, i - 120) : i]
    row = re.search(r'row="(\d+)"', snippet[::-1][::-1] and snippet)
    # simpler: find item containing widget
    mm = re.search(rf'<item row="(\d+)"[^>]*>[^<]*<widget class="[^"]+" name="{name}"', t)
    if not mm:
        mm = re.search(rf'<item row="(\d+)"[^>]*>.*?name="{name}"', t, re.S)
    print(name, "row", mm.group(1) if mm else "?")
