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


def _meipass_roots():
    from pathlib import Path

    roots = []
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        roots.append(Path(meipass))
    roots.append(Path(sys.executable).resolve().parent / "_internal")
    return roots


def _preload_cv2_pyd() -> bool:
    """冻结包里 cv2/*.py 常被字节码阴影；直接加载 cv2.pyd 供 resize 使用。"""
    if "cv2" in sys.modules and hasattr(sys.modules["cv2"], "resize"):
        return True
    if not getattr(sys, "frozen", False):
        try:
            import cv2  # noqa: F401

            return hasattr(cv2, "resize")
        except Exception:
            return False
    import importlib.util

    for root in _meipass_roots():
        pyd = root / "cv2" / "cv2.pyd"
        if not pyd.is_file():
            continue
        try:
            spec = importlib.util.spec_from_file_location("cv2", pyd)
            if spec is None or spec.loader is None:
                continue
            mod = importlib.util.module_from_spec(spec)
            sys.modules["cv2"] = mod
            spec.loader.exec_module(mod)
            if hasattr(mod, "resize"):
                return True
        except Exception as exc:
            LOG.debug("preload cv2.pyd failed: %s", exc)
            sys.modules.pop("cv2", None)
    return False


def _resize_rgb(rgb: np.ndarray, new_w: int, new_h: int) -> np.ndarray:
    # 1) OpenCV（冻结环境优先 pyd）
    try:
        if _preload_cv2_pyd():
            import cv2

            return cv2.resize(rgb, (new_w, new_h), interpolation=cv2.INTER_AREA)
    except Exception:
        pass
    # 2) PyAV/libswscale（已随 av 打包，远快于 PIL）
    try:
        frame = VideoFrame.from_ndarray(rgb, format="rgb24")
        return frame.reformat(width=new_w, height=new_h).to_ndarray(format="rgb24")
    except Exception:
        pass
    from PIL import Image

    return np.asarray(Image.fromarray(rgb).resize((new_w, new_h), Image.Resampling.BILINEAR))


def _preload_dxcam_numpy_kernels() -> None:
    """冻结环境下预加载 Cython 扩展，避免 PyInstaller 把 .c 当源码（null bytes）。"""
    if not getattr(sys, "frozen", False):
        return
    name = "dxcam.processor._numpy_kernels"
    if name in sys.modules:
        return
    import importlib.util

    for root in _meipass_roots():
        pyds = sorted((root / "dxcam" / "processor").glob("_numpy_kernels*.pyd"))
        if not pyds:
            continue
        spec = importlib.util.spec_from_file_location(name, pyds[0])
        if spec is None or spec.loader is None:
            continue
        mod = importlib.util.module_from_spec(spec)
        sys.modules[name] = mod
        spec.loader.exec_module(mod)
        return


def _create_backend():
    if sys.platform == "win32":
        try:
            import dxcam

            # 冻结包里 cv2 的 .py 常被 PYZ 字节码阴影导致 null bytes；用 numpy+Cython 核
            _preload_dxcam_numpy_kernels()
            cam = dxcam.create(
                output_idx=0, output_color="RGB", processor_backend="numpy"
            )
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
                    return last, False
                return (None if last is None else last), True

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
        return arr[:, :, :3][:, :, ::-1].copy(), False

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
        self.native_width = int(native_w)
        self.native_height = int(native_h)
        self.width = int(native_w)
        self.height = int(native_h)
        self._logged_encode_size = False
        # 周期统计：排查糊屏/掉帧用
        self._stat_frames = 0
        self._stat_skips = 0
        self._stat_stale = 0  # grab 拿不到新帧、复用上一帧
        self._stat_null = 0
        self._stat_resized = 0
        self._stat_grab_ms = 0.0
        self._stat_resize_ms = 0.0
        self._stat_period_t0 = time.time()

    def take_period_stats(self) -> dict:
        """取出并清零上一周期计数，供 main 汇总打印。"""
        now = time.time()
        dt = max(now - self._stat_period_t0, 1e-6)
        frames = self._stat_frames
        skips = self._stat_skips
        stale = self._stat_stale
        nulls = self._stat_null
        resized = self._stat_resized
        grab_ms = self._stat_grab_ms
        resize_ms = self._stat_resize_ms
        self._stat_frames = 0
        self._stat_skips = 0
        self._stat_stale = 0
        self._stat_null = 0
        self._stat_resized = 0
        self._stat_grab_ms = 0.0
        self._stat_resize_ms = 0.0
        self._stat_period_t0 = now
        return {
            "dt": dt,
            "frames": frames,
            "fps": frames / dt,
            "skips": skips,
            "stale": stale,
            "null": nulls,
            "resized": resized,
            "avg_grab_ms": (grab_ms / frames) if frames else 0.0,
            "avg_resize_ms": (resize_ms / resized) if resized else 0.0,
            "encode_w": self.width,
            "encode_h": self.height,
            "native_w": self.native_width,
            "native_h": self.native_height,
            "max_width": self.max_width,
            "target_fps": self.fps,
        }

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
                self._stat_skips += 1
                wait = self._start + (self._timestamp / VIDEO_CLOCK_RATE) - time.time()
            if wait > 0:
                await asyncio.sleep(wait)
        else:
            self._start = time.time()
            self._timestamp = 0
        return self._timestamp, VIDEO_TIME_BASE

    async def recv(self):
        pts, time_base = await self.next_timestamp()
        t0 = time.perf_counter()
        grabbed = self._grab()
        grab_ms = (time.perf_counter() - t0) * 1000.0
        self._stat_grab_ms += grab_ms

        # 兼容旧 grab() 只返回 ndarray
        if isinstance(grabbed, tuple):
            rgb, stale = grabbed
        else:
            rgb, stale = grabbed, False
        if stale:
            self._stat_stale += 1
        if rgb is None:
            self._stat_null += 1
            rgb = np.zeros((self.native_height, self.native_width, 3), dtype=np.uint8)

        native_h, native_w = rgb.shape[:2]
        self.native_width = int(native_w)
        self.native_height = int(native_h)

        h, w = native_h, native_w
        if self.max_width > 0 and w > self.max_width:
            new_w = self.max_width
            new_h = max(1, int(h * (new_w / float(w))))
            t1 = time.perf_counter()
            rgb = _resize_rgb(rgb, new_w, new_h)
            self._stat_resize_ms += (time.perf_counter() - t1) * 1000.0
            self._stat_resized += 1
            self.width, self.height = new_w, new_h
        else:
            self.width, self.height = w, h

        if not self._logged_encode_size:
            self._logged_encode_size = True
            LOG.info(
                "video encode size %sx%s (native %sx%s maxWidth=%s track_fps=%s)",
                self.width,
                self.height,
                native_w,
                native_h,
                self.max_width,
                self.fps,
            )

        self._stat_frames += 1
        frame = VideoFrame.from_ndarray(rgb, format="rgb24")
        frame.pts = pts
        frame.time_base = time_base if time_base else fractions.Fraction(1, 90000)
        return frame

    def stop(self) -> None:
        try:
            self._close_backend()
        except Exception:
            pass

