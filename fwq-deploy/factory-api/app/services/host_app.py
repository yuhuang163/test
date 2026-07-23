"""上位机运行环境与安装包存储。"""

import io
import json
import zipfile
from datetime import datetime
from pathlib import Path

from app.config import settings

RUNTIME_ENV_DIR = Path(r"D:\code\new_product_test\路特上位机运行环境")
RUNTIME_ENV_ZIP = "runtime_env.zip"
VERSIONS_MANIFEST = "versions.json"


def _root() -> Path:
    path = settings.storage_path / "host_app"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _runtime_env_zip_path() -> Path:
    return _root() / RUNTIME_ENV_ZIP


def _manifest_path() -> Path:
    return _root() / VERSIONS_MANIFEST


def _load_manifest() -> dict:
    path = _manifest_path()
    if not path.is_file():
        return {"versions": []}
    return json.loads(path.read_text(encoding="utf-8"))


def _save_manifest(data: dict) -> None:
    _manifest_path().write_text(
        json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8"
    )


def storage_root() -> Path:
    return _root()


def _exe_path(package_name: str, build_id: str) -> Path:
    return _root() / f"{package_name}_{build_id}.exe"


def save_build(
    build_id: str,
    content: bytes,
    package_name: str = "new_production",
    app_version: str = "",
    release_notes: str = "",
    force_upgrade: bool = False,
    sha256: str = "",
    gray_rules: dict | None = None,
) -> dict:
    safe_id = build_id.strip()
    if not safe_id:
        raise ValueError("buildId 不能为空")
    pkg = package_name.strip() or "new_production"

    now = datetime.now()
    timestamp = now.strftime("%Y%m%d%H%M%S")

    # 每个版本存为唯一文件名，永不覆盖
    exe_filename = f"{pkg}_{safe_id}_{timestamp}.exe"
    exe_path = _root() / exe_filename
    exe_path.write_bytes(content)
    size = exe_path.stat().st_size

    manifest = _load_manifest()
    versions = manifest["versions"]

    entry = {
        "appVersion": app_version,
        "buildId": safe_id,
        "packageName": pkg,
        "fileName": exe_filename,
        "releaseNotes": release_notes,
        "forceUpgrade": force_upgrade,
        "sha256": sha256,
        "grayRules": gray_rules or {},
        "size": size,
        "uploadedAt": now.isoformat(timespec="seconds"),
    }
    versions.insert(0, entry)

    _save_manifest(manifest)
    return entry


def list_versions() -> list[dict]:
    manifest = _load_manifest()
    return manifest["versions"]


def delete_version(
    build_id: str,
    package_name: str | None = None,
    uploaded_at: str | None = None,
) -> dict:
    """从清单移除一条版本记录，并尽量删除对应 exe 文件。"""
    bid = (build_id or "").strip()
    if not bid:
        raise ValueError("buildId 不能为空")

    manifest = _load_manifest()
    versions = manifest.get("versions") or []
    idx = -1
    for i, v in enumerate(versions):
        if v.get("buildId") != bid:
            continue
        if package_name is not None and (v.get("packageName") or "new_production") != package_name:
            continue
        if uploaded_at and (v.get("uploadedAt") or "") != uploaded_at:
            continue
        idx = i
        break

    if idx < 0:
        raise ValueError("未找到对应版本记录")

    entry = versions.pop(idx)
    _save_manifest(manifest)

    root = _root()
    removed_file = False
    fn = entry.get("fileName")
    if fn:
        path = root / fn
        if path.is_file():
            path.unlink()
            removed_file = True
    else:
        # 旧格式回退
        pkg = entry.get("packageName") or "new_production"
        for candidate in (
            root / f"{pkg}_{bid}.exe",
            root / f"new_production_{bid}.exe",
            root / f"{bid}.exe",
        ):
            if candidate.is_file():
                candidate.unlink()
                removed_file = True
                break

    return {"deleted": entry, "fileRemoved": removed_file}


def get_version(build_id: str, package_name: str | None = None) -> dict | None:
    manifest = _load_manifest()
    for v in manifest["versions"]:
        if v["buildId"] == build_id:
            if package_name is None or v["packageName"] == package_name:
                return v
    return None


def resolve_build_path(build_id: str, package_name: str | None = None,
                       uploaded_at: str | None = None) -> Path | None:
    """找到对应版本记录的 exe 文件（优先用记录的 fileName）。"""
    root = _root()

    # 有 uploadedAt 时，找到精确匹配的记录，用其 fileName
    if uploaded_at:
        for v in list_versions():
            if v["buildId"] == build_id and v.get("uploadedAt") == uploaded_at:
                if package_name is None or v.get("packageName") == package_name:
                    fn = v.get("fileName")
                    if fn:
                        p = root / fn
                        if p.is_file():
                            return p
                break

    # 无 uploadedAt 时，找该 buildId 最新记录的 fileName
    for v in list_versions():
        if v["buildId"] == build_id:
            if package_name is None or v.get("packageName") == package_name:
                fn = v.get("fileName")
                if fn:
                    p = root / fn
                    if p.is_file():
                        return p
                break

    # 旧格式回退
    candidates: list[Path] = []
    if package_name:
        candidates.append(root / f"{package_name}_{build_id}.exe")
    candidates.append(root / f"new_production_{build_id}.exe")
    candidates.append(root / f"{build_id}.exe")
    for path in candidates:
        if path.is_file():
            return path
    return None


def save_runtime_env_zip(content: bytes) -> None:
    """保存上传的运行环境 zip。"""
    _runtime_env_zip_path().write_bytes(content)


def build_runtime_env_zip() -> bytes:
    """返回运行环境 zip（优先返回已上传的，其次从本地目录打包）。"""
    stored = _runtime_env_zip_path()
    if stored.is_file():
        return stored.read_bytes()
    root = RUNTIME_ENV_DIR
    if not root.is_dir():
        raise FileNotFoundError(f"运行环境未上传，且本地目录不存在: {root}")
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(root.rglob("*")):
            if path.is_file():
                rel = path.relative_to(root).as_posix()
                zf.write(path, rel)
    return buf.getvalue()


def get_runtime_env_info() -> dict:
    """返回运行环境的基本信息（从已上传的 zip 或本地目录）。"""
    stored = _runtime_env_zip_path()
    if stored.is_file():
        size = stored.stat().st_size
        import zipfile
        with zipfile.ZipFile(stored, "r") as zf:
            files = [n for n in zf.namelist() if not n.endswith("/")]
        exe_name = next((f for f in files if f.endswith(".exe") and "_" in f), "")
        build_id = exe_name.split("_", 1)[1].rsplit(".", 1)[0] if exe_name else ""
        return {
            "exists": True,
            "fileCount": len(files),
            "sizeBytes": size,
            "buildId": build_id,
        }
    root = RUNTIME_ENV_DIR
    if not root.is_dir():
        return {"exists": False}
    files = [p for p in root.rglob("*") if p.is_file()]
    total_size = sum(p.stat().st_size for p in files)
    exe_files = [p for p in files if p.suffix.lower() == ".exe"]
    build_id = ""
    for exe in exe_files:
        name = exe.stem
        if "_" in name:
            build_id = name.split("_", 1)[1]
            break
    return {
        "exists": True,
        "fileCount": len(files),
        "sizeBytes": total_size,
        "buildId": build_id,
    }
