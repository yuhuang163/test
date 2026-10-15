# -*- coding: utf-8 -*-
import re
from pathlib import Path

p = Path("mainwindow.ui")
t = p.read_text(encoding="utf-8")
pat = (
    r'<property name="pixmap"><pixmap resource="new_production\.qrc">'
    r':/stytle/picture/status/status-[a-z]+-badge\.png</pixmap></property>'
)
t2, n = re.subn(pat, "", t)
print("stripped", n)
print("badge left", len(re.findall(r"status-[a-z]+-badge", t2)))
p.write_bytes(t2.encode("utf-8"))
