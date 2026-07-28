"""抬高 aiortc 上限，并尽量使用硬件 H264 编码以拉高实际帧率。"""

from __future__ import annotations

import fractions
import logging
from typing import Iterator

import av

LOG = logging.getLogger("remote_agent.low_latency")

SELECTED_ENCODER = "libx264"


def selected_encoder() -> str:
    return SELECTED_ENCODER


def is_hardware_encoder(name: str | None = None) -> bool:
    n = (name or SELECTED_ENCODER).lower()
    return n in {"h264_nvenc", "h264_qsv", "h264_amf", "h264_mf"}


def _configure_encoder(ctx: av.CodecContext, name: str, width: int, height: int, fps: int, bitrate: int) -> None:
    ctx.width = width
    ctx.height = height
    ctx.bit_rate = int(bitrate)
    ctx.pix_fmt = "yuv420p"
    ctx.framerate = fractions.Fraction(fps, 1)
    ctx.time_base = fractions.Fraction(1, fps)
    ctx.gop_size = max(int(fps), 1)
    if name == "libx264":
        ctx.options = {
            "preset": "ultrafast",
            "tune": "zerolatency",
            "profile": "baseline",
            "bf": "0",
            "sc_threshold": "0",
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


def pick_h264_encoder(*, fps: int, bitrate: int) -> str:
    global SELECTED_ENCODER
    for name in ("h264_nvenc", "h264_qsv", "h264_amf", "h264_mf", "libx264"):
        if _probe_encoder(name, fps=fps, bitrate=min(bitrate, 4_000_000)):
            SELECTED_ENCODER = name
            LOG.info("selected video encoder: %s", name)
            return name
    SELECTED_ENCODER = "libx264"
    LOG.warning("fallback encoder: libx264")
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
    def _encode_frame(self, frame: av.VideoFrame, force_keyframe: bool) -> Iterator[bytes]:
        if self.codec and (
            frame.width != self.codec.width
            or frame.height != self.codec.height
            or abs(self.target_bitrate - self.codec.bit_rate) / max(self.codec.bit_rate, 1) > 0.1
        ):
            self.buffer_data = b""
            self.buffer_pts = None
            self.codec = None

        if force_keyframe:
            frame.pict_type = av.video.frame.PictureType.I
        else:
            frame.pict_type = av.video.frame.PictureType.NONE

        if frame.format.name != "yuv420p":
            frame = frame.reformat(format="yuv420p")

        if self.codec is None:
            name = encoder_name
            try:
                self.codec = av.CodecContext.create(name, "w")
                _configure_encoder(self.codec, name, frame.width, frame.height, fps, int(self.target_bitrate))
            except Exception as exc:
                LOG.warning("encoder %s failed (%s), use libx264", name, exc)
                name = "libx264"
                self.codec = av.CodecContext.create(name, "w")
                _configure_encoder(self.codec, name, frame.width, frame.height, fps, int(self.target_bitrate))
            LOG.info(
                "H264Encoder using %s %sx%s @%sfps br=%s",
                name,
                frame.width,
                frame.height,
                fps,
                self.target_bitrate,
            )

        data_to_send = b""
        for package in self.codec.encode(frame):
            data_to_send += bytes(package)

        if data_to_send:
            yield from h264_mod.H264Encoder._split_bitstream(data_to_send)

    h264_mod.H264Encoder._encode_frame = _encode_frame
