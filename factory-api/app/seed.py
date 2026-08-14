"""启动种子数据。"""

from sqlalchemy.orm import Session

from app.config import settings
from app.models import Factory, User
from app.security import hash_password

# 启动时种子数据（仅补全缺失项）；运行中新增工厂请用管理端「工厂管理」，无需重启服务。
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
    """仅补全缺失工厂；已存在的不覆盖 enabled（避免重启把失能工厂重新打开）。"""
    for code, display_name, sort_order in FACTORY_SEED:
        row = db.get(Factory, code)
        if row:
            # 同步展示名与排序，保留管理员设置的启用/失能状态
            row.display_name = display_name
            row.sort_order = sort_order
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
