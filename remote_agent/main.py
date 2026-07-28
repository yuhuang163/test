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
import json
import logging
import os
import sys
from pathlib import Path

from aiortc import RTCConfiguration, RTCIceServer, RTCPeerConnection, RTCSessionDescription
from aiortc.sdp import candidate_from_sdp
from websockets.asyncio.client import connect as ws_connect

from input_win import ensure_dpi_aware, handle_input_event, screen_size
from screen import ScreenVideoTrack

LOG = logging.getLogger("remote_agent")


def _apply_input(evt: dict, *, via: str) -> None:
    try:
        handle_input_event(evt)
    except Exception as exc:
        LOG.warning("input via %s failed: %s evt=%s", via, exc, evt.get("type"))


def _bind_datachannel(channel, *, label: str) -> None:
    @channel.on("open")
    def on_open() -> None:
        LOG.info("datachannel open: %s", label)

    @channel.on("message")
    def on_message(message) -> None:
        try:
            if isinstance(message, bytes):
                message = message.decode("utf-8", errors="ignore")
            evt = json.loads(message)
            if isinstance(evt, dict):
                _apply_input(evt, via=f"dc:{label}")
        except Exception as exc:
            LOG.warning("datachannel message failed: %s", exc)


def _setup_log(log_dir: Path | None) -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[logging.StreamHandler(sys.stdout)],
    )
    if log_dir:
        log_dir.mkdir(parents=True, exist_ok=True)
        fh = logging.FileHandler(log_dir / "remote_agent.log", encoding="utf-8")
        fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
        logging.getLogger().addHandler(fh)


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
    session_id = str(cfg.get("sessionId") or "").strip()
    token = str(cfg.get("agentToken") or "").strip()
    signaling_url = str(cfg.get("signalingUrl") or "").strip()
    if not session_id or not token or not signaling_url:
        raise SystemExit("config 缺少 sessionId / agentToken / signalingUrl")

    ensure_dpi_aware()
    sw, sh = screen_size()
    LOG.info("session=%s screen=%sx%s signaling=%s", session_id, sw, sh, signaling_url)

    pc = RTCPeerConnection(RTCConfiguration(iceServers=_ice_servers(cfg.get("iceServers"))))
    # 默认 1920/60fps；maxWidth=0 表示不缩小（最清晰，带宽更大）
    max_width = int(cfg["maxWidth"]) if cfg.get("maxWidth") is not None else 1920
    fps = int(cfg.get("fps") or 60)
    # 60fps 桌面推流需要更高码率；可通过 config.maxBitrate 覆盖
    max_bitrate = int(cfg.get("maxBitrate") or 12_000_000)
    track = ScreenVideoTrack(max_width=max_width, fps=fps)
    video_sender = pc.addTrack(track)
    channel = pc.createDataChannel("input")
    _bind_datachannel(channel, label="input")

    @pc.on("datachannel")
    def on_datachannel(ch) -> None:
        # 浏览器侧也可能自建通道；一并收键鼠
        _bind_datachannel(ch, label=str(getattr(ch, "label", "") or "peer"))

    async def _apply_video_quality() -> None:
        try:
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

    stop_event = asyncio.Event()
    offer_sent = False
    offer_lock = asyncio.Lock()

    async with ws_connect(
        signaling_url,
        open_timeout=20,
        max_size=4 * 1024 * 1024,
        ping_interval=15,
        ping_timeout=30,
    ) as ws:
        LOG.info("signaling connected")

        async def send_offer() -> None:
            nonlocal offer_sent
            async with offer_lock:
                if offer_sent:
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
                LOG.info("offer sent")

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
                            LOG.info("answer applied")
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
        try:
            await ws.send(json.dumps({"type": "hangup", "reason": "agent_exit"}, ensure_ascii=False))
        except Exception:
            pass

    await pc.close()
    track.stop()
    LOG.info("session ended")


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
