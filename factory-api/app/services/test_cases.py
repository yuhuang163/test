"""测试用例 bundle 存储（网页上传 / 上位机 manifest+bundle 下载）。"""

import hashlib
import io
import json
import re
import shutil
import zipfile
from pathlib import Path

from app.config import settings
from app.services.versioning import next_version

MANIFEST_NAME = "manifest.json"
FLOW_INI_NAME = "总的测试流程.ini"
MAX_BUNDLE_MB = 50


def _root() -> Path:
    path = settings.storage_path / "test_cases" / "current"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _manifest_path() -> Path:
    return _root() / MANIFEST_NAME


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fp:
        for chunk in iter(lambda: fp.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _is_safe_rel_path(rel: str) -> bool:
    rel = rel.replace("\\", "/").strip().lstrip("/")
    if not rel or rel.startswith("..") or "/.." in rel:
        return False
    return True


def _scan_files(root: Path) -> list[dict[str, str]]:
    files: list[dict[str, str]] = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.name == MANIFEST_NAME:
            continue
        rel = path.relative_to(root).as_posix()
        files.append(
            {
                "path": rel,
                "version": "1",
                "sha256": _file_sha256(path),
            }
        )
    return files


def _parse_station_flows(root: Path) -> dict[str, dict[str, list[str]]]:
    """从 总的测试流程.ini [Station] 解析各工站 Items。"""
    flow_ini = root / FLOW_INI_NAME
    if not flow_ini.is_file():
        return {}
    items_re = re.compile(r"^(.+?)[\\/]Items=(.+)$", re.IGNORECASE)
    flows: dict[str, dict[str, list[str]]] = {}
    for raw_line in flow_ini.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith("["):
            continue
        match = items_re.match(line)
        if not match:
            continue
        station = match.group(1).strip()
        items_str = match.group(2).strip().strip('"').strip("'")
        items = [part.strip() for part in items_str.split(",") if part.strip()]
        flows[station] = {"items": items}
    return flows


def ensure_demo_bundle() -> None:
    """首次启动写入示例 manifest 与 ini，供联调。"""
    root = _root()
    manifest_path = _manifest_path()
    if manifest_path.exists():
        return
    demo_ini = root / "demo.ini"
    demo_ini.write_text("[Demo]\nName=示例用例\n", encoding="utf-8")
    bundle_version = next_version(None)
    manifest = {
        "bundleVersion": bundle_version,
        "files": _scan_files(root),
    }
    _save_manifest(manifest)


def _load_manifest_raw() -> dict:
    ensure_demo_bundle()
    return json.loads(_manifest_path().read_text(encoding="utf-8"))


def _save_manifest(manifest: dict) -> None:
    _manifest_path().write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")


def read_manifest() -> dict:
    manifest = _load_manifest_raw()
    root = _root()
    bundle_version = str(manifest.get("bundleVersion") or next_version(None))
    files = manifest.get("files") or []
    normalized: list[dict[str, str]] = []
    for item in files:
        if isinstance(item, dict) and item.get("path"):
            rel = str(item["path"]).replace("\\", "/")
            abs_path = root / rel
            entry = {"path": rel}
            if abs_path.is_file():
                entry["version"] = str(item.get("version") or "1")
                entry["sha256"] = item.get("sha256") or _file_sha256(abs_path)
                normalized.append(entry)
    if not normalized:
        normalized = _scan_files(root)
    return {
        "bundleVersion": bundle_version,
        "stationFlows": _parse_station_flows(root),
        "files": normalized,
    }


def build_bundle_zip() -> bytes:
    manifest = read_manifest()
    root = _root()
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        for item in manifest["files"]:
            rel = item["path"]
            abs_path = root / rel
            if abs_path.is_file():
                zf.write(abs_path, rel.replace("\\", "/"))
    return buf.getvalue()


def bundle_root() -> Path:
    return _root()


def read_file_text(rel_path: str) -> str:
    root = _root()
    rel_path = rel_path.replace("\\", "/")
    if not _is_safe_rel_path(rel_path):
        raise ValueError("非法路径")
    abs_path = (root / rel_path).resolve()
    if not str(abs_path).startswith(str(root.resolve())):
        raise ValueError("非法路径")
    if not abs_path.is_file():
        raise FileNotFoundError(rel_path)
    return abs_path.read_text(encoding="utf-8")


def write_file_text(rel_path: str, content: str) -> None:
    root = _root()
    rel_path = rel_path.replace("\\", "/")
    if not _is_safe_rel_path(rel_path):
        raise ValueError("非法路径")
    if not rel_path.lower().endswith(".ini"):
        raise ValueError("仅允许 .ini 文件")
    abs_path = (root / rel_path).resolve()
    if not str(abs_path).startswith(str(root.resolve())):
        raise ValueError("非法路径")
    abs_path.parent.mkdir(parents=True, exist_ok=True)
    abs_path.write_text(content, encoding="utf-8")
    _refresh_manifest_files()


def delete_file(rel_path: str) -> None:
    root = _root()
    rel_path = rel_path.replace("\\", "/")
    if not _is_safe_rel_path(rel_path):
        raise ValueError("非法路径")
    abs_path = (root / rel_path).resolve()
    if not str(abs_path).startswith(str(root.resolve())):
        raise ValueError("非法路径")
    if not abs_path.is_file():
        raise FileNotFoundError(rel_path)
    abs_path.unlink()
    _refresh_manifest_files()


def import_bundle_zip(zip_bytes: bytes) -> dict:
    """管理端上传 zip：解压并覆盖当前 test_case 文件（不自动升版本，须再点发布）。"""
    max_bytes = MAX_BUNDLE_MB * 1024 * 1024
    if len(zip_bytes) > max_bytes:
        raise ValueError(f"zip 超过 {MAX_BUNDLE_MB}MB 限制")
    if zip_bytes[:2] != b"PK":
        raise ValueError("请上传 zip 文件")

    root = _root()
    staging = root.parent / "_upload_staging"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    imported = 0
    with zipfile.ZipFile(io.BytesIO(zip_bytes), "r") as zf:
        for info in zf.infolist():
            name = info.filename.replace("\\", "/")
            if info.is_dir() or name.endswith("/"):
                continue
            if name.startswith("__MACOSX/") or "/." in name:
                continue
            rel = name.lstrip("/")
            if not _is_safe_rel_path(rel):
                raise ValueError(f"非法路径: {name}")
            if not rel.lower().endswith(".ini"):
                raise ValueError(f"仅允许 .ini 文件: {rel}")
            target = staging / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(zf.read(info))
            imported += 1

    if imported == 0:
        shutil.rmtree(staging, ignore_errors=True)
        raise ValueError("zip 内无 ini 文件")

    # 清空当前目录（保留 manifest.json，上传内容覆盖业务 ini）
    for path in list(root.iterdir()):
        if path.name == MANIFEST_NAME:
            continue
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()

    for path in staging.rglob("*"):
        if path.is_file():
            rel = path.relative_to(staging)
            dest = root / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest)

    shutil.rmtree(staging, ignore_errors=True)
    _refresh_manifest_files()
    manifest = read_manifest()
    manifest["importedCount"] = imported
    return manifest


def publish_bundle(bundle_version: str | None = None) -> dict:
    """管理端发布：刷新文件清单并递增 bundleVersion。"""
    ensure_demo_bundle()
    manifest = _load_manifest_raw()
    current = str(manifest.get("bundleVersion") or "")
    manifest["bundleVersion"] = bundle_version or next_version(current)
    manifest["files"] = _scan_files(_root())
    _save_manifest(manifest)
    return read_manifest()


def _refresh_manifest_files() -> None:
    if not _manifest_path().exists():
        ensure_demo_bundle()
        return
    manifest = _load_manifest_raw()
    manifest["files"] = _scan_files(_root())
    _save_manifest(manifest)
