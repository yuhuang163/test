# -*- coding: utf-8 -*-
"""Capture specific pages listed in ui需求.md 19-26."""
import time
from pathlib import Path

import ctypes
import win32con
import win32gui
import win32ui
from ctypes import wintypes
from PIL import Image
from pywinauto import Application

out = Path(r"D:\code\new_product_test\docs\ui-preview-pack\runtime-audit")
out.mkdir(parents=True, exist_ok=True)
log = []


def logp(m):
    log.append(str(m))
    try:
        print(m)
    except Exception:
        pass


hwnds = []


def enum_cb(hwnd, _):
    if not win32gui.IsWindowVisible(hwnd):
        return
    title = win32gui.GetWindowText(hwnd)
    cls = win32gui.GetClassName(hwnd)
    if "Qt5152QWindowIcon" in cls and "产品测试工具" in title:
        hwnds.append(hwnd)


win32gui.EnumWindows(enum_cb, None)
if not hwnds:
    raise SystemExit("no window")
hwnd = hwnds[0]
win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
win32gui.SetWindowPos(hwnd, win32con.HWND_TOPMOST, 40, 40, 1680, 960, 0)
time.sleep(0.3)
try:
    win32gui.SetForegroundWindow(hwnd)
except Exception as e:
    logp(e)

app = Application(backend="uia").connect(handle=hwnd)
win = app.window(handle=hwnd)


def shot(name):
    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    w, h = max(1, right - left), max(1, bottom - top)
    hdc = win32gui.GetWindowDC(hwnd)
    mfc = win32ui.CreateDCFromHandle(hdc)
    save = mfc.CreateCompatibleDC()
    bmp = win32ui.CreateBitmap()
    bmp.CreateCompatibleBitmap(mfc, w, h)
    save.SelectObject(bmp)
    ok = ctypes.windll.user32.PrintWindow(wintypes.HWND(hwnd), save.GetSafeHdc(), 2)
    img = Image.frombuffer("RGB", (w, h), bmp.GetBitmapBits(True), "raw", "BGRX", 0, 1)
    p = out / f"{name}.png"
    img.save(p)
    win32gui.DeleteObject(bmp.GetHandle())
    save.DeleteDC()
    mfc.DeleteDC()
    win32gui.ReleaseDC(hwnd, hdc)
    logp(f"saved {p.name} {img.size} pw={ok}")


def click_nav(partial):
    for el in win.descendants(control_type="RadioButton"):
        n = (el.window_text() or "").strip()
        if partial in n:
            el.click_input()
            time.sleep(0.8)
            logp(f"nav {n}")
            return True
    return False


def click_tab(text):
    for el in win.descendants(control_type="TabItem"):
        n = (el.window_text() or "").strip()
        if text in n:
            try:
                el.select()
            except Exception:
                el.click_input()
            time.sleep(0.7)
            logp(f"tab {n}")
            return n
    return None


# 外设：屏幕 / 电池
click_nav("外设")
time.sleep(0.3)
click_tab("屏幕")
shot("fix_screen")
click_tab("电池")
shot("fix_battery")

# Dongle：吸力 / AT
click_nav("Dongle")
time.sleep(0.3)
click_tab("吸力")
shot("fix_dongle_suction")
click_tab("AT")
shot("fix_dongle_at")

# OTA：本地/云端/蓝牙
click_nav("OTA")
time.sleep(0.3)
tabs = []
for el in win.descendants(control_type="TabItem"):
    n = (el.window_text() or "").strip()
    if n and n not in tabs:
        tabs.append(n)
(out / "tabs_ota.txt").write_text("\n".join(tabs), encoding="utf-8")
logp("ota tabs: " + ", ".join(tabs))
for want in ("蓝牙", "本地", "云端", "OTA", "压测"):
    for n in tabs:
        if want in n and "上位机" not in n and "Dongle日志" not in n:
            click_tab(n)
            safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in n)[:40]
            shot(f"fix_ota_{safe}")
            break

# 其它：找预览
click_nav("其它")
time.sleep(0.3)
tabs = []
for el in win.descendants(control_type="TabItem"):
    n = (el.window_text() or "").strip()
    if n and n not in tabs:
        tabs.append(n)
(out / "tabs_misc.txt").write_text("\n".join(tabs), encoding="utf-8")
logp("misc tabs: " + ", ".join(tabs))
for want in ("预览", "UI", "分析", "其它"):
    for n in tabs:
        if want in n:
            click_tab(n)
            safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in n)[:40]
            shot(f"fix_misc_{safe}")
            break

win32gui.SetWindowPos(hwnd, win32con.HWND_NOTOPMOST, 0, 0, 0, 0, win32con.SWP_NOMOVE | win32con.SWP_NOSIZE)
(out / "fix_log.txt").write_text("\n".join(log), encoding="utf-8")
logp("done")
