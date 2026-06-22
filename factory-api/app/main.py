"""FastAPI 应用入口。"""

from contextlib import asynccontextmanager

from fastapi import FastAPI, Request
from fastapi.exceptions import HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from app.config import settings
from app.database import SessionLocal, init_db
from app.routers import auth, logs, meta, test_records
from app.routers import thresholds, test_cases, host_app, releases, admin_extra
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
app.include_router(thresholds.router, prefix=settings.api_prefix)
app.include_router(thresholds.admin_router, prefix=settings.api_prefix)
app.include_router(test_cases.router, prefix=settings.api_prefix)
app.include_router(host_app.router, prefix=settings.api_prefix)
app.include_router(host_app.admin_router, prefix=settings.api_prefix)
app.include_router(releases.router, prefix=settings.api_prefix)
app.include_router(admin_extra.router, prefix=settings.api_prefix)


@app.get("/health")
def health():
    return {"status": "ok"}
