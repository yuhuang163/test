"""鉴权与安全工具。"""

from datetime import datetime, timedelta, timezone

from jose import JWTError, jwt
from passlib.context import CryptContext

from app.config import settings

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
ALGORITHM = "HS256"


def hash_password(password: str) -> str:
    return pwd_context.hash(password)


def verify_password(plain: str, hashed: str) -> bool:
    return pwd_context.verify(plain, hashed)


def create_access_token(subject: str, extra: dict | None = None) -> tuple[str, datetime]:
    expire = datetime.now(timezone.utc) + timedelta(hours=settings.access_token_expire_hours)
    payload = {"sub": subject, "exp": expire, **(extra or {})}
    token = jwt.encode(payload, settings.secret_key, algorithm=ALGORITHM)
    return token, expire


def decode_token(token: str) -> dict:
    return jwt.decode(token, settings.secret_key, algorithms=[ALGORITHM])


def parse_roles(raw: str) -> list[str]:
    return [r.strip() for r in raw.split(",") if r.strip()]


def parse_station_keys(raw: str) -> list[str]:
    return [s.strip() for s in raw.split(",") if s.strip()]


# 管理端历史简称与上位机 SYSTEM/station 互认
_STATION_KEY_ALIASES: dict[str, frozenset[str]] = {
    "PCBA": frozenset({"PCBA", "PCBA_TEST"}),
    "PCBA_TEST": frozenset({"PCBA", "PCBA_TEST"}),
    "AGING": frozenset({"AGING", "AGE_TEST"}),
    "AGE_TEST": frozenset({"AGING", "AGE_TEST"}),
    "PACK": frozenset({"PACK", "MAIN_TEST"}),
    "MAIN_TEST": frozenset({"PACK", "MAIN_TEST"}),
}


def station_key_authorized(allowed: set[str], requested: str) -> bool:
    """判断 requested 是否落在 allowed 任一工站（含简称别名）内。"""
    if not allowed:
        return True
    if requested in allowed:
        return True
    req_aliases = _STATION_KEY_ALIASES.get(requested, frozenset({requested}))
    for key in allowed:
        if key in req_aliases:
            return True
        key_aliases = _STATION_KEY_ALIASES.get(key, frozenset({key}))
        if requested in key_aliases:
            return True
    return False
