"""上位机版本（OTA）管理（简化版）。"""

from typing import Annotated

from fastapi import APIRouter, Depends, File, Form, UploadFile

from app.deps import get_current_user
from app.models import User
from app.response import ok

router = APIRouter(prefix="/host-app", tags=["host-app"])
admin_router = APIRouter(prefix="/admin/host-app", tags=["host-app-admin"])


def _require_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles:
        from app.response import fail

        fail(403, "仅 admin 可访问", 403)


@router.get("/check")
def check(
    packageName: str,
    buildId: str | None = None,
    appVersion: str | None = None,
):
    """上位机检查更新：当前直接返回「无更新」。"""
    return ok({"hasUpdate": False, "latest": None})


@admin_router.get("/versions")
def list_versions(user: Annotated[User, Depends(get_current_user)]):
    _require_admin(user)
    return ok({"items": []})


@admin_router.post("/versions")
async def create_version(
    user: Annotated[User, Depends(get_current_user)],
    file: UploadFile | None = File(default=None),
    appVersion: str = Form(...),
    buildId: str = Form(...),
    packageName: str = Form("new_production"),
    releaseNotes: str = Form(""),
    forceUpgrade: str = Form("false"),
    sha256: str | None = Form(default=None),
    grayRules: str | None = Form(default=None),
):
    _require_admin(user)
    if not appVersion or not buildId:
        from app.response import fail

        fail(400, "appVersion 与 buildId 不能为空", 400)
    return ok(message="版本已接收（示例实现）")
