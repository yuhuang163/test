"""Windows 剪贴板：文字 / PNG 图片 / 文件（CF_HDROP）。"""

from __future__ import annotations

import ctypes
import io
import logging
import os
import tempfile
import time
from ctypes import wintypes
from pathlib import Path

LOG = logging.getLogger("remote_agent.clipboard")

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
shell32 = ctypes.WinDLL("shell32", use_last_error=True)

CF_UNICODETEXT = 13
CF_DIB = 8
CF_HDROP = 15
GMEM_MOVEABLE = 0x0002

MAX_TEXT_CHARS = 200_000
MAX_IMAGE_BYTES = 6 * 1024 * 1024
MAX_FILE_BYTES = 12 * 1024 * 1024
MAX_FILES = 10
MAX_TOTAL_FILE_BYTES = 24 * 1024 * 1024

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
user32.RegisterClipboardFormatW.argtypes = [wintypes.LPCWSTR]
user32.RegisterClipboardFormatW.restype = wintypes.UINT

kernel32.GlobalAlloc.argtypes = [wintypes.UINT, ctypes.c_size_t]
kernel32.GlobalAlloc.restype = wintypes.HGLOBAL
kernel32.GlobalLock.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalLock.restype = wintypes.LPVOID
kernel32.GlobalUnlock.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalUnlock.restype = wintypes.BOOL
kernel32.GlobalSize.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalSize.restype = ctypes.c_size_t

shell32.DragQueryFileW.argtypes = [wintypes.HANDLE, wintypes.UINT, wintypes.LPWSTR, wintypes.UINT]
shell32.DragQueryFileW.restype = wintypes.UINT


class POINT(ctypes.Structure):
    _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]


class DROPFILES(ctypes.Structure):
    _fields_ = [
        ("pFiles", wintypes.DWORD),
        ("pt", POINT),
        ("fNC", wintypes.BOOL),
        ("fWide", wintypes.BOOL),
    ]


_PNG_FMT = 0


def _png_format() -> int:
    global _PNG_FMT
    if not _PNG_FMT:
        _PNG_FMT = int(user32.RegisterClipboardFormatW("PNG") or 0)
    return _PNG_FMT


def _open_clipboard(retries: int = 8) -> None:
    for i in range(retries):
        if user32.OpenClipboard(None):
            return
        time.sleep(0.02 * (i + 1))
    raise ctypes.WinError(ctypes.get_last_error())


def _alloc_set(fmt: int, data: bytes) -> None:
    hmem = kernel32.GlobalAlloc(GMEM_MOVEABLE, len(data))
    if not hmem:
        raise ctypes.WinError(ctypes.get_last_error())
    ptr = kernel32.GlobalLock(hmem)
    if not ptr:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        ctypes.memmove(ptr, data, len(data))
    finally:
        kernel32.GlobalUnlock(hmem)
    if not user32.SetClipboardData(fmt, hmem):
        raise ctypes.WinError(ctypes.get_last_error())


def set_clipboard_text(text: str) -> None:
    payload = (text or "").encode("utf-16-le") + b"\x00\x00"
    _open_clipboard()
    try:
        if not user32.EmptyClipboard():
            raise ctypes.WinError(ctypes.get_last_error())
        _alloc_set(CF_UNICODETEXT, payload)
    finally:
        user32.CloseClipboard()


def get_clipboard_text(*, max_chars: int = MAX_TEXT_CHARS) -> str:
    _open_clipboard()
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


def _read_hglobal_bytes(hmem) -> bytes:
    size = int(kernel32.GlobalSize(hmem) or 0)
    if size <= 0:
        return b""
    ptr = kernel32.GlobalLock(hmem)
    if not ptr:
        return b""
    try:
        return ctypes.string_at(ptr, size)
    finally:
        kernel32.GlobalUnlock(hmem)


def _dib_to_png(dib: bytes) -> bytes:
    from PIL import Image

    # CF_DIB = BITMAPINFOHEADER (+ palette) + pixels，无 BITMAPFILEHEADER
    if len(dib) < 40:
        raise ValueError("DIB too small")
    header = b"BM" + (14 + len(dib)).to_bytes(4, "little") + (0).to_bytes(4, "little") + (14 + 40).to_bytes(4, "little")
    # 简化：用 raw DIB 偏移；有调色板时 14+40 可能不对，改用 Pillow 从 BI 解析
    bi_size = int.from_bytes(dib[0:4], "little")
    bpp = int.from_bytes(dib[14:16], "little")
    clr_used = int.from_bytes(dib[32:36], "little")
    palette_bytes = 0
    if bpp <= 8:
        palette_bytes = (clr_used or (1 << bpp)) * 4
    off = 14 + bi_size + palette_bytes
    header = b"BM" + (14 + len(dib)).to_bytes(4, "little") + (0).to_bytes(4, "little") + off.to_bytes(4, "little")
    img = Image.open(io.BytesIO(header + dib))
    img.load()
    if img.mode not in ("RGB", "RGBA"):
        img = img.convert("RGBA" if "A" in img.getbands() else "RGB")
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def _png_to_dib(png: bytes) -> bytes:
    from PIL import Image

    img = Image.open(io.BytesIO(png))
    img.load()
    if img.mode != "RGB":
        img = img.convert("RGB")
    # 自下而上的 BI_RGB DIB
    w, h = img.size
    row = ((w * 3 + 3) // 4) * 4
    pixels = bytearray()
    raw = img.tobytes()
    stride = w * 3
    for y in range(h - 1, -1, -1):
        start = y * stride
        line = raw[start : start + stride]
        pixels.extend(line)
        pixels.extend(b"\x00" * (row - stride))
    header = bytearray(40)
    header[0:4] = (40).to_bytes(4, "little")
    header[4:8] = int(w).to_bytes(4, "little", signed=True)
    header[8:12] = int(h).to_bytes(4, "little", signed=True)
    header[12:14] = (1).to_bytes(2, "little")
    header[14:16] = (24).to_bytes(2, "little")
    return bytes(header) + bytes(pixels)


def get_clipboard_image_png(*, max_bytes: int = MAX_IMAGE_BYTES) -> bytes | None:
    _open_clipboard()
    try:
        png_fmt = _png_format()
        if png_fmt and user32.IsClipboardFormatAvailable(png_fmt):
            hmem = user32.GetClipboardData(png_fmt)
            if hmem:
                data = _read_hglobal_bytes(hmem)
                if data and (max_bytes <= 0 or len(data) <= max_bytes):
                    return data
        if user32.IsClipboardFormatAvailable(CF_DIB):
            hmem = user32.GetClipboardData(CF_DIB)
            if hmem:
                dib = _read_hglobal_bytes(hmem)
                if not dib:
                    return None
                png = _dib_to_png(dib)
                if max_bytes > 0 and len(png) > max_bytes:
                    raise ValueError(f"image too large ({len(png)} > {max_bytes})")
                return png
        return None
    finally:
        user32.CloseClipboard()


def set_clipboard_image_png(png: bytes) -> None:
    if not png:
        raise ValueError("empty png")
    if len(png) > MAX_IMAGE_BYTES:
        raise ValueError(f"image too large ({len(png)})")
    dib = _png_to_dib(png)
    png_fmt = _png_format()
    _open_clipboard()
    try:
        if not user32.EmptyClipboard():
            raise ctypes.WinError(ctypes.get_last_error())
        if png_fmt:
            _alloc_set(png_fmt, png)
        _alloc_set(CF_DIB, dib)
    finally:
        user32.CloseClipboard()


def get_clipboard_file_paths() -> list[str]:
    _open_clipboard()
    try:
        if not user32.IsClipboardFormatAvailable(CF_HDROP):
            return []
        hdrop = user32.GetClipboardData(CF_HDROP)
        if not hdrop:
            return []
        count = int(shell32.DragQueryFileW(hdrop, 0xFFFFFFFF, None, 0))
        out: list[str] = []
        buf = ctypes.create_unicode_buffer(32768)
        for i in range(count):
            n = int(shell32.DragQueryFileW(hdrop, i, buf, len(buf)))
            if n > 0:
                out.append(buf.value)
        return out
    finally:
        user32.CloseClipboard()


def set_clipboard_files(paths: list[str]) -> None:
    clean = [str(Path(p).resolve()) for p in paths if p and Path(p).exists()]
    if not clean:
        raise ValueError("no existing files")
    offset = ctypes.sizeof(DROPFILES)
    body = ("\0".join(clean) + "\0\0").encode("utf-16-le")
    total = offset + len(body)
    hmem = kernel32.GlobalAlloc(GMEM_MOVEABLE, total)
    if not hmem:
        raise ctypes.WinError(ctypes.get_last_error())
    ptr = kernel32.GlobalLock(hmem)
    if not ptr:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        drop = DROPFILES()
        drop.pFiles = offset
        drop.pt.x = 0
        drop.pt.y = 0
        drop.fNC = False
        drop.fWide = True
        ctypes.memmove(ptr, ctypes.byref(drop), offset)
        ctypes.memmove(ptr + offset, body, len(body))
    finally:
        kernel32.GlobalUnlock(hmem)
    _open_clipboard()
    try:
        if not user32.EmptyClipboard():
            raise ctypes.WinError(ctypes.get_last_error())
        if not user32.SetClipboardData(CF_HDROP, hmem):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        user32.CloseClipboard()


def read_files_for_transfer(paths: list[str]) -> list[dict]:
    """读取文件内容供回传；超限文件以 error 字段说明。"""
    files: list[dict] = []
    total = 0
    for p in paths[:MAX_FILES]:
        path = Path(p)
        name = path.name
        if not path.is_file():
            files.append({"name": name, "error": "not a file (dirs not supported)"})
            continue
        size = path.stat().st_size
        if size > MAX_FILE_BYTES:
            files.append({"name": name, "error": f"too large ({size} bytes)"})
            continue
        if total + size > MAX_TOTAL_FILE_BYTES:
            files.append({"name": name, "error": "total size limit exceeded"})
            continue
        data = path.read_bytes()
        total += len(data)
        files.append({"name": name, "data": data})
    return files


def staging_dir() -> Path:
    d = Path(tempfile.gettempdir()) / "remote_agent_clipboard"
    d.mkdir(parents=True, exist_ok=True)
    return d


def write_incoming_files(files: list[dict]) -> list[str]:
    """写入管理端发来的文件，返回本地路径列表。"""
    out: list[str] = []
    root = staging_dir() / f"in_{int(time.time() * 1000)}"
    root.mkdir(parents=True, exist_ok=True)
    for item in files[:MAX_FILES]:
        name = Path(str(item.get("name") or "file.bin")).name
        data = item.get("data") or b""
        if not isinstance(data, (bytes, bytearray)):
            continue
        if len(data) > MAX_FILE_BYTES:
            raise ValueError(f"{name} too large")
        dest = root / name
        dest.write_bytes(bytes(data))
        out.append(str(dest.resolve()))
    if not out:
        raise ValueError("no files written")
    return out


def snapshot_clipboard() -> dict:
    """优先文件 > 图片 > 文字。"""
    try:
        paths = get_clipboard_file_paths()
    except Exception as exc:
        LOG.warning("read HDROP failed: %s", exc)
        paths = []
    if paths:
        return {"kind": "files", "files": read_files_for_transfer(paths), "paths": paths}

    try:
        png = get_clipboard_image_png()
    except Exception as exc:
        LOG.warning("read image failed: %s", exc)
        png = None
    if png:
        return {"kind": "image", "mime": "image/png", "data": png}

    try:
        text = get_clipboard_text()
    except Exception as exc:
        LOG.warning("read text failed: %s", exc)
        text = ""
    if text:
        return {"kind": "text", "text": text}
    return {"kind": "empty"}
