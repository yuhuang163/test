"""元数据路由。"""

from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import get_current_user
from app.factory_scope import list_visible_factories
from app.models import Factory, User
from app.response import fail, ok
from app.schemas import FactoryItem
from app.security import parse_roles

router = APIRouter(prefix="/admin/meta", tags=["meta"])


def _require_admin(user: User) -> None:
    if "admin" not in parse_roles(user.roles):
        fail(403, "仅 admin 可访问", 403)


def _factory_item(row: Factory) -> dict:
    return {
        "code": row.code,
        "displayName": row.display_name,
        "sortOrder": row.sort_order,
        "enabled": row.enabled,
    }


@router.get("/factories")
def list_factories(db: Annotated[Session, Depends(get_db)], user: Annotated[User, Depends(get_current_user)]):
    rows = list_visible_factories(db, user)
    items = [FactoryItem(code=r.code, displayName=r.display_name).model_dump() for r in rows]
    return ok(items)


@router.get("/factories/all")
def list_all_factories(db: Annotated[Session, Depends(get_db)], user: Annotated[User, Depends(get_current_user)]):
    """管理端：含已停用工厂，新增后无需重启服务即可用于批量导入。"""
    _require_admin(user)
    rows = db.query(Factory).order_by(Factory.sort_order.asc(), Factory.code.asc()).all()
    return ok([_factory_item(r) for r in rows])


@router.post("/factories")
def create_factory(
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    code = (body.get("code") or "").strip()
    display_name = (body.get("displayName") or "").strip()
    if not code or not display_name:
        fail(400, "工厂代码与显示名不能为空", 400)
    if len(code) > 32:
        fail(400, "工厂代码过长", 400)
    if db.get(Factory, code):
        fail(400, "工厂代码已存在", 400)
    sort_order = body.get("sortOrder")
    if sort_order is None:
        max_order = db.query(func.max(Factory.sort_order)).scalar() or 0
        sort_order = int(max_order) + 10
    row = Factory(
        code=code,
        display_name=display_name,
        sort_order=int(sort_order),
        enabled=bool(body.get("enabled", True)),
    )
    db.add(row)
    db.commit()
    return ok(_factory_item(row), message="工厂已添加")


@router.put("/factories/{code}")
def update_factory(
    code: str,
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    row = db.get(Factory, code)
    if not row:
        fail(404, "工厂不存在", 404)
    if "displayName" in body:
        name = (body.get("displayName") or "").strip()
        if not name:
            fail(400, "显示名不能为空", 400)
        row.display_name = name
    if "sortOrder" in body:
        row.sort_order = int(body.get("sortOrder") or 0)
    if "enabled" in body:
        row.enabled = bool(body.get("enabled"))
    db.commit()
    return ok(_factory_item(row), message="已保存")


@router.get("/stations")
def list_stations(user: Annotated[User, Depends(get_current_user)]):
    """工站字典：与上位机 SYSTEM/station 一致。"""
    data = [
        {"key": "FREE_WORK", "name": "自由工站"},
        {"key": "MAIN_TEST", "name": "调试上位机"},
        {"key": "QUIESCENT_CURRENT", "name": "静态电流工站"},
        {"key": "MOTOR_TEST", "name": "电机工站"},
        {"key": "IMU_CALI", "name": "IMU 校准工站"},
        {"key": "SCREEN_TEST", "name": "屏幕工站"},
        {"key": "CAMERA_TEST", "name": "摄像头工站"},
        {"key": "WIFIBLE_TEST", "name": "蓝牙/WiFi 工站"},
        {"key": "AGE_TEST", "name": "老化工站"},
        {"key": "PRESS_TEST", "name": "压感工站"},
        {"key": "PCBA_TEST", "name": "板厂工站"},
        {"key": "KEY_TEST", "name": "按键工站"},
        {"key": "SUCTION_TEST", "name": "吸力工站"},
    ]
    return ok(data)
