"""WebRTC 远控：创建/停止会话 + WebSocket 信令转发。"""

from __future__ import annotations

import asyncio
import json
from typing import Annotated, Any

from fastapi import APIRouter, Body, Depends, Query, WebSocket, WebSocketDisconnect
from jose import JWTError

from app.deps import get_current_user
from app.models import User
from app.response import fail, ok
from app.services import device_runtime
from app.services import remote_desktop as rd

router = APIRouter(prefix="/admin/remote-desktop", tags=["remote-desktop-admin"])
ws_router = APIRouter(tags=["remote-desktop-ws"])


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        fail(403, "仅 engineer / admin 可远控", 403)


def _ws_url_hint(session_id: str, role: str, token: str) -> str:
    return f"/api/factory-tool/remote-desktop/ws?sessionId={session_id}&role={role}&token={token}"


@router.get("/online-devices")
def list_remote_devices(user: Annotated[User, Depends(get_current_user)]):
    """在线产线机列表（远控入口）。"""
    _require_engineer_or_admin(user)
    items = device_runtime.list_online_devices()
    return ok({"items": items})


@router.post("/sessions")
def create_remote_session(
    user: Annotated[User, Depends(get_current_user)],
    body: Annotated[dict[str, Any], Body()],
):
    """创建远控会话并通知产线机拉起 Agent。"""
    _require_engineer_or_admin(user)
    device_id = str(body.get("deviceId") or "").strip()
    if not device_id:
        fail(400, "deviceId 不能为空", 400)
    if not device_runtime.is_online(device_id):
        fail(404, "设备不在线或心跳已过期", 404)

    online = {d["deviceId"]: d for d in device_runtime.list_online_devices()}
    host_name = str((online.get(device_id) or {}).get("hostName") or device_id)

    try:
        sess = rd.create_session(
            device_id=device_id,
            host_name=host_name,
            created_by=user.username,
        )
    except ValueError as exc:
        fail(409, str(exc), 409)

    signaling_base = "/api/factory-tool/remote-desktop/ws"
    payload = {
        "sessionId": sess.session_id,
        "agentToken": sess.agent_token,
        "signalingPath": signaling_base,
        "iceServers": rd.make_turn_credentials().get("iceServers") or [],
        "role": "agent",
    }
    try:
        device_runtime.enqueue_command(device_id, "start_remote_desktop", payload)
    except ValueError as exc:
        rd.stop_session(sess.session_id, "enqueue_failed")
        fail(400, str(exc), 400)

    data = rd.session_public(sess)
    data["viewerWsPath"] = _ws_url_hint(sess.session_id, "viewer", sess.viewer_token)
    return ok(data)


@router.post("/sessions/{session_id}/stop")
def stop_remote_session(
    session_id: str,
    user: Annotated[User, Depends(get_current_user)],
):
    _require_engineer_or_admin(user)
    sess = rd.get_session(session_id)
    if not sess:
        fail(404, "会话不存在", 404)
    rd.stop_session(session_id, user.username)
    try:
        device_runtime.enqueue_command(
            sess.device_id,
            "stop_remote_desktop",
            {"sessionId": session_id},
        )
    except ValueError:
        pass
    # 信令侧：尽量通知仍在线的 WS（失败忽略，Agent 也会收到 stop 命令）
    room = rd.get_room(session_id)
    if room:
        sockets = []
        with room.lock:
            if room.viewer is not None:
                sockets.append(room.viewer)
            if room.agent is not None:
                sockets.append(room.agent)
        msg = json.dumps({"type": "hangup", "reason": "stopped"}, ensure_ascii=False)

        async def _notify() -> None:
            for ws in sockets:
                try:
                    await ws.send_text(msg)
                except Exception:
                    pass

        try:
            loop = asyncio.get_running_loop()
            loop.create_task(_notify())
        except RuntimeError:
            pass
    return ok({"sessionId": session_id, "status": "stopped"})


@router.get("/sessions/{session_id}")
def get_remote_session(
    session_id: str,
    user: Annotated[User, Depends(get_current_user)],
):
    _require_engineer_or_admin(user)
    sess = rd.get_session(session_id)
    if not sess:
        fail(404, "会话不存在", 404)
    return ok(rd.session_public(sess))


async def _broadcast_hangup(room: rd.SignalingRoom, reason: str) -> None:
    msg = json.dumps({"type": "hangup", "reason": reason}, ensure_ascii=False)
    sockets = []
    with room.lock:
        if room.viewer is not None:
            sockets.append(room.viewer)
        if room.agent is not None:
            sockets.append(room.agent)
    for ws in sockets:
        try:
            await ws.send_text(msg)
        except Exception:
            pass


@ws_router.websocket("/remote-desktop/ws")
async def remote_desktop_ws(
    websocket: WebSocket,
    sessionId: Annotated[str, Query()],
    role: Annotated[str, Query()],
    token: Annotated[str, Query()],
):
    """信令通道：转发 offer/answer/ice/hangup。role=viewer|agent。"""
    role_norm = (role or "").strip().lower()
    if role_norm not in {"viewer", "agent"}:
        await websocket.close(code=4400)
        return

    try:
        claims = rd.decode_signaling_token(token)
    except JWTError:
        await websocket.close(code=4401)
        return

    if claims.get("sid") != sessionId or claims.get("role") != role_norm:
        await websocket.close(code=4403)
        return

    sess = rd.get_session(sessionId)
    if not sess or sess.status == "stopped":
        await websocket.close(code=4404)
        return
    if claims.get("deviceId") != sess.device_id:
        await websocket.close(code=4403)
        return

    room = rd.get_room(sessionId)
    if not room:
        await websocket.close(code=4404)
        return

    await websocket.accept()

    with room.lock:
        if role_norm == "viewer":
            old = room.viewer
            room.viewer = websocket
            pending = list(room.pending_to_viewer)
            room.pending_to_viewer.clear()
        else:
            old = room.agent
            room.agent = websocket
            pending = list(room.pending_to_agent)
            room.pending_to_agent.clear()

    if old is not None and old is not websocket:
        try:
            await old.close(code=4000)
        except Exception:
            pass

    for cached in pending:
        try:
            await websocket.send_text(cached)
        except Exception:
            break

    if role_norm == "agent":
        rd.mark_session_active(sessionId)

    # 双方都在线时提示 agent 发 offer（可重复，Agent 侧去重）
    with room.lock:
        both = room.viewer is not None and room.agent is not None
        agent_ws = room.agent
    if both and agent_ws is not None:
        try:
            await agent_ws.send_text(json.dumps({"type": "ready"}, ensure_ascii=False))
        except Exception:
            pass

    try:
        while True:
            raw = await websocket.receive_text()
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if not isinstance(msg, dict):
                continue
            msg_type = str(msg.get("type") or "").strip()
            if msg_type not in {"offer", "answer", "ice", "hangup", "ready", "input"}:
                continue

            if msg_type == "hangup":
                rd.stop_session(sessionId, str(claims.get("sub") or role_norm))
                await _broadcast_hangup(room, "peer_hangup")
                break

            # 键鼠信令：仅 viewer→agent，不缓存（避免堆积）
            if msg_type == "input":
                if role_norm != "viewer":
                    continue
                with room.lock:
                    peer = room.agent
                if peer is None:
                    continue
                try:
                    await peer.send_text(json.dumps(msg, ensure_ascii=False))
                except Exception:
                    pass
                continue

            payload = json.dumps(msg, ensure_ascii=False)
            with room.lock:
                peer = room.agent if role_norm == "viewer" else room.viewer
                if peer is None:
                    # 缓存最新 offer/answer；ice 追加（限制条数）
                    bucket = room.pending_to_agent if role_norm == "viewer" else room.pending_to_viewer
                    if msg_type in {"offer", "answer"}:
                        bucket[:] = [m for m in bucket if '"type": "ice"' in m or '"type":"ice"' in m]
                        bucket.insert(0, payload)
                    else:
                        bucket.append(payload)
                        if len(bucket) > 64:
                            del bucket[:-64]
                    peer = None
            if peer is None:
                continue
            try:
                await peer.send_text(payload)
            except Exception:
                pass
    except WebSocketDisconnect:
        pass
    finally:
        with room.lock:
            if role_norm == "viewer" and room.viewer is websocket:
                room.viewer = None
            if role_norm == "agent" and room.agent is websocket:
                room.agent = None
            peer = room.agent if role_norm == "viewer" else room.viewer
        if peer is not None:
            try:
                await peer.send_text(
                    json.dumps({"type": "hangup", "reason": "peer_disconnect"}, ensure_ascii=False)
                )
            except Exception:
                pass
        # viewer 断开则结束会话并通知 agent 退出
        if role_norm == "viewer":
            still = rd.get_session(sessionId)
            if still and still.status != "stopped":
                rd.stop_session(sessionId, str(claims.get("sub") or "viewer"))
                try:
                    device_runtime.enqueue_command(
                        still.device_id,
                        "stop_remote_desktop",
                        {"sessionId": sessionId},
                    )
                except ValueError:
                    pass
