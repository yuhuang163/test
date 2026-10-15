# -*- coding: utf-8 -*-
"""启动上位机，按导航文案切换并截图。"""
import os
import shutil
import subprocess
import sys
import time

from PIL import ImageGrab
from pywinauto import Application
from pywinauto.findwindows import ElementNotFoundError

EXE = r"D:\code\new_product_test\build\Desktop_Qt_5_15_2_MSVC2019_64bit-Release\bin\new_production.exe"
OUT = r"D:\code\new_product_test\build\ui_review_shots"
os.makedirs(OUT, exist_ok=True)

_BIN = os.path.dirname(EXE)
_EXTRA = [
    r"D:\Qt\5.15.2\msvc2019_64\bin",
    _BIN,
    os.path.join(_BIN, "mvs_runtime"),
    r"D:\code\new_product_test\lib\nfc",
    r"D:\code\new_product_test\lib\mvs\runtime\win64",
]
os.environ["PATH"] = os.pathsep.join(_EXTRA) + os.pathsep + os.environ.get("PATH", "")
_dcrf_src = r"D:\code\new_product_test\lib\nfc\dcrf32.dll"
_dcrf_dst = os.path.join(_BIN, "dcrf32.dll")
if os.path.isfile(_dcrf_src) and not os.path.isfile(_dcrf_dst):
    shutil.copy2(_dcrf_src, _dcrf_dst)


def grab_hwnd(hwnd, path):
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.35)

    class RECT(ctypes.Structure):
        _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                    ("right", ctypes.c_long), ("bottom", ctypes.c_long)]

    rc = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rc))
    img = ImageGrab.grab(bbox=(rc.left, rc.top, rc.right, rc.bottom), all_screens=True)
    img.save(path)
    print("saved", path, img.size)


def main():
    subprocess.run(["taskkill", "/F", "/IM", "new_production.exe"], capture_output=True)
    time.sleep(0.6)
    proc = subprocess.Popen([EXE], cwd=_BIN)
    print("pid", proc.pid)

    app = None
    win = None
    for i in range(40):
        time.sleep(0.5)
        try:
            app = Application(backend="uia").connect(process=proc.pid)
            win = app.top_window()
            rect = win.rectangle()
            if rect.width() >= 500 and rect.height() >= 400:
                break
        except Exception as e:
            print("waiting", i, type(e).__name__)
            win = None
    if win is None:
        print("ERROR: no window")
        proc.kill()
        return 1

    win.set_focus()
    hwnd = win.handle
    print("window", win.window_text(), win.rectangle())
    grab_hwnd(hwnd, os.path.join(OUT, "01_connect.png"))

    pages = [
        ("02_biz.png", "业务控制"),
        ("03_periph.png", "外设"),
        ("04_dongle.png", "Dongle"),
        ("05_ota.png", "OTA/压测"),
        ("06_misc.png", "其它"),
    ]
    for fname, label in pages:
        clicked = False
        # 优先精确按钮名
        for ctrl in win.descendants():
            try:
                name = (ctrl.window_text() or "").strip()
                ctype = ctrl.element_info.control_type
            except Exception:
                continue
            if name != label:
                continue
            if ctype not in ("Button", "CheckBox", "TabItem", "ListItem", "Text", "Hyperlink"):
                # Qt UIA 有时把 PushButton 报成 Button
                pass
            try:
                print("click", label, ctype, name)
                ctrl.click_input()
                clicked = True
                break
            except Exception as e:
                print("click fail", label, e)
        if not clicked:
            # 模糊包含
            for ctrl in win.descendants():
                try:
                    name = (ctrl.window_text() or "").strip()
                except Exception:
                    continue
                if label in name and len(name) < 20:
                    try:
                        print("click-fuzzy", name)
                        ctrl.click_input()
                        clicked = True
                        break
                    except Exception as e:
                        print("fuzzy fail", e)
        if not clicked:
            print("WARN: not found", label)
        time.sleep(0.8)
        grab_hwnd(hwnd, os.path.join(OUT, fname))

    # 半窗
    import ctypes
    ctypes.windll.user32.MoveWindow(hwnd, 40, 40, 1000, 720, True)
    time.sleep(0.7)
    grab_hwnd(hwnd, os.path.join(OUT, "07_half_window.png"))

    # 回到业务再截半窗业务（可选）
    print("done", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
