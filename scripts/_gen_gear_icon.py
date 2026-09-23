# -*- coding: utf-8 -*-
"""生成顶栏设置/帮助图标：同一 SVG 管线，保证颜色、线宽、尺寸一致。"""
import os

from reportlab.graphics import renderPM
from reportlab.lib.colors import Color
from svglib.svglib import svg2rlg
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "stytle", "picture", "status")
SIZE = 56
STROKE = "#3d5a80"
STROKE_W = 1.85


def wrap_svg(inner: str) -> str:
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        f'width="{SIZE}" height="{SIZE}">\n'
        f'  <g fill="none" stroke="{STROKE}" stroke-width="{STROKE_W}" '
        'stroke-linecap="round" stroke-linejoin="round">\n'
        f"    {inner}\n"
        "  </g>\n"
        "</svg>\n"
    )


# Feather Icons 同源路径，顶栏成对使用
ICONS = {
    "icon-help": (
        '<circle cx="12" cy="12" r="10"/>'
        '<path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"/>'
        '<line x1="12" y1="17" x2="12.01" y2="17"/>'
    ),
    "icon-gear": (
        # 略缩小，避免 Feather Settings 细节多导致比帮助图标更沉
        '<g transform="translate(12 12) scale(0.90) translate(-12 -12)">'
        '<circle cx="12" cy="12" r="3"/>'
        '<path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0'
        "l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2"
        "v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83"
        "l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09"
        "A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0"
        "l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09"
        "a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83"
        "l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2"
        'h-.09a1.65 1.65 0 0 0-1.51 1z"/>'
        "</g>"
    ),
}


def save_png(name: str, inner: str) -> None:
    path = os.path.join(OUT_DIR, name + ".png")
    tmp = path + ".svg"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(wrap_svg(inner))
    drawing = svg2rlg(tmp)
    if drawing is None:
        raise RuntimeError("svg2rlg failed: " + name)
    sx = SIZE / drawing.width if drawing.width else 1
    sy = SIZE / drawing.height if drawing.height else 1
    drawing.width = SIZE
    drawing.height = SIZE
    drawing.scale(sx, sy)
    renderPM.drawToFile(drawing, path, fmt="PNG", dpi=72, bg=Color(1, 1, 1, 0))
    os.remove(tmp)

    im = Image.open(path).convert("RGBA")
    out = []
    for r, g, b, a in im.getdata():
        if r > 245 and g > 245 and b > 245:
            out.append((0, 0, 0, 0))
        else:
            out.append((r, g, b, a))
    im.putdata(out)
    bbox = im.getbbox()
    if bbox:
        crop = im.crop(bbox)
        canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        canvas.paste(crop, ((SIZE - crop.width) // 2, (SIZE - crop.height) // 2), crop)
        im = canvas
    im.save(path, "PNG")
    print("wrote", path)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, inner in ICONS.items():
        save_png(name, inner)
    print("done")


if __name__ == "__main__":
    main()
