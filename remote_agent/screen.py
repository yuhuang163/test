"""主屏抓取 → aiortc VideoStreamTrack。

Windows 优先 DXGI Desktop Duplication（dxcam）：
同机自控时 Edge/Chrome 等 GPU 加速窗口用 mss/BitBlt 常被采成纯黑，DXGI 采的是合成后桌面。
"""

from __future__ import annotations

import asyncio
import fractions
import logging
import sys
import time

import numpy as np
from av import VideoFrame
from aiortc import VideoStreamTrack
from PIL import Image

LOG = logging.getLogger("remote_agent.screen")


def _create_backend():
    """返回 (grab_fn, width, height, close_fn)。grab_fn() -> RGB ndarray | None。"""
    if sys.platform == "win32":
        try:
            import dxcam

            # output_color=RGB，与 VideoFrame rgb24 一致
            cam = dxcam.create(output_idx=0, output_color="RGB")
            if cam is None:
                raise RuntimeError("dxcam.create returned None")
            # 先抓一帧拿分辨率；DXGI 可能首帧为 None
            frame = None
            for _ in range(50):
                frame = cam.grab()
                if frame is not None:
                    break
                time.sleep(0.02)
            if frame is None:
                # 仍无帧时用显示器枚举尺寸，后续 grab 再等
                w, h = 1920, 1080
                try:
                    import mss as _mss

                    mon = _mss.mss().monitors[1]
                    w, h = int(mon["width"]), int(mon["height"])
                except Exception:
                    pass
            else:
                h, w = frame.shape[:2]

            last = frame  # 可能为 None，recv 里再等

            def grab():
                nonlocal last
                img = cam.grab()
                if img is not None:
                    # dxcam 可能复用缓冲，必须拷贝后再交给编码器
                    last = np.array(img, copy=True)
                return None if last is None else last

            def close():
                try:
                    cam.release()
                except Exception:
                    pass

            LOG.info("screen capture backend=dxcam (DXGI) %sx%s", w, h)
            return grab, w, h, close
        except Exception as exc:
            LOG.warning("dxcam unavailable (%s), fallback to mss", exc)

    import mss

    sct = mss.mss()
    monitor = sct.monitors[1]
    w = int(monitor["width"])
    h = int(monitor["height"])

    def grab():
        shot = sct.grab(monitor)
        arr = np.frombuffer(shot.raw, dtype=np.uint8).reshape((shot.height, shot.width, 4))
        # BGRA -> RGB
        return arr[:, :, :3][:, :, ::-1].copy()

    def close():
        try:
            sct.close()
        except Exception:
            pass

    LOG.info("screen capture backend=mss %sx%s", w, h)
    return grab, w, h, close


class ScreenVideoTrack(VideoStreamTrack):
    kind = "video"

    def __init__(self, max_width: int = 1920, fps: int = 60):
        super().__init__()
        self._grab, native_w, native_h, self._close_backend = _create_backend()
        # max_width<=0 表示不缩放，保留原生分辨率
        self.max_width = int(max_width)
        self.fps = max(1, min(int(fps), 60))
        self._frame_interval = 1.0 / self.fps
        self._last_t = 0.0
        self.width = int(native_w)
        self.height = int(native_h)

    async def recv(self):
        pts, time_base = await self.next_timestamp()
        now = time.time()
        delay = self._frame_interval - (now - self._last_t)
        if delay > 0:
            await asyncio.sleep(delay)
        self._last_t = time.time()

        rgb = self._grab()
        if rgb is None:
            # DXGI 尚未产出帧时回退黑帧，避免卡死
            rgb = np.zeros((self.height, self.width, 3), dtype=np.uint8)

        h, w = rgb.shape[:2]
        if self.max_width > 0 and w > self.max_width:
            new_w = self.max_width
            new_h = max(1, int(h * (new_w / w)))
            # LANCZOS 缩小比最近邻清晰很多（文字/UI）
            img = Image.fromarray(rgb)
            rgb = np.asarray(img.resize((new_w, new_h), Image.Resampling.LANCZOS))
            self.width, self.height = new_w, new_h
        else:
            self.width, self.height = w, h

        frame = VideoFrame.from_ndarray(rgb, format="rgb24")
        frame.pts = pts
        frame.time_base = time_base if time_base else fractions.Fraction(1, 90000)
        return frame

    def stop(self) -> None:
        try:
            self._close_backend()
        except Exception:
            pass
