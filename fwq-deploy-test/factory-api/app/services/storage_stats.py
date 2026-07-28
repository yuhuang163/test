"""存储占用统计：盘符容量、日志目录、测试数据库等。"""

from __future__ import annotations

import shutil
from pathlib import Path

from sqlalchemy import func, or_
from sqlalchemy.orm import Session

from app.config import settings
from app.database import BASE_DIR
from app.models import LogArchive, LogFile, TestRecord, TestRecordItem
from app.time_util import to_utc_iso_z


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


def _host_key_expr(model):
    """电脑展示键：优先 host_name，否则 device_id。"""
    return func.coalesce(func.nullif(model.host_name, ""), model.device_id)


def list_test_data_hosts(db: Session) -> list[dict]:
    """按电脑汇总测试记录数量，便于批量清理。"""
    host_key = _host_key_expr(TestRecord).label("hostKey")
    rows = (
        db.query(
            host_key,
            func.count(TestRecord.id).label("recordCount"),
            func.min(TestRecord.created_at).label("firstAt"),
            func.max(TestRecord.created_at).label("lastAt"),
        )
        .group_by(host_key)
        .order_by(func.count(TestRecord.id).desc())
        .all()
    )
    items = []
    for r in rows:
        key = (r.hostKey or "").strip()
        if not key:
            continue
        log_count = (
            db.query(func.count(LogArchive.id))
            .filter(
                or_(
                    LogArchive.host_name == key,
                    LogArchive.device_id == key,
                )
            )
            .scalar()
            or 0
        )
        items.append(
            {
                "hostName": key,
                "recordCount": int(r.recordCount or 0),
                "logCount": int(log_count),
                "firstAt": to_utc_iso_z(r.firstAt),
                "lastAt": to_utc_iso_z(r.lastAt),
            }
        )
    return items


def _delete_log_archive_files(archive: LogArchive) -> None:
    if not archive.object_key:
        return
    zip_path = settings.storage_path / archive.object_key
    abs_dir = zip_path.parent
    try:
        if abs_dir.exists() and abs_dir.is_dir():
            shutil.rmtree(abs_dir, ignore_errors=True)
    except OSError:
        pass


def delete_test_data_by_host(db: Session, host_name: str, *, delete_logs: bool = False) -> dict:
    """删除某电脑的全部测试记录（及分项）；可选同时删除该电脑日志包。"""
    key = (host_name or "").strip()
    if not key:
        raise ValueError("电脑名不能为空")

    record_ids = [
        int(r[0])
        for r in db.query(TestRecord.id)
        .filter(
            or_(
                TestRecord.host_name == key,
                TestRecord.device_id == key,
            )
        )
        .all()
    ]
    deleted_records = 0
    deleted_items = 0
    if record_ids:
        # 解除日志与测试记录的关联，避免悬空引用
        db.query(LogArchive).filter(LogArchive.test_record_id.in_(record_ids)).update(
            {LogArchive.test_record_id: None},
            synchronize_session=False,
        )
        deleted_items = (
            db.query(TestRecordItem)
            .filter(TestRecordItem.record_id.in_(record_ids))
            .delete(synchronize_session=False)
        )
        deleted_records = (
            db.query(TestRecord)
            .filter(TestRecord.id.in_(record_ids))
            .delete(synchronize_session=False)
        )

    deleted_logs = 0
    freed_log_bytes = 0
    if delete_logs:
        archives = (
            db.query(LogArchive)
            .filter(
                or_(
                    LogArchive.host_name == key,
                    LogArchive.device_id == key,
                )
            )
            .all()
        )
        for archive in archives:
            freed_log_bytes += int(archive.size or 0)
            _delete_log_archive_files(archive)
            db.query(LogFile).filter(LogFile.archive_id == archive.id).delete(synchronize_session=False)
            db.delete(archive)
            deleted_logs += 1

    db.commit()
    return {
        "hostName": key,
        "deletedRecords": int(deleted_records or 0),
        "deletedItems": int(deleted_items or 0),
        "deletedLogs": deleted_logs,
        "freedLogBytes": freed_log_bytes,
    }
