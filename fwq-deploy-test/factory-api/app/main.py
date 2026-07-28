"""FastAPI 应用入口。"""

from contextlib import asynccontextmanager

from fastapi import FastAPI, Request
from fastapi.exceptions import HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from app.config import settings
from app.database import SessionLocal, init_db
from app.routers import analytics, auth, logs, meta, test_records
from app.routers import test_cases, host_app, admin_extra, downloads, remote_desktop
from app.seed import seed_admin, seed_factories
from app.services.test_cases import ensure_demo_bundle


@asynccontextmanager
async def lifespan(_app: FastAPI):
    # 初始化存储目录
    _ = settings.storage_path
    init_db()
    db = SessionLocal()
    try:
        seed_factories(db)
        seed_admin(db)
        ensure_demo_bundle()
    finally:
        db.close()
    yield


app = FastAPI(title="路特产线管理平台 API", version="0.1.0", lifespan=lifespan)


class _StripEmptyWebSocketExtensions:
    """ARR 不支持 permessage-deflate；去掉扩展头，避免协商压缩或空头触发 uvicorn 400。"""

    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] == "websocket":
            headers = [
                (k, v)
                for k, v in scope.get("headers", [])
                if k != b"sec-websocket-extensions"
            ]
            scope = {**scope, "headers": headers}
        await self.app(scope, receive, send)


app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origin_list,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.exception_handler(HTTPException)
async def http_exception_handler(_request: Request, exc: HTTPException):
    if isinstance(exc.detail, dict) and "code" in exc.detail:
        return JSONResponse(status_code=exc.status_code, content=exc.detail)
    return JSONResponse(
        status_code=exc.status_code,
        content={"code": exc.status_code, "message": str(exc.detail), "data": None},
    )


app.include_router(auth.router, prefix=settings.api_prefix)
app.include_router(logs.router, prefix=settings.api_prefix)
app.include_router(test_records.router, prefix=settings.api_prefix)
app.include_router(meta.router, prefix=settings.api_prefix)
app.include_router(test_cases.router, prefix=settings.api_prefix)
app.include_router(test_cases.admin_router, prefix=settings.api_prefix)
app.include_router(test_cases.device_router, prefix=settings.api_prefix)
app.include_router(host_app.router, prefix=settings.api_prefix)
app.include_router(host_app.admin_router, prefix=settings.api_prefix)
app.include_router(admin_extra.router, prefix=settings.api_prefix)
app.include_router(analytics.router, prefix=settings.api_prefix)
app.include_router(downloads.router, prefix=settings.api_prefix)
app.include_router(remote_desktop.router, prefix=settings.api_prefix)
app.include_router(remote_desktop.ws_router, prefix=settings.api_prefix)


@app.get("/health")
def health():
    return {"status": "ok"}


# 必须包在最外层：过滤 IIS 注入的空 Sec-WebSocket-Extensions
app = _StripEmptyWebSocketExtensions(app)