# -*- coding: utf-8 -*-
from pathlib import Path
from xml.dom.minidom import parse

p = Path(__file__).resolve().parents[1] / "mainwindow.ui"
dom = parse(str(p))


def strip_ws(node):
    remove = []
    for c in list(node.childNodes):
        if c.nodeType == c.TEXT_NODE and not (c.data or "").strip():
            remove.append(c)
        elif c.nodeType == c.ELEMENT_NODE:
            strip_ws(c)
    for c in remove:
        node.removeChild(c)


strip_ws(dom)
decl = '<?xml version="1.0" encoding="UTF-8"?>'
body = dom.documentElement.toprettyxml(indent=" ")
lines = [ln for ln in (decl + "\n" + body).splitlines() if ln.strip()]
text = "\r\n".join(lines) + "\r\n"
p.write_bytes(text.encode("utf-8"))
print("lines", len(lines), "bytes", p.stat().st_size)
