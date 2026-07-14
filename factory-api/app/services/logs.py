"""日志存储与解压。"""

import io
import zipfile
from pathlib import Path, PurePosixPath

from sqlalchemy.orm import Session

from app.config import settings
from app.models import LogArchive, LogFile
from app.time_util import utc_now_naive


TEXT_EXTENSIONS = {".txt", ".log", ".ini", ".csv", ".md", ".json"}


def _safe_relative_path(name: str) -> str | None:
    # zip 内路径统一为 posix，拒绝路径穿越
    path = PurePosixPath(name.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts:
        return None
    return path.as_posix()


def save_and_index_log(
    db: Session,
    *,
    factory_name: str,
    device_id: str,
    station: str,
    zip_bytes: bytes,
    host_name: str | None = None,
    sn: str | None = None,
    mac: str | None = None,
    test_result: str | None = None,
    client_version: str | None = None,
    test_record_id: int | None = None,
) -> LogArchive:
    now = utc_now_naive()
    archive = LogArchive(
        factory_name=factory_name,
        device_id=device_id,
        host_name=host_name or device_id,
        station=station,
        sn=sn,
        mac=mac,
        test_result=test_result,
        client_version=client_version,
        object_key="",
        size=len(zip_bytes),
        file_count=0,
        test_record_id=test_record_id if test_record_id and test_record_id > 0 else None,
        created_at=now,
    )
    db.add(archive)
    db.flush()

    rel_dir = Path("logs") / factory_name / f"{now:%Y}" / f"{now:%m}" / device_id / str(archive.id)
    abs_dir = settings.storage_path / rel_dir
    abs_dir.mkdir(parents=True, exist_ok=True)

    zip_path = abs_dir / "archive.zip"
    zip_path.write_bytes(zip_bytes)
    archive.object_key = str(rel_dir / "archive.zip").replace("\\", "/")

    extract_dir = abs_dir / "extracted"
    extract_dir.mkdir(parents=True, exist_ok=True)
    file_rows: list[LogFile] = []

    with zipfile.ZipFile(io.BytesIO(zip_bytes)) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            rel = _safe_relative_path(info.filename)
            if not rel:
                continue
            data = zf.read(info)
            suffix = PurePosixPath(rel).suffix.lower()
            content_type = "text/plain; charset=utf-8" if suffix in TEXT_EXTENSIONS else "application/octet-stream"
            preview_path = None
            if suffix in TEXT_EXTENSIONS:
                safe_name = rel.replace("/", "_")
                out_file = extract_dir / safe_name
                out_file.parent.mkdir(parents=True, exist_ok=True)
                out_file.write_bytes(data)
                preview_path = f"{rel_dir.as_posix()}/extracted/{safe_name}"
            file_rows.append(
                LogFile(
                    archive_id=archive.id,
                    relative_path=rel,
                    size=len(data),
                    content_type=content_type,
                    preview_path=preview_path,
                )
            )

    archive.file_count = len(file_rows)
    db.add_all(file_rows)
    db.commit()
    db.refresh(archive)
    return archive


def read_zip_bytes(archive: LogArchive) -> bytes:
    path = settings.storage_path / archive.object_key
    return path.read_bytes()


def read_preview_bytes(log_file: LogFile) -> bytes | None:
    if not log_file.preview_path:
        return None
    path = settings.storage_path / log_file.preview_path
    if not path.exists():
        return None
    return path.read_bytes()
