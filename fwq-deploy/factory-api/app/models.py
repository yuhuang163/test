"""ORM 模型。"""

from datetime import datetime

from sqlalchemy import Boolean, DateTime, ForeignKey, Integer, String, Text, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.database import Base


class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    username: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    password_hash: Mapped[str] = mapped_column(String(255))
    password_plain: Mapped[str | None] = mapped_column(String(128), nullable=True)  # 管理员可见，创建/重置时写入
    roles: Mapped[str] = mapped_column(String(255), default="operator")  # 逗号分隔
    station_keys: Mapped[str] = mapped_column(Text, default="")  # 逗号分隔
    status: Mapped[str] = mapped_column(String(16), default="active")
    failed_login_count: Mapped[int] = mapped_column(Integer, default=0)
    locked_until: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now())
    last_login_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    last_login_host: Mapped[str | None] = mapped_column(String(128), nullable=True)


class LoginAudit(Base):
    __tablename__ = "login_audit"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    user_id: Mapped[int | None] = mapped_column(Integer, nullable=True)
    username: Mapped[str] = mapped_column(String(64))
    host_name: Mapped[str | None] = mapped_column(String(128), nullable=True)
    device_id: Mapped[str | None] = mapped_column(String(128), nullable=True)
    station_key: Mapped[str | None] = mapped_column(String(64), nullable=True)
    action: Mapped[str] = mapped_column(String(32))
    ip: Mapped[str | None] = mapped_column(String(64), nullable=True)
    client_version: Mapped[str | None] = mapped_column(String(64), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now())


class Factory(Base):
    __tablename__ = "factories"

    code: Mapped[str] = mapped_column(String(32), primary_key=True)
    display_name: Mapped[str] = mapped_column(String(64))
    sort_order: Mapped[int] = mapped_column(Integer, default=0)
    enabled: Mapped[bool] = mapped_column(Boolean, default=True)


class LogArchive(Base):
    __tablename__ = "log_archives"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    factory_name: Mapped[str] = mapped_column(String(32), index=True)
    device_id: Mapped[str] = mapped_column(String(128), index=True)
    host_name: Mapped[str | None] = mapped_column(String(128), nullable=True)
    station: Mapped[str] = mapped_column(String(128), index=True)
    sn: Mapped[str | None] = mapped_column(String(128), nullable=True)
    test_result: Mapped[str | None] = mapped_column(String(32), nullable=True)
    client_version: Mapped[str | None] = mapped_column(String(64), nullable=True)
    object_key: Mapped[str] = mapped_column(String(512))
    size: Mapped[int] = mapped_column(Integer, default=0)
    file_count: Mapped[int] = mapped_column(Integer, default=0)
    created_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now(), index=True)

    files: Mapped[list["LogFile"]] = relationship(back_populates="archive", cascade="all, delete-orphan")


class LogFile(Base):
    __tablename__ = "log_files"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    archive_id: Mapped[int] = mapped_column(ForeignKey("log_archives.id", ondelete="CASCADE"), index=True)
    relative_path: Mapped[str] = mapped_column(String(512))
    size: Mapped[int] = mapped_column(Integer, default=0)
    content_type: Mapped[str] = mapped_column(String(64), default="application/octet-stream")
    preview_path: Mapped[str | None] = mapped_column(String(512), nullable=True)

    archive: Mapped["LogArchive"] = relationship(back_populates="files")


class TestRecord(Base):
    __tablename__ = "test_records"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    factory_name: Mapped[str] = mapped_column(String(32), index=True)
    device_id: Mapped[str] = mapped_column(String(128), index=True)
    host_name: Mapped[str | None] = mapped_column(String(128), nullable=True)
    station: Mapped[str] = mapped_column(String(128), index=True)
    station_key: Mapped[str | None] = mapped_column(String(64), nullable=True)
    sn: Mapped[str | None] = mapped_column(String(128), index=True)
    test_result: Mapped[str | None] = mapped_column(String(32), nullable=True)
    machine_no: Mapped[str | None] = mapped_column(String(64), nullable=True)
    product: Mapped[str | None] = mapped_column(String(64), nullable=True)
    lot_name: Mapped[str | None] = mapped_column(String(128), nullable=True)
    user_no: Mapped[str | None] = mapped_column(String(64), nullable=True)
    client_version: Mapped[str | None] = mapped_column(String(64), nullable=True)
    tested_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    item_count: Mapped[int] = mapped_column(Integer, default=0)
    created_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now(), index=True)

    items: Mapped[list["TestRecordItem"]] = relationship(
        back_populates="record", cascade="all, delete-orphan"
    )


class TestRecordItem(Base):
    __tablename__ = "test_record_items"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    record_id: Mapped[int] = mapped_column(ForeignKey("test_records.id", ondelete="CASCADE"), index=True)
    name: Mapped[str] = mapped_column(String(128))
    value: Mapped[str | None] = mapped_column(String(256), nullable=True)
    max_value: Mapped[str | None] = mapped_column(String(64), nullable=True)
    min_value: Mapped[str | None] = mapped_column(String(64), nullable=True)
    standard_value: Mapped[str | None] = mapped_column(String(64), nullable=True)
    unit: Mapped[str | None] = mapped_column(String(32), nullable=True)
    result: Mapped[str | None] = mapped_column(String(32), nullable=True)

    record: Mapped["TestRecord"] = relationship(back_populates="items")


class AdminDevice(Base):
    __tablename__ = "admin_devices"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    host_name: Mapped[str] = mapped_column(String(128), unique=True, index=True)
    line_name: Mapped[str | None] = mapped_column(String(64), nullable=True)
    station_label: Mapped[str | None] = mapped_column(String(64), nullable=True)
    remark: Mapped[str | None] = mapped_column(String(256), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now())
    updated_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now())
