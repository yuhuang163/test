"""测试用例 bundle 存储（上位机 manifest / bundle 下载）。"""

import io
import json
import zipfile
from pathlib import Path

from app.config import settings

MANIFEST_NAME = "manifest.json"
DEFAULT_BUNDLE_VERSION = "demo-1"


def _root() -> Path:
    path = settings.storage_path / "test_cases" / "current"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _manifest_path() -> Path:
    return _root() / MANIFEST_NAME


def ensure_demo_bundle() -> None:
    """首次启动写入示例 manifest 与 ini，供上位机联调。"""
    root = _root()
    manifest_path = _manifest_path()
    if manifest_path.exists():
        return
    demo_ini = root / "demo.ini"
    demo_ini.write_text("[Demo]\nName=示例用例\n", encoding="utf-8")
    manifest = {
        "bundleVersion": DEFAULT_BUNDLE_VERSION,
        "files": [{"path": "demo.ini"}],
    }
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")


def read_manifest() -> dict:
    ensure_demo_bundle()
    data = json.loads(_manifest_path().read_text(encoding="utf-8"))
    bundle_version = str(data.get("bundleVersion") or DEFAULT_BUNDLE_VERSION)
    files = data.get("files") or []
    normalized: list[dict[str, str]] = []
    for item in files:
        if isinstance(item, dict) and item.get("path"):
            normalized.append({"path": str(item["path"]).replace("\\", "/")})
    if not normalized:
        root = _root()
        for p in sorted(root.rglob("*")):
            if p.is_file() and p.name != MANIFEST_NAME:
                normalized.append({"path": p.relative_to(root).as_posix()})
    return {"bundleVersion": bundle_version, "files": normalized}


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
    abs_path = (root / rel_path).resolve()
    if not str(abs_path).startswith(str(root.resolve())):
        raise ValueError("非法路径")
    if not abs_path.is_file():
        raise FileNotFoundError(rel_path)
    return abs_path.read_text(encoding="utf-8")


def write_file_text(rel_path: str, content: str) -> None:
    root = _root()
    abs_path = (root / rel_path).resolve()
    if not str(abs_path).startswith(str(root.resolve())):
        raise ValueError("非法路径")
    abs_path.parent.mkdir(parents=True, exist_ok=True)
    abs_path.write_text(content, encoding="utf-8")


def publish_bundle(bundle_version: str) -> dict:
    """管理端发布：刷新 manifest 中的 bundleVersion。"""
    ensure_demo_bundle()
    manifest = read_manifest()
    manifest["bundleVersion"] = bundle_version
    _manifest_path().write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    return manifest
