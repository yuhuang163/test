"""云端配置版本号：yyyyMMdd-NNN + 版本快照管理。"""

import difflib
import json
import shutil
from datetime import datetime
from pathlib import Path

from app.config import settings


def next_version(current: str | None) -> str:
    today = datetime.now().strftime("%Y%m%d")
    if current:
        parts = current.split("-", 1)
        if len(parts) == 2 and parts[0] == today and parts[1].isdigit():
            return f"{today}-{int(parts[1]) + 1:03d}"
    return f"{today}-001"


MANIFEST_NAME = "manifest.json"


def _versions_root() -> Path:
    path = settings.storage_path / "test_cases" / "versions"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _current_root() -> Path:
    return settings.storage_path / "test_cases" / "current"


def _version_dir(version: str) -> Path:
    return _versions_root() / version


def save_snapshot(version: str) -> None:
    """发布时将当前文件快照保存到 versions/{version}/。"""
    src = _current_root()
    dst = _version_dir(version)
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True)
    for item in src.iterdir():
        if item.is_file():
            if item.name == MANIFEST_NAME:
                manifest = json.loads(item.read_text(encoding="utf-8"))
                manifest["bundleVersion"] = version
                dst.joinpath(MANIFEST_NAME).write_text(
                    json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
                )
            else:
                shutil.copy2(item, dst / item.name)
        elif item.is_dir():
            shutil.copytree(item, dst / item.name)


def list_versions() -> list[dict]:
    """返回所有历史版本，按时间降序。"""
    root = _versions_root()
    versions = []
    for entry in sorted(root.iterdir(), reverse=True):
        if entry.is_dir():
            manifest_path = entry / MANIFEST_NAME
            manifest = {}
            if manifest_path.is_file():
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            files = manifest.get("files") or []
            versions.append({
                "version": entry.name,
                "fileCount": len(files),
                "createdAt": datetime.fromtimestamp(
                    (manifest_path.stat().st_mtime) if manifest_path.is_file() else entry.stat().st_mtime
                ).isoformat(),
            })
    return versions


def get_version_files(version: str) -> list[dict]:
    """获取指定版本的文件清单。"""
    vdir = _version_dir(version)
    if not vdir.is_dir():
        raise ValueError(f"版本 {version} 不存在")
    manifest_path = vdir / MANIFEST_NAME
    if manifest_path.is_file():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        return manifest.get("files") or []
    files = []
    for path in sorted(vdir.rglob("*")):
        if path.is_file() and path.name != MANIFEST_NAME:
            rel = path.relative_to(vdir).as_posix()
            files.append({"path": rel, "version": "1"})
    return files


def read_version_file(version: str, rel_path: str) -> str:
    """读取指定版本中的文件内容。"""
    vdir = _version_dir(version)
    if not vdir.is_dir():
        raise ValueError(f"版本 {version} 不存在")
    rel_path = rel_path.replace("\\", "/")
    if not rel_path or rel_path.startswith("..") or "/.." in rel_path:
        raise ValueError("非法路径")
    abs_path = (vdir / rel_path).resolve()
    if not str(abs_path).startswith(str(vdir.resolve())):
        raise ValueError("非法路径")
    if not abs_path.is_file():
        raise FileNotFoundError(rel_path)
    return abs_path.read_text(encoding="utf-8")


def diff_versions(from_version: str, to_version: str) -> dict:
    """比较两个版本的差异。"""
    from_files = {f["path"]: f for f in get_version_files(from_version)}
    to_files = {f["path"]: f for f in get_version_files(to_version)}

    all_paths = sorted(set(from_files) | set(to_files))
    file_diffs = []
    changed_diffs = {}

    for path in all_paths:
        in_from = path in from_files
        in_to = path in to_files
        if in_from and not in_to:
            file_diffs.append({"path": path, "status": "removed"})
        elif not in_from and in_to:
            file_diffs.append({"path": path, "status": "added"})
        else:
            from_f = from_files[path]
            to_f = to_files[path]
            sha_changed = from_f.get("sha256") != to_f.get("sha256")
            if sha_changed:
                file_diffs.append({"path": path, "status": "changed"})
                try:
                    old_text = read_version_file(from_version, path)
                    new_text = read_version_file(to_version, path)
                    changed_diffs[path] = _compute_line_diff(old_text, new_text)
                except (FileNotFoundError, ValueError):
                    changed_diffs[path] = []
            else:
                file_diffs.append({"path": path, "status": "unchanged"})

    return {
        "from": from_version,
        "to": to_version,
        "fileCount": len(file_diffs),
        "addedCount": sum(1 for f in file_diffs if f["status"] == "added"),
        "removedCount": sum(1 for f in file_diffs if f["status"] == "removed"),
        "changedCount": sum(1 for f in file_diffs if f["status"] == "changed"),
        "unchangedCount": sum(1 for f in file_diffs if f["status"] == "unchanged"),
        "files": file_diffs,
        "fileDiffs": changed_diffs,
    }


def _compute_line_diff(old_text: str, new_text: str) -> list[dict]:
    old_lines = old_text.splitlines(keepends=True)
    new_lines = new_text.splitlines(keepends=True)
    differ = difflib.Differ()
    result = []
    for line in differ.compare(old_lines, new_lines):
        if line.startswith("  "):
            result.append({"type": "ctx", "text": line[2:]})
        elif line.startswith("- "):
            result.append({"type": "del", "text": line[2:]})
        elif line.startswith("+ "):
            result.append({"type": "add", "text": line[2:]})
    return result
