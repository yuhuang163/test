"""数据库与会话。"""

from collections.abc import Generator
from pathlib import Path

from sqlalchemy import create_engine
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker

from app.config import settings

BASE_DIR = Path(__file__).resolve().parent.parent


class Base(DeclarativeBase):
    pass


def _resolve_sqlite_url(url: str) -> str:
    if not url.startswith("sqlite:///"):
        return url
    rel = url.replace("sqlite:///", "", 1)
    if rel.startswith("./"):
        abs_path = (BASE_DIR / rel[2:]).resolve()
    else:
        # 处理绝对路径（如 d:/data/factory.db）
        abs_path = Path(rel).resolve()
    # 创建数据库文件所在的目录
    abs_path.parent.mkdir(parents=True, exist_ok=True)
    return f"sqlite:///{abs_path.as_posix()}"


database_url = _resolve_sqlite_url(settings.database_url)
connect_args = {"check_same_thread": False} if database_url.startswith("sqlite") else {}
engine = create_engine(database_url, connect_args=connect_args)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)


def get_db() -> Generator[Session, None, None]:
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


def init_db() -> None:
    from app import models  # noqa: F401

    Base.metadata.create_all(bind=engine)
