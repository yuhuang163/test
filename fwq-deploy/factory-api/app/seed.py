"""启动种子数据。"""

from sqlalchemy.orm import Session

from app.config import settings
from app.models import Factory, User
from app.security import hash_password

# 与上位机 Mes/FACTORY 代码一致；display_name 与 box_base 状态栏/业务称呼对齐
FACTORY_SEED = [
    ("lx", "立讯精密", 10),
    ("xwd", "欣旺达", 20),
    ("hq", "华勤技术", 30),
    ("wks", "伟克森", 40),
    ("ydm", "亚达明", 50),
    ("byd", "比亚迪", 60),
    ("hz", "华庄", 70),
    ("无mes厂", "无MES厂", 80),
]


def seed_factories(db: Session) -> None:
    for code, display_name, sort_order in FACTORY_SEED:
        row = db.get(Factory, code)
        if row:
            row.display_name = display_name
            row.sort_order = sort_order
            row.enabled = True
        else:
            db.add(Factory(code=code, display_name=display_name, sort_order=sort_order, enabled=True))
    db.commit()


def seed_admin(db: Session) -> None:
    username = settings.default_admin_user
    exists = db.query(User).filter(User.username == username).first()
    if exists:
        return
    db.add(
        User(
            username=username,
            password_hash=hash_password(settings.default_admin_password),
            password_plain=settings.default_admin_password,
            roles="admin",
            station_keys="",
            status="active",
        )
    )
    db.commit()


def get_factory_display_name(db: Session, code: str) -> str:
    row = db.get(Factory, code)
    return row.display_name if row else code
