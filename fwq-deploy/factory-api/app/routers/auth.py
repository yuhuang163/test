"""认证路由。"""

from datetime import datetime, timezone
from typing import Annotated

from fastapi import APIRouter, Depends, Request
from sqlalchemy.orm import Session

from app.database import get_db
from app.deps import client_ip, get_current_user
from app.factory_scope import get_user_factory_scope
from app.models import LoginAudit, User
from app.response import fail, ok
from app.schemas import LoginRequest, LoginResponseData, UserMeData
from app.security import (
    create_access_token,
    parse_roles,
    parse_station_keys,
    station_key_authorized,
    verify_password,
)

router = APIRouter(prefix="/auth", tags=["auth"])


@router.post("/login")
def login(body: LoginRequest, request: Request, db: Annotated[Session, Depends(get_db)]):
    user = db.query(User).filter(User.username == body.username).first()
    ip = client_ip(request)

    def audit(action: str, uid: int | None = None) -> None:
        db.add(
            LoginAudit(
                user_id=uid,
                username=body.username,
                host_name=body.hostName,
                device_id=body.deviceId,
                station_key=body.stationKey,
                action=action,
                ip=ip,
            )
        )
        db.commit()

    if not user or user.status != "active":
        audit("login_fail")
        fail(401, "账号或密码错误", 401)

    if user.locked_until and user.locked_until > datetime.now(timezone.utc).replace(tzinfo=None):
        audit("login_fail", user.id)
        fail(403, "账号已锁定，请稍后再试", 403)

    if not verify_password(body.password, user.password_hash):
        user.failed_login_count = (user.failed_login_count or 0) + 1
        if user.failed_login_count >= 5:
            from datetime import timedelta

            user.locked_until = datetime.utcnow() + timedelta(minutes=30)
        db.commit()
        audit("login_fail", user.id)
        fail(401, "账号或密码错误", 401)

    if body.stationKey:
        allowed = set(parse_station_keys(user.station_keys))
        if allowed and not station_key_authorized(allowed, body.stationKey) and "admin" not in parse_roles(user.roles):
            audit("login_fail", user.id)
            allowed_text = "、".join(sorted(allowed))
            fail(403, f"工站未授权：当前上报 {body.stationKey}，账号已授权 {allowed_text}", 403)

    user.failed_login_count = 0
    user.locked_until = None
    user.last_login_at = datetime.utcnow()
    user.last_login_host = body.hostName
    db.commit()

    token, expire = create_access_token(user.username)
    audit("login", user.id)

    data = LoginResponseData(
        accessToken=token,
        expireAt=expire,
        roles=parse_roles(user.roles),
        stationKeys=parse_station_keys(user.station_keys),
        factoryCode=get_user_factory_scope(db, user) or None,
    )
    return ok(data.model_dump(mode="json"))


@router.get("/me")
def me(
    user: Annotated[User, Depends(get_current_user)],
    db: Annotated[Session, Depends(get_db)],
):
    data = UserMeData(
        username=user.username,
        roles=parse_roles(user.roles),
        stationKeys=parse_station_keys(user.station_keys),
        factoryCode=get_user_factory_scope(db, user) or None,
    )
    return ok(data.model_dump(mode="json"))


@router.post("/logout")
def logout(user: Annotated[User, Depends(get_current_user)]):
    return ok(message="已退出")
