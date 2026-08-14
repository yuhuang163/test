"""按登录用户限定可见工厂范围。"""

from __future__ import annotations

from sqlalchemy.orm import Session

from app.config import settings
from app.models import Factory, User
from app.response import fail
from app.services.user_batch import factory_username_slug


def infer_factory_code_from_username(db: Session, username: str) -> str | None:
    """从批量账号命名（如 bydop）反推工厂 code。"""
    name = (username or "").strip().lower()
    if not name:
        return None
    if name == settings.default_admin_user.strip().lower():
        return None

    pairs: list[tuple[str, str]] = []
    for row in db.query(Factory).all():
        slug = factory_username_slug(row.code)
        if slug:
            pairs.append((slug, row.code))
    pairs.sort(key=lambda item: len(item[0]), reverse=True)

    for suffix in ("op", "eng", "adm"):
        for slug, code in pairs:
            if name == f"{slug}{suffix}":
                return code
    return None


def get_user_factory_scope(db: Session, user: User) -> str | None:
    """
    返回用户绑定的工厂 code；None 表示可看全部工厂。
    优先级：显式 factory_code > 默认管理员账号 > 用户名推断（批量账号）> 未绑定则全部可见。
    """
    explicit = (getattr(user, "factory_code", None) or "").strip()
    if explicit:
        return explicit

    if user.username.strip().lower() == settings.default_admin_user.strip().lower():
        return None

    inferred = infer_factory_code_from_username(db, user.username)
    if inferred:
        return inferred

    # 手工创建且未绑定工厂：与工站「全部」一致，可查看全部工厂数据
    return None


def is_platform_admin(db: Session, user: User) -> bool:
    return get_user_factory_scope(db, user) is None


def apply_factory_name_filter(q, column, db: Session, user: User, factory_name: str | None):
    scope = get_user_factory_scope(db, user)
    if scope is not None:
        if scope == "":
            return q.filter(column == "__no_factory_access__")
        return q.filter(column == scope)
    if factory_name:
        return q.filter(column == factory_name.strip())
    return q


def assert_factory_access(db: Session, user: User, factory_name: str) -> None:
    scope = get_user_factory_scope(db, user)
    if scope is None:
        return
    if scope == "" or (factory_name or "").strip() != scope:
        fail(403, "无权访问该工厂数据", 403)


def list_visible_factories(db: Session, user: User) -> list[Factory]:
    scope = get_user_factory_scope(db, user)
    q = db.query(Factory).filter(Factory.enabled.is_(True)).order_by(Factory.sort_order)
    if scope is not None:
        if scope == "":
            return []
        q = q.filter(Factory.code == scope)
    return q.all()


def backfill_user_factory_codes(db: Session) -> None:
    """为历史批量账号补写 factory_code。"""
    changed = False
    for user in db.query(User).all():
        if (user.factory_code or "").strip():
            continue
        inferred = infer_factory_code_from_username(db, user.username)
        if inferred:
            user.factory_code = inferred
            changed = True
    if changed:
        db.commit()
