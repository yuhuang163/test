"""主屏抓取 → aiortc VideoStreamTrack。Windows 优先 DXGI（dxcam）。"""

from __future__ import annotations

import asyncio
import fractions
import logging
import sys
import time

import numpy as np
from av import VideoFrame
from aiortc import VideoStreamTrack
from aiortc.mediastreams import VIDEO_CLOCK_RATE, VIDEO_TIME_BASE, MediaStreamError

LOG = logging.getLogger("remote_agent.screen")


def _resize_rgb(rgb: np.ndarray, new_w: int, new_h: int) -> np.ndarray:
    try:
        import cv2

        return cv2.resize(rgb, (new_w, new_h), interpolation=cv2.INTER_AREA)
    except Exception:
        from PIL import Image

        return np.asarray(Image.fromarray(rgb).resize((new_w, new_h), Image.Resampling.BILINEAR))


def _create_backend():
    if sys.platform == "win32":
        try:
            import dxcam

            cam = dxcam.create(output_idx=0, output_color="RGB")
            if cam is None:
                raise RuntimeError("dxcam.create returned None")
            frame = None
            for _ in range(50):
                frame = cam.grab()
                if frame is not None:
                    break
                time.sleep(0.02)
            if frame is None:
                w, h = 1920, 1080
                try:
                    import mss as _mss

                    mon = _mss.mss().monitors[1]
                    w, h = int(mon["width"]), int(mon["height"])
                except Exception:
                    pass
            else:
                h, w = frame.shape[:2]

            last = frame

            def grab():
                nonlocal last
                img = cam.grab()
                if img is not None:
                    # 必须拷贝：dxcam 可能复用缓冲
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

    def __init__(self, max_width: int = 1920, fps: int = 30):
        super().__init__()
        self._grab, native_w, native_h, self._close_backend = _create_backend()
        self.max_width = int(max_width)
        self.fps = max(1, min(int(fps), 60))
        self.width = int(native_w)
        self.height = int(native_h)
        self._logged_encode_size = False

    async def next_timestamp(self):
        if self.readyState != "live":
            raise MediaStreamError
        step = int((1.0 / self.fps) * VIDEO_CLOCK_RATE)
        if hasattr(self, "_timestamp"):
            self._timestamp += step
            wait = self._start + (self._timestamp / VIDEO_CLOCK_RATE) - time.time()
            # 落后则跳帧追赶，避免个位数帧还越堆越卡
            while wait < -(1.0 / self.fps):
                self._timestamp += step
                wait = self._start + (self._timestamp / VIDEO_CLOCK_RATE) - time.time()
            if wait > 0:
                await asyncio.sleep(wait)
        else:
            self._start = time.time()
            self._timestamp = 0
        return self._timestamp, VIDEO_TIME_BASE

    async def recv(self):
        pts, time_base = await self.next_timestamp()
        rgb = self._grab()
        if rgb is None:
            rgb = np.zeros((self.height, self.width, 3), dtype=np.uint8)

        h, w = rgb.shape[:2]
        if self.max_width > 0 and w > self.max_width:
            new_w = self.max_width
            new_h = max(1, int(h * (new_w / float(w))))
            rgb = _resize_rgb(rgb, new_w, new_h)
            self.width, self.height = new_w, new_h
        else:
            self.width, self.height = w, h

        if not self._logged_encode_size:
            self._logged_encode_size = True
            LOG.info(
                "video encode size %sx%s (native %sx%s maxWidth=%s track_fps=%s)",
                self.width,
                self.height,
                w,
                h,
                self.max_width,
                self.fps,
            )

        frame = VideoFrame.from_ndarray(rgb, format="rgb24")
        frame.pts = pts
        frame.time_base = time_base if time_base else fractions.Fraction(1, 90000)
        return frame

    def stop(self) -> None:
        try:
            self._close_backend()
        except Exception:
            pass
