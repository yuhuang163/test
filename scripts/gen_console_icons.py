# -*- coding: utf-8 -*-
"""从 HTML NAV_ICONS / sprite 导出控制台 PNG 图标。"""
import os
from svglib.svglib import svg2rlg
from reportlab.graphics import renderPM

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_NAV = os.path.join(ROOT, "stytle", "picture", "nav")
OUT_STATUS = os.path.join(ROOT, "stytle", "picture", "status")
SIZE = 48


def wrap_svg(inner, stroke="#4f6480", fill="none", stroke_width=1.7):
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="{SIZE}" height="{SIZE}">\n'
        f'  <g fill="{fill}" stroke="{stroke}" stroke-width="{stroke_width}" '
        'stroke-linecap="round" stroke-linejoin="round">\n'
        f"    {inner}\n"
        "  </g>\n"
        "</svg>\n"
    )


NAV = {
    "nav-connect": (
        '<path d="M10 13a3 3 0 004.24 0l1.41-1.41a3 3 0 10-4.24-4.24L9.76 9.76"/>'
        '<path d="M14 11a3 3 0 00-4.24 0L8.35 12.41a3 3 0 004.24 4.24l1.41-1.41"/>'
    ),
    "nav-business": (
        '<path d="M5 7h14"/><path d="M5 12h14"/><path d="M5 17h14"/>'
        '<circle cx="9" cy="7" r="2" fill="#4f6480" stroke="none"/>'
        '<circle cx="15" cy="12" r="2" fill="#4f6480" stroke="none"/>'
        '<circle cx="11" cy="17" r="2" fill="#4f6480" stroke="none"/>'
    ),
    "nav-peripheral": (
        '<rect x="7" y="7" width="10" height="10" rx="1.5"/>'
        '<path d="M12 7V4"/><path d="M12 20v-3"/><path d="M7 12H4"/>'
        '<path d="M20 12h-3"/><path d="M10 10h4"/><path d="M10 14h2"/>'
    ),
    "nav-dongle": (
        '<path d="M9 4h6a1 1 0 011 1v6h2.5L17 20H7L5.5 11H8V5a1 1 0 011-1z"/>'
        '<path d="M10 7h4"/><path d="M9.5 14h5"/>'
    ),
    "nav-ota": (
        '<path d="M12 4v7"/><path d="M9 8l3-3 3 3"/>'
        '<rect x="5" y="14" width="14" height="6" rx="1"/><path d="M9 17h6"/>'
    ),
    "nav-misc": (
        '<path d="M14.7 6.3a1 1 0 000 1.4l1.6 1.6a1 1 0 001.4 0l3.77-3.77'
        "a6 6 0 01-7.94 7.94l-6.91 6.91a2.12 2.12 0 01-3-3l6.91-6.91"
        'a6 6 0 017.94-7.94l-3.76 3.76z"/>'
    ),
}

STATUS = {
    "status-device": (
        '<path d="M12 3.5 5.5 6.5v5.6c0 3.9 2.7 7.5 6.5 8.4 3.8-.9 6.5-4.5 6.5-8.4V6.5L12 3.5z"/>'
        '<path d="M9.2 12l2 2 3.6-4"/>'
    ),
    "status-motor": (
        '<circle cx="12" cy="12" r="7"/><circle cx="12" cy="12" r="1.5"/>'
        '<path d="M12 5.5 14.8 9.5 12 12zM16.5 15.8 11.8 17 12 12zM7.5 15.8 9.2 9.5 12 12z"/>'
    ),
    "status-pump": (
        '<path d="M8 8h8l1.5 9.5H6.5L8 8z"/><path d="M10.5 8V5.5h3V8"/>'
        '<path d="M12 14.2c-1.3 0-2 .7-2 1.6 0 .7.7 1.3 2 1.3s2-.6 2-1.3c0-.9-.7-1.6-2-1.6z"/>'
    ),
    "status-valve": '<path d="M4 7 12 12 4 17zM20 7 12 12 20 17z"/><path d="M12 4.5v15"/>',
    "status-battery": (
        '<rect x="7" y="5" width="10" height="16" rx="1.5"/>'
        '<path d="M10 3h4v2h-4z"/><path d="M9 15h6"/>'
    ),
    "status-temp": (
        '<path d="M10.5 14.5V6.2a1.5 1.5 0 0 1 3 0V14.5a3.5 3.5 0 1 1-3 0z"/>'
        '<path d="M12 8.5v6"/>'
    ),
    "status-current": '<path d="M2 12h3.5l2-4.5 3.5 9 2.5-6H22"/>',
    "icon-link": (
        '<path d="M9 15 15 9m-2-3 1-1a4 4 0 0 1 6 6l-3 3a4 4 0 0 1-6 0'
        'm0-4a4 4 0 0 0-6 0l-3 3a4 4 0 0 0 6 6l1-1"/>'
    ),
    "icon-gear": (
        '<circle cx="12" cy="12" r="3"/>'
        '<path d="M19 14.5v-5l-2-.7-.8-1.8.9-1.9-3.6-2-1.4 1.5h-2L8.7 3.1l-3.6 2'
        "L6 7l-.8 1.8-2 .7v5l2 .7L6 17l-.9 1.9 3.6 2 1.4-1.5h2l1.4 1.5 3.6-2"
        '-.9-1.9.8-1.8 2-.7Z"/>'
    ),
    "icon-refresh": '<path d="M20 7v5h-5M4 17v-5h5M18 12a6 6 0 0 0-10.5-4M6 12a6 6 0 0 0 10.5 4"/>',
    "icon-filter": '<path d="M4 6h16M7 12h10M10 18h4"/>',
    "icon-flask": '<path d="M9 3h6M10 3v6l-5 9a2 2 0 0 0 2 3h10a2 2 0 0 0 2-3l-5-9V3M8 15h8"/>',
}


def save(path, svg_text):
    from reportlab.lib.colors import Color
    from PIL import Image

    tmp = path + ".svg"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(svg_text)
    drawing = svg2rlg(tmp)
    if drawing is None:
        raise RuntimeError("failed " + path)
    sx = SIZE / drawing.width if drawing.width else 1
    sy = SIZE / drawing.height if drawing.height else 1
    drawing.width = SIZE
    drawing.height = SIZE
    drawing.scale(sx, sy)
    # 透明底，避免选中态蓝底上出现白块
    renderPM.drawToFile(drawing, path, fmt="PNG", dpi=72, bg=Color(1, 1, 1, 0))
    os.remove(tmp)
    # 再抠一次近白像素，兼容渲染器仍铺白底的情况
    im = Image.open(path).convert("RGBA")
    px = list(im.getdata())
    out = []
    for r, g, b, a in px:
        if r > 245 and g > 245 and b > 245:
            out.append((0, 0, 0, 0))
        else:
            out.append((r, g, b, a))
    im.putdata(out)
    # 按非透明像素包围盒居中，避免路径/描边导致图标在画布内偏一侧
    bbox = im.getbbox()
    if bbox:
        crop = im.crop(bbox)
        canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        canvas.paste(crop, ((SIZE - crop.width) // 2, (SIZE - crop.height) // 2), crop)
        im = canvas
    im.save(path, "PNG")
    print("wrote", path)


def make_on_from_off(off_path, on_path):
    from PIL import Image

    im = Image.open(off_path).convert("RGBA")
    out = []
    for r, g, b, a in im.getdata():
        if r > 245 and g > 245 and b > 245 or a < 8:
            out.append((0, 0, 0, 0))
        else:
            alpha = max(a, 255 - (r + g + b) // 3)
            out.append((255, 255, 255, alpha))
    im.putdata(out)
    im.save(on_path, "PNG")
    print("wrote", on_path)


def main():
    os.makedirs(OUT_NAV, exist_ok=True)
    os.makedirs(OUT_STATUS, exist_ok=True)
    for name, inner in NAV.items():
        off = os.path.join(OUT_NAV, name + ".png")
        save(off, wrap_svg(inner, "#4f6480"))
        make_on_from_off(off, os.path.join(OUT_NAV, name + "-on.png"))
    for name, inner in STATUS.items():
        if name == "icon-gear":
            # 顶栏齿轮/帮助成对生成（同色同线宽），见 _gen_gear_icon.py
            import runpy

            runpy.run_path(os.path.join(os.path.dirname(__file__), "_gen_gear_icon.py"))
            continue
        save(os.path.join(OUT_STATUS, name + ".png"), wrap_svg(inner, "#476383", stroke_width=1.6))
    print("done")


if __name__ == "__main__":
    main()
