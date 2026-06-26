"""路特产线管理平台 API 配置。"""

from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", extra="ignore")

    secret_key: str = "dev-secret-change-me"
    database_url: str = "sqlite:///./data/factory.db"
    storage_dir: str = "./data/storage"
    log_upload_max_mb: int = 500
    log_upload_allow_anonymous: bool = True
    cors_origins: str = "http://localhost:5173,http://127.0.0.1:5173"
    access_token_expire_hours: int = 12
    default_admin_user: str = "admin"
    default_admin_password: str = "admin123"
    api_prefix: str = "/api/factory-tool"

    @property
    def cors_origin_list(self) -> list[str]:
        return [o.strip() for o in self.cors_origins.split(",") if o.strip()]

    @property
    def storage_path(self) -> Path:
        path = Path(self.storage_dir)
        # 如果不是绝对路径，则相对于项目根目录
        if not path.is_absolute():
            path = Path(__file__).resolve().parent.parent / path
        path.mkdir(parents=True, exist_ok=True)
        return path


settings = Settings()
