"""主屏抓取 → aiortc VideoStreamTrack。"""

from __future__ import annotations

import asyncio
import fractions
import time

import mss
import numpy as np
from av import VideoFrame
from aiortc import VideoStreamTrack
from PIL import Image


class ScreenVideoTrack(VideoStreamTrack):
    kind = "video"

    def __init__(self, max_width: int = 1920, fps: int = 60):
        super().__init__()
        self._sct = mss.mss()
        self._monitor = self._sct.monitors[1]
        # max_width<=0 表示不缩放，保留原生分辨率
        self.max_width = int(max_width)
        self.fps = max(1, min(int(fps), 60))
        self._frame_interval = 1.0 / self.fps
        self._last_t = 0.0
        self.width = int(self._monitor["width"])
        self.height = int(self._monitor["height"])

    async def recv(self):
        pts, time_base = await self.next_timestamp()
        now = time.time()
        delay = self._frame_interval - (now - self._last_t)
        if delay > 0:
            await asyncio.sleep(delay)
        self._last_t = time.time()

        shot = self._sct.grab(self._monitor)
        # BGRA -> RGB
        arr = np.frombuffer(shot.raw, dtype=np.uint8).reshape((shot.height, shot.width, 4))
        rgb = arr[:, :, :3][:, :, ::-1].copy()

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
            self._sct.close()
        except Exception:
            pass
