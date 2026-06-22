"""上位机 OTA 安装包存储。"""

from pathlib import Path

from app.config import settings


def _root() -> Path:
    path = settings.storage_path / "host_app"
    path.mkdir(parents=True, exist_ok=True)
    return path


def storage_root() -> Path:
    return _root()


def save_build(build_id: str, content: bytes, package_name: str = "new_production") -> Path:
    safe_id = build_id.strip()
    if not safe_id:
        raise ValueError("buildId 不能为空")
    path = _root() / f"{package_name}_{safe_id}.exe"
    path.write_bytes(content)
    return path


def resolve_build_path(build_id: str, package_name: str | None = None) -> Path | None:
    root = _root()
    candidates: list[Path] = []
    if package_name:
        candidates.append(root / f"{package_name}_{build_id}.exe")
    candidates.append(root / f"new_production_{build_id}.exe")
    candidates.append(root / f"{build_id}.exe")
    for path in candidates:
        if path.is_file():
            return path
    return None
