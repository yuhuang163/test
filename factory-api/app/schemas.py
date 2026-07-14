"""Pydantic 模式。"""

from datetime import datetime
from typing import Annotated

from pydantic import BaseModel, Field, PlainSerializer

from app.time_util import to_utc_iso_z

# API 输出的 datetime 统一带 Z（UTC），前端再转 Asia/Shanghai 显示
ApiDateTime = Annotated[datetime, PlainSerializer(to_utc_iso_z, return_type=str)]


class LoginRequest(BaseModel):
    username: str
    password: str
    hostName: str = "web-console"
    deviceId: str | None = None
    stationKey: str | None = None


class LoginResponseData(BaseModel):
    accessToken: str
    expireAt: ApiDateTime
    roles: list[str]
    stationKeys: list[str]
    factoryCode: str | None = None


class UserMeData(BaseModel):
    username: str
    roles: list[str]
    stationKeys: list[str]
    factoryCode: str | None = None


class FactoryItem(BaseModel):
    code: str
    displayName: str


class LogFileItem(BaseModel):
    relativePath: str
    size: int
    contentType: str
    previewable: bool


class LogListItem(BaseModel):
    id: int
    factoryName: str
    factoryDisplayName: str
    deviceId: str
    hostName: str | None
    station: str
    sn: str | None
    mac: str | None = None
    testResult: str | None
    clientVersion: str | None
    size: int
    fileCount: int
    createdAt: ApiDateTime


class LogDetailData(LogListItem):
    files: list[LogFileItem]


class LogArchiveSummary(BaseModel):
    id: int
    createdAt: ApiDateTime
    fileCount: int
    size: int
    files: list[LogFileItem]


class LogUploadData(BaseModel):
    logId: int


class LogListData(BaseModel):
    items: list[LogListItem]
    total: int
    page: int
    pageSize: int


class TestRecordItemIn(BaseModel):
    name: str
    value: str | None = None
    maxValue: str | None = None
    minValue: str | None = None
    standardValue: str | None = None
    unit: str | None = None
    result: str | None = None


class TestRecordUploadIn(BaseModel):
    factoryName: str
    deviceId: str
    hostName: str | None = None
    station: str
    stationKey: str | None = None
    sn: str | None = None
    mac: str | None = None
    testResult: str | None = None
    machineNo: str | None = None
    product: str | None = None
    lotName: str | None = None
    userNo: str | None = None
    clientVersion: str | None = None
    testedAt: str | None = None
    items: list[TestRecordItemIn] = Field(default_factory=list)


class TestRecordItemOut(BaseModel):
    name: str
    value: str | None
    maxValue: str | None
    minValue: str | None
    standardValue: str | None
    unit: str | None
    result: str | None


class TestRecordListItem(BaseModel):
    id: int
    factoryName: str
    factoryDisplayName: str
    deviceId: str
    hostName: str | None
    station: str
    stationKey: str | None
    sn: str | None
    mac: str | None = None
    testResult: str | None
    machineNo: str | None
    product: str | None
    clientVersion: str | None
    itemCount: int
    testedAt: ApiDateTime | None
    createdAt: ApiDateTime


class TestRecordDetailData(TestRecordListItem):
    lotName: str | None
    userNo: str | None
    items: list[TestRecordItemOut]
    logArchive: LogArchiveSummary | None = None


class TestRecordListData(BaseModel):
    items: list[TestRecordListItem]
    total: int
    page: int
    pageSize: int


class TestRecordUploadData(BaseModel):
    recordId: int
