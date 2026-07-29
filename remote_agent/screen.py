"""主屏抓取 → aiortc VideoStreamTrack。Windows 优先 DXGI（dxcam）。"""

from __future__ import annotations

import asyncio
import fractions
import logging
import sys
import threading
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
    # INTER_LINEAR 比 INTER_AREA 快很多，远控降延迟优先（2560→1920 常可省数 ms）
    try:
        if _preload_cv2_pyd():
            import cv2

            return cv2.resize(rgb, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
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


def _rgb_to_yuv420_ndarray(rgb: np.ndarray) -> np.ndarray:
    """RGB24 → I420 ndarray（与 yuv420p 布局一致）。"""
    h, w = rgb.shape[:2]
    w2 = w - (w % 2)
    h2 = h - (h % 2)
    if w2 < 2 or h2 < 2:
        raise ValueError(f"frame too small for yuv420p: {w}x{h}")
    if w2 != w or h2 != h:
        rgb = rgb[:h2, :w2]
    if _preload_cv2_pyd():
        import cv2

        return cv2.cvtColor(rgb, cv2.COLOR_RGB2YUV_I420)
    frame = VideoFrame.from_ndarray(rgb, format="rgb24")
    return frame.reformat(format="yuv420p").to_ndarray(format="yuv420p")


def _rgb_to_yuv420_frame(rgb: np.ndarray) -> VideoFrame:
    """
    RGB24 → YUV420P。优先 OpenCV（产线机上 PyAV reformat 可达 30~90ms/帧）。
    I420 与 yuv420p 布局一致；宽高须为偶数。
    """
    try:
        yuv = _rgb_to_yuv420_ndarray(rgb)
        return VideoFrame.from_ndarray(yuv, format="yuv420p")
    except Exception as exc:
        LOG.debug("cv2 rgb2yuv failed, fallback PyAV: %s", exc)
    frame = VideoFrame.from_ndarray(rgb, format="rgb24")
    return frame.reformat(format="yuv420p")


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
        self._stat_caps = 0  # 抓屏线程处理次数
        self._stat_skips = 0
        self._stat_stale = 0  # grab 拿不到新帧、复用上一帧
        self._stat_null = 0
        self._stat_resized = 0
        self._stat_grab_ms = 0.0
        self._stat_resize_ms = 0.0
        self._stat_frame_ms = 0.0  # rgb→yuv（在抓屏线程）
        self._stat_recv_ms = 0.0  # recv 侧取最新帧耗时（应接近 0）
        self._stat_max_grab_ms = 0.0
        self._stat_max_resize_ms = 0.0
        self._stat_max_recv_ms = 0.0
        self._stat_skip_behind_ms = 0.0  # 跳帧时落后墙钟累计
        self._stat_period_t0 = time.time()
        self._stat_lock = threading.Lock()
        # 后台线程持续 grab+resize+yuv，事件循环只取最新帧，避免编码阻塞导致画面越来越旧
        self._latest_lock = threading.Lock()
        self._latest_yuv = None  # type: np.ndarray | None
        self._latest_meta = {
            "w": self.width,
            "h": self.height,
            "native_w": self.native_width,
            "native_h": self.native_height,
            "stale": False,
            "null": False,
        }
        self._worker_stop = threading.Event()
        self._worker = threading.Thread(
            target=self._capture_loop,
            name="screen-capture",
            daemon=True,
        )
        self._worker.start()

    def _add_worker_stats(
        self,
        *,
        grab_ms: float,
        resize_ms: float,
        frame_ms: float,
        resized: bool,
        stale: bool,
        null: bool,
    ) -> None:
        with self._stat_lock:
            self._stat_caps += 1
            self._stat_grab_ms += grab_ms
            if grab_ms > self._stat_max_grab_ms:
                self._stat_max_grab_ms = grab_ms
            if resized:
                self._stat_resize_ms += resize_ms
                self._stat_resized += 1
                if resize_ms > self._stat_max_resize_ms:
                    self._stat_max_resize_ms = resize_ms
            self._stat_frame_ms += frame_ms
            if stale:
                self._stat_stale += 1
            if null:
                self._stat_null += 1

    def _capture_loop(self) -> None:
        period = 1.0 / float(self.fps)
        while not self._worker_stop.is_set():
            t_loop0 = time.perf_counter()
            t0 = time.perf_counter()
            grabbed = self._grab()
            grab_ms = (time.perf_counter() - t0) * 1000.0

            if isinstance(grabbed, tuple):
                rgb, stale = grabbed
            else:
                rgb, stale = grabbed, False
            null = rgb is None
            if null:
                rgb = np.zeros((self.native_height, self.native_width, 3), dtype=np.uint8)

            native_h, native_w = rgb.shape[:2]
            h, w = native_h, native_w
            resize_ms = 0.0
            resized = False
            if self.max_width > 0 and w > self.max_width:
                new_w = self.max_width
                new_h = max(2, int(h * (new_w / float(w))))
                new_h -= new_h % 2
                new_w -= new_w % 2
                t1 = time.perf_counter()
                rgb = _resize_rgb(rgb, new_w, new_h)
                resize_ms = (time.perf_counter() - t1) * 1000.0
                resized = True
                w, h = new_w, new_h

            t2 = time.perf_counter()
            try:
                yuv = _rgb_to_yuv420_ndarray(rgb)
            except Exception:
                yuv = np.zeros((h + h // 2, w), dtype=np.uint8)
            frame_ms = (time.perf_counter() - t2) * 1000.0

            with self._latest_lock:
                self._latest_yuv = yuv
                self._latest_meta = {
                    "w": int(w),
                    "h": int(h),
                    "native_w": int(native_w),
                    "native_h": int(native_h),
                    "stale": bool(stale),
                    "null": bool(null),
                }

            self._add_worker_stats(
                grab_ms=grab_ms,
                resize_ms=resize_ms,
                frame_ms=frame_ms,
                resized=resized,
                stale=bool(stale),
                null=bool(null),
            )

            # 按目标帧率节流；慢于目标则不等待，始终刷新最新画面
            elapsed = time.perf_counter() - t_loop0
            sleep_s = period - elapsed
            if sleep_s > 0.001:
                self._worker_stop.wait(timeout=sleep_s)

    def take_period_stats(self) -> dict:
        """取出并清零上一周期计数，供 main 汇总打印。"""
        now = time.time()
        with self._stat_lock:
            dt = max(now - self._stat_period_t0, 1e-6)
            frames = self._stat_frames
            caps = self._stat_caps
            skips = self._stat_skips
            stale = self._stat_stale
            nulls = self._stat_null
            resized = self._stat_resized
            grab_ms = self._stat_grab_ms
            resize_ms = self._stat_resize_ms
            frame_ms = self._stat_frame_ms
            recv_ms = self._stat_recv_ms
            max_grab = self._stat_max_grab_ms
            max_resize = self._stat_max_resize_ms
            max_recv = self._stat_max_recv_ms
            skip_behind = self._stat_skip_behind_ms
            self._stat_frames = 0
            self._stat_caps = 0
            self._stat_skips = 0
            self._stat_stale = 0
            self._stat_null = 0
            self._stat_resized = 0
            self._stat_grab_ms = 0.0
            self._stat_resize_ms = 0.0
            self._stat_frame_ms = 0.0
            self._stat_recv_ms = 0.0
            self._stat_max_grab_ms = 0.0
            self._stat_max_resize_ms = 0.0
            self._stat_max_recv_ms = 0.0
            self._stat_skip_behind_ms = 0.0
            self._stat_period_t0 = now
        # grab/resize/rgb2yuv 在后台线程；recv 只取最新帧
        cap_n = max(caps, 1)
        return {
            "dt": dt,
            "frames": frames,
            "fps": frames / dt,
            "skips": skips,
            "stale": stale,
            "null": nulls,
            "resized": resized,
            "avg_grab_ms": grab_ms / cap_n,
            "avg_resize_ms": (resize_ms / resized) if resized else 0.0,
            "avg_frame_ms": frame_ms / cap_n,
            "avg_recv_ms": (recv_ms / frames) if frames else 0.0,
            "max_grab_ms": max_grab,
            "max_resize_ms": max_resize,
            "max_recv_ms": max_recv,
            "skip_behind_ms": skip_behind,
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
                # 记录落后量，便于区分「编码堵事件循环」还是「故意跳帧」
                with self._stat_lock:
                    self._stat_skip_behind_ms += (-wait) * 1000.0
                    self._stat_skips += 1
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
        t_recv0 = time.perf_counter()

        # 等首帧；之后始终取最新，丢弃排队中的旧画面
        yuv = None
        meta = None
        for _ in range(50):
            with self._latest_lock:
                yuv = self._latest_yuv
                meta = dict(self._latest_meta)
            if yuv is not None:
                break
            await asyncio.sleep(0.01)

        if yuv is None:
            w = max(2, self.width - (self.width % 2))
            h = max(2, self.height - (self.height % 2))
            yuv = np.zeros((h + h // 2, w), dtype=np.uint8)
            meta = {
                "w": w,
                "h": h,
                "native_w": self.native_width,
                "native_h": self.native_height,
                "stale": False,
                "null": True,
            }

        self.width = int(meta["w"])
        self.height = int(meta["h"])
        self.native_width = int(meta["native_w"])
        self.native_height = int(meta["native_h"])

        if not self._logged_encode_size:
            self._logged_encode_size = True
            LOG.info(
                "video encode size %sx%s (native %sx%s maxWidth=%s track_fps=%s capture=bg_thread)",
                self.width,
                self.height,
                self.native_width,
                self.native_height,
                self.max_width,
                self.fps,
            )

        frame = VideoFrame.from_ndarray(yuv, format="yuv420p")
        recv_ms = (time.perf_counter() - t_recv0) * 1000.0
        with self._stat_lock:
            self._stat_frames += 1
            self._stat_recv_ms += recv_ms
            if recv_ms > self._stat_max_recv_ms:
                self._stat_max_recv_ms = recv_ms
        frame.pts = pts
        frame.time_base = time_base if time_base else fractions.Fraction(1, 90000)
        return frame

    def stop(self) -> None:
        self._worker_stop.set()
        try:
            self._worker.join(timeout=1.0)
        except Exception:
            pass
        try:
            self._close_backend()
        except Exception:
            pass

