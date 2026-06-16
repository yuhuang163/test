"""测试用例管理（简化版，当前使用内置示例结构）。"""

from typing import Annotated

from fastapi import APIRouter, Body, Depends

from app.deps import get_current_user
from app.models import User
from app.response import ok

router = APIRouter(prefix="/admin/test-cases", tags=["test-cases"])


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        from app.response import fail

        fail(403, "仅 engineer / admin 可访问", 403)


@router.get("/files")
def list_files(user: Annotated[User, Depends(get_current_user)]):
    """返回一个简化的文件树，方便前端联调。"""
    _require_engineer_or_admin(user)
    data = {
        "bundleVersion": "demo-1",
        "files": [
            {"path": "test_case/demo.ini"},
        ],
    }
    return ok(data)


@router.get("/files/{path:path}")
def get_file(path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    ini = "[Demo]\nName=示例用例\n"
    return ini


@router.put("/files/{path:path}")
def save_file(path: str, content: str = Body(""), user: Annotated[User, Depends(get_current_user)] = None):
    _require_engineer_or_admin(user)
    # 仅做回显，不真正落盘
    return ok(message="已保存（示例实现）")


@router.post("/publish")
def publish_bundle(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok({"bundleVersion": "demo-1"}, message="发布成功（示例实现）")

