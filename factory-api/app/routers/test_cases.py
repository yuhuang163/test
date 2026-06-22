"""测试用例：上位机 manifest/bundle + 管理端文件维护。"""

from typing import Annotated

from fastapi import APIRouter, Body, Depends
from fastapi.responses import Response

from app.deps import get_current_user
from app.models import User
from app.response import fail, ok
from app.services import test_cases as test_case_service

router = APIRouter(prefix="/test-cases", tags=["test-cases"])
admin_router = APIRouter(prefix="/admin/test-cases", tags=["test-cases-admin"])


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        fail(403, "仅 engineer / admin 可访问", 403)


@router.get("/manifest")
def client_manifest(user: Annotated[User, Depends(get_current_user)]):
    """上位机同步：查询当前 bundle 版本与文件清单。"""
    return ok(test_case_service.read_manifest())


@router.get("/bundle")
def client_bundle(user: Annotated[User, Depends(get_current_user)]):
    """上位机同步：下载当前 bundle（zip，非 JSON 包络）。"""
    zip_bytes = test_case_service.build_bundle_zip()
    if not zip_bytes:
        fail(404, "bundle 为空", 404)
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": 'attachment; filename="test_case_bundle.zip"'},
    )


@admin_router.get("/files")
def list_files(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok(test_case_service.read_manifest())


@admin_router.get("/files/{path:path}")
def get_file(path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        return test_case_service.read_file_text(path)
    except ValueError:
        fail(400, "非法路径", 400)
    except FileNotFoundError:
        fail(404, "文件不存在", 404)


@admin_router.put("/files/{path:path}")
def save_file(path: str, content: str = Body(""), user: Annotated[User, Depends(get_current_user)] = None):
    _require_engineer_or_admin(user)
    try:
        test_case_service.write_file_text(path, content)
    except ValueError:
        fail(400, "非法路径", 400)
    return ok(message="已保存")


@admin_router.post("/publish")
def publish_bundle(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    manifest = test_case_service.read_manifest()
    version = manifest["bundleVersion"]
    # 简单递增：demo-1 -> demo-2；也可由前端传版本号，此处先自动 bump
    if version.startswith("demo-"):
        try:
            num = int(version.split("-", 1)[1]) + 1
            version = f"demo-{num}"
        except ValueError:
            version = f"{version}-published"
    else:
        version = f"{version}-published"
    data = test_case_service.publish_bundle(version)
    return ok({"bundleVersion": data["bundleVersion"]}, message="发布成功")
