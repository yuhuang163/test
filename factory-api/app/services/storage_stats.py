"""存储占用统计：盘符容量、日志目录、测试数据库等。"""

from __future__ import annotations

import shutil
from pathlib import Path

from app.config import settings
from app.database import BASE_DIR


def _safe_dir_size(path: Path) -> int:
    if not path.exists():
        return 0
    total = 0
    try:
        for p in path.rglob("*"):
            try:
                if p.is_file():
                    total += p.stat().st_size
            except OSError:
                continue
    except OSError:
        return total
    return total


def _file_size(path: Path) -> int:
    try:
        if path.is_file():
            return path.stat().st_size
    except OSError:
        pass
    return 0


def resolve_sqlite_db_path() -> Path | None:
    url = settings.database_url
    if not url.startswith("sqlite:///"):
        return None
    rel = url.replace("sqlite:///", "", 1)
    if rel.startswith("./"):
        return (BASE_DIR / rel[2:]).resolve()
    return Path(rel).resolve()


def _disk_info(path: Path) -> dict:
    path = path.resolve()
    usage = shutil.disk_usage(path)
    total = usage.total
    free = usage.free
    used = total - free
    used_percent = round(used * 100.0 / total, 2) if total else 0.0
    # Windows: D:\ ；POSIX: /
    anchor = path.anchor or str(path)
    return {
        "mount": anchor.rstrip("\\/") + ("\\" if "\\" in anchor else "/"),
        "path": str(path),
        "totalBytes": total,
        "usedBytes": used,
        "freeBytes": free,
        "usedPercent": used_percent,
    }


def _build_alert(disk: dict, warn_pct: float, crit_pct: float) -> dict:
    pct = float(disk.get("usedPercent") or 0)
    free = int(disk.get("freeBytes") or 0)
    mount = disk.get("mount") or ""
    if pct >= crit_pct:
        return {
            "level": "critical",
            "message": f"{mount} 磁盘已用 {pct:.1f}%，剩余 {_fmt_bytes(free)}，空间即将耗尽，请尽快清理日志或扩容。",
        }
    if pct >= warn_pct:
        return {
            "level": "warn",
            "message": f"{mount} 磁盘已用 {pct:.1f}%，剩余 {_fmt_bytes(free)}，建议及时清理旧日志/数据。",
        }
    return {
        "level": "ok",
        "message": f"{mount} 磁盘空间充足（已用 {pct:.1f}%，剩余 {_fmt_bytes(free)}）。",
    }


def _fmt_bytes(n: int) -> str:
    n = max(0, int(n))
    units = ["B", "KB", "MB", "GB", "TB"]
    v = float(n)
    for u in units:
        if v < 1024 or u == units[-1]:
            if u == "B":
                return f"{int(v)} {u}"
            return f"{v:.2f} {u}"
        v /= 1024.0
    return f"{n} B"


def collect_storage_info() -> dict:
    storage = settings.storage_path.resolve()
    logs_dir = storage / "logs"
    test_cases_dir = storage / "test_cases"
    host_app_dir = storage / "host_app"

    logs_size = _safe_dir_size(logs_dir)
    test_cases_size = _safe_dir_size(test_cases_dir)
    host_app_size = _safe_dir_size(host_app_dir)
    storage_total = _safe_dir_size(storage)
    known = logs_size + test_cases_size + host_app_size
    other_size = max(0, storage_total - known)

    db_path = resolve_sqlite_db_path()
    db_size = _file_size(db_path) if db_path else 0

    disk = _disk_info(storage)
    db_disk = None
    if db_path is not None:
        try:
            db_disk = _disk_info(db_path.parent)
            if db_disk["mount"].upper() == disk["mount"].upper():
                db_disk = None  # 同盘不重复展示
        except OSError:
            db_disk = None

    warn_pct = float(settings.storage_warn_percent)
    crit_pct = float(settings.storage_critical_percent)
    alert = _build_alert(disk, warn_pct, crit_pct)

    return {
        "disk": disk,
        "databaseDisk": db_disk,
        "storageRoot": {
            "path": str(storage),
            "sizeBytes": storage_total,
        },
        "database": {
            "engine": "sqlite" if db_path else "other",
            "path": str(db_path) if db_path else settings.database_url,
            "sizeBytes": db_size,
        },
        "breakdown": [
            {
                "key": "logs",
                "label": "日志",
                "path": str(logs_dir),
                "sizeBytes": logs_size,
            },
            {
                "key": "testData",
                "label": "测试数据",
                "path": str(db_path) if db_path else settings.database_url,
                "sizeBytes": db_size,
                "hint": "测试记录存于数据库文件",
            },
            {
                "key": "testCases",
                "label": "测试用例",
                "path": str(test_cases_dir),
                "sizeBytes": test_cases_size,
            },
            {
                "key": "hostApp",
                "label": "上位机包",
                "path": str(host_app_dir),
                "sizeBytes": host_app_size,
            },
            {
                "key": "storageOther",
                "label": "存储目录其它",
                "path": str(storage),
                "sizeBytes": other_size,
            },
        ],
        "thresholds": {
            "warnPercent": warn_pct,
            "criticalPercent": crit_pct,
        },
        "alert": alert,
    }
