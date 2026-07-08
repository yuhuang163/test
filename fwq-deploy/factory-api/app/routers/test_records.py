"""测试过站数据上报与查询。"""

from datetime import datetime
from typing import Annotated

from fastapi import APIRouter, Depends, Query
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import get_current_user, get_optional_user
from app.models import TestRecord, TestRecordItem, User
from app.response import fail, ok
from app.schemas import (
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
        testResult=row.test_result,
        machineNo=row.machine_no,
        product=row.product,
        clientVersion=row.client_version,
        itemCount=row.item_count,
        testedAt=row.tested_at,
        createdAt=row.created_at,
    )


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
    hostName: str | None = None,
    sn: str | None = None,
    testResult: str | None = None,
    startTime: str | None = None,
    endTime: str | None = None,
    page: int = Query(1, ge=1),
    pageSize: int = Query(20, ge=1, le=100),
):
    q = db.query(TestRecord)
    if factoryName:
        q = q.filter(TestRecord.factory_name == factoryName)
    if station:
        q = q.filter(TestRecord.station == station)
    if hostName:
        q = q.filter(TestRecord.host_name.contains(hostName))
    if sn:
        q = q.filter(TestRecord.sn.contains(sn))
    if testResult:
        q = q.filter(TestRecord.test_result == testResult)
    if startTime:
        q = q.filter(TestRecord.created_at >= datetime.fromisoformat(startTime))
    if endTime:
        q = q.filter(TestRecord.created_at <= datetime.fromisoformat(endTime))

    total = q.with_entities(func.count(TestRecord.id)).scalar() or 0
    rows = q.order_by(TestRecord.created_at.desc()).offset((page - 1) * pageSize).limit(pageSize).all()
    items = [_to_list_item(db, r) for r in rows]
    data = TestRecordListData(items=items, total=total, page=page, pageSize=pageSize)
    return ok(data.model_dump(mode="json"))


@router.get("/{record_id}")
def test_record_detail(
    record_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    row = db.get(TestRecord, record_id)
    if not row:
        fail(404, "记录不存在", 404)
    items = [
        TestRecordItemOut(
            name=i.name,
            value=i.value,
            maxValue=i.max_value,
            minValue=i.min_value,
            standardValue=i.standard_value,
            unit=i.unit,
            result=i.result,
        )
        for i in row.items
    ]
    base = _to_list_item(db, row).model_dump(mode="json")
    data = TestRecordDetailData(**base, lotName=row.lot_name, userNo=row.user_no, items=items)
    return ok(data.model_dump(mode="json"))
