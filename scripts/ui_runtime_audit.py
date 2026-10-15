# -*- coding: utf-8 -*-
"""Capture host console pages via PrintWindow (not screen grab)."""
import time
from pathlib import Path

import win32con
import win32gui
import win32ui
from PIL import Image
from pywinauto import Application

out = Path(r"D:\code\new_product_test\docs\ui-preview-pack\runtime-audit")
out.mkdir(parents=True, exist_ok=True)
log = []


def logp(msg):
    log.append(str(msg))
    try:
        print(msg)
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
logp(f"hwnd={hwnd}")

# restore + topmost briefly so UIA clicks land on it
win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
win32gui.SetWindowPos(
    hwnd,
    win32con.HWND_TOPMOST,
    0,
    0,
    0,
    0,
    win32con.SWP_NOMOVE | win32con.SWP_NOSIZE,
)
time.sleep(0.2)
try:
    win32gui.SetForegroundWindow(hwnd)
except Exception as e:
    logp(f"SetForegroundWindow: {e}")
time.sleep(0.3)

app = Application(backend="uia").connect(handle=hwnd)
win = app.window(handle=hwnd)


def shot(name):
    import ctypes
    from ctypes import wintypes

    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    w, h = max(1, right - left), max(1, bottom - top)
    hwnd_dc = win32gui.GetWindowDC(hwnd)
    mfc_dc = win32ui.CreateDCFromHandle(hwnd_dc)
    save_dc = mfc_dc.CreateCompatibleDC()
    bmp = win32ui.CreateBitmap()
    bmp.CreateCompatibleBitmap(mfc_dc, w, h)
    save_dc.SelectObject(bmp)
    user32 = ctypes.windll.user32
    # PW_RENDERFULLCONTENT = 2
    result = user32.PrintWindow(wintypes.HWND(hwnd), save_dc.GetSafeHdc(), 2)
    if not result:
        result = user32.PrintWindow(wintypes.HWND(hwnd), save_dc.GetSafeHdc(), 0)
    bmpinfo = bmp.GetInfo()
    bmpstr = bmp.GetBitmapBits(True)
    img = Image.frombuffer(
        "RGB",
        (bmpinfo["bmWidth"], bmpinfo["bmHeight"]),
        bmpstr,
        "raw",
        "BGRX",
        0,
        1,
    )
    p = out / f"{name}.png"
    img.save(p)
    win32gui.DeleteObject(bmp.GetHandle())
    save_dc.DeleteDC()
    mfc_dc.DeleteDC()
    win32gui.ReleaseDC(hwnd, hwnd_dc)
    logp(f"saved {p.name} {img.size} pw={result}")


def click_xy(x, y):
    import win32api

    win32api.SetCursorPos((x, y))
    win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    time.sleep(0.8)


def find_click_nav(partial):
    for ctype in ("RadioButton", "Button", "CheckBox"):
        for el in win.descendants(control_type=ctype):
            try:
                name = (el.window_text() or "").strip()
                if not name:
                    name = (el.element_info.name or "").strip()
            except Exception:
                continue
            if partial in name and len(name) < 20:
                try:
                    rect = el.rectangle()
                    cx = (rect.left + rect.right) // 2
                    cy = (rect.top + rect.bottom) // 2
                    wl, wt, wr, wb = win32gui.GetWindowRect(hwnd)
                    if not (wl <= cx <= wr and wt <= cy <= wb):
                        continue
                    if cx - wl > 280:
                        continue
                    click_xy(cx, cy)
                    logp(f"clicked nav {partial!r} via {ctype} at {cx},{cy}")
                    return True
                except Exception as e:
                    logp(f"nav click fail {partial}: {e}")
    return False


def click_tab(text):
    for el in win.descendants(control_type="TabItem"):
        try:
            name = (el.window_text() or "").strip()
        except Exception:
            continue
        if text in name:
            try:
                el.select()
            except Exception:
                try:
                    el.click_input()
                except Exception as e:
                    logp(f"tab fail {name}: {e}")
                    return None
            time.sleep(0.7)
            logp(f"tab {name}")
            return name
    return None


nav_names = [
    ("00_connect", "连接"),
    ("01_biz", "业务控制"),
    ("02_periph", "外设"),
    ("03_dongle", "Dongle"),
    ("04_ota", "OTA"),
    ("05_misc", "其它"),
]

for key, label in nav_names:
    ok = find_click_nav(label)
    logp(f"nav {key} ok={ok}")
    shot(key)

# Dongle tabs
find_click_nav("Dongle")
time.sleep(0.3)
for tab in ("吸力专项测试", "AT 测试"):
    n = click_tab(tab)
    if n:
        shot("dongle_" + n.replace(" ", "_"))

# Biz tabs
find_click_nav("业务控制")
time.sleep(0.3)
tabs = []
for el in win.descendants(control_type="TabItem"):
    try:
        name = (el.window_text() or "").strip()
    except Exception:
        continue
    if name and name not in tabs:
        tabs.append(name)
(out / "tabs_biz.txt").write_text("\n".join(tabs), encoding="utf-8")
seen = set()
for want in ("基础外设", "工厂", "模式", "序列号", "三元组", "自定义", "吸力", "循环"):
    for name in tabs:
        if want in name and name not in seen:
            seen.add(name)
            click_tab(name)
            shot("biz_" + "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in name)[:40])
            break

# Periph tabs
find_click_nav("外设")
time.sleep(0.3)
tabs = []
for el in win.descendants(control_type="TabItem"):
    try:
        name = (el.window_text() or "").strip()
    except Exception:
        continue
    if name and name not in tabs:
        tabs.append(name)
(out / "tabs_periph.txt").write_text("\n".join(tabs), encoding="utf-8")
seen = set()
for want in ("外设状态", "加热", "泵", "阀", "电池", "振子", "蓝牙", "电机", "陀螺仪", "屏幕", "摄像头", "LED"):
    for name in tabs:
        if (name == want or want in name) and name not in seen and len(name) <= 10:
            seen.add(name)
            click_tab(name)
            shot("periph_" + "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in name)[:40])
            break

# clear topmost
win32gui.SetWindowPos(
    hwnd,
    win32con.HWND_NOTOPMOST,
    0,
    0,
    0,
    0,
    win32con.SWP_NOMOVE | win32con.SWP_NOSIZE,
)

(out / "audit_log.txt").write_text("\n".join(log), encoding="utf-8")
logp("done")
