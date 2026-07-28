"""账号、设备、登录审计等管理接口（简化版）。"""

from app.time_util import to_utc_iso_z, utc_now_naive
from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import get_current_user
from app.models import AdminDevice, LoginAudit, User
from app.response import ok
from app.security import hash_password, parse_roles
from app.services import user_batch

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
                "password": r.password_plain or "",
                "factoryCode": r.factory_code or "",
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
    factory_code = (body.get("factoryCode") or "").strip() or None
    if not username or not password:
        from app.response import fail

        fail(400, "用户名和密码不能为空", 400)
    if db.query(User).filter(User.username == username).first():
        from app.response import fail

        fail(400, "用户名已存在", 400)
    u = User(
        username=username,
        password_hash=hash_password(password),
        password_plain=password,
        roles=",".join(roles),
        station_keys=",".join(station_keys),
        factory_code=factory_code,
        status="active",
    )
    db.add(u)
    db.commit()
    return ok(message="已创建")


@router.post("/admin/users/batch-import/preview")
def batch_import_preview(
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    """预览按工厂生成的三类账号（账号如 lxop，密码为账号+26）。"""
    _require_admin(user)
    data = user_batch.preview_batch_accounts(
        db,
        factory_codes=body.get("factoryCodes"),
        roles=body.get("roles"),
    )
    return ok(data)


@router.post("/admin/users/batch-import")
def batch_import_users(
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    data = user_batch.import_batch_accounts(
        db,
        factory_codes=body.get("factoryCodes"),
        roles=body.get("roles"),
        station_keys=body.get("stationKeys"),
        skip_existing=bool(body.get("skipExisting", True)),
    )
    return ok(data, message=f"已创建 {data['summary']['created']} 个账号")


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
    if "factoryCode" in body:
        code = (body.get("factoryCode") or "").strip()
        u.factory_code = code or None
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
    u.password_plain = new_pwd
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


def _active_admin_count(db: Session) -> int:
    count = 0
    for row in db.query(User).filter(User.status == "active").all():
        if "admin" in parse_roles(row.roles):
            count += 1
    return count


def _delete_user_row(db: Session, operator: User, user_id: int) -> User:
    u = db.get(User, user_id)
    if not u:
        from app.response import fail

        fail(404, "用户不存在", 404)
    if u.id == operator.id:
        from app.response import fail

        fail(400, "不能删除当前登录账号", 400)
    if "admin" in parse_roles(u.roles) and _active_admin_count(db) <= 1:
        from app.response import fail

        fail(400, "不能删除最后一个启用的管理员", 400)
    db.delete(u)
    db.commit()
    return u


@router.delete("/admin/users/{user_id}")
def delete_user(
    user_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    _delete_user_row(db, user, user_id)
    return ok(message="已删除")


@router.post("/admin/users/{user_id}/delete")
def delete_user_post(
    user_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    """POST 删除（与 reset-password / unlock 一致，避免部分代理拦截 DELETE）。"""
    _require_admin(user)
    _delete_user_row(db, user, user_id)
    return ok(message="已删除")


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
    user.password_plain = new
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


@router.get("/admin/devices")
def list_devices(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
    keyword: str | None = None,
):
    _require_admin(user)
    q = db.query(AdminDevice)
    if keyword:
        q = q.filter(AdminDevice.host_name.contains(keyword))
    rows = q.order_by(AdminDevice.created_at.desc()).all()
    items = [
        {
            "id": r.id,
            "hostName": r.host_name,
            "lineName": r.line_name or "",
            "stationLabel": r.station_label or "",
            "remark": r.remark or "",
            "createdAt": to_utc_iso_z(r.created_at) if r.created_at else None,
        }
        for r in rows
    ]
    return ok({"items": items})


@router.post("/admin/devices")
def create_device(
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    host_name = (body.get("hostName") or "").strip()
    if not host_name:
        from app.response import fail

        fail(400, "电脑名不能为空", 400)
    existing = db.query(AdminDevice).filter(AdminDevice.host_name == host_name).first()
    if existing:
        from app.response import fail

        fail(400, "该电脑已登记", 400)
    d = AdminDevice(
        host_name=host_name,
        line_name=(body.get("lineName") or "").strip() or None,
        station_label=(body.get("stationLabel") or "").strip() or None,
        remark=(body.get("remark") or "").strip() or None,
    )
    db.add(d)
    db.commit()
    return ok(message="已登记")


@router.put("/admin/devices/{device_id}")
def update_device(
    device_id: int,
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    d = db.get(AdminDevice, device_id)
    if not d:
        from app.response import fail

        fail(404, "设备不存在", 404)
    if "hostName" in body:
        d.host_name = body["hostName"].strip()
    if "lineName" in body:
        d.line_name = (body["lineName"] or "").strip() or None
    if "stationLabel" in body:
        d.station_label = (body["stationLabel"] or "").strip() or None
    if "remark" in body:
        d.remark = (body["remark"] or "").strip() or None
    db.commit()
    return ok(message="已保存")


@router.delete("/admin/devices/{device_id}")
def delete_device(
    device_id: int,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_admin(user)
    d = db.get(AdminDevice, device_id)
    if not d:
        from app.response import fail

        fail(404, "设备不存在", 404)
    db.delete(d)
    db.commit()
    return ok(message="已删除")


@router.get("/admin/storage/info")
def storage_info(user: Annotated[User, Depends(get_current_user)]):
    """盘符容量、日志/测试数据等占用与爆满预警。"""
    _require_admin(user)
    from app.services.storage_stats import collect_storage_info

    return ok(collect_storage_info())


@router.get("/admin/storage/hosts")
def storage_hosts(
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    """按电脑汇总测试记录，供存储管理批量清理。"""
    _require_admin(user)
    from app.services.storage_stats import list_test_data_hosts

    return ok({"items": list_test_data_hosts(db)})


@router.post("/admin/storage/hosts/delete")
def storage_hosts_delete(
    body: dict,
    db: Annotated[Session, Depends(get_db)],
    user: Annotated[User, Depends(get_current_user)],
):
    """批量删除某电脑的测试数据（可选同时删日志）。"""
    _require_admin(user)
    from app.response import fail
    from app.services.storage_stats import delete_test_data_by_host

    host_name = str((body or {}).get("hostName") or "").strip()
    delete_logs = bool((body or {}).get("deleteLogs"))
    if not host_name:
        fail(400, "电脑名不能为空", 400)
    try:
        result = delete_test_data_by_host(db, host_name, delete_logs=delete_logs)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(result, message="已删除")

