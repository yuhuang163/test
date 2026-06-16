"""阈值模板与下发（简化版，当前返回内置示例数据）。"""

from typing import Annotated

from fastapi import APIRouter, Depends

from app.deps import get_current_user
from app.models import User
from app.response import ok

router = APIRouter(tags=["thresholds"])
admin_router = APIRouter(prefix="/admin/threshold-templates", tags=["thresholds-admin"])


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        from app.response import fail

        fail(403, "仅 engineer / admin 可访问", 403)


@router.get("/thresholds")
def get_thresholds(
    stationKey: str,
    productModel: str | None = None,
    user: Annotated[User, Depends(get_current_user)] = None,
):
    """上位机拉取阈值：当前返回固定示例，后续可接 DB。"""
    items: list[dict[str, str]] = []
    if stationKey.upper() == "FREE_WORK":
        items = [
            {"settingsKey": "BLE/LowRssi", "value": "-80"},
            {"settingsKey": "BLE/HighRssi", "value": "-40"},
        ]
    data = {
        "version": 1,
        "stationKey": stationKey,
        "productModel": productModel,
        "items": items,
    }
    return ok(data)


@admin_router.get("")
def list_templates(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    data = {
        "items": [
            {
                "id": 1,
                "name": "自由工站默认阈值",
                "stationKey": "FREE_WORK",
                "productModel": "",
                "version": 1,
                "status": "published",
                "updatedAt": "2026-06-16T00:00:00",
            }
        ]
    }
    return ok(data)


@admin_router.get("/{tpl_id}")
def get_template(tpl_id: int, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    data = {
        "id": tpl_id,
        "name": "自由工站默认阈值",
        "stationKey": "FREE_WORK",
        "productModel": "",
        "items": [
            {"settingsKey": "BLE/LowRssi", "value": "-80"},
            {"settingsKey": "BLE/HighRssi", "value": "-40"},
        ],
    }
    return ok(data)


@admin_router.post("")
def create_template(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok({"id": 1}, message="已保存（示例实现）")


@admin_router.put("/{tpl_id}")
def update_template(tpl_id: int, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok(message="已保存（示例实现）")


@admin_router.post("/{tpl_id}/publish")
def publish_template(tpl_id: int, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok({"version": 1}, message="发布成功（示例实现）")
