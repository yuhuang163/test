"""上位机版本（OTA）与运行环境管理。"""

import json
import re
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


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        fail(403, "仅 engineer / admin 可访问", 403)


def _parse_version(v: str) -> tuple[int, ...]:
    """将 '1.6.2' 转为 (1, 6, 2) 用于数值比较。"""
    parts = re.split(r"[._\-]", v.strip())
    nums = []
    for p in parts:
        try:
            nums.append(int(p))
        except ValueError:
            nums.append(0)
    return tuple(nums)


def _parse_build_id(build_id: str) -> tuple[str, int]:
    """buildId：yyyyMMdd 视为序号 0；yyyyMMdd-N 为同日第 N 次构建。
    上位机侧由 host_ota_version.h 维护（与 exe 文件名无关）；请保持此格式以便比较。
    """
    bid = (build_id or "").strip()
    if not bid:
        return ("", -1)
    m = re.match(r"^(\d{8})(?:-(\d+))?$", bid)
    if m:
        return (m.group(1), int(m.group(2)) if m.group(2) else 0)
    return (bid, 0)


def _compare_build_id(a: str, b: str) -> int:
    """返回 -1/0/1，表示 a 相对于 b 的新旧（先比日期再比 -N 序号）。"""
    da, sa = _parse_build_id(a)
    db, sb = _parse_build_id(b)
    if da != db:
        return -1 if da < db else 1
    if sa != sb:
        return -1 if sa < sb else 1
    return 0


@router.get("/check")
def check(
    packageName: str,
    buildId: str | None = None,
    appVersion: str | None = None,
    stationKey: str | None = None,
    deviceId: str | None = None,
):
    """上位机检查更新：按 appVersion 比较，有新版本则提示更新。"""
    del stationKey, deviceId
    pkg = (packageName or "new_production").strip()
    current_ver = (appVersion or "").strip()
    current_build = (buildId or "").strip()
    versions = host_app_service.list_versions()
    pkg_versions = [v for v in versions if v["packageName"] == pkg]
    if not pkg_versions:
        host_has = current_ver or current_build
        return ok({"hasUpdate": False, "hostNewer": bool(host_has), "latest": None})

    latest = max(pkg_versions, key=lambda v: _parse_build_id(v.get("buildId") or ""))
    latest_ver = latest.get("appVersion") or ""
    latest_build = latest.get("buildId") or ""

    # 用于显示的版本信息（appVersion 为空时回退到 buildId）
    display_app_version = latest.get("appVersion") or latest["buildId"]
    # 用于比较的版本号（appVersion 为空时当 0.0.0，避免用日期字符串比较）
    compare_app_version = latest.get("appVersion") or "0.0.0"

    latest_info = {
        "appVersion": display_app_version,
        "buildId": latest["buildId"],
        "downloadUrl": "",
        "sha256": latest.get("sha256") or "",
        "forceUpgrade": latest.get("forceUpgrade", False),
        "releaseNotes": latest.get("releaseNotes") or "",
        "packageName": pkg,
        "uploadedAt": latest.get("uploadedAt") or "",
    }

    server_newer = _parse_version(current_ver) < _parse_version(compare_app_version) if (current_ver and compare_app_version) else False
    build_cmp = _compare_build_id(current_build, latest_info["buildId"]) if (current_build and latest_info["buildId"]) else 0
    server_newer = server_newer or build_cmp < 0

    host_newer = _parse_version(current_ver) > _parse_version(compare_app_version) if (current_ver and compare_app_version) else False
    host_newer = host_newer or build_cmp > 0

    return ok(
        {
            "hasUpdate": server_newer,
            "hostNewer": host_newer,
            "latest": latest_info,
        }
    )


@router.get("/versions")
def host_list_versions():
    """上位机获取所有可用版本列表。"""
    return ok({"items": host_app_service.list_versions()})


@router.get("/download/{build_id}")
def download_build(build_id: str, packageName: str | None = None,
                   uploadedAt: str | None = None):
    """上位机下载安装包（二进制 exe，非 JSON 包络）。
    uploadedAt 用于精确匹配版本记录，避免同 buildId 多版本时下载错误。
    """
    path = host_app_service.resolve_build_path(build_id, packageName, uploadedAt)
    if not path:
        fail(404, "安装包不存在", 404)
    return FileResponse(path, media_type="application/octet-stream", filename=path.name)


@router.post("/upload")
async def host_upload(
    user: Annotated[User, Depends(get_current_user)],
    file: Annotated[UploadFile, File()],
    appVersion: str = Form(...),
    buildId: str = Form(...),
    packageName: str = Form("new_production"),
    releaseNotes: str = Form(""),
    sha256: str | None = Form(default=None),
):
    """上位机上传自身 exe（首次部署或主动上报版本）。"""
    _require_engineer_or_admin(user)
    if not appVersion or not buildId:
        fail(400, "appVersion 与 buildId 不能为空", 400)
    content = await file.read()
    if len(content) < 1024:
        fail(400, "文件过小", 400)
    host_app_service.save_build(
        build_id=buildId.strip(),
        content=content,
        package_name=packageName.strip() or "new_production",
        app_version=appVersion,
        release_notes=releaseNotes,
        sha256=sha256 or "",
    )
    return ok(message="版本已上传")


@admin_router.get("/versions")
def list_versions(user: Annotated[User, Depends(get_current_user)]):
    _require_admin(user)
    return ok({"items": host_app_service.list_versions()})


@admin_router.delete("/versions")
def delete_version(
    user: Annotated[User, Depends(get_current_user)],
    buildId: str,
    uploadedAt: str | None = None,
    packageName: str | None = None,
):
    """删除一条上位机版本记录及其 exe（同 buildId 多条时用 uploadedAt 精确定位）。"""
    _require_admin(user)
    if not (buildId or "").strip():
        fail(400, "buildId 不能为空", 400)
    try:
        result = host_app_service.delete_version(
            build_id=buildId.strip(),
            package_name=(packageName.strip() if packageName else None),
            uploaded_at=(uploadedAt.strip() if uploadedAt else None),
        )
    except ValueError as exc:
        fail(404, str(exc), 404)
    return ok(result, message="版本已删除")


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
        fail(400, "appVersion 与 buildId 不能为空", 400)
    if not file:
        fail(400, "请上传 exe 安装包", 400)
    content = await file.read()
    if len(content) < 1024:
        fail(400, "安装包文件过小", 400)

    gray = {}
    if grayRules:
        try:
            gray = json.loads(grayRules)
        except json.JSONDecodeError:
            fail(400, "grayRules 格式错误", 400)

    host_app_service.save_build(
        build_id=buildId.strip(),
        content=content,
        package_name=packageName.strip() or "new_production",
        app_version=appVersion,
        release_notes=releaseNotes,
        force_upgrade=forceUpgrade.lower() == "true",
        sha256=sha256 or "",
        gray_rules=gray,
    )
    return ok(message="版本已上传")


@admin_router.post("/runtime-env")
async def upload_runtime_env(
    user: Annotated[User, Depends(get_current_user)],
    file: Annotated[UploadFile, File()],
):
    """上传路特上位机运行环境（zip 包）。"""
    _require_admin(user)
    content = await file.read()
    if len(content) < 1024:
        fail(400, "文件过小", 400)
    if content[:2] != b"PK":
        fail(400, "请上传 zip 文件", 400)
    host_app_service.save_runtime_env_zip(content)
    return ok(message="运行环境已上传")


@admin_router.get("/runtime-env")
def download_runtime_env(user: Annotated[User, Depends(get_current_user)]):
    """管理员/工程师下载路特上位机运行环境（首次使用上位机时需下载）。"""
    _require_engineer_or_admin(user)
    from urllib.parse import quote

    from fastapi.responses import Response

    try:
        zip_bytes = host_app_service.build_runtime_env_zip()
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except Exception as exc:  # noqa: BLE001 — 打包/读盘异常转可读错误，避免裸 500
        fail(500, f"运行环境打包失败：{exc}", 500)

    # starlette 响应头按 latin-1 编码，中文文件名须用 RFC5987 filename*
    ascii_name = "lute_host_runtime_env.zip"
    utf8_name = quote("路特上位机运行环境.zip")
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={
            "Content-Disposition": f'attachment; filename="{ascii_name}"; filename*=UTF-8\'\'{utf8_name}',
        },
    )


@admin_router.get("/runtime-env/info")
def runtime_env_info(user: Annotated[User, Depends(get_current_user)]):
    """查询运行环境信息。"""
    _require_engineer_or_admin(user)
    return ok(host_app_service.get_runtime_env_info())
