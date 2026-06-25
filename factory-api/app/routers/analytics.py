"""数据分析接口：数据曲线、良率统计。"""

from datetime import datetime
from typing import Annotated

from fastapi import APIRouter, Depends, Query
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import get_current_user
from app.models import TestRecord, TestRecordItem, User
from app.response import ok
from app.seed import get_factory_display_name
from app.time_util import to_utc_iso_z, utc_now_naive

router = APIRouter(prefix="/analytics", tags=["analytics"])


@router.get("/curve")
def data_curve(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    factoryName: str | None = None,
    station: str | None = None,
    itemName: str | None = None,
    startTime: str | None = None,
    endTime: str | None = None,
    limit: int = Query(200, ge=1, le=1000),
):
    q = db.query(TestRecordItem, TestRecord).join(
        TestRecord, TestRecordItem.record_id == TestRecord.id
    )
    if factoryName:
        q = q.filter(TestRecord.factory_name == factoryName)
    if station:
        q = q.filter(TestRecord.station == station)
    if itemName:
        q = q.filter(TestRecordItem.name.contains(itemName))
    if startTime:
        q = q.filter(TestRecord.created_at >= datetime.fromisoformat(startTime))
    if endTime:
        q = q.filter(TestRecord.created_at <= datetime.fromisoformat(endTime))

    rows = q.order_by(TestRecord.created_at.desc()).limit(limit).all()

    points = []
    for item, record in reversed(rows):
        points.append(
            {
                "recordId": record.id,
                "sn": record.sn,
                "factoryName": record.factory_name,
                "station": record.station,
                "testedAt": to_utc_iso_z(record.tested_at or record.created_at),
                "name": item.name,
                "value": item.value,
                "maxValue": item.max_value,
                "minValue": item.min_value,
                "standardValue": item.standard_value,
                "unit": item.unit,
                "result": item.result,
                "testResult": record.test_result,
            }
        )
    return ok({"points": points, "total": len(points)})


@router.get("/curve/item-names")
def curve_item_names(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    factoryName: str | None = None,
    station: str | None = None,
    keyword: str | None = None,
):
    q = db.query(TestRecordItem.name).distinct().join(
        TestRecord, TestRecordItem.record_id == TestRecord.id
    )
    if factoryName:
        q = q.filter(TestRecord.factory_name == factoryName)
    if station:
        q = q.filter(TestRecord.station == station)
    if keyword:
        q = q.filter(TestRecordItem.name.contains(keyword))
    rows = q.order_by(TestRecordItem.name.asc()).limit(100).all()
    names = [r[0] for r in rows if r[0]]
    return ok({"names": names})


@router.get("/yield")
def yield_stats(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    factoryName: str | None = None,
    station: str | None = None,
    startTime: str | None = None,
    endTime: str | None = None,
    groupBy: str = Query("day", regex="^(day|week|month)$"),
):
    q = db.query(TestRecord)
    if factoryName:
        q = q.filter(TestRecord.factory_name == factoryName)
    if station:
        q = q.filter(TestRecord.station == station)
    if startTime:
        q = q.filter(TestRecord.created_at >= datetime.fromisoformat(startTime))
    if endTime:
        q = q.filter(TestRecord.created_at <= datetime.fromisoformat(endTime))

    all_rows = q.order_by(TestRecord.created_at.asc()).all()

    total = len(all_rows)
    pass_count = sum(
        1 for r in all_rows if r.test_result and r.test_result.upper() in ("PASS", "OK", "通过")
    )
    fail_count = total - pass_count
    pass_rate = round(pass_count / total * 100, 1) if total else 0.0

    trend_map = {}
    periods = []
    for r in all_rows:
        dt = r.created_at
        if groupBy == "day":
            key = dt.strftime("%Y-%m-%d")
        elif groupBy == "week":
            key = dt.strftime("%Y-W%V")
        else:
            key = dt.strftime("%Y-%m")
        if key not in trend_map:
            trend_map[key] = {"total": 0, "pass": 0}
            periods.append(key)
        trend_map[key]["total"] += 1
        if r.test_result and r.test_result.upper() in ("PASS", "OK", "通过"):
            trend_map[key]["pass"] += 1

    trend = []
    for p in periods:
        d = trend_map[p]
        fail = d["total"] - d["pass"]
        trend.append(
            {
                "period": p,
                "total": d["total"],
                "pass": d["pass"],
                "fail": fail,
                "passRate": round(d["pass"] / d["total"] * 100, 1) if d["total"] else 0.0,
            }
        )

    fail_items_q = (
        db.query(TestRecordItem.name, func.count(TestRecordItem.id).label("cnt"))
        .join(TestRecord, TestRecordItem.record_id == TestRecord.id)
        .filter(TestRecordItem.result.isnot(None))
        .filter(func.upper(TestRecordItem.result).in_(["NG", "FAIL", "失败"]))
    )
    if factoryName:
        fail_items_q = fail_items_q.filter(TestRecord.factory_name == factoryName)
    if station:
        fail_items_q = fail_items_q.filter(TestRecord.station == station)
    if startTime:
        fail_items_q = fail_items_q.filter(TestRecord.created_at >= datetime.fromisoformat(startTime))
    if endTime:
        fail_items_q = fail_items_q.filter(TestRecord.created_at <= datetime.fromisoformat(endTime))

    fail_items_rows = (
        fail_items_q.group_by(TestRecordItem.name)
        .order_by(func.count(TestRecordItem.id).desc())
        .limit(10)
        .all()
    )
    top_fail_items = [{"name": r[0], "failCount": r[1]} for r in fail_items_rows]

    return ok(
        {
            "overallPassRate": pass_rate,
            "totalCount": total,
            "passCount": pass_count,
            "failCount": fail_count,
            "trend": trend,
            "topFailItems": top_fail_items,
        }
    )
