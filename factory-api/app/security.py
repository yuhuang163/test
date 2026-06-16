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
