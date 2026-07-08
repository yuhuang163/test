"""依赖注入。"""

from typing import Annotated

from fastapi import Depends, Header, Request
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from jose import JWTError
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import User
from app.response import fail
from app.security import decode_token

bearer_scheme = HTTPBearer(auto_error=False)


def get_optional_user(
    credentials: Annotated[HTTPAuthorizationCredentials | None, Depends(bearer_scheme)],
    db: Annotated[Session, Depends(get_db)],
) -> User | None:
    if not credentials:
        return None
    try:
        payload = decode_token(credentials.credentials)
        username = payload.get("sub")
        if not username:
            return None
        user = db.query(User).filter(User.username == username, User.status == "active").first()
        return user
    except JWTError:
        return None


def get_current_user(
    user: Annotated[User | None, Depends(get_optional_user)],
) -> User:
    if not user:
        fail(401, "未登录或 Token 无效", 401)
    return user


def require_roles(*roles: str):
    def checker(user: Annotated[User, Depends(get_current_user)]) -> User:
        user_roles = set((user.roles or "").split(","))
        if not user_roles.intersection(roles) and "admin" not in user_roles:
            fail(403, "无权限", 403)
        return user

    return checker


def client_ip(request: Request) -> str | None:
    forwarded = request.headers.get("x-forwarded-for")
    if forwarded:
        return forwarded.split(",")[0].strip()
    if request.client:
        return request.client.host
    return None
