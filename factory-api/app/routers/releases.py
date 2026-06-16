"""统一发布（示例实现，仅返回空列表）。"""

from typing import Annotated

from fastapi import APIRouter, Depends

from app.deps import get_current_user
from app.models import User
from app.response import ok

router = APIRouter(prefix="/admin/releases", tags=["releases"])


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        from app.response import fail

        fail(403, "仅 engineer / admin 可访问", 403)


@router.get("")
def list_releases(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok({"items": []})


@router.post("")
def create_release(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok(message="发布单已创建（示例实现）")

