"""上位机版本（OTA）管理。"""

from typing import Annotated

from fastapi import APIRouter, Depends, File, Form, UploadFile
from fastapi.responses import FileResponse

from app.deps import get_current_user
from app.models import User
from app.response import fail, ok
from app.services import host_app as host_app_service

router = APIRouter(prefix="/host-app", tags=["host-app"])
admin_router = APIRouter(prefix="/admin/host-app", tags=["host-app-admin"])


def _require_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles:
        fail(403, "仅 admin 可访问", 403)


@router.get("/check")
def check(
    packageName: str,
    buildId: str | None = None,
    appVersion: str | None = None,
    stationKey: str | None = None,
    deviceId: str | None = None,
):
    """上位机检查更新：扫描已上传安装包，比 buildId 新则提示更新。"""
    del stationKey, deviceId  # 预留灰度字段
    pkg = (packageName or "new_production").strip()
    current_build = (buildId or "").strip()
    root = host_app_service.storage_root()
    candidates = sorted(root.glob(f"{pkg}_*.exe"), reverse=True)
    latest_path = candidates[0] if candidates else None
    if not latest_path:
        return ok({"hasUpdate": False, "latest": None})

    latest_build = latest_path.stem.replace(f"{pkg}_", "", 1)
    if current_build and current_build >= latest_build:
        return ok({"hasUpdate": False, "latest": None})

    return ok(
        {
            "hasUpdate": True,
            "latest": {
                "appVersion": appVersion or latest_build,
                "buildId": latest_build,
                "downloadUrl": "",
                "sha256": "",
                "forceUpgrade": False,
                "releaseNotes": "",
                "packageName": pkg,
            },
        }
    )


@router.get("/download/{build_id}")
def download_build(build_id: str, packageName: str | None = None):
    """上位机下载安装包（二进制 exe，非 JSON 包络）。"""
    path = host_app_service.resolve_build_path(build_id, packageName)
    if not path:
        fail(404, "安装包不存在", 404)
    return FileResponse(path, media_type="application/octet-stream", filename=path.name)


@admin_router.get("/versions")
def list_versions(user: Annotated[User, Depends(get_current_user)]):
    _require_admin(user)
    root = host_app_service.storage_root()
    items = []
    for path in sorted(root.glob("*.exe"), reverse=True):
        name = path.stem
        if "_" in name:
            package_name, build_id = name.split("_", 1)
        else:
            package_name, build_id = "new_production", name
        items.append(
            {
                "packageName": package_name,
                "buildId": build_id,
                "size": path.stat().st_size,
            }
        )
    return ok({"items": items})


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
    del releaseNotes, forceUpgrade, sha256, grayRules
    if not appVersion or not buildId:
        fail(400, "appVersion 与 buildId 不能为空", 400)
    if not file:
        fail(400, "请上传 exe 安装包", 400)
    content = await file.read()
    if len(content) < 1024:
        fail(400, "安装包文件过小", 400)
    host_app_service.save_build(buildId.strip(), content, packageName.strip() or "new_production")
    return ok(message="版本已上传")
