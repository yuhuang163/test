"""产线设备运行时：心跳在线状态 + 简易命令队列（文件存储）。"""

from __future__ import annotations

import json
import re
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from app.config import settings

ONLINE_TTL_SEC = 180
COMMAND_TTL_SEC = 300


def _runtime_root() -> Path:
    path = settings.storage_path / "device_runtime"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _safe_id(value: str) -> str:
    text = (value or "").strip()
    if not text:
        raise ValueError("deviceId 不能为空")
    # 仅允许路径安全字符，避免目录穿越
    cleaned = re.sub(r"[^\w.\-]+", "_", text, flags=re.UNICODE)
    if not cleaned or cleaned in {".", ".."}:
        raise ValueError("非法 deviceId")
    return cleaned


def _device_dir(device_id: str) -> Path:
    path = _runtime_root() / _safe_id(device_id)
    path.mkdir(parents=True, exist_ok=True)
    return path


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def heartbeat(
    device_id: str,
    *,
    host_name: str | None = None,
    station_key: str | None = None,
    station_name: str | None = None,
    app_version: str | None = None,
    stations: list | None = None,
    remote_desktop: bool | None = None,
) -> dict[str, Any]:
    did = _safe_id(device_id)
    normalized_stations: list[dict[str, str]] = []
    seen: set[str] = set()
    for item in stations or []:
        if not isinstance(item, dict):
            continue
        key = str(item.get("stationKey") or "").strip()
        name = str(item.get("displayName") or item.get("stationName") or "").strip()
        if not key and not name:
            continue
        if not key:
            key = name
        if not name:
            name = key
        # 同名去重，保留先上报的
        dedupe = name.lower()
        if dedupe in seen:
            continue
        seen.add(dedupe)
        normalized_stations.append({"stationKey": key, "displayName": name})

    prev = _load_heartbeat(did) or {}
    rd_flag = bool(remote_desktop) if remote_desktop is not None else bool(prev.get("remoteDesktop"))

    payload = {
        "deviceId": did,
        "hostName": (host_name or did).strip() or did,
        "stationKey": (station_key or "").strip(),
        "stationName": (station_name or "").strip(),
        "appVersion": (app_version or "").strip(),
        "stations": normalized_stations,
        "remoteDesktop": rd_flag,
        "lastSeenAt": _utc_now_iso(),
        "lastSeenTs": time.time(),
    }
    path = _device_dir(did) / "heartbeat.json"
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return payload


def _load_heartbeat(device_id: str) -> dict[str, Any] | None:
    path = _device_dir(device_id) / "heartbeat.json"
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return data if isinstance(data, dict) else None


def is_online(device_id: str, ttl_sec: int = ONLINE_TTL_SEC) -> bool:
    data = _load_heartbeat(device_id)
    if not data:
        return False
    ts = float(data.get("lastSeenTs") or 0)
    return (time.time() - ts) <= ttl_sec


def list_online_devices(ttl_sec: int = ONLINE_TTL_SEC) -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    root = _runtime_root()
    now = time.time()
    for child in sorted(root.iterdir()):
        if not child.is_dir():
            continue
        hb_path = child / "heartbeat.json"
        if not hb_path.is_file():
            continue
        try:
            data = json.loads(hb_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(data, dict):
            continue
        ts = float(data.get("lastSeenTs") or 0)
        age = now - ts
        online = age <= ttl_sec
        # 下拉框只返回在线设备，避免灰色不可选项
        if not online:
            continue
        items.append(
            {
                "deviceId": data.get("deviceId") or child.name,
                "hostName": data.get("hostName") or child.name,
                "stationKey": data.get("stationKey") or "",
                "stationName": data.get("stationName") or "",
                "appVersion": data.get("appVersion") or "",
                "stations": data.get("stations") if isinstance(data.get("stations"), list) else [],
                "remoteDesktop": True if "remoteDesktop" not in data else bool(data.get("remoteDesktop")),
                "lastSeenAt": data.get("lastSeenAt"),
                "online": True,
                "ageSec": int(age) if age >= 0 else None,
            }
        )
    items.sort(key=lambda x: (x.get("hostName") or ""))
    return items


def _commands_path(device_id: str) -> Path:
    return _device_dir(device_id) / "commands.json"


def _load_commands(device_id: str) -> list[dict[str, Any]]:
    path = _commands_path(device_id)
    if not path.is_file():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    return data if isinstance(data, list) else []


def _save_commands(device_id: str, commands: list[dict[str, Any]]) -> None:
    path = _commands_path(device_id)
    path.write_text(json.dumps(commands, ensure_ascii=False, indent=2), encoding="utf-8")


def enqueue_command(
    device_id: str,
    command_type: str,
    payload: dict[str, Any] | None = None,
) -> dict[str, Any]:
    did = _safe_id(device_id)
    cmd_type = (command_type or "").strip()
    if not cmd_type:
        raise ValueError("commandType 不能为空")
    now = time.time()
    cmd = {
        "id": str(uuid.uuid4()),
        "type": cmd_type,
        "payload": payload or {},
        "createdAt": _utc_now_iso(),
        "createdTs": now,
        "expireTs": now + COMMAND_TTL_SEC,
    }
    commands = [c for c in _load_commands(did) if float(c.get("expireTs") or 0) > now]
    commands.append(cmd)
    _save_commands(did, commands)
    return cmd


def poll_commands(device_id: str) -> list[dict[str, Any]]:
    """领取并清空未过期命令（上位机拉取一次即消费）。"""
    did = _safe_id(device_id)
    now = time.time()
    pending = [c for c in _load_commands(did) if float(c.get("expireTs") or 0) > now]
    _save_commands(did, [])
    return pending
