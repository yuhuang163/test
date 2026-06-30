"""按工厂批量生成三类权限账号（账号/密码规则统一、便于产线记忆）。"""

import re
from typing import Any

from sqlalchemy.orm import Session

from app.models import Factory, User
from app.security import hash_password

# 工厂 code 含中文时，用户名使用 ASCII 缩写
_FACTORY_SLUG: dict[str, str] = {
    "无mes厂": "nomes",
}

# (角色, 用户名后缀, 密码后缀)；密码 = 用户名 + pwd_suffix
_ROLE_TEMPLATES: tuple[tuple[str, str, str], ...] = (
    ("operator", "op", "26"),
    ("engineer", "eng", "26"),
    ("admin", "adm", "26"),
)


def factory_username_slug(code: str) -> str:
    code = (code or "").strip()
    if code in _FACTORY_SLUG:
        return _FACTORY_SLUG[code]
    slug = re.sub(r"[^a-z0-9]", "", code.lower())
    return slug or "factory"


def build_batch_account(factory_code: str, factory_name: str, role: str) -> dict[str, Any] | None:
    role = (role or "").strip()
    tpl = next((t for t in _ROLE_TEMPLATES if t[0] == role), None)
    if not tpl:
        return None
    _, user_suffix, pwd_suffix = tpl
    slug = factory_username_slug(factory_code)
    username = f"{slug}{user_suffix}"
    password = f"{username}{pwd_suffix}"
    return {
        "factoryCode": factory_code,
        "factoryName": factory_name,
        "username": username,
        "password": password,
        "roles": [role],
    }


def list_enabled_factories(db: Session, factory_codes: list[str] | None) -> list[Factory]:
    q = db.query(Factory).filter(Factory.enabled.is_(True)).order_by(Factory.sort_order.asc())
    if factory_codes:
        codes = {c.strip() for c in factory_codes if c and c.strip()}
        q = q.filter(Factory.code.in_(codes))
    return q.all()


def preview_batch_accounts(
    db: Session,
    factory_codes: list[str] | None,
    roles: list[str] | None,
) -> dict[str, Any]:
    role_set = {r.strip() for r in (roles or []) if r and r.strip()}
    if not role_set:
        role_set = {t[0] for t in _ROLE_TEMPLATES}

    factories = list_enabled_factories(db, factory_codes)
    existing_names = {u.username for u in db.query(User.username).all()}

    items: list[dict[str, Any]] = []
    for factory in factories:
        for role, _, _ in _ROLE_TEMPLATES:
            if role not in role_set:
                continue
            row = build_batch_account(factory.code, factory.display_name, role)
            if not row:
                continue
            row["exists"] = row["username"] in existing_names
            items.append(row)

    to_create = sum(1 for x in items if not x["exists"])
    return {
        "items": items,
        "summary": {
            "total": len(items),
            "existing": len(items) - to_create,
            "toCreate": to_create,
        },
    }


def import_batch_accounts(
    db: Session,
    factory_codes: list[str] | None,
    roles: list[str] | None,
    station_keys: list[str] | None,
    skip_existing: bool = True,
) -> dict[str, Any]:
    preview = preview_batch_accounts(db, factory_codes, roles)
    stations = ",".join(s for s in (station_keys or []) if s and s.strip())

    created: list[dict[str, Any]] = []
    skipped: list[dict[str, Any]] = []

    for row in preview["items"]:
        if row["exists"]:
            if skip_existing:
                skipped.append({"username": row["username"], "reason": "账号已存在"})
                continue
            skipped.append({"username": row["username"], "reason": "账号已存在且未跳过"})
            continue
        u = User(
            username=row["username"],
            password_hash=hash_password(row["password"]),
            password_plain=row["password"],
            roles=",".join(row["roles"]),
            station_keys=stations,
            status="active",
        )
        db.add(u)
        created.append(
            {
                "factoryCode": row["factoryCode"],
                "factoryName": row["factoryName"],
                "username": row["username"],
                "password": row["password"],
                "roles": row["roles"],
            }
        )

    if created:
        db.commit()
    else:
        db.rollback()

    return {
        "created": created,
        "skipped": skipped,
        "summary": {
            "created": len(created),
            "skipped": len(skipped),
        },
    }
