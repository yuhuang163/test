"""日志路由。"""

from datetime import datetime
from typing import Annotated
from urllib.parse import quote

from fastapi import APIRouter, Depends, File, Form, Query, UploadFile
from fastapi.responses import FileResponse, PlainTextResponse, Response
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.config import settings
from app.database import get_db
from app.deps import get_current_user, get_optional_user
from app.factory_scope import apply_factory_name_filter, assert_factory_access
from app.models import LogArchive, LogFile, User
from app.response import fail, ok
from app.schemas import LogDetailData, LogFileItem, LogListData, LogListItem, LogUploadData
from app.seed import get_factory_display_name
from app.services.logs import read_preview_bytes, read_zip_bytes, save_and_index_log

router = APIRouter(prefix="/logs", tags=["logs"])


def _to_list_item(db: Session, row: LogArchive) -> LogListItem:
    return LogListItem(
        id=row.id,
        factoryName=row.factory_name,
        factoryDisplayName=get_factory_display_name(db, row.factory_name),
        deviceId=row.device_id,
        hostName=row.host_name,
        station=row.station,
        sn=row.sn,
        mac=row.mac,
        testResult=row.test_result,
        clientVersion=row.client_version,
        size=row.size,
        fileCount=row.file_count,
        createdAt=row.created_at,
    )


@router.post("/upload")
async def upload_log(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User | None, Depends(get_optional_user)],
    factoryName: Annotated[str, Form()],
    deviceId: Annotated[str, Form()],
    station: Annotated[str, Form()],
    file: Annotated[UploadFile, File()],
    hostName: Annotated[str | None, Form()] = None,
    sn: Annotated[str | None, Form()] = None,
    mac: Annotated[str | None, Form()] = None,
    testResult: Annotated[str | None, Form()] = None,
    clientVersion: Annotated[str | None, Form()] = None,
    testRecordId: Annotated[int | None, Form()] = None,
):
    if not settings.log_upload_allow_anonymous and not user:
        fail(401, "请先登录", 401)

    factory_name = factoryName.strip()
    device_id = deviceId.strip()
    station_name = station.strip()
    if not factory_name:
        fail(400, "factoryName 不能为空", 400)
    if not device_id:
        fail(400, "deviceId 不能为空", 400)
    if not station_name:
        fail(400, "station 不能为空", 400)

    content = await file.read()
    max_bytes = settings.log_upload_max_mb * 1024 * 1024
    if len(content) > max_bytes:
        fail(400, f"文件超过 {settings.log_upload_max_mb}MB 限制", 400)
    if content[:2] != b"PK":
        fail(400, "请上传 zip 文件", 400)

    archive = save_and_index_log(
        db,
        factory_name=factory_name,
        device_id=device_id,
        station=station_name,
        zip_bytes=content,
        host_name=(hostName or device_id).strip() or device_id,
        sn=sn,
        mac=mac,
        test_result=testResult,
        client_version=clientVersion,
        test_record_id=testRecordId,
    )
    return ok(LogUploadData(logId=archive.id).model_dump(mode="json"), message="上传成功")


@router.get("")
def list_logs(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    factoryName: str | None = None,
    station: str | None = None,
    deviceId: str | None = None,
    hostName: str | None = None,
    sn: str | None = None,
    mac: str | None = None,
    testResult: str | None = None,
    startTime: str | None = None,
    endTime: str | None = None,
    page: int = Query(1, ge=1),
    pageSize: int = Query(20, ge=1, le=100),
):
    q = db.query(LogArchive)
    q = apply_factory_name_filter(q, LogArchive.factory_name, db, user, factoryName)
    if station:
        q = q.filter(LogArchive.station.contains(station))
    if deviceId:
        q = q.filter(LogArchive.device_id.contains(deviceId))
    if hostName:
        q = q.filter(LogArchive.host_name.contains(hostName))
    if sn:
        q = q.filter(LogArchive.sn.contains(sn))
    if mac:
        q = q.filter(LogArchive.mac.contains(mac))
    if testResult:
        q = q.filter(LogArchive.test_result == testResult)
    if startTime:
        q = q.filter(LogArchive.created_at >= datetime.fromisoformat(startTime))
    if endTime:
        q = q.filter(LogArchive.created_at <= datetime.fromisoformat(endTime))

    total = q.with_entities(func.count(LogArchive.id)).scalar() or 0
    rows = q.order_by(LogArchive.created_at.desc()).offset((page - 1) * pageSize).limit(pageSize).all()
    items = [_to_list_item(db, r) for r in rows]
    data = LogListData(items=items, total=total, page=page, pageSize=pageSize)
    return ok(data.model_dump(mode="json"))


@router.get("/{log_id}")
def log_detail(
    log_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    row = db.get(LogArchive, log_id)
    if not row:
        fail(404, "日志不存在", 404)
    assert_factory_access(db, user, row.factory_name)
    files = [
        LogFileItem(
            relativePath=f.relative_path,
            size=f.size,
            contentType=f.content_type,
            previewable=bool(f.preview_path),
        )
        for f in row.files
    ]
    base = _to_list_item(db, row).model_dump(mode="json")
    data = LogDetailData(**base, files=files)
    return ok(data.model_dump(mode="json"))


@router.get("/{log_id}/files/{file_path:path}")
def log_file_preview(
    log_id: int,
    file_path: str,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    offset: int = Query(0, ge=0),
    limit: int = Query(200000, ge=1, le=500000),
):
    row = db.get(LogArchive, log_id)
    if not row:
        fail(404, "日志不存在", 404)
    assert_factory_access(db, user, row.factory_name)
    target = db.query(LogFile).filter(LogFile.archive_id == log_id, LogFile.relative_path == file_path).first()
    if not target:
        fail(404, "文件不存在", 404)
    raw = read_preview_bytes(target)
    if raw is None:
        fail(400, "该文件不支持在线预览", 400)
    text = raw.decode("utf-8", errors="replace")
    if offset or len(text) > limit:
        text = text[offset : offset + limit]
    return PlainTextResponse(text, media_type="text/plain; charset=utf-8")


@router.get("/{log_id}/download")
def log_download(
    log_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    row = db.get(LogArchive, log_id)
    if not row:
        fail(404, "日志不存在", 404)
    assert_factory_access(db, user, row.factory_name)
    zip_bytes = read_zip_bytes(row)
    filename = f"log_{row.factory_name}_{row.device_id}_{row.id}.zip"
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="{quote(filename)}"'},
    )
