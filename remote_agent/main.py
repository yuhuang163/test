"""
产线远控 Agent：WebSocket 信令 + WebRTC 推屏 + DataChannel 收键鼠。

用法：
  # 推荐：环境变量传配置（上位机默认，不落盘）
  set REMOTE_AGENT_CONFIG={...}
  python main.py --config env:REMOTE_AGENT_CONFIG

  # 或 stdin / 本地文件（手工调试）
  python main.py --config -
  python main.py --config session.json

配置字段：sessionId, agentToken, signalingUrl, iceServers
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import json
import logging
import os
import sys
import time
from pathlib import Path

from aiortc import RTCConfiguration, RTCIceServer, RTCPeerConnection, RTCSessionDescription
from aiortc.sdp import candidate_from_sdp
from websockets.asyncio.client import connect as ws_connect

from clipboard_win import (
    snapshot_clipboard,
    set_clipboard_files,
    set_clipboard_image_png,
    set_clipboard_text,
    write_incoming_files,
)
from input_win import ensure_dpi_aware, get_cursor_css, handle_input_event, key_combo, screen_size, VK_CONTROL, VK_V
from low_latency import (
    apply_aiortc_speedups,
    encode_size_for_max_width,
    is_hardware_encoder,
    probe_soft_encode_sustain,
    selected_encoder,
    take_encoder_period_stats,
)
from screen import ScreenVideoTrack

LOG = logging.getLogger("remote_agent")

# 日志里可据此确认是否为新版 Agent（与推流参数变更同步递增）
STREAM_PROFILE = "softprobe+cv2yuv+brfloor+bgcap"

# 软编探测仅告警，不自动降档（画质由网页下发，保持用户选择）
_SOFT_PROBE_WARN_ONLY = True
_SOFT_FALLBACK_WIDTH = 1280
_SOFT_FALLBACK_FPS = 20

# DataChannel 剪贴板分片（原始字节，base64 后约 *4/3）
_CLIP_CHUNK = 24 * 1024
_CLIP_INCOMING: dict = {}

# 已打开的 DataChannel，用于广播光标形状
_OPEN_CHANNELS: list = []


def _dc_send(channel, obj: dict) -> None:
    channel.send(json.dumps(obj, ensure_ascii=False))


def _b64_chunks(data: bytes, size: int = _CLIP_CHUNK) -> list[str]:
    out = []
    for i in range(0, len(data), size):
        out.append(base64.b64encode(data[i : i + size]).decode("ascii"))
    return out or [""]


def _send_clipboard_snapshot(channel, req_id) -> None:
    snap = snapshot_clipboard()
    kind = snap.get("kind") or "empty"
    if kind == "text":
        _dc_send(
            channel,
            {"type": "clipboard", "reqId": req_id, "kind": "text", "text": snap.get("text") or ""},
        )
        return
    if kind == "image":
        data = snap.get("data") or b""
        parts = _b64_chunks(data)
        _dc_send(
            channel,
            {
                "type": "clipboard",
                "reqId": req_id,
                "kind": "image",
                "mime": snap.get("mime") or "image/png",
                "bytes": len(data),
                "parts": len(parts),
            },
        )
        for i, chunk in enumerate(parts):
            _dc_send(
                channel,
                {
                    "type": "clipboard_part",
                    "reqId": req_id,
                    "role": "image",
                    "index": i,
                    "total": len(parts),
                    "data": chunk,
                },
            )
        return
    if kind == "files":
        files = snap.get("files") or []
        meta_files = []
        for fi, f in enumerate(files):
            name = str(f.get("name") or f"file{fi}")
            if f.get("error"):
                meta_files.append({"name": name, "error": str(f.get("error"))})
                continue
            data = f.get("data") or b""
            parts = _b64_chunks(data)
            meta_files.append({"name": name, "bytes": len(data), "parts": len(parts), "fileIndex": fi})
        _dc_send(
            channel,
            {
                "type": "clipboard",
                "reqId": req_id,
                "kind": "files",
                "files": meta_files,
            },
        )
        for fi, f in enumerate(files):
            if f.get("error"):
                continue
            data = f.get("data") or b""
            parts = _b64_chunks(data)
            name = str(f.get("name") or f"file{fi}")
            for i, chunk in enumerate(parts):
                _dc_send(
                    channel,
                    {
                        "type": "clipboard_part",
                        "reqId": req_id,
                        "role": "file",
                        "name": name,
                        "fileIndex": fi,
                        "index": i,
                        "total": len(parts),
                        "data": chunk,
                    },
                )
        return
    _dc_send(channel, {"type": "clipboard", "reqId": req_id, "kind": "empty"})


def _finish_incoming_paste(channel, req_id: str, slot: dict) -> None:
    kind = slot.get("kind")
    try:
        if kind == "text":
            set_clipboard_text(str(slot.get("text") or ""))
            time.sleep(0.05)
            key_combo([VK_CONTROL, VK_V])
            _dc_send(channel, {"type": "paste_done", "reqId": req_id, "kind": "text", "ok": True})
            return
        if kind == "image":
            raw = b"".join(slot.get("parts") or [])
            set_clipboard_image_png(raw)
            time.sleep(0.05)
            key_combo([VK_CONTROL, VK_V])
            _dc_send(
                channel,
                {"type": "paste_done", "reqId": req_id, "kind": "image", "ok": True, "bytes": len(raw)},
            )
            return
        if kind == "files":
            files_map: dict = slot.get("files") or {}
            packed = []
            for idx in sorted(files_map.keys()):
                info = files_map[idx]
                packed.append(
                    {
                        "name": info.get("name") or f"file{idx}",
                        "data": b"".join(info.get("parts") or []),
                    }
                )
            paths = write_incoming_files(packed)
            set_clipboard_files(paths)
            _dc_send(
                channel,
                {
                    "type": "paste_done",
                    "reqId": req_id,
                    "kind": "files",
                    "ok": True,
                    "count": len(paths),
                    "hint": "files on remote clipboard; Ctrl+V in Explorer",
                },
            )
            return
        raise ValueError(f"unknown paste kind {kind}")
    except Exception as exc:
        LOG.warning("paste apply failed: %s", exc)
        try:
            _dc_send(
                channel,
                {"type": "paste_done", "reqId": req_id, "kind": kind, "ok": False, "error": str(exc)},
            )
        except Exception:
            pass


def _handle_clipboard_message(channel, evt: dict) -> bool:
    """处理剪贴板相关 DC 消息；返回 True 表示已消费。"""
    mtype = str(evt.get("type") or "")
    if mtype == "clipboard_get":
        req_id = evt.get("reqId")
        try:
            _send_clipboard_snapshot(channel, req_id)
        except Exception as exc:
            LOG.warning("clipboard_get failed: %s", exc)
            try:
                _dc_send(
                    channel,
                    {
                        "type": "clipboard",
                        "reqId": req_id,
                        "kind": "empty",
                        "error": str(exc),
                    },
                )
            except Exception:
                pass
        return True

    if mtype == "paste":
        req_id = str(evt.get("reqId") or f"p{time.time_ns()}")
        kind = str(evt.get("kind") or "text")
        if kind == "text":
            slot = {"kind": "text", "text": str(evt.get("text") or "")}
            _finish_incoming_paste(channel, req_id, slot)
            return True
        if kind == "image":
            total = int(evt.get("parts") or 0)
            _CLIP_INCOMING[req_id] = {
                "kind": "image",
                "parts": [None] * max(total, 0),
                "total": total,
                "got": 0,
            }
            if total <= 0:
                b64 = str(evt.get("data") or "")
                if b64:
                    _CLIP_INCOMING[req_id]["parts"] = [base64.b64decode(b64)]
                    _CLIP_INCOMING[req_id]["total"] = 1
                    _CLIP_INCOMING[req_id]["got"] = 1
                    _finish_incoming_paste(channel, req_id, _CLIP_INCOMING.pop(req_id))
            return True
        if kind == "files":
            files = {}
            for meta in evt.get("files") or []:
                fi = int(meta.get("fileIndex") or 0)
                total = int(meta.get("parts") or 0)
                files[fi] = {
                    "name": str(meta.get("name") or f"file{fi}"),
                    "parts": [None] * max(total, 0),
                    "total": total,
                    "got": 0,
                }
            _CLIP_INCOMING[req_id] = {"kind": "files", "files": files}
            return True
        return True

    if mtype == "clipboard_part" and evt.get("dir") == "to_agent":
        req_id = str(evt.get("reqId") or "")
        slot = _CLIP_INCOMING.get(req_id)
        if not slot:
            return True
        role = str(evt.get("role") or "")
        idx = int(evt.get("index") or 0)
        raw = base64.b64decode(str(evt.get("data") or ""))
        if role == "image" and slot.get("kind") == "image":
            parts = slot["parts"]
            if 0 <= idx < len(parts) and parts[idx] is None:
                parts[idx] = raw
                slot["got"] = int(slot.get("got") or 0) + 1
            if slot["got"] >= int(slot.get("total") or 0) and all(p is not None for p in parts):
                _finish_incoming_paste(channel, req_id, _CLIP_INCOMING.pop(req_id))
            return True
        if role == "file" and slot.get("kind") == "files":
            fi = int(evt.get("fileIndex") or 0)
            info = (slot.get("files") or {}).get(fi)
            if not info:
                return True
            parts = info["parts"]
            if 0 <= idx < len(parts) and parts[idx] is None:
                parts[idx] = raw
                info["got"] = int(info.get("got") or 0) + 1
            files = slot.get("files") or {}
            if files and all(
                int(f.get("got") or 0) >= int(f.get("total") or 0)
                and all(p is not None for p in (f.get("parts") or []))
                for f in files.values()
            ):
                _finish_incoming_paste(channel, req_id, _CLIP_INCOMING.pop(req_id))
            return True
        return True

    return False


def _apply_input(evt: dict, *, via: str) -> None:
    try:
        handle_input_event(evt)
    except Exception as exc:
        LOG.warning("input via %s failed: %s evt=%s", via, exc, evt.get("type"))


def _bind_datachannel(channel, *, label: str) -> None:
    @channel.on("open")
    def on_open() -> None:
        LOG.info("datachannel open: %s", label)
        if channel not in _OPEN_CHANNELS:
            _OPEN_CHANNELS.append(channel)

    @channel.on("close")
    def on_close() -> None:
        try:
            _OPEN_CHANNELS.remove(channel)
        except ValueError:
            pass

    @channel.on("message")
    def on_message(message) -> None:
        try:
            if isinstance(message, bytes):
                message = message.decode("utf-8", errors="ignore")
            evt = json.loads(message)
            if not isinstance(evt, dict):
                return
            if evt.get("type") == "ping":
                try:
                    _dc_send(channel, {"type": "pong", "t": evt.get("t")})
                except Exception:
                    pass
                return
            if _handle_clipboard_message(channel, evt):
                return
            # 兼容旧 paste：{type:paste, text:...}
            if evt.get("type") == "paste" and evt.get("kind") in (None, "", "text") and "text" in evt:
                try:
                    set_clipboard_text(str(evt.get("text") or ""))
                    time.sleep(0.05)
                    key_combo([VK_CONTROL, VK_V])
                except Exception as exc:
                    LOG.warning("legacy paste failed: %s", exc)
                return
            _apply_input(evt, via=f"dc:{label}")
        except Exception as exc:
            LOG.warning("datachannel message failed: %s", exc)


def _broadcast_cursor(css: str) -> None:
    payload = json.dumps({"type": "cursor", "cursor": css}, ensure_ascii=False)
    dead = []
    for ch in list(_OPEN_CHANNELS):
        try:
            if getattr(ch, "readyState", "") != "open":
                dead.append(ch)
                continue
            ch.send(payload)
        except Exception:
            dead.append(ch)
    for ch in dead:
        try:
            _OPEN_CHANNELS.remove(ch)
        except ValueError:
            pass


class _FlushFileHandler(logging.FileHandler):
    """每条日志立即落盘，便于远控过程中用编辑器实时看 remote_agent.log。"""

    def emit(self, record: logging.LogRecord) -> None:
        super().emit(record)
        self.flush()


def _setup_log(log_dir: Path | None) -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[logging.StreamHandler(sys.stdout)],
        force=True,
    )
    if log_dir:
        log_dir.mkdir(parents=True, exist_ok=True)
        log_path = (log_dir / "remote_agent.log").resolve()
        fh = _FlushFileHandler(log_path, encoding="utf-8")
        fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
        logging.getLogger().addHandler(fh)
        logging.getLogger().info("log file: %s", log_path)


def _sdp_video_codecs(sdp: str) -> list[str]:
    """从 SDP 抽出视频 payload 对应的 codec 名，便于确认协商结果。"""
    if not sdp:
        return []
    lines = sdp.splitlines()
    in_video = False
    pt_names: dict[str, str] = {}
    used: list[str] = []
    for line in lines:
        if line.startswith("m=video"):
            in_video = True
            parts = line.split()
            used = parts[3:] if len(parts) > 3 else []
            continue
        if line.startswith("m="):
            in_video = False
            continue
        if not in_video:
            continue
        if line.startswith("a=rtpmap:"):
            # a=rtpmap:96 H264/90000
            body = line[len("a=rtpmap:") :]
            pt, _, rest = body.partition(" ")
            name = rest.split("/")[0].strip() if rest else ""
            if pt and name:
                pt_names[pt] = name
    return [pt_names.get(pt, pt) for pt in used]


async def _collect_outbound_video_stats(pc: RTCPeerConnection) -> dict:
    """汇总 outbound-rtp / codec / candidate-pair，用于糊屏排查。"""
    out: dict = {}
    try:
        report = await pc.getStats()
    except Exception as exc:
        return {"error": str(exc)}

    stats_list = list(report.values()) if report is not None else []

    def _g(stat, name, default=None):
        if isinstance(stat, dict):
            return stat.get(name, default)
        return getattr(stat, name, default)

    for stat in stats_list:
        stype = _g(stat, "type")
        kind = _g(stat, "kind")
        if stype == "outbound-rtp" and kind == "video":
            out["frames_encoded"] = _g(stat, "framesEncoded")
            out["frames_sent"] = _g(stat, "framesSent")
            out["bytes_sent"] = _g(stat, "bytesSent")
            out["frame_w"] = _g(stat, "frameWidth")
            out["frame_h"] = _g(stat, "frameHeight")
            out["fps"] = _g(stat, "framesPerSecond")
            out["key_frames"] = _g(stat, "keyFramesEncoded")
            out["fir"] = _g(stat, "firCount")
            out["pli"] = _g(stat, "pliCount")
            out["nack"] = _g(stat, "nackCount")
            out["qi_reason"] = _g(stat, "qualityLimitationReason")
            out["qi_durations"] = _g(stat, "qualityLimitationDurations")
            out["encode_time"] = _g(stat, "totalEncodeTime")
            out["target_br"] = _g(stat, "targetBitrate")
            out["codec_id"] = _g(stat, "codecId")
        elif stype == "candidate-pair" and (_g(stat, "state") == "succeeded" or _g(stat, "nominated")):
            rtt = _g(stat, "currentRoundTripTime")
            if rtt is not None:
                out["rtt_ms"] = round(float(rtt) * 1000)
            aob = _g(stat, "availableOutgoingBitrate")
            if aob is not None:
                out["avail_out_kbps"] = round(float(aob) / 1000)

    codec_id = out.get("codec_id")
    if codec_id:
        for stat in stats_list:
            if _g(stat, "type") == "codec" and _g(stat, "id") == codec_id:
                out["mime"] = _g(stat, "mimeType")
                out["clock"] = _g(stat, "clockRate")
                break
    return out


def _fmt_qi_durations(raw) -> str:
    if not raw:
        return "-"
    if isinstance(raw, dict):
        parts = []
        for k, v in raw.items():
            try:
                parts.append(f"{k}={float(v):.1f}s")
            except Exception:
                parts.append(f"{k}={v}")
        return ",".join(parts) if parts else "-"
    return str(raw)


def _ice_servers(raw) -> list[RTCIceServer]:
    servers: list[RTCIceServer] = []
    for item in raw or []:
        if not isinstance(item, dict):
            continue
        urls = item.get("urls")
        if isinstance(urls, str):
            urls = [urls]
        if not urls:
            continue
        servers.append(
            RTCIceServer(
                urls=urls,
                username=item.get("username"),
                credential=item.get("credential"),
            )
        )
    if not servers:
        servers.append(RTCIceServer(urls=["stun:stun.l.google.com:19302"]))
    return servers


async def run_session(cfg: dict) -> None:
    t_boot = time.perf_counter()
    session_id = str(cfg.get("sessionId") or "").strip()
    token = str(cfg.get("agentToken") or "").strip()
    signaling_url = str(cfg.get("signalingUrl") or "").strip()
    if not session_id or not token or not signaling_url:
        raise SystemExit("config 缺少 sessionId / agentToken / signalingUrl")

    LOG.info("agent profile=%s", STREAM_PROFILE)
    ensure_dpi_aware()
    sw, sh = screen_size()
    LOG.info("session=%s screen=%sx%s signaling=%s", session_id, sw, sh, signaling_url)

    pc = RTCPeerConnection(RTCConfiguration(iceServers=_ice_servers(cfg.get("iceServers"))))
    max_width = int(cfg.get("maxWidth", 1920))
    fps = int(cfg.get("fps", 30))
    max_bitrate = int(cfg.get("maxBitrate", 12_000_000))
    LOG.info(
        "config from host: maxWidth=%s fps=%s maxBitrate=%s (raw maxWidth=%s fps=%s)",
        max_width,
        fps,
        max_bitrate,
        cfg.get("maxWidth"),
        cfg.get("fps"),
    )
    t0 = time.perf_counter()
    enc_name = apply_aiortc_speedups(fps=fps, max_bitrate=max_bitrate)
    LOG.info("startup encoder ready in %.0fms (%s)", (time.perf_counter() - t0) * 1000, enc_name)
    if not is_hardware_encoder(enc_name):
        # 探测仅用于诊断；画质始终按网页/上位机下发，不自动降档
        enc_w, enc_h = encode_size_for_max_width(sw, sh, max_width)
        ok, avg_ms = probe_soft_encode_sustain(
            width=enc_w, height=enc_h, fps=fps, bitrate=max_bitrate
        )
        if ok:
            LOG.info(
                "soft encode %s: keep host %sx%s @%sfps (probe avg=%.1fms; synthetic black frames)",
                enc_name,
                enc_w,
                enc_h,
                fps,
                avg_ms,
            )
        else:
            LOG.warning(
                "soft encode %s: probe too slow (avg=%.1fms) for %sx%s@%sfps — "
                "仍保持网页画质不降档；若实跑 skip 多请看 [stream] 分阶段耗时",
                enc_name,
                avg_ms,
                enc_w,
                enc_h,
                fps,
            )
            if not _SOFT_PROBE_WARN_ONLY:
                adj_w = min(max_width if max_width > 0 else _SOFT_FALLBACK_WIDTH, _SOFT_FALLBACK_WIDTH)
                adj_fps = min(max(1, fps), _SOFT_FALLBACK_FPS)
                max_width = adj_w
                fps = adj_fps
                try:
                    import aiortc.mediastreams as ms

                    ms.VIDEO_PTIME = 1.0 / float(fps)
                except Exception:
                    pass
    LOG.info("capture/encode plan: maxWidth=%s fps=%s bitrate=%s encoder=%s", max_width, fps, max_bitrate, selected_encoder())

    # 抓屏初始化与信令连接并行，缩短「等待推流」
    loop = asyncio.get_running_loop()
    track_fut = loop.run_in_executor(
        None, lambda: ScreenVideoTrack(max_width=max_width, fps=fps)
    )

    channel = pc.createDataChannel("input")
    _bind_datachannel(channel, label="input")

    @pc.on("datachannel")
    def on_datachannel(ch) -> None:
        # 浏览器侧也可能自建通道；一并收键鼠
        _bind_datachannel(ch, label=str(getattr(ch, "label", "") or "peer"))

    stop_event = asyncio.Event()
    offer_sent = False
    offer_lock = asyncio.Lock()
    video_sender = None
    track = None

    async with ws_connect(
        signaling_url,
        open_timeout=20,
        max_size=4 * 1024 * 1024,
        ping_interval=15,
        ping_timeout=30,
    ) as ws:
        LOG.info(
            "signaling connected (%.0fms since boot)",
            (time.perf_counter() - t_boot) * 1000,
        )
        t_cap = time.perf_counter()
        track = await track_fut
        video_sender = pc.addTrack(track)
        LOG.info(
            "capture ready %sx%s in %.0fms (parallel with signaling)",
            track.width,
            track.height,
            (time.perf_counter() - t_cap) * 1000,
        )
        try:
            from aiortc import RTCRtpSender

            caps = RTCRtpSender.getCapabilities("video")
            if caps and caps.codecs:
                preferred = sorted(
                    caps.codecs,
                    key=lambda c: 0 if "h264" in (c.mimeType or "").lower() else 1,
                )
                for tr in pc.getTransceivers():
                    if tr.sender is video_sender:
                        tr.setCodecPreferences(preferred)
                        break
        except Exception as exc:
            LOG.warning("setCodecPreferences failed: %s", exc)

        async def _apply_video_quality() -> None:
            try:
                # 部分 aiortc 版本无 getParameters；编码上限已由 low_latency 热更新覆盖
                if not hasattr(video_sender, "getParameters") or not hasattr(video_sender, "setParameters"):
                    LOG.info(
                        "video quality (encoder-side): maxWidth=%s fps=%s maxBitrate=%s",
                        max_width,
                        fps,
                        max_bitrate,
                    )
                    return
                params = video_sender.getParameters()
                if not params.encodings:
                    from aiortc.rtcrtpparameters import RTCRtpEncodingParameters

                    params.encodings = [
                        RTCRtpEncodingParameters(maxBitrate=max_bitrate, maxFramerate=float(fps))
                    ]
                else:
                    for enc in params.encodings:
                        if isinstance(enc, dict):
                            enc["maxBitrate"] = max_bitrate
                            enc["maxFramerate"] = float(fps)
                        else:
                            enc.maxBitrate = max_bitrate
                            enc.maxFramerate = float(fps)
                await video_sender.setParameters(params)
                LOG.info("video quality: maxWidth=%s fps=%s maxBitrate=%s", max_width, fps, max_bitrate)
            except Exception as exc:
                LOG.warning("set video bitrate failed: %s", exc)

        async def send_offer() -> None:
            nonlocal offer_sent
            async with offer_lock:
                if offer_sent:
                    return
                if track is None:
                    return
                offer = await pc.createOffer()
                await pc.setLocalDescription(offer)
                await ws.send(
                    json.dumps(
                        {
                            "type": "offer",
                            "sdp": pc.localDescription.sdp,
                            "screenWidth": track.width or sw,
                            "screenHeight": track.height or sh,
                        },
                        ensure_ascii=False,
                    )
                )
                offer_sent = True
                LOG.info(
                    "offer sent (%.0fms since boot)",
                    (time.perf_counter() - t_boot) * 1000,
                )

        async def reader() -> None:
            try:
                async for raw in ws:
                    try:
                        msg = json.loads(raw)
                    except json.JSONDecodeError:
                        continue
                    if not isinstance(msg, dict):
                        continue
                    mtype = str(msg.get("type") or "")
                    if mtype == "ready":
                        try:
                            await send_offer()
                        except Exception:
                            LOG.exception("send_offer on ready failed")
                    elif mtype == "answer":
                        sdp = str(msg.get("sdp") or "")
                        if sdp:
                            await pc.setRemoteDescription(RTCSessionDescription(sdp=sdp, type="answer"))
                            codecs = _sdp_video_codecs(sdp)
                            LOG.info("answer applied negotiated_video=%s", codecs or "?")
                            await _apply_video_quality()
                    elif mtype == "ice":
                        c = msg.get("candidate") or {}
                        cand_str = str(c.get("candidate") or "")
                        if not cand_str:
                            continue
                        try:
                            ice = candidate_from_sdp(cand_str)
                            ice.sdpMid = c.get("sdpMid")
                            ice.sdpMLineIndex = c.get("sdpMLineIndex")
                            await pc.addIceCandidate(ice)
                        except Exception as exc:
                            LOG.warning("addIceCandidate failed: %s", exc)
                    elif mtype == "input":
                        data = msg.get("data")
                        if isinstance(data, dict):
                            _apply_input(data, via="ws")
                    elif mtype == "hangup":
                        LOG.info("hangup: %s", msg.get("reason"))
                        stop_event.set()
                        break
            except Exception as exc:
                LOG.warning("signaling reader ended: %s", exc)

        reader_task = asyncio.create_task(reader())

        async def _cursor_loop() -> None:
            last = ""
            while not stop_event.is_set():
                try:
                    css = get_cursor_css()
                    if css != last and _OPEN_CHANNELS:
                        _broadcast_cursor(css)
                        last = css
                except Exception as exc:
                    LOG.debug("cursor poll failed: %s", exc)
                try:
                    await asyncio.wait_for(stop_event.wait(), timeout=0.08)
                    break
                except asyncio.TimeoutError:
                    pass

        cursor_task = asyncio.create_task(_cursor_loop())

        async def _stream_diag_loop() -> None:
            """每 5 秒打一条推流诊断：抓屏 / 编码 / WebRTC 出站。"""
            last_bytes = 0
            last_frames = 0
            last_t = time.time()
            # 等 offer/answer 后再开始，避免空统计刷屏
            while not stop_event.is_set():
                try:
                    await asyncio.wait_for(stop_event.wait(), timeout=5.0)
                    break
                except asyncio.TimeoutError:
                    pass
                if stop_event.is_set():
                    break
                try:
                    cap = track.take_period_stats()
                    enc = take_encoder_period_stats()
                    rtp = await _collect_outbound_video_stats(pc)
                    now = time.time()
                    dt = max(now - last_t, 1e-6)
                    bytes_sent = int(rtp.get("bytes_sent") or 0)
                    frames_enc = int(rtp.get("frames_encoded") or 0)
                    rtp_kbps = ((bytes_sent - last_bytes) * 8.0 / dt) / 1000.0 if last_bytes else 0.0
                    rtp_fps = (frames_enc - last_frames) / dt if last_frames else float(rtp.get("fps") or 0)
                    last_bytes = bytes_sent
                    last_frames = frames_enc
                    last_t = now

                    qi = rtp.get("qi_reason") or "none"
                    budget_ms = 1000.0 / max(float(cap.get("target_fps") or 30), 1.0)
                    avg_grab = float(cap.get("avg_grab_ms") or 0)
                    avg_resize = float(cap.get("avg_resize_ms") or 0)
                    avg_frame = float(cap.get("avg_frame_ms") or 0)
                    avg_recv = float(cap.get("avg_recv_ms") or 0)
                    avg_yuv = float(enc.get("avg_yuv_ms") or 0)
                    avg_enc = float(enc.get("avg_encode_ms") or 0)
                    pipeline_ms = avg_recv + avg_yuv + avg_enc
                    LOG.info(
                        "[stream] capture native=%sx%s encode=%sx%s fps=%.1f/%s skip=%s stale=%s null=%s "
                        "grab=%.1f/%.1fms resize=%.1f/%.1fms/%s rgb2yuv=%.1fms recv=%.1f/%.1fms "
                        "behind=%.0fms | enc %s %sx%s fps=%.1f yuv=%.1f/%.1fms enc=%.1f/%.1fms "
                        "kbps=%.0f target_br=%s reopen=%s br_hot=%s key=%s | "
                        "rtp fps=%.1f kbps=%.0f size=%sx%s mime=%s qi=%s qi_dur=%s "
                        "pli=%s nack=%s fir=%s rtt=%sms avail_out=%skbps ice_target_br=%s",
                        cap.get("native_w"),
                        cap.get("native_h"),
                        cap.get("encode_w"),
                        cap.get("encode_h"),
                        cap.get("fps") or 0,
                        cap.get("target_fps"),
                        cap.get("skips"),
                        cap.get("stale"),
                        cap.get("null"),
                        avg_grab,
                        float(cap.get("max_grab_ms") or 0),
                        avg_resize,
                        float(cap.get("max_resize_ms") or 0),
                        cap.get("resized"),
                        avg_frame,
                        avg_recv,
                        float(cap.get("max_recv_ms") or 0),
                        float(cap.get("skip_behind_ms") or 0),
                        enc.get("codec") or selected_encoder(),
                        enc.get("width"),
                        enc.get("height"),
                        enc.get("fps") or 0,
                        avg_yuv,
                        float(enc.get("max_yuv_ms") or 0),
                        avg_enc,
                        float(enc.get("max_encode_ms") or 0),
                        enc.get("kbps") or 0,
                        enc.get("target_br"),
                        enc.get("reopens"),
                        enc.get("br_hot"),
                        enc.get("keyframes"),
                        rtp_fps,
                        rtp_kbps,
                        rtp.get("frame_w") or "?",
                        rtp.get("frame_h") or "?",
                        rtp.get("mime") or "?",
                        qi,
                        _fmt_qi_durations(rtp.get("qi_durations")),
                        rtp.get("pli"),
                        rtp.get("nack"),
                        rtp.get("fir"),
                        rtp.get("rtt_ms"),
                        rtp.get("avail_out_kbps"),
                        rtp.get("target_br"),
                    )
                    # 瓶颈归因：事件循环上 mainly enc；grab/resize 在后台线程（仍可能抢 CPU）
                    reasons = []
                    if avg_enc >= budget_ms * 0.55:
                        reasons.append(f"encode_cpu({avg_enc:.1f}ms)")
                    if avg_yuv >= budget_ms * 0.25:
                        reasons.append(f"rgb2yuv({avg_yuv:.1f}ms)")
                    if avg_recv >= budget_ms * 0.25:
                        reasons.append(f"recv({avg_recv:.1f}ms)")
                    if avg_resize >= 12.0:
                        reasons.append(f"bg_resize({avg_resize:.1f}ms)")
                    if avg_grab >= 20.0:
                        reasons.append(f"bg_grab({avg_grab:.1f}ms)")
                    target_br = int(enc.get("target_br") or 0)
                    if target_br and target_br < 2_000_000 and (cap.get("encode_w") or 0) >= 1280:
                        reasons.append(f"low_bitrate({target_br})")
                    if (cap.get("skips") or 0) > (cap.get("target_fps") or 30) and not reasons:
                        reasons.append("timeline_behind(event_loop_busy?)")
                    LOG.info(
                        "[stream.bottleneck] budget=%.1fms/frame loop≈%.1fms (recv=%.1f yuv=%.1f enc=%.1f) "
                        "bg(grab=%.1f resize=%.1f rgb2yuv=%.1f) -> %s",
                        budget_ms,
                        pipeline_ms,
                        avg_recv,
                        avg_yuv,
                        avg_enc,
                        avg_grab,
                        avg_resize,
                        avg_frame,
                        "+".join(reasons) if reasons else "ok",
                    )
                    # 糊屏高相关告警（target_br 已是编码器实际码率；floor 后应 ≥2Mbps@1280）
                    if qi and qi not in ("none", "None"):
                        LOG.warning(
                            "[stream] qualityLimitationReason=%s（bandwidth/cpu 都会让画面变糊）dur=%s",
                            qi,
                            _fmt_qi_durations(rtp.get("qi_durations")),
                        )
                    ice_br = int(rtp.get("target_br") or 0)
                    if ice_br and ice_br < 2_000_000 and (cap.get("encode_w") or 0) >= 1280:
                        LOG.warning(
                            "[stream] GCC/ICE 估带宽偏低 ice_target_br=%s（已用码率下限保护，实际 enc_br=%s）@%sx%s",
                            ice_br,
                            target_br,
                            cap.get("encode_w"),
                            cap.get("encode_h"),
                        )
                    if (cap.get("skips") or 0) > (cap.get("target_fps") or 30):
                        LOG.warning(
                            "[stream] 抓屏侧跳帧偏多 skip=%s behind=%.0fms（多为编码/转换阻塞事件循环）",
                            cap.get("skips"),
                            float(cap.get("skip_behind_ms") or 0),
                        )
                except Exception as exc:
                    LOG.warning("[stream] diag failed: %s", exc)

        # connection / ICE 状态也打日志，区分「连不上」和「连上但糊」
        @pc.on("connectionstatechange")
        def on_conn_state() -> None:
            LOG.info("pc connectionState=%s", pc.connectionState)

        @pc.on("iceconnectionstatechange")
        def on_ice_state() -> None:
            LOG.info("pc iceConnectionState=%s", pc.iceConnectionState)

        diag_task = asyncio.create_task(_stream_diag_loop())

        async def _kick_offer() -> None:
            for delay in (0.0, 0.5, 1.5, 3.0):
                if stop_event.is_set() or offer_sent:
                    return
                if delay > 0:
                    await asyncio.sleep(delay)
                if stop_event.is_set() or offer_sent:
                    return
                try:
                    await send_offer()
                    return
                except Exception:
                    LOG.exception("send_offer retry failed")

        asyncio.create_task(_kick_offer())

        await asyncio.wait(
            [asyncio.create_task(stop_event.wait()), reader_task],
            return_when=asyncio.FIRST_COMPLETED,
        )
        stop_event.set()
        reader_task.cancel()
        cursor_task.cancel()
        diag_task.cancel()
        try:
            await ws.send(json.dumps({"type": "hangup", "reason": "agent_exit"}, ensure_ascii=False))
        except Exception:
            pass

    await pc.close()
    if track is not None:
        track.stop()
    LOG.info("session ended (total %.0fms)", (time.perf_counter() - t_boot) * 1000)


def _load_config(spec: str) -> dict:
    """spec: 文件路径 | '-'（stdin）| 'env:NAME'（环境变量）。"""
    raw = ""
    if spec == "-" or not spec.strip():
        raw = sys.stdin.read()
        if not raw.strip():
            raw = os.environ.get("REMOTE_AGENT_CONFIG", "")
    elif spec.startswith("env:"):
        raw = os.environ.get(spec[4:], "")
    else:
        raw = Path(spec).read_text(encoding="utf-8")
    if not str(raw).strip():
        raise SystemExit("config 为空（请用 --config 文件/env:VAR/-）")
    data = json.loads(raw)
    if not isinstance(data, dict):
        raise SystemExit("config 必须是 JSON 对象")
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description="Lute remote desktop agent")
    parser.add_argument(
        "--config",
        required=True,
        help="session json 路径，或 env:VAR / -（stdin）",
    )
    parser.add_argument("--log-dir", default="", help="optional log directory")
    args = parser.parse_args()
    cfg = _load_config(args.config)
    log_dir = Path(args.log_dir) if args.log_dir else Path.cwd()
    _setup_log(log_dir)
    try:
        asyncio.run(run_session(cfg))
    except KeyboardInterrupt:
        pass
    except Exception:
        LOG.exception("agent crashed")
        raise SystemExit(1)


if __name__ == "__main__":
    main()
