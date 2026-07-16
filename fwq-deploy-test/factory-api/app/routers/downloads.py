"""登录用户下载上位机资源：exe、测试用例、运行环境。"""

from typing import Annotated
from urllib.parse import quote

from fastapi import APIRouter, Depends, Query
from fastapi.responses import FileResponse, Response

from app.deps import get_current_user
from app.models import User
from app.response import fail, ok
from app.services import host_app as host_app_service
from app.services import test_cases as test_case_service

router = APIRouter(prefix="/downloads", tags=["downloads"])


def _latest_host_version(package_name: str = "new_production") -> dict | None:
    pkg = (package_name or "new_production").strip() or "new_production"
    for item in host_app_service.list_versions():
        if (item.get("packageName") or "new_production") == pkg:
            return item
    return None


@router.get("/summary")
def download_summary(
    user: Annotated[User, Depends(get_current_user)],
    packageName: str = Query("new_production"),
):
    """下载页摘要：最新 exe、用例包、运行环境。"""
    del user
    latest = _latest_host_version(packageName)
    host = None
    if latest:
        host = {
            "appVersion": latest.get("appVersion") or "",
            "buildId": latest.get("buildId") or "",
            "packageName": latest.get("packageName") or packageName,
            "uploadedAt": latest.get("uploadedAt") or "",
            "releaseNotes": latest.get("releaseNotes") or "",
            "size": latest.get("size") or 0,
            "fileName": latest.get("fileName") or "",
        }

    manifest = test_case_service.read_manifest()
    cases = {
        "bundleVersion": manifest.get("bundleVersion") or "",
        "fileCount": len(manifest.get("files") or []),
        "updatedAt": manifest.get("updatedAt") or manifest.get("publishedAt") or "",
    }

    runtime = host_app_service.get_runtime_env_info()
    return ok({"hostApp": host, "testCases": cases, "runtimeEnv": runtime})


@router.get("/host-exe")
def download_latest_host_exe(
    user: Annotated[User, Depends(get_current_user)],
    packageName: str = Query("new_production"),
):
    """下载最新上位机 exe。"""
    del user
    latest = _latest_host_version(packageName)
    if not latest:
        fail(404, "暂无上位机安装包", 404)
    path = host_app_service.resolve_build_path(
        latest["buildId"],
        latest.get("packageName"),
        latest.get("uploadedAt"),
    )
    if not path:
        fail(404, "安装包文件不存在", 404)
    filename = path.name
    return FileResponse(
        path,
        media_type="application/octet-stream",
        filename=filename,
        headers={
            "Content-Disposition": f'attachment; filename="{filename}"',
        },
    )


@router.get("/test-cases")
def download_test_cases(user: Annotated[User, Depends(get_current_user)]):
    """下载当前测试用例包 zip。"""
    del user
    zip_bytes = test_case_service.build_bundle_zip()
    if not zip_bytes:
        fail(404, "用例包为空", 404)
    manifest = test_case_service.read_manifest()
    version = manifest.get("bundleVersion") or "bundle"
    ascii_name = f"test_case_{version}.zip"
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="{ascii_name}"'},
    )


@router.get("/runtime-env")
def download_runtime_env(user: Annotated[User, Depends(get_current_user)]):
    """下载路特上位机运行环境 zip（任意登录用户）。"""
    del user
    try:
        zip_bytes = host_app_service.build_runtime_env_zip()
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except Exception as exc:  # noqa: BLE001
        fail(500, f"运行环境打包失败：{exc}", 500)

    ascii_name = "lute_host_runtime_env.zip"
    utf8_name = quote("路特上位机运行环境.zip")
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={
            "Content-Disposition": f'attachment; filename="{ascii_name}"; filename*=UTF-8\'\'{utf8_name}',
        },
    )
