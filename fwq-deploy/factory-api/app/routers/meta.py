"""元数据路由。"""

from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import get_current_user
from app.models import Factory, User
from app.response import ok
from app.schemas import FactoryItem

router = APIRouter(prefix="/admin/meta", tags=["meta"])


@router.get("/factories")
def list_factories(db: Annotated[Session, Depends(get_db)], user: Annotated[User, Depends(get_current_user)]):
    rows = db.query(Factory).filter(Factory.enabled.is_(True)).order_by(Factory.sort_order).all()
    items = [FactoryItem(code=r.code, displayName=r.display_name).model_dump() for r in rows]
    return ok(items)


@router.get("/stations")
def list_stations(user: Annotated[User, Depends(get_current_user)]):
    """工站字典：当前返回内置列表，后续可接 DB。"""
    data = [
        {"key": "FREE_WORK", "name": "自由工站"},
        {"key": "PCBA", "name": "PCBA"},
        {"key": "AGING", "name": "老化"},
        {"key": "PACK", "name": "包装"},
    ]
    return ok(data)
