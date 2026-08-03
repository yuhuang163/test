"""Windows 键鼠注入（SendInput）+ 剪贴板粘贴。"""

from __future__ import annotations

import ctypes
import logging
import time
from ctypes import wintypes

LOG = logging.getLogger("remote_agent.input")

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

INPUT_MOUSE = 0
INPUT_KEYBOARD = 1
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_MIDDLEDOWN = 0x0020
MOUSEEVENTF_MIDDLEUP = 0x0040
MOUSEEVENTF_WHEEL = 0x0800
MOUSEEVENTF_ABSOLUTE = 0x8000
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_EXTENDEDKEY = 0x0001
WHEEL_DELTA = 120

SM_CXSCREEN = 0
SM_CYSCREEN = 1

VK_CONTROL = 0x11
VK_MENU = 0x12  # Alt
VK_SHIFT = 0x10
VK_LWIN = 0x5B
VK_ESCAPE = 0x1B
VK_TAB = 0x09
VK_DELETE = 0x2E
VK_V = 0x56

CF_UNICODETEXT = 13
GMEM_MOVEABLE = 0x0002

# 系统光标 ID → CSS cursor（远控网页同步形状用）
IDC_ARROW = 32512
IDC_IBEAM = 32513
IDC_WAIT = 32514
IDC_CROSS = 32515
IDC_UPARROW = 32516
IDC_SIZENWSE = 32642
IDC_SIZENESW = 32643
IDC_SIZEWE = 32644
IDC_SIZENS = 32645
IDC_SIZEALL = 32646
IDC_NO = 32648
IDC_HAND = 32649
IDC_APPSTARTING = 32650
IDC_HELP = 32651

CURSOR_SHOWING = 0x00000001

# 64 位下 ULONG_PTR / 结构体对齐必须正确，否则 SendInput 会失败或乱点
ULONG_PTR = ctypes.c_size_t


class POINT(ctypes.Structure):
    _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]


class CURSORINFO(ctypes.Structure):
    _fields_ = [
        ("cbSize", wintypes.DWORD),
        ("flags", wintypes.DWORD),
        ("hCursor", wintypes.HANDLE),
        ("ptScreenPos", POINT),
    ]


user32.GetCursorInfo.argtypes = [ctypes.POINTER(CURSORINFO)]
user32.GetCursorInfo.restype = wintypes.BOOL
user32.LoadCursorW.argtypes = [wintypes.HINSTANCE, ctypes.c_void_p]
user32.LoadCursorW.restype = wintypes.HANDLE

_CURSOR_CSS_CACHE: dict[int, str] = {}


def _system_cursor_handle(cursor_id: int):
    # MAKEINTRESOURCE：低位字为资源 ID
    return user32.LoadCursorW(None, ctypes.c_void_p(cursor_id))


def _build_cursor_map() -> dict[int, str]:
    mapping = {
        IDC_ARROW: "default",
        IDC_IBEAM: "text",
        IDC_WAIT: "wait",
        IDC_CROSS: "crosshair",
        IDC_UPARROW: "n-resize",
        IDC_SIZENWSE: "nwse-resize",
        IDC_SIZENESW: "nesw-resize",
        IDC_SIZEWE: "ew-resize",
        IDC_SIZENS: "ns-resize",
        IDC_SIZEALL: "move",
        IDC_NO: "not-allowed",
        IDC_HAND: "pointer",
        IDC_APPSTARTING: "progress",
        IDC_HELP: "help",
    }
    out: dict[int, str] = {}
    for cid, css in mapping.items():
        h = int(ctypes.cast(_system_cursor_handle(cid), ctypes.c_void_p).value or 0)
        if h:
            out[h] = css
    return out


def get_cursor_css() -> str:
    """读取当前系统光标，映射为 CSS cursor 值。"""
    global _CURSOR_CSS_CACHE
    if not _CURSOR_CSS_CACHE:
        _CURSOR_CSS_CACHE = _build_cursor_map()
    info = CURSORINFO()
    info.cbSize = ctypes.sizeof(CURSORINFO)
    if not user32.GetCursorInfo(ctypes.byref(info)):
        return "default"
    if not (info.flags & CURSOR_SHOWING):
        return "none"
    h = int(ctypes.cast(info.hCursor, ctypes.c_void_p).value or 0)
    return _CURSOR_CSS_CACHE.get(h, "default")

user32.OpenClipboard.argtypes = [wintypes.HWND]
user32.OpenClipboard.restype = wintypes.BOOL
user32.CloseClipboard.argtypes = []
user32.CloseClipboard.restype = wintypes.BOOL
user32.EmptyClipboard.argtypes = []
user32.EmptyClipboard.restype = wintypes.BOOL
user32.SetClipboardData.argtypes = [wintypes.UINT, wintypes.HANDLE]
user32.SetClipboardData.restype = wintypes.HANDLE
user32.GetClipboardData.argtypes = [wintypes.UINT]
user32.GetClipboardData.restype = wintypes.HANDLE
user32.IsClipboardFormatAvailable.argtypes = [wintypes.UINT]
user32.IsClipboardFormatAvailable.restype = wintypes.BOOL
user32.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
user32.SetCursorPos.restype = wintypes.BOOL
user32.SendInput.argtypes = [wintypes.UINT, ctypes.c_void_p, ctypes.c_int]
user32.SendInput.restype = wintypes.UINT
kernel32.GlobalAlloc.argtypes = [wintypes.UINT, ctypes.c_size_t]
kernel32.GlobalAlloc.restype = wintypes.HGLOBAL
kernel32.GlobalLock.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalLock.restype = wintypes.LPVOID
kernel32.GlobalUnlock.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalUnlock.restype = wintypes.BOOL

_dpi_ready = False


def ensure_dpi_aware() -> None:
    """与 mss 物理分辨率对齐，避免高 DPI 下点击偏移/无效。"""
    global _dpi_ready
    if _dpi_ready:
        return
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)  # PER_MONITOR_AWARE_V2
    except Exception:
        try:
            user32.SetProcessDPIAware()
        except Exception:
            pass
    _dpi_ready = True


class MOUSEINPUT(ctypes.Structure):
    _fields_ = (
        ("dx", wintypes.LONG),
        ("dy", wintypes.LONG),
        ("mouseData", wintypes.DWORD),
        ("dwFlags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ULONG_PTR),
    )


class KEYBDINPUT(ctypes.Structure):
    _fields_ = (
        ("wVk", wintypes.WORD),
        ("wScan", wintypes.WORD),
        ("dwFlags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ULONG_PTR),
    )


class HARDWAREINPUT(ctypes.Structure):
    _fields_ = (
        ("uMsg", wintypes.DWORD),
        ("wParamL", wintypes.WORD),
        ("wParamH", wintypes.WORD),
    )


class INPUT_UNION(ctypes.Union):
    _fields_ = (("mi", MOUSEINPUT), ("ki", KEYBDINPUT), ("hi", HARDWAREINPUT))


class INPUT(ctypes.Structure):
    _fields_ = (("type", wintypes.DWORD), ("union", INPUT_UNION))


def _send(inputs: list[INPUT]) -> None:
    n = len(inputs)
    if n <= 0:
        return
    arr = (INPUT * n)(*inputs)
    sent = user32.SendInput(n, ctypes.byref(arr), ctypes.sizeof(INPUT))
    if sent != n:
        raise ctypes.WinError(ctypes.get_last_error())


def screen_size() -> tuple[int, int]:
    ensure_dpi_aware()
    return int(user32.GetSystemMetrics(SM_CXSCREEN)), int(user32.GetSystemMetrics(SM_CYSCREEN))


def _abs_from_norm(nx: float, ny: float) -> tuple[int, int, int, int]:
    """归一化坐标 → 主屏像素 + SendInput 绝对坐标(0~65535)。"""
    ensure_dpi_aware()
    nx = max(0.0, min(1.0, float(nx)))
    ny = max(0.0, min(1.0, float(ny)))
    w = max(1, int(user32.GetSystemMetrics(SM_CXSCREEN)))
    h = max(1, int(user32.GetSystemMetrics(SM_CYSCREEN)))
    px = int(round(nx * (w - 1)))
    py = int(round(ny * (h - 1)))
    ax = int(px * 65535 / max(1, w - 1))
    ay = int(py * 65535 / max(1, h - 1))
    return px, py, ax, ay


def move_abs_norm(nx: float, ny: float) -> None:
    """nx/ny: 0~1 归一化到主屏。"""
    px, py, ax, ay = _abs_from_norm(nx, ny)
    user32.SetCursorPos(px, py)
    inp = INPUT(type=INPUT_MOUSE)
    inp.union.mi = MOUSEINPUT(ax, ay, 0, MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, 0, 0)
    _send([inp])


def mouse_button(button: str, down: bool) -> None:
    flags = {
        ("left", True): MOUSEEVENTF_LEFTDOWN,
        ("left", False): MOUSEEVENTF_LEFTUP,
        ("right", True): MOUSEEVENTF_RIGHTDOWN,
        ("right", False): MOUSEEVENTF_RIGHTUP,
        ("middle", True): MOUSEEVENTF_MIDDLEDOWN,
        ("middle", False): MOUSEEVENTF_MIDDLEUP,
    }.get((button, down))
    if flags is None:
        return
    inp = INPUT(type=INPUT_MOUSE)
    inp.union.mi = MOUSEINPUT(0, 0, 0, flags, 0, 0)
    _send([inp])


def mouse_down_at(nx: float, ny: float, button: str) -> None:
    """先绝对移动再按下（分开发送，避免部分环境合并标志无效）。"""
    move_abs_norm(nx, ny)
    mouse_button(button, True)


def mouse_up_at(nx: float, ny: float, button: str) -> None:
    move_abs_norm(nx, ny)
    mouse_button(button, False)


def mouse_wheel(delta_y: float) -> None:
    steps = int(delta_y)
    if steps == 0:
        return
    data = int(-steps / 100 * WHEEL_DELTA) if abs(steps) >= 10 else int(-steps * WHEEL_DELTA)
    inp = INPUT(type=INPUT_MOUSE)
    inp.union.mi = MOUSEINPUT(0, 0, data & 0xFFFFFFFF, MOUSEEVENTF_WHEEL, 0, 0)
    _send([inp])


def key_event(vk: int, down: bool, *, extended: bool = False) -> None:
    if vk <= 0:
        return
    flags = 0 if down else KEYEVENTF_KEYUP
    if extended:
        flags |= KEYEVENTF_EXTENDEDKEY
    inp = INPUT(type=INPUT_KEYBOARD)
    inp.union.ki = KEYBDINPUT(vk & 0xFF, 0, flags, 0, 0)
    _send([inp])


def key_combo(vks: list[int]) -> None:
    """按下组合键后按相反顺序抬起。"""
    clean = [int(v) for v in vks if int(v) > 0]
    if not clean:
        return
    for vk in clean:
        key_event(vk, True)
    time.sleep(0.03)
    for vk in reversed(clean):
        key_event(vk, False)


def set_clipboard_text(text: str) -> None:
    data = (text or "").encode("utf-16-le") + b"\x00\x00"
    if not user32.OpenClipboard(None):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        if not user32.EmptyClipboard():
            raise ctypes.WinError(ctypes.get_last_error())
        hmem = kernel32.GlobalAlloc(GMEM_MOVEABLE, len(data))
        if not hmem:
            raise ctypes.WinError(ctypes.get_last_error())
        ptr = kernel32.GlobalLock(hmem)
        if not ptr:
            raise ctypes.WinError(ctypes.get_last_error())
        ctypes.memmove(ptr, data, len(data))
        kernel32.GlobalUnlock(hmem)
        if not user32.SetClipboardData(CF_UNICODETEXT, hmem):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        user32.CloseClipboard()


def get_clipboard_text(*, max_chars: int = 200_000) -> str:
    """读取产线机剪贴板 Unicode 文本（无文字则空串）。"""
    if not user32.OpenClipboard(None):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        if not user32.IsClipboardFormatAvailable(CF_UNICODETEXT):
            return ""
        hmem = user32.GetClipboardData(CF_UNICODETEXT)
        if not hmem:
            return ""
        ptr = kernel32.GlobalLock(hmem)
        if not ptr:
            return ""
        try:
            text = ctypes.wstring_at(ptr) or ""
        finally:
            kernel32.GlobalUnlock(hmem)
        if max_chars > 0 and len(text) > max_chars:
            return text[:max_chars]
        return text
    finally:
        user32.CloseClipboard()


def paste_text(text: str) -> None:
    set_clipboard_text(text)
    time.sleep(0.05)
    key_combo([VK_CONTROL, VK_V])


_SPECIAL = {
    "win": [VK_LWIN],
    "ctrl_esc": [VK_CONTROL, VK_ESCAPE],
    "alt_tab": [VK_MENU, VK_TAB],
    "ctrl_shift_esc": [VK_CONTROL, VK_SHIFT, VK_ESCAPE],
    # Windows 禁止普通进程伪造 Ctrl+Alt+Del，改为任务管理器
    "cad": [VK_CONTROL, VK_SHIFT, VK_ESCAPE],
    "win_l": [VK_LWIN, 0x4C],
    "win_d": [VK_LWIN, 0x44],
    "win_e": [VK_LWIN, 0x45],
    "alt_f4": [VK_MENU, 0x73],
}


def _btn_name(button) -> str:
    return {0: "left", 1: "middle", 2: "right"}.get(int(button if button is not None else 0), "left")


def handle_input_event(evt: dict) -> None:
    ensure_dpi_aware()
    et = str(evt.get("type") or "")
    if et == "mousemove":
        move_abs_norm(evt.get("x", 0), evt.get("y", 0))
    elif et == "mousedown":
        mouse_down_at(evt.get("x", 0), evt.get("y", 0), _btn_name(evt.get("button", 0)))
    elif et == "mouseup":
        mouse_up_at(evt.get("x", 0), evt.get("y", 0), _btn_name(evt.get("button", 0)))
    elif et == "wheel":
        move_abs_norm(evt.get("x", 0), evt.get("y", 0))
        mouse_wheel(float(evt.get("deltaY") or 0))
    elif et == "keydown":
        key_event(int(evt.get("vk") or 0), True)
    elif et == "keyup":
        key_event(int(evt.get("vk") or 0), False)
    elif et == "combo":
        vks = evt.get("vks") or []
        if isinstance(vks, list):
            key_combo(vks)
    elif et == "special":
        name = str(evt.get("name") or "").strip().lower()
        if name in _SPECIAL:
            key_combo(_SPECIAL[name])
    elif et == "paste":
        paste_text(str(evt.get("text") or ""))
    else:
        LOG.debug("ignore input type=%s", et)
