"""云端配置版本号：yyyyMMdd-NNN。"""

from datetime import datetime


def next_version(current: str | None) -> str:
    today = datetime.now().strftime("%Y%m%d")
    if current:
        parts = current.split("-", 1)
        if len(parts) == 2 and parts[0] == today and parts[1].isdigit():
            return f"{today}-{int(parts[1]) + 1:03d}"
    return f"{today}-001"
