"""数据库与会话。"""

from collections.abc import Generator
from pathlib import Path

from sqlalchemy import create_engine, text
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
    _migrate_sqlite_schema(engine)
    with SessionLocal() as db:
        from app.factory_scope import backfill_user_factory_codes

        backfill_user_factory_codes(db)


def _migrate_sqlite_schema(eng) -> None:
    """SQLite 轻量补列（create_all 不会改已有表）。"""
    url = str(eng.url)
    if not url.startswith("sqlite"):
        return
    with eng.connect() as conn:
        rows = conn.execute(text("PRAGMA table_info(users)")).fetchall()
        cols = {r[1] for r in rows}
        if "password_plain" not in cols:
            conn.execute(text("ALTER TABLE users ADD COLUMN password_plain VARCHAR(128)"))
            conn.commit()

        user_cols = {r[1] for r in conn.execute(text("PRAGMA table_info(users)")).fetchall()}
        if "factory_code" not in user_cols:
            conn.execute(text("ALTER TABLE users ADD COLUMN factory_code VARCHAR(32)"))
            conn.execute(text("CREATE INDEX IF NOT EXISTS ix_users_factory_code ON users(factory_code)"))
            conn.commit()

        log_cols = {r[1] for r in conn.execute(text("PRAGMA table_info(log_archives)")).fetchall()}
        if "test_record_id" not in log_cols:
            conn.execute(text("ALTER TABLE log_archives ADD COLUMN test_record_id INTEGER"))
            conn.execute(text("CREATE INDEX IF NOT EXISTS ix_log_archives_test_record_id ON log_archives(test_record_id)"))
            conn.commit()
            log_cols = {r[1] for r in conn.execute(text("PRAGMA table_info(log_archives)")).fetchall()}
        if "mac" not in log_cols:
            conn.execute(text("ALTER TABLE log_archives ADD COLUMN mac VARCHAR(64)"))
            conn.execute(text("CREATE INDEX IF NOT EXISTS ix_log_archives_mac ON log_archives(mac)"))
            conn.commit()

        record_cols = {r[1] for r in conn.execute(text("PRAGMA table_info(test_records)")).fetchall()}
        if "mac" not in record_cols:
            conn.execute(text("ALTER TABLE test_records ADD COLUMN mac VARCHAR(64)"))
            conn.execute(text("CREATE INDEX IF NOT EXISTS ix_test_records_mac ON test_records(mac)"))
            conn.commit()
