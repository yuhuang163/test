"""账号、设备、登录审计等管理接口（简化版）。"""

from app.time_util import to_utc_iso_z, utc_now_naive
from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import get_current_user
from app.models import LoginAudit, User
from app.response import ok
from app.security import hash_password

router = APIRouter(tags=["admin-extra"])


def _require_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles:
        from app.response import fail

        fail(403, "仅 admin 可访问", 403)


@router.get("/admin/users")
def list_users(db: Annotated[Session, Depends(get_db)], user: Annotated[User, Depends(get_current_user)]):
    _require_admin(user)
    rows = db.query(User).order_by(User.id.asc()).all()
    items = []
    for r in rows:
        items.append(
            {
                "id": r.id,
                "username": r.username,
                "roles": (r.roles or "").split(","),
                "stationKeys": [s for s in (r.station_keys or "").split(",") if s],
                "status": r.status,
                "failedLoginCount": r.failed_login_count,
                "lockedUntil": r.locked_until,
                "lastLoginAt": r.last_login_at,
                "lastLoginHost": r.last_login_host,
                "remark": "",
            }
        )
    return ok({"items": items})


@router.post("/admin/users")
def create_user(
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    username = (body.get("username") or "").strip()
    password = body.get("password") or ""
    roles = body.get("roles") or ["operator"]
    station_keys = body.get("stationKeys") or []
    if not username or not password:
        from app.response import fail

        fail(400, "用户名和密码不能为空", 400)
    if db.query(User).filter(User.username == username).first():
        from app.response import fail

        fail(400, "用户名已存在", 400)
    u = User(
        username=username,
        password_hash=hash_password(password),
        roles=",".join(roles),
        station_keys=",".join(station_keys),
        status="active",
    )
    db.add(u)
    db.commit()
    return ok(message="已创建")


@router.put("/admin/users/{user_id}")
def update_user(
    user_id: int,
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    u = db.get(User, user_id)
    if not u:
        from app.response import fail

        fail(404, "用户不存在", 404)
    if "roles" in body:
        u.roles = ",".join(body.get("roles") or [])
    if "stationKeys" in body:
        u.station_keys = ",".join(body.get("stationKeys") or [])
    if "status" in body:
        u.status = body.get("status") or "active"
    db.commit()
    return ok(message="已保存")


@router.post("/admin/users/{user_id}/reset-password")
def reset_password(
    user_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    u = db.get(User, user_id)
    if not u:
        from app.response import fail

        fail(404, "用户不存在", 404)
    new_pwd = "ChangeMe123"
    u.password_hash = hash_password(new_pwd)
    u.failed_login_count = 0
    u.locked_until = None
    db.commit()
    return ok({"password": new_pwd}, message="密码已重置")


@router.post("/admin/users/{user_id}/unlock")
def unlock_user(
    user_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    u = db.get(User, user_id)
    if not u:
        from app.response import fail

        fail(404, "用户不存在", 404)
    u.failed_login_count = 0
    u.locked_until = None
    db.commit()
    return ok(message="已解锁")


@router.post("/auth/change-password")
def change_password(
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    old = body.get("oldPassword") or ""
    new = body.get("newPassword") or ""
    from app.security import verify_password

    if not old or not new:
        from app.response import fail

        fail(400, "密码不能为空", 400)
    if not verify_password(old, user.password_hash):
        from app.response import fail

        fail(400, "旧密码不正确", 400)
    user.password_hash = hash_password(new)
    user.failed_login_count = 0
    user.locked_until = None
    db.commit()
    return ok(message="密码已修改")


@router.get("/admin/audit-logins")
def list_audit_logins(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    username: str | None = None,
    hostName: str | None = None,
    page: int = 1,
    pageSize: int = 20,
):
    _require_admin(user)
    q = db.query(LoginAudit)
    if username:
        q = q.filter(LoginAudit.username == username)
    if hostName:
        q = q.filter(LoginAudit.host_name.contains(hostName))
    total = q.count()
    rows = (
        q.order_by(LoginAudit.created_at.desc())
        .offset((page - 1) * pageSize)
        .limit(pageSize)
        .all()
    )
    items = [
        {
            "id": r.id,
            "username": r.username,
            "hostName": r.host_name,
            "deviceId": r.device_id,
            "stationKey": r.station_key,
            "action": r.action,
            "ip": r.ip,
            "createdAt": to_utc_iso_z(r.created_at) or to_utc_iso_z(utc_now_naive()),
        }
        for r in rows
    ]
    return ok({"items": items, "total": total, "page": page, "pageSize": pageSize})

