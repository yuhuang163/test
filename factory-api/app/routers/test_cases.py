"""测试用例：上位机上传/下载用例包 + 管理端查看编辑 + 版本管理与差异比对。"""

from typing import Annotated

from fastapi import APIRouter, Body, Depends, File, Form, Query, UploadFile
from fastapi.responses import PlainTextResponse, Response

from app.deps import get_current_user
from app.models import User
from app.response import fail, ok
from app.services import test_cases as test_case_service
from app.services import versioning as versioning_service

router = APIRouter(prefix="/test-cases", tags=["test-cases"])
admin_router = APIRouter(prefix="/admin/test-cases", tags=["test-cases-admin"])


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        fail(403, "仅 engineer / admin 可访问", 403)


@router.get("/manifest")
def client_manifest(user: Annotated[User, Depends(get_current_user)]):
    """上位机同步：查询当前用例包版本与文件清单。"""
    return ok(test_case_service.read_manifest())


@router.get("/bundle")
def client_bundle(user: Annotated[User, Depends(get_current_user)]):
    """上位机同步：下载当前用例包（zip，非 JSON 包络）。"""
    zip_bytes = test_case_service.build_bundle_zip()
    if not zip_bytes:
        fail(404, "用例包为空", 404)
    manifest = test_case_service.read_manifest()
    version = manifest.get("bundleVersion") or "bundle"
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="test_case_{version}.zip"'},
    )


@router.post("/upload")
async def client_upload_bundle(
    user: Annotated[User, Depends(get_current_user)],
    file: Annotated[UploadFile, File()],
    deviceId: Annotated[str | None, Form()] = None,
    stationKey: Annotated[str | None, Form()] = None,
    hostName: Annotated[str | None, Form()] = None,
):
    """上位机上传 test_case zip，导入并自动发布用例包。"""
    _require_engineer_or_admin(user)
    content = await file.read()
    try:
        data = test_case_service.import_bundle_zip(content)
        published = test_case_service.publish_bundle()
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(
        {
            "bundleVersion": published.get("bundleVersion"),
            "fileCount": len(published.get("files") or []),
            "importedCount": data.get("importedCount", 0),
            "deviceId": deviceId,
            "stationKey": stationKey,
            "hostName": hostName,
        },
        message="上传并发布成功",
    )


@admin_router.get("/files")
def list_files(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok(test_case_service.read_manifest())


@admin_router.get("/bundle")
def admin_download_bundle(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    zip_bytes = test_case_service.build_bundle_zip()
    if not zip_bytes:
        fail(404, "用例包为空", 404)
    manifest = test_case_service.read_manifest()
    version = manifest.get("bundleVersion") or "bundle"
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="test_case_{version}.zip"'},
    )


@admin_router.get("/files/{path:path}")
def get_file(path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        text = test_case_service.read_file_text(path)
    except ValueError:
        fail(400, "非法路径", 400)
    except FileNotFoundError:
        fail(404, "文件不存在", 404)
    return PlainTextResponse(text, media_type="text/plain; charset=utf-8")


@admin_router.put("/files/{path:path}")
def save_file(path: str, content: str = Body(""), user: Annotated[User, Depends(get_current_user)] = None):
    _require_engineer_or_admin(user)
    try:
        test_case_service.write_file_text(path, content)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(message="已保存")


@admin_router.delete("/files/{path:path}")
def remove_file(path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        test_case_service.delete_file(path)
    except ValueError:
        fail(400, "非法路径", 400)
    except FileNotFoundError:
        fail(404, "文件不存在", 404)
    return ok(message="已删除")


@admin_router.get("/versions")
def list_versions(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok(versioning_service.list_versions())


@admin_router.get("/versions/{version}/files")
def get_version_files(version: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        files = versioning_service.get_version_files(version)
    except ValueError as exc:
        fail(404, str(exc), 404)
    return ok({"version": version, "files": files})


@admin_router.get("/versions/{version}/files/{path:path}")
def get_version_file(version: str, path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        text = versioning_service.read_version_file(version, path)
    except ValueError:
        fail(400, "非法路径", 400)
    except FileNotFoundError:
        fail(404, "文件不存在", 404)
    return PlainTextResponse(text, media_type="text/plain; charset=utf-8")


@admin_router.get("/versions/diff")
def diff_versions(
    from_: Annotated[str, Query(..., alias="from")],
    to: Annotated[str, Query(...)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_engineer_or_admin(user)
    try:
        data = versioning_service.diff_versions(from_, to)
    except ValueError as exc:
        fail(404, str(exc), 404)
    return ok(data)


@admin_router.post("/publish")
def publish_bundle(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    data = test_case_service.publish_bundle()
    return ok({"bundleVersion": data["bundleVersion"]}, message="发布成功")
