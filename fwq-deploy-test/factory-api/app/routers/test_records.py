"""测试过站数据上报与查询。"""

from datetime import datetime, timedelta
from typing import Annotated

from fastapi import APIRouter, Depends, Query
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import get_current_user, get_optional_user
from app.factory_scope import apply_factory_name_filter, assert_factory_access
from app.models import LogArchive, LogFile, TestRecord, TestRecordItem, User
from app.response import fail, ok
from app.schemas import (
    LogArchiveSummary,
    LogFileItem,
    TestRecordDetailData,
    TestRecordItemOut,
    TestRecordListData,
    TestRecordListItem,
    TestRecordUploadData,
    TestRecordUploadIn,
)
from app.seed import get_factory_display_name
from app.time_util import utc_now_naive

router = APIRouter(prefix="/test-records", tags=["test-records"])


def _parse_tested_at(raw: str | None) -> datetime | None:
    if not raw or not raw.strip():
        return None
    text = raw.strip().replace("Z", "+00:00")
    try:
        dt = datetime.fromisoformat(text)
        return dt.replace(tzinfo=None) if dt.tzinfo else dt
    except ValueError:
        return None


def _to_list_item(db: Session, row: TestRecord) -> TestRecordListItem:
    return TestRecordListItem(
        id=row.id,
        factoryName=row.factory_name,
        factoryDisplayName=get_factory_display_name(db, row.factory_name),
        deviceId=row.device_id,
        hostName=row.host_name,
        station=row.station,
        stationKey=row.station_key,
        sn=row.sn,
        mac=row.mac,
        testResult=row.test_result,
        machineNo=row.machine_no,
        product=row.product,
        clientVersion=row.client_version,
        itemCount=row.item_count,
        testedAt=row.tested_at,
        createdAt=row.created_at,
    )


def _log_files_for_archive(db: Session, archive: LogArchive) -> list[LogFileItem]:
    rows = db.query(LogFile).filter(LogFile.archive_id == archive.id).all()
    return [
        LogFileItem(
            relativePath=f.relative_path,
            size=f.size,
            contentType=f.content_type,
            previewable=bool(f.preview_path),
        )
        for f in rows
    ]


def _to_log_archive_summary(db: Session, archive: LogArchive) -> LogArchiveSummary:
    return LogArchiveSummary(
        id=archive.id,
        createdAt=archive.created_at,
        fileCount=archive.file_count,
        size=archive.size,
        files=_log_files_for_archive(db, archive),
    )


def _resolve_log_archive_for_record(db: Session, row: TestRecord) -> LogArchive | None:
    linked = db.query(LogArchive).filter(LogArchive.test_record_id == row.id).first()
    if linked:
        return linked

    anchor = row.tested_at or row.created_at
    if not anchor:
        return None

    window_start = anchor - timedelta(minutes=10)
    window_end = anchor + timedelta(minutes=10)
    q = (
        db.query(LogArchive)
        .filter(
            LogArchive.test_record_id.is_(None),
            LogArchive.factory_name == row.factory_name,
            LogArchive.device_id == row.device_id,
            LogArchive.station == row.station,
            LogArchive.created_at >= window_start,
            LogArchive.created_at <= window_end,
        )
    )
    if row.sn:
        q = q.filter(LogArchive.sn == row.sn)
    candidates = q.all()
    if not candidates:
        return None
    return min(candidates, key=lambda a: abs((a.created_at - anchor).total_seconds()))


@router.post("")
def upload_test_record(
    body: TestRecordUploadIn,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User | None, Depends(get_optional_user)],
):
    factory_name = body.factoryName.strip()
    device_id = body.deviceId.strip()
    station = body.station.strip()
    if not factory_name:
        fail(400, "factoryName 不能为空", 400)
    if not device_id:
        fail(400, "deviceId 不能为空", 400)
    if not station:
        fail(400, "station 不能为空", 400)

    tested_at = _parse_tested_at(body.testedAt) or utc_now_naive()
    row = TestRecord(
        factory_name=factory_name,
        device_id=device_id,
        host_name=(body.hostName or device_id).strip() or device_id,
        station=station,
        station_key=(body.stationKey or "").strip() or None,
        sn=(body.sn or "").strip() or None,
        mac=(body.mac or "").strip() or None,
        test_result=(body.testResult or "").strip() or None,
        machine_no=(body.machineNo or "").strip() or None,
        product=(body.product or "").strip() or None,
        lot_name=(body.lotName or "").strip() or None,
        user_no=(body.userNo or "").strip() or None,
        client_version=(body.clientVersion or "").strip() or None,
        tested_at=tested_at,
        item_count=len(body.items),
        created_at=utc_now_naive(),
    )
    db.add(row)
    db.flush()

    for item in body.items:
        name = item.name.strip()
        if not name:
            continue
        db.add(
            TestRecordItem(
                record_id=row.id,
                name=name,
                value=item.value,
                max_value=item.maxValue,
                min_value=item.minValue,
                standard_value=item.standardValue,
                unit=item.unit,
                result=item.result,
            )
        )

    db.commit()
    db.refresh(row)
    return ok(TestRecordUploadData(recordId=row.id).model_dump(mode="json"), message="上报成功")


@router.get("")
def list_test_records(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    factoryName: str | None = None,
    station: str | None = None,
    product: str | None = None,
    hostName: str | None = None,
    sn: str | None = None,
    mac: str | None = None,
    testResult: str | None = None,
    startTime: str | None = None,
    endTime: str | None = None,
    page: int = Query(1, ge=1),
    pageSize: int = Query(20, ge=1, le=100),
):
    q = db.query(TestRecord)
    q = apply_factory_name_filter(q, TestRecord.factory_name, db, user, factoryName)
    if station:
        q = q.filter(TestRecord.station.contains(station))
    if product:
        q = q.filter(TestRecord.product == product)
    if hostName:
        q = q.filter(TestRecord.host_name.contains(hostName))
    if sn:
        q = q.filter(TestRecord.sn.contains(sn))
    if mac:
        q = q.filter(TestRecord.mac.contains(mac))
    if testResult:
        q = q.filter(TestRecord.test_result == testResult)
    # 与列表「测试时间」列一致：优先 tested_at，否则 created_at
    display_time = func.coalesce(TestRecord.tested_at, TestRecord.created_at)
    if startTime:
        q = q.filter(display_time >= datetime.fromisoformat(startTime))
    if endTime:
        q = q.filter(display_time <= datetime.fromisoformat(endTime))

    total = q.with_entities(func.count(TestRecord.id)).scalar() or 0
    rows = (
        q.order_by(display_time.desc(), TestRecord.id.desc())
        .offset((page - 1) * pageSize)
        .limit(pageSize)
        .all()
    )
    items = [_to_list_item(db, r) for r in rows]
    data = TestRecordListData(items=items, total=total, page=page, pageSize=pageSize)
    return ok(data.model_dump(mode="json"))


def _normalize_test_item_fields(value: str | None, result: str | None) -> tuple[str | None, str | None]:
    """兼容上位机旧版将实测值写入 result（FAIL;4120）的写法。"""
    val = (value or "").strip() or None
    res = (result or "").strip() or None
    if not val and res and res.upper().startswith("FAIL;"):
        val = res[5:].strip() or None
        res = "FAIL"
    return val, res


def _to_test_item_out(item: TestRecordItem) -> TestRecordItemOut:
    value, result = _normalize_test_item_fields(item.value, item.result)
    return TestRecordItemOut(
        name=item.name,
        value=value,
        maxValue=item.max_value,
        minValue=item.min_value,
        standardValue=item.standard_value,
        unit=item.unit,
        result=result,
    )


@router.get("/{record_id}")
def test_record_detail(
    record_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    row = db.get(TestRecord, record_id)
    if not row:
        fail(404, "记录不存在", 404)
    assert_factory_access(db, user, row.factory_name)
    items = [_to_test_item_out(i) for i in row.items]
    base = _to_list_item(db, row).model_dump(mode="json")
    archive = _resolve_log_archive_for_record(db, row)
    if archive:
        assert_factory_access(db, user, archive.factory_name)
    log_archive = _to_log_archive_summary(db, archive) if archive else None
    data = TestRecordDetailData(**base, lotName=row.lot_name, userNo=row.user_no, items=items, logArchive=log_archive)
    return ok(data.model_dump(mode="json"))
