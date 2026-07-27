"""WebRTC 远控：会话、TURN 临时凭证、信令房间。"""

from __future__ import annotations

import hashlib
import hmac
import threading
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from typing import Any

from jose import JWTError, jwt

from app.config import settings
from app.security import ALGORITHM

SESSION_TTL_SEC_DEFAULT = 1800
SIGNAL_TOKEN_TTL_SEC = 1800


def _utc_now() -> datetime:
    return datetime.now(timezone.utc)


def _utc_now_iso() -> str:
    return _utc_now().replace(microsecond=0).isoformat().replace("+00:00", "Z")


def make_turn_credentials(ttl_sec: int | None = None) -> dict[str, Any]:
    """coturn REST API 风格临时用户名/密码；无 TURN_SECRET 时只返回 urls（可为空）。"""
    urls = settings.turn_url_list
    ttl = int(ttl_sec or settings.turn_ttl_sec or 600)
    if not urls or not settings.turn_secret:
        ice: list[dict[str, Any]] = []
        # 本机调试可用公共 STUN（无中继）
        ice.append({"urls": ["stun:stun.l.google.com:19302"]})
        return {"iceServers": ice, "ttlSec": ttl, "turnConfigured": False}

    expiry = int(time.time()) + ttl
    username = f"{expiry}:lute"
    credential = hmac.new(
        settings.turn_secret.encode("utf-8"),
        username.encode("utf-8"),
        hashlib.sha1,
    ).digest()
    # coturn 期望 base64
    import base64

    password = base64.b64encode(credential).decode("ascii")
    ice_servers: list[dict[str, Any]] = [{"urls": ["stun:stun.l.google.com:19302"]}]
    ice_servers.append(
        {
            "urls": urls,
            "username": username,
            "credential": password,
        }
    )
    return {
        "iceServers": ice_servers,
        "ttlSec": ttl,
        "turnConfigured": True,
        "realm": settings.turn_realm,
    }


def create_signaling_token(
    *,
    session_id: str,
    role: str,
    device_id: str,
    username: str | None = None,
) -> str:
    expire = _utc_now() + timedelta(seconds=SIGNAL_TOKEN_TTL_SEC)
    payload = {
        "sub": username or f"agent:{device_id}",
        "exp": expire,
        "typ": "remote_desktop",
        "sid": session_id,
        "role": role,
        "deviceId": device_id,
    }
    return jwt.encode(payload, settings.secret_key, algorithm=ALGORITHM)


def decode_signaling_token(token: str) -> dict[str, Any]:
    payload = jwt.decode(token, settings.secret_key, algorithms=[ALGORITHM])
    if payload.get("typ") != "remote_desktop":
        raise JWTError("invalid token type")
    return payload


@dataclass
class RemoteSession:
    session_id: str
    device_id: str
    host_name: str
    created_by: str
    created_at: str
    expire_ts: float
    viewer_token: str
    agent_token: str
    status: str = "pending"  # pending | active | stopped
    stopped_at: str | None = None
    stopped_by: str | None = None


@dataclass
class SignalingRoom:
    session_id: str
    viewer: Any | None = None  # WebSocket
    agent: Any | None = None
    lock: threading.Lock = field(default_factory=threading.Lock)
    # 对端未连时缓存信令，避免 offer/answer 丢失导致一直等推流
    pending_to_viewer: list[str] = field(default_factory=list)
    pending_to_agent: list[str] = field(default_factory=list)


_sessions: dict[str, RemoteSession] = {}
_device_session: dict[str, str] = {}  # deviceId -> sessionId
_rooms: dict[str, SignalingRoom] = {}
_guard = threading.Lock()


def _session_ttl() -> int:
    return int(settings.remote_desktop_session_ttl_sec or SESSION_TTL_SEC_DEFAULT)


def get_session(session_id: str) -> RemoteSession | None:
    with _guard:
        s = _sessions.get(session_id)
        if not s:
            return None
        if s.status != "stopped" and s.expire_ts < time.time():
            s.status = "stopped"
            s.stopped_at = _utc_now_iso()
            s.stopped_by = "expire"
            _device_session.pop(s.device_id, None)
        return s


def find_active_session_for_device(device_id: str) -> RemoteSession | None:
    with _guard:
        sid = _device_session.get(device_id)
        if not sid:
            return None
    return get_session(sid)


def viewer_connected(session_id: str) -> bool:
    """浏览器刷新后 viewer WS 会断；用于判断会话是否仍被占用。"""
    room = get_room(session_id)
    if not room:
        return False
    with room.lock:
        return room.viewer is not None


def create_session(
    *,
    device_id: str,
    host_name: str,
    created_by: str,
    force: bool = False,
) -> tuple[RemoteSession, RemoteSession | None]:
    """创建会话。返回 (新会话, 被顶替的旧会话或 None)。

    刷新网页后前端丢了 sessionId，但服务端仍占着设备锁。
    若旧会话已无 viewer 在线（典型刷新残留），自动顶替；viewer 仍在线时需 force。
    """
    did = (device_id or "").strip()
    if not did:
        raise ValueError("deviceId 不能为空")
    existing = find_active_session_for_device(did)
    replaced: RemoteSession | None = None
    if existing and existing.status != "stopped":
        stale = not viewer_connected(existing.session_id)
        if not force and not stale:
            raise ValueError("该设备已有进行中的远控会话，请先断开")
        replaced = stop_session(existing.session_id, "replaced" if force or stale else created_by)

    session_id = str(uuid.uuid4())
    viewer_token = create_signaling_token(
        session_id=session_id, role="viewer", device_id=did, username=created_by
    )
    agent_token = create_signaling_token(
        session_id=session_id, role="agent", device_id=did, username=None
    )
    sess = RemoteSession(
        session_id=session_id,
        device_id=did,
        host_name=(host_name or did).strip() or did,
        created_by=created_by,
        created_at=_utc_now_iso(),
        expire_ts=time.time() + _session_ttl(),
        viewer_token=viewer_token,
        agent_token=agent_token,
        status="pending",
    )
    with _guard:
        _sessions[session_id] = sess
        _device_session[did] = session_id
        _rooms[session_id] = SignalingRoom(session_id=session_id)
    return sess, replaced


def stop_session(session_id: str, stopped_by: str) -> RemoteSession | None:
    sess = get_session(session_id)
    if not sess:
        return None
    with _guard:
        sess.status = "stopped"
        sess.stopped_at = _utc_now_iso()
        sess.stopped_by = stopped_by
        _device_session.pop(sess.device_id, None)
    return sess


def stop_active_session_for_device(device_id: str, stopped_by: str) -> RemoteSession | None:
    """按设备断开（刷新后前端不知道 sessionId 时用）。"""
    existing = find_active_session_for_device(device_id)
    if not existing or existing.status == "stopped":
        return None
    return stop_session(existing.session_id, stopped_by)


def mark_session_active(session_id: str) -> None:
    sess = get_session(session_id)
    if sess and sess.status == "pending":
        sess.status = "active"


def get_room(session_id: str) -> SignalingRoom | None:
    with _guard:
        return _rooms.get(session_id)


def session_public(sess: RemoteSession, *, include_agent_token: bool = False) -> dict[str, Any]:
    ice = make_turn_credentials()
    data: dict[str, Any] = {
        "sessionId": sess.session_id,
        "deviceId": sess.device_id,
        "hostName": sess.host_name,
        "status": sess.status,
        "createdBy": sess.created_by,
        "createdAt": sess.created_at,
        "expireAt": datetime.fromtimestamp(sess.expire_ts, tz=timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z"),
        "viewerToken": sess.viewer_token,
        "iceServers": ice.get("iceServers") or [],
        "turnConfigured": bool(ice.get("turnConfigured")),
        "signalingPath": f"/api/factory-tool/remote-desktop/ws",
    }
    if include_agent_token:
        data["agentToken"] = sess.agent_token
    return data
