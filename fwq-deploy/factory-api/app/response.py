"""统一 JSON 响应包络。"""

from typing import Any, Generic, TypeVar

from fastapi import HTTPException
from pydantic import BaseModel

T = TypeVar("T")


class ApiResponse(BaseModel, Generic[T]):
    code: int = 0
    message: str = "ok"
    data: T | None = None


def ok(data: Any = None, message: str = "ok") -> dict:
    return {"code": 0, "message": message, "data": data}


def fail(code: int, message: str, http_status: int = 400) -> None:
    raise HTTPException(status_code=http_status, detail={"code": code, "message": message, "data": None})
