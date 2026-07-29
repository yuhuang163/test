"""抬高 aiortc 上限，并尽量使用硬件 H264 编码以拉高实际帧率。"""

from __future__ import annotations

import fractions
import json
import logging
import sys
import time
from pathlib import Path
from typing import Iterator

import av

LOG = logging.getLogger("remote_agent.low_latency")

SELECTED_ENCODER = "libx264"

# 编码侧周期统计（main 汇总打印后清零）
_ENC_STATS: dict = {
    "frames": 0,
    "bytes": 0,
    "reopens": 0,
    "br_hot": 0,  # 码率热更新次数（不重建）
    "keyframes": 0,
    "last_br": 0,
    "last_w": 0,
    "last_h": 0,
    "codec": "",
    "t0": 0.0,
    "encode_ms_sum": 0.0,
    "encode_ms_max": 0.0,
    "yuv_ms_sum": 0.0,
    "yuv_ms_max": 0.0,
}


def selected_encoder() -> str:
    return SELECTED_ENCODER


def is_hardware_encoder(name: str | None = None) -> bool:
    n = (name or SELECTED_ENCODER).lower()
    return n in {"h264_nvenc", "h264_qsv", "h264_amf", "h264_mf"}


def encode_size_for_max_width(screen_w: int, screen_h: int, max_width: int) -> tuple[int, int]:
    """按 maxWidth 等比缩放，宽高取偶数（与抓屏轨一致）。"""
    sw = max(int(screen_w), 2)
    sh = max(int(screen_h), 2)
    mw = int(max_width) if max_width and max_width > 0 else sw
    w = min(mw, sw)
    w -= w % 2
    h = int(round(sh * (w / float(sw))))
    h -= h % 2
    return max(w, 2), max(h, 2)


def probe_soft_encode_sustain(
    *,
    width: int,
    height: int,
    fps: int,
    bitrate: int,
    frames: int = 6,
) -> tuple[bool, float]:
    """
    软编能否稳住目标 fps：实测几帧 libx264 编码耗时。
    预留约 15ms 给抓屏+缩放；超时则建议降档。
    返回 (可维持, 平均每帧编码 ms)。
    """
    fps = max(1, int(fps))
    width = max(2, int(width) - int(width) % 2)
    height = max(2, int(height) - int(height) % 2)
    budget_ms = max(1000.0 / fps - 15.0, 8.0)
    probe_br = max(500_000, min(int(bitrate), 4_000_000))
    try:
        ctx = av.CodecContext.create("libx264", "w")
        _configure_encoder(ctx, "libx264", width, height, fps, probe_br)
        frame = av.VideoFrame(width=width, height=height, format="yuv420p")
        for p in frame.planes:
            p.update(bytes(p.buffer_size))
        # 预热 1 帧（含 open/首帧开销）
        frame.pts = 0
        frame.time_base = ctx.time_base
        list(ctx.encode(frame))
        times: list[float] = []
        for i in range(max(1, int(frames))):
            frame.pts = i + 1
            t0 = time.perf_counter()
            list(ctx.encode(frame))
            times.append((time.perf_counter() - t0) * 1000.0)
        list(ctx.encode(None))
        avg_ms = sum(times) / len(times)
        ok = avg_ms <= budget_ms
        LOG.info(
            "soft encode probe %sx%s @%sfps: avg=%.1fms budget=%.1fms -> %s",
            width,
            height,
            fps,
            avg_ms,
            budget_ms,
            "ok" if ok else "too_slow",
        )
        return ok, avg_ms
    except Exception as exc:
        LOG.warning("soft encode probe failed: %s", exc)
        return False, 9999.0


def take_encoder_period_stats() -> dict:
    """取出并清零上一周期编码统计。"""
    import time

    now = time.time()
    t0 = float(_ENC_STATS.get("t0") or now)
    dt = max(now - t0, 1e-6)
    frames = int(_ENC_STATS.get("frames") or 0)
    nbytes = int(_ENC_STATS.get("bytes") or 0)
    enc_sum = float(_ENC_STATS.get("encode_ms_sum") or 0.0)
    yuv_sum = float(_ENC_STATS.get("yuv_ms_sum") or 0.0)
    out = {
        "dt": dt,
        "frames": frames,
        "fps": frames / dt,
        "kbps": (nbytes * 8.0 / dt) / 1000.0,
        "reopens": int(_ENC_STATS.get("reopens") or 0),
        "br_hot": int(_ENC_STATS.get("br_hot") or 0),
        "keyframes": int(_ENC_STATS.get("keyframes") or 0),
        "target_br": int(_ENC_STATS.get("last_br") or 0),
        "width": int(_ENC_STATS.get("last_w") or 0),
        "height": int(_ENC_STATS.get("last_h") or 0),
        "codec": str(_ENC_STATS.get("codec") or ""),
        "avg_encode_ms": (enc_sum / frames) if frames else 0.0,
        "max_encode_ms": float(_ENC_STATS.get("encode_ms_max") or 0.0),
        "avg_yuv_ms": (yuv_sum / frames) if frames else 0.0,
        "max_yuv_ms": float(_ENC_STATS.get("yuv_ms_max") or 0.0),
    }
    _ENC_STATS["frames"] = 0
    _ENC_STATS["bytes"] = 0
    _ENC_STATS["reopens"] = 0
    _ENC_STATS["br_hot"] = 0
    _ENC_STATS["keyframes"] = 0
    _ENC_STATS["encode_ms_sum"] = 0.0
    _ENC_STATS["encode_ms_max"] = 0.0
    _ENC_STATS["yuv_ms_sum"] = 0.0
    _ENC_STATS["yuv_ms_max"] = 0.0
    _ENC_STATS["t0"] = now
    return out


def _configure_encoder(ctx: av.CodecContext, name: str, width: int, height: int, fps: int, bitrate: int) -> None:
    ctx.width = width
    ctx.height = height
    ctx.bit_rate = int(bitrate)
    ctx.pix_fmt = "yuv420p"
    ctx.framerate = fractions.Fraction(fps, 1)
    ctx.time_base = fractions.Fraction(1, fps)
    # 较短 GOP：花屏恢复快，远控延迟体感更好
    ctx.gop_size = max(int(fps) // 2, 8)
    if name == "libx264":
        # bufsize 偏小 → 更快跟上码率变化、降低编码器缓冲延迟
        buf = max(int(bitrate) // 4, 250_000)
        ctx.options = {
            "preset": "ultrafast",
            "tune": "zerolatency",
            "profile": "baseline",
            "bf": "0",
            "sc_threshold": "0",
            "rc-lookahead": "0",
            "sync-lookahead": "0",
            # 2 线程：1920 软编单线程常 16~26ms，略并行仍可压进 33ms 预算
            "threads": "2",
            "sliced-threads": "0",
            "maxrate": str(int(bitrate)),
            "bufsize": str(buf),
        }
        try:
            ctx.profile = "Baseline"
        except Exception:
            pass
    elif name == "h264_nvenc":
        ctx.options = {
            "preset": "p1",
            "tune": "ll",
            "rc": "cbr",
            "delay": "0",
            "zerolatency": "1",
            "bf": "0",
        }
    elif name == "h264_qsv":
        ctx.options = {"look_ahead": "0", "bf": "0"}
    elif name == "h264_amf":
        ctx.options = {"usage": "ultralowlatency", "quality": "speed", "bf": "0"}
    elif name == "h264_mf":
        ctx.options = {"hw_encoding": "1", "bf": "0"}


def _min_bitrate_for_size(width: int, height: int = 0) -> int:
    """远控清晰度下限：防止 GCC/REMB 把码率打到 500kbps 导致糊屏。"""
    w = int(width or 0)
    if w >= 1920:
        return 3_000_000
    if w >= 1280:
        return 2_000_000
    return 800_000


def _clamp_encode_bitrate(requested: int, *, width: int, max_cap: int | None = None) -> int:
    floor = _min_bitrate_for_size(width)
    br = max(floor, int(requested or 0))
    if max_cap and max_cap > 0:
        br = min(br, int(max_cap))
    return max(br, floor)


def _encoder_cache_path() -> Path:
    # onefile 解压目录不可靠，优先写 exe/脚本同目录
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent / "encoder_cache.json"
    return Path(__file__).resolve().parent / "encoder_cache.json"


def _load_encoder_cache() -> str | None:
    try:
        path = _encoder_cache_path()
        if not path.is_file():
            return None
        data = json.loads(path.read_text(encoding="utf-8"))
        name = str(data.get("encoder") or "").strip()
        ts = float(data.get("ts") or 0)
        # 7 天内有效；机器换显卡可手动删 encoder_cache.json
        if name and (time.time() - ts) < 7 * 24 * 3600:
            return name
    except Exception:
        pass
    return None


def _save_encoder_cache(name: str) -> None:
    try:
        path = _encoder_cache_path()
        path.write_text(
            json.dumps({"encoder": name, "ts": time.time()}, ensure_ascii=False),
            encoding="utf-8",
        )
    except Exception as exc:
        LOG.debug("encoder cache write failed: %s", exc)


def _probe_encoder(name: str, fps: int, bitrate: int) -> bool:
    """探测编码器能否真正工作。"""
    if name not in av.codecs_available:
        return False
    width, height = 640, 360
    try:
        ctx = av.CodecContext.create(name, "w")
        _configure_encoder(ctx, name, width, height, fps, bitrate)
        frame = av.VideoFrame(width=width, height=height, format="yuv420p")
        for p in frame.planes:
            p.update(bytes(p.buffer_size))
        frame.pts = 0
        frame.time_base = ctx.time_base
        list(ctx.encode(frame))
        list(ctx.encode(None))
        return True
    except Exception as exc:
        LOG.info("encoder %s unavailable: %s", name, exc)
        return False


def _probe_encoder_timed(name: str, fps: int, bitrate: int, *, timeout_s: float = 1.2) -> bool:
    """带超时的探测：h264_mf 等在部分机器上会卡死 open2。"""
    import threading

    box: list[bool | None] = [None]

    def _run() -> None:
        try:
            box[0] = _probe_encoder(name, fps=fps, bitrate=bitrate)
        except Exception as exc:
            LOG.info("encoder %s probe error: %s", name, exc)
            box[0] = False

    t0 = time.perf_counter()
    th = threading.Thread(target=_run, name=f"probe-{name}", daemon=True)
    th.start()
    th.join(timeout=max(0.2, float(timeout_s)))
    elapsed_ms = (time.perf_counter() - t0) * 1000
    if th.is_alive():
        # 无法强杀原生卡死线程，标记跳过；daemon 随进程退出
        LOG.info("encoder %s probe timeout (%.0fms), skip", name, elapsed_ms)
        return False
    ok = bool(box[0])
    if ok:
        LOG.info("encoder %s probe ok (%.0fms)", name, elapsed_ms)
    return ok


def pick_h264_encoder(*, fps: int, bitrate: int) -> str:
    global SELECTED_ENCODER
    probe_br = min(bitrate, 4_000_000)
    # 跳过 h264_mf：Media Foundation 在不少产线机上会卡死探测
    candidates = ("h264_nvenc", "h264_qsv", "h264_amf", "libx264")

    cached = _load_encoder_cache()
    if cached:
        t0 = time.perf_counter()
        # 缓存命中也限时，避免坏缓存反复卡启动
        ok = _probe_encoder_timed(cached, fps=fps, bitrate=probe_br, timeout_s=1.5)
        LOG.info(
            "encoder cache try %s -> %s (%.0fms)",
            cached,
            "ok" if ok else "fail",
            (time.perf_counter() - t0) * 1000,
        )
        if ok:
            SELECTED_ENCODER = cached
            LOG.info("selected video encoder: %s (cached)", cached)
            return cached

    t0 = time.perf_counter()
    for name in candidates:
        if cached and name == cached:
            continue
        # 硬件限时；软编给稍长一点
        timeout_s = 2.5 if name == "libx264" else 1.2
        if _probe_encoder_timed(name, fps=fps, bitrate=probe_br, timeout_s=timeout_s):
            SELECTED_ENCODER = name
            _save_encoder_cache(name)
            LOG.info(
                "selected video encoder: %s (probe %.0fms)",
                name,
                (time.perf_counter() - t0) * 1000,
            )
            return name
    SELECTED_ENCODER = "libx264"
    _save_encoder_cache(SELECTED_ENCODER)
    LOG.warning("fallback encoder: libx264 (probe %.0fms)", (time.perf_counter() - t0) * 1000)
    return SELECTED_ENCODER


def apply_aiortc_speedups(*, fps: int, max_bitrate: int) -> str:
    fps = max(1, min(int(fps), 60))
    max_bitrate = max(500_000, int(max_bitrate))

    import aiortc.mediastreams as ms
    from aiortc.codecs import h264 as h264_mod
    from aiortc.codecs import vpx as vpx_mod

    ms.VIDEO_PTIME = 1.0 / fps

    h264_mod.MAX_FRAME_RATE = max(fps, getattr(h264_mod, "MAX_FRAME_RATE", 30))
    h264_mod.MAX_BITRATE = max(max_bitrate, getattr(h264_mod, "MAX_BITRATE", 3_000_000))
    h264_mod.DEFAULT_BITRATE = min(max_bitrate, h264_mod.MAX_BITRATE)

    # 尽量别走 VP8：软编 1080p 更慢
    vpx_mod.MAX_FRAME_RATE = max(fps, getattr(vpx_mod, "MAX_FRAME_RATE", 30))
    vpx_mod.MAX_BITRATE = max(max_bitrate, getattr(vpx_mod, "MAX_BITRATE", 1_500_000))
    vpx_mod.DEFAULT_BITRATE = min(max_bitrate, vpx_mod.MAX_BITRATE)

    enc_name = pick_h264_encoder(fps=fps, bitrate=max_bitrate)
    _patch_h264_encoder(h264_mod, fps=fps, encoder_name=enc_name)
    LOG.info("aiortc speedups: fps=%s bitrate_cap=%s encoder=%s", fps, h264_mod.MAX_BITRATE, enc_name)
    return enc_name


def _patch_h264_encoder(h264_mod, *, fps: int, encoder_name: str) -> None:
    import time

    _ENC_STATS["t0"] = time.time()
    # 码率热更新日志节流：避免 GCC 每帧微调刷屏
    last_br_log_t = 0.0
    last_logged_br = 0
    max_cap = int(getattr(h264_mod, "MAX_BITRATE", 12_000_000) or 12_000_000)

    def _encode_frame(self, frame: av.VideoFrame, force_keyframe: bool) -> Iterator[bytes]:
        nonlocal last_br_log_t, last_logged_br
        reopen_reason = ""

        # 仅分辨率变化才重建；码率变化热更新 bit_rate，避免糊屏闪一下
        if self.codec and (frame.width != self.codec.width or frame.height != self.codec.height):
            reopen_reason = "size %sx%s->%sx%s" % (
                self.codec.width,
                self.codec.height,
                frame.width,
                frame.height,
            )
            LOG.warning("H264Encoder reopen: %s", reopen_reason)
            self.buffer_data = b""
            self.buffer_pts = None
            self.codec = None
            _ENC_STATS["reopens"] = int(_ENC_STATS.get("reopens") or 0) + 1
        elif self.codec:
            raw_br = max(100_000, int(self.target_bitrate))
            new_br = _clamp_encode_bitrate(raw_br, width=frame.width, max_cap=max_cap)
            old_br = int(self.codec.bit_rate or 0)
            if old_br > 0 and abs(new_br - old_br) / max(old_br, 1) > 0.02:
                # 已打开的编码器只改 bit_rate，勿 reopen / 勿改 options（改 options 常无效且可能异常）
                self.codec.bit_rate = new_br
                _ENC_STATS["br_hot"] = int(_ENC_STATS.get("br_hot") or 0) + 1
                now = time.time()
                if now - last_br_log_t >= 2.0 or abs(new_br - last_logged_br) / max(last_logged_br, 1) > 0.25:
                    if new_br > raw_br:
                        LOG.info(
                            "H264Encoder bitrate hot-update %s->%s (floor, gcc asked %s)",
                            old_br,
                            new_br,
                            raw_br,
                        )
                    else:
                        LOG.info("H264Encoder bitrate hot-update %s->%s (no reopen)", old_br, new_br)
                    last_br_log_t = now
                    last_logged_br = new_br

        if force_keyframe:
            frame.pict_type = av.video.frame.PictureType.I
            _ENC_STATS["keyframes"] = int(_ENC_STATS.get("keyframes") or 0) + 1
        else:
            frame.pict_type = av.video.frame.PictureType.NONE

        t_yuv0 = time.perf_counter()
        if frame.format.name != "yuv420p":
            # 兜底：抓屏轨应已输出 yuv420p；若仍是 rgb24，优先 cv2
            try:
                if frame.format.name == "rgb24":
                    import cv2
                    from av import VideoFrame as _VF

                    pts = frame.pts
                    tb = frame.time_base
                    rgb = frame.to_ndarray(format="rgb24")
                    yuv = cv2.cvtColor(rgb, cv2.COLOR_RGB2YUV_I420)
                    frame = _VF.from_ndarray(yuv, format="yuv420p")
                    frame.pts = pts
                    frame.time_base = tb
                else:
                    frame = frame.reformat(format="yuv420p")
            except Exception:
                frame = frame.reformat(format="yuv420p")
        yuv_ms = (time.perf_counter() - t_yuv0) * 1000.0
        _ENC_STATS["yuv_ms_sum"] = float(_ENC_STATS.get("yuv_ms_sum") or 0.0) + yuv_ms
        _ENC_STATS["yuv_ms_max"] = max(float(_ENC_STATS.get("yuv_ms_max") or 0.0), yuv_ms)

        if self.codec is None:
            name = encoder_name
            init_br = _clamp_encode_bitrate(
                int(self.target_bitrate), width=frame.width, max_cap=max_cap
            )
            try:
                self.codec = av.CodecContext.create(name, "w")
                _configure_encoder(self.codec, name, frame.width, frame.height, fps, init_br)
            except Exception as exc:
                LOG.warning("encoder %s failed (%s), use libx264", name, exc)
                name = "libx264"
                self.codec = av.CodecContext.create(name, "w")
                _configure_encoder(self.codec, name, frame.width, frame.height, fps, init_br)
            LOG.info(
                "H264Encoder using %s %sx%s @%sfps br=%s (floor=%s) reason=%s",
                name,
                frame.width,
                frame.height,
                fps,
                init_br,
                _min_bitrate_for_size(frame.width),
                reopen_reason or "init",
            )
            _ENC_STATS["codec"] = name
            last_logged_br = init_br

        t_enc0 = time.perf_counter()
        data_to_send = b""
        for package in self.codec.encode(frame):
            data_to_send += bytes(package)
        encode_ms = (time.perf_counter() - t_enc0) * 1000.0
        _ENC_STATS["encode_ms_sum"] = float(_ENC_STATS.get("encode_ms_sum") or 0.0) + encode_ms
        _ENC_STATS["encode_ms_max"] = max(float(_ENC_STATS.get("encode_ms_max") or 0.0), encode_ms)

        _ENC_STATS["frames"] = int(_ENC_STATS.get("frames") or 0) + 1
        _ENC_STATS["bytes"] = int(_ENC_STATS.get("bytes") or 0) + len(data_to_send)
        _ENC_STATS["last_br"] = int(self.codec.bit_rate or self.target_bitrate or 0)
        _ENC_STATS["last_w"] = int(frame.width)
        _ENC_STATS["last_h"] = int(frame.height)

        if data_to_send:
            yield from h264_mod.H264Encoder._split_bitstream(data_to_send)

    h264_mod.H264Encoder._encode_frame = _encode_frame
