"""时间工具：库内统一 UTC 存储，API 输出带 Z 便于前端转本地。"""

from datetime import datetime, timezone


def utc_now_naive() -> datetime:
    """写入数据库用的 naive UTC（与 SQLite datetime('now') 语义一致）。"""
    return datetime.now(timezone.utc).replace(tzinfo=None)


def to_utc_iso_z(dt: datetime | None) -> str | None:
    """naive 时间按 UTC 序列化为 ISO8601 并带 Z 后缀。"""
    if dt is None:
        return None
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    else:
        dt = dt.astimezone(timezone.utc)
    return dt.strftime("%Y-%m-%dT%H:%M:%S.%f").rstrip("0").rstrip(".") + "Z"
