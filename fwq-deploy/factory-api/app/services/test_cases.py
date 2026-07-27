"""测试用例包存储（网页上传 / 上位机 manifest+用例包下载 / 工站 Profile 草稿）。"""

import hashlib
import io
import json
import re
import shutil
import zipfile
from datetime import datetime, timezone
from pathlib import Path

from app.config import settings
from app.services.versioning import next_version, save_snapshot

MANIFEST_NAME = "manifest.json"
FLOW_INI_NAME = "总的测试流程.ini"
MAX_BUNDLE_MB = 50
PROFILES_DIR_NAME = "profiles"


def _root() -> Path:
    path = settings.storage_path / "test_cases" / "current"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _staging_root() -> Path:
    path = settings.storage_path / "test_cases" / "staging"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _merge_history_root() -> Path:
    path = settings.storage_path / "test_cases" / "merge_history"
    path.mkdir(parents=True, exist_ok=True)
    return path


MERGE_HISTORY_KEEP = 100


def _safe_segment(value: str, *, field: str, allow_cjk: bool = False) -> str:
    text = (value or "").strip()
    if not text:
        raise ValueError(f"{field} 不能为空")
    if ".." in text or "/" in text or "\\" in text:
        raise ValueError(f"非法 {field}")
    if allow_cjk:
        # 工站中文显示名作目录名：禁止 Windows 非法字符
        cleaned = re.sub(r'[<>:"/\\|?*\x00-\x1f]+', "_", text).strip(" .")
    else:
        cleaned = re.sub(r"[^\w.\-]+", "_", text, flags=re.UNICODE)
    if not cleaned or cleaned in {".", ".."}:
        raise ValueError(f"非法 {field}")
    return cleaned


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _read_ini_value(text: str, section: str, key: str) -> str | None:
    current = None
    section_l = section.strip().lower()
    key_l = key.strip().lower()
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].strip().lower()
            continue
        if current != section_l or "=" not in line:
            continue
        k, _, v = line.partition("=")
        if k.strip().lower() == key_l:
            return v.strip().strip('"').strip("'")
    return None


def _parse_profile_version(raw: str | None) -> int:
    try:
        return max(1, int(str(raw or "1").strip()))
    except ValueError:
        return 1


def _write_ini_value(path: Path, section: str, key: str, value: str) -> None:
    """写入/更新 ini 节内键值（UTF-8）；节不存在则追加。"""
    text = path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""
    lines = text.splitlines()
    section_l = section.strip().lower()
    key_l = key.strip().lower()
    out: list[str] = []
    in_section = False
    found_section = False
    replaced = False

    for raw in lines:
        stripped = raw.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            if in_section and not replaced:
                out.append(f"{key}={value}")
                replaced = True
            in_section = stripped[1:-1].strip().lower() == section_l
            if in_section:
                found_section = True
            out.append(raw)
            continue
        if in_section and "=" in stripped and not stripped.startswith("#") and not stripped.startswith(";"):
            k = stripped.split("=", 1)[0].strip()
            if k.lower() == key_l:
                out.append(f"{key}={value}")
                replaced = True
                continue
        out.append(raw)

    if found_section and not replaced:
        rebuilt: list[str] = []
        in_section = False
        inserted = False
        for raw in out:
            stripped = raw.strip()
            if stripped.startswith("[") and stripped.endswith("]"):
                if in_section and not inserted:
                    rebuilt.append(f"{key}={value}")
                    inserted = True
                in_section = stripped[1:-1].strip().lower() == section_l
            rebuilt.append(raw)
        if in_section and not inserted:
            rebuilt.append(f"{key}={value}")
            inserted = True
        out = rebuilt
        replaced = inserted

    if not found_section:
        if out and out[-1].strip():
            out.append("")
        out.append(f"[{section}]")
        out.append(f"{key}={value}")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def _manifest_path() -> Path:
    return _root() / MANIFEST_NAME


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fp:
        for chunk in iter(lambda: fp.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _file_updated_at(path: Path) -> str:
    """文件最近修改时间（UTC ISO，与前端 formatTime 一致）。"""
    mtime = path.stat().st_mtime
    return (
        datetime.fromtimestamp(mtime, tz=timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z")
    )


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
                "updatedAt": _file_updated_at(path),
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
                entry["updatedAt"] = _file_updated_at(abs_path)
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


def _parse_flow_stations(root: Path) -> dict[str, str]:
    """总的测试流程.ini [FlowStations] → stationKey -> displayName。"""
    flow_ini = root / FLOW_INI_NAME
    if not flow_ini.is_file():
        return {}
    mapping: dict[str, str] = {}
    in_section = False
    for raw in flow_ini.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            in_section = line[1:-1].strip().lower() == "flowstations"
            continue
        if not in_section or "=" not in line:
            continue
        key, _, value = line.partition("=")
        k = key.strip()
        v = value.strip().strip('"').strip("'")
        if k and v:
            mapping[k] = v
    return mapping


def resolve_published_profile_dir(station_key: str) -> tuple[Path, str, str]:
    """
    在 current/profiles 中定位已发布工站目录。
    返回 (目录, stationKey, displayName)。
    """
    skey = _safe_segment(station_key, field="stationKey")
    root = _root()
    profiles = root / PROFILES_DIR_NAME
    if not profiles.is_dir():
        raise FileNotFoundError(f"云端尚无工站用例：{skey}")

    flow_map = _parse_flow_stations(root)
    preferred_name = flow_map.get(skey)

    candidates: list[Path] = []
    if preferred_name:
        preferred_path = profiles / preferred_name
        if preferred_path.is_dir():
            candidates.append(preferred_path)
    # 再扫一遍：按 profile.ini 的 StationKey 匹配
    for child in sorted(profiles.iterdir()):
        if not child.is_dir():
            continue
        if child in candidates:
            continue
        meta_path = child / "profile.ini"
        if not meta_path.is_file():
            continue
        text = meta_path.read_text(encoding="utf-8", errors="replace")
        ini_key = _read_ini_value(text, "Profile", "StationKey") or ""
        if ini_key.strip() == skey or child.name == skey:
            candidates.append(child)

    if not candidates and (profiles / skey).is_dir():
        candidates.append(profiles / skey)

    if not candidates:
        raise FileNotFoundError(f"云端尚无工站用例：{skey}（请先在网页合入并发布）")

    profile_dir = candidates[0]
    meta_path = profile_dir / "profile.ini"
    display_name = profile_dir.name
    if meta_path.is_file():
        text = meta_path.read_text(encoding="utf-8", errors="replace")
        display_name = (_read_ini_value(text, "Profile", "DisplayName") or display_name).strip() or display_name
    return profile_dir, skey, display_name


def list_published_profiles() -> list[dict]:
    """已发布正式包中可下载的工站列表（供上位机选择下载）。"""
    root = _root()
    profiles = root / PROFILES_DIR_NAME
    if not profiles.is_dir():
        return []

    flow_map = _parse_flow_stations(root)  # stationKey -> displayName
    name_to_key = {v: k for k, v in flow_map.items()}
    items: list[dict] = []
    seen_keys: set[str] = set()

    for child in sorted(profiles.iterdir(), key=lambda p: p.name):
        if not child.is_dir():
            continue
        meta_path = child / "profile.ini"
        if not meta_path.is_file():
            continue
        text = meta_path.read_text(encoding="utf-8", errors="replace")
        ini_key = (_read_ini_value(text, "Profile", "StationKey") or "").strip()
        display_name = (_read_ini_value(text, "Profile", "DisplayName") or child.name).strip() or child.name
        profile_version = (_read_ini_value(text, "Profile", "ProfileVersion") or "1").strip() or "1"
        skey = ini_key or name_to_key.get(child.name) or name_to_key.get(display_name) or child.name
        if skey in seen_keys:
            continue
        seen_keys.add(skey)
        items.append(
            {
                "stationKey": skey,
                "displayName": display_name,
                "profileVersion": profile_version,
            }
        )

    # FlowStations 中有映射但 profiles 目录缺失的不列入（无法下载）
    return items


def read_profile_manifest(station_key: str) -> dict:
    """已发布正式包中某工站 Profile 的清单（供上位机下载比对）。"""
    profile_dir, skey, display_name = resolve_published_profile_dir(station_key)
    profile_version = "1"
    meta_path = profile_dir / "profile.ini"
    if meta_path.is_file():
        text = meta_path.read_text(encoding="utf-8", errors="replace")
        profile_version = (_read_ini_value(text, "Profile", "ProfileVersion") or "1").strip() or "1"

    files = []
    for path in sorted(profile_dir.rglob("*")):
        if not path.is_file() or not path.name.lower().endswith(".ini"):
            continue
        rel = path.relative_to(profile_dir).as_posix()
        files.append({"path": rel, "sha256": _file_sha256(path)})

    bundle = read_manifest()
    return {
        "stationKey": skey,
        "displayName": display_name,
        "profileVersion": profile_version,
        "bundleVersion": bundle.get("bundleVersion"),
        "fileCount": len(files),
        "files": files,
    }


def build_profile_bundle_zip(station_key: str) -> bytes:
    """打包已发布正式包中某工站 Profile（zip 根即 profile 目录内容）。"""
    profile_dir, _, _ = resolve_published_profile_dir(station_key)
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(profile_dir.rglob("*")):
            if not path.is_file():
                continue
            if not path.name.lower().endswith(".ini"):
                continue
            rel = path.relative_to(profile_dir).as_posix()
            zf.write(path, rel)
    data = buf.getvalue()
    if not data or len(data) < 22:
        raise FileNotFoundError("工站用例包为空")
    return data


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
    """管理端发布：保存快照、刷新文件清单并递增版本号。"""
    ensure_demo_bundle()
    manifest = _load_manifest_raw()
    current = str(manifest.get("bundleVersion") or "")
    new_version = bundle_version or next_version(current)
    manifest["bundleVersion"] = new_version
    manifest["files"] = _scan_files(_root())
    _save_manifest(manifest)
    save_snapshot(new_version)
    return read_manifest()


def _refresh_manifest_files() -> None:
    if not _manifest_path().exists():
        ensure_demo_bundle()
        return
    manifest = _load_manifest_raw()
    manifest["files"] = _scan_files(_root())
    _save_manifest(manifest)


def _staging_dir(device_id: str, station_key: str) -> Path:
    return _staging_root() / _safe_segment(device_id, field="deviceId") / _safe_segment(
        station_key, field="stationKey"
    )


def _extract_profile_zip_to(staging_extract: Path, zip_bytes: bytes) -> tuple[int, str | None, str | None]:
    """解压 Profile zip 到临时目录，返回 (文件数, displayName, profileVersion)。"""
    if staging_extract.exists():
        shutil.rmtree(staging_extract)
    staging_extract.mkdir(parents=True)

    imported = 0
    profile_ini_text: str | None = None
    with zipfile.ZipFile(io.BytesIO(zip_bytes), "r") as zf:
        for info in zf.infolist():
            name = info.filename.replace("\\", "/")
            if info.is_dir() or name.endswith("/"):
                continue
            if name.startswith("__MACOSX/") or "/." in name:
                continue
            rel = name.lstrip("/")
            # 允许 zip 根即 Profile 内容，或带一层 profiles/{name}/
            parts = [p for p in rel.split("/") if p]
            if len(parts) >= 3 and parts[0].lower() == PROFILES_DIR_NAME:
                rel = "/".join(parts[2:])
            elif len(parts) >= 2 and parts[0].lower() == PROFILES_DIR_NAME:
                rel = "/".join(parts[1:])
            if not rel or not _is_safe_rel_path(rel):
                raise ValueError(f"非法路径: {name}")
            if not rel.lower().endswith(".ini"):
                raise ValueError(f"仅允许 .ini 文件: {rel}")
            target = staging_extract / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            data = zf.read(info)
            target.write_bytes(data)
            imported += 1
            if rel.replace("\\", "/").lower() == "profile.ini":
                profile_ini_text = data.decode("utf-8", errors="replace")

    if imported == 0:
        shutil.rmtree(staging_extract, ignore_errors=True)
        raise ValueError("zip 内无 ini 文件")

    display_name = None
    profile_version = None
    if profile_ini_text:
        display_name = _read_ini_value(profile_ini_text, "Profile", "DisplayName")
        profile_version = _read_ini_value(profile_ini_text, "Profile", "ProfileVersion")
    return imported, display_name, profile_version


def save_profile_staging(
    *,
    device_id: str,
    station_key: str,
    zip_bytes: bytes,
    display_name: str | None = None,
    host_name: str | None = None,
    profile_version: str | None = None,
    source: str = "upload",
    remark: str | None = None,
) -> dict:
    """上位机上报工站 Profile 草稿（不发布、不改 current）。"""
    max_bytes = MAX_BUNDLE_MB * 1024 * 1024
    if len(zip_bytes) > max_bytes:
        raise ValueError(f"zip 超过 {MAX_BUNDLE_MB}MB 限制")
    if zip_bytes[:2] != b"PK":
        raise ValueError("请上传 zip 文件")

    did = _safe_segment(device_id, field="deviceId")
    skey = _safe_segment(station_key, field="stationKey")
    target_dir = _staging_dir(did, skey)
    target_dir.mkdir(parents=True, exist_ok=True)

    extract_tmp = target_dir / "_extract"
    imported, ini_display, ini_version = _extract_profile_zip_to(extract_tmp, zip_bytes)

    resolved_display = (display_name or ini_display or skey).strip() or skey
    resolved_version = str(profile_version or ini_version or "1").strip() or "1"
    src_tag = (source or "upload").strip() or "upload"
    if src_tag not in {"upload", "pull"}:
        src_tag = "upload"

    resolved_remark = (remark or "").strip()
    if len(resolved_remark) > 500:
        raise ValueError("上传说明最多 500 字")
    if src_tag == "upload" and not resolved_remark:
        raise ValueError("请填写上传说明")
    if src_tag == "pull" and not resolved_remark:
        resolved_remark = "网页拉取回传"

    zip_path = target_dir / "profile.zip"
    zip_path.write_bytes(zip_bytes)

    # 保留解压副本便于网页预览文件列表
    files_dir = target_dir / "files"
    if files_dir.exists():
        shutil.rmtree(files_dir)
    shutil.move(str(extract_tmp), str(files_dir))

    file_list = []
    for path in sorted(files_dir.rglob("*")):
        if path.is_file():
            file_list.append(path.relative_to(files_dir).as_posix())

    meta = {
        "deviceId": did,
        "stationKey": skey,
        "displayName": resolved_display,
        "hostName": (host_name or did).strip() or did,
        "profileVersion": resolved_version,
        "source": src_tag,
        "remark": resolved_remark,
        "uploadedAt": _utc_now_iso(),
        "fileCount": imported,
        "files": file_list,
    }
    (target_dir / "meta.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")
    return meta


def list_profile_staging() -> list[dict]:
    items: list[dict] = []
    root = _staging_root()
    if not root.exists():
        return items
    for device_dir in sorted(root.iterdir()):
        if not device_dir.is_dir():
            continue
        for station_dir in sorted(device_dir.iterdir()):
            if not station_dir.is_dir():
                continue
            meta_path = station_dir / "meta.json"
            if not meta_path.is_file():
                continue
            try:
                meta = json.loads(meta_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(meta, dict):
                items.append(meta)
    items.sort(key=lambda m: m.get("uploadedAt") or "", reverse=True)
    return items


def get_profile_staging(device_id: str, station_key: str) -> dict:
    target_dir = _staging_dir(device_id, station_key)
    meta_path = target_dir / "meta.json"
    if not meta_path.is_file():
        raise FileNotFoundError("草稿不存在")
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    if not isinstance(meta, dict):
        raise FileNotFoundError("草稿损坏")
    return meta


def delete_profile_staging(device_id: str, station_key: str) -> dict:
    """清除指定产线草稿（无差异或已不需要时）。"""
    target_dir = _staging_dir(device_id, station_key)
    if not target_dir.exists():
        raise FileNotFoundError("草稿不存在")
    meta = {}
    meta_path = target_dir / "meta.json"
    if meta_path.is_file():
        try:
            loaded = json.loads(meta_path.read_text(encoding="utf-8"))
            if isinstance(loaded, dict):
                meta = loaded
        except (OSError, json.JSONDecodeError):
            pass
    shutil.rmtree(target_dir, ignore_errors=True)
    return {
        "deviceId": meta.get("deviceId") or device_id,
        "stationKey": meta.get("stationKey") or station_key,
        "displayName": meta.get("displayName") or "",
        "cleared": True,
    }

def _upsert_flow_station(station_key: str, display_name: str) -> None:
    """确保 总的测试流程.ini [FlowStations] 含有该工站映射。"""
    root = _root()
    flow_path = root / FLOW_INI_NAME
    if flow_path.is_file():
        text = flow_path.read_text(encoding="utf-8")
    else:
        text = ""

    lines = text.splitlines()
    out: list[str] = []
    in_section = False
    found_section = False
    replaced = False
    key_l = station_key.strip().lower()

    for raw in lines:
        stripped = raw.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            if in_section and not replaced:
                out.append(f"{station_key}={display_name}")
                replaced = True
            in_section = stripped[1:-1].strip().lower() == "flowstations"
            if in_section:
                found_section = True
            out.append(raw)
            continue
        if in_section and "=" in stripped and not stripped.startswith("#"):
            k = stripped.split("=", 1)[0].strip()
            if k.lower() == key_l:
                out.append(f"{station_key}={display_name}")
                replaced = True
                continue
        out.append(raw)

    if found_section and not replaced:
        # 插到节尾
        rebuilt: list[str] = []
        in_section = False
        inserted = False
        for raw in out:
            stripped = raw.strip()
            if stripped.startswith("[") and stripped.endswith("]"):
                if in_section and not inserted:
                    rebuilt.append(f"{station_key}={display_name}")
                    inserted = True
                in_section = stripped[1:-1].strip().lower() == "flowstations"
            rebuilt.append(raw)
        if in_section and not inserted:
            rebuilt.append(f"{station_key}={display_name}")
            inserted = True
        out = rebuilt
        replaced = inserted

    if not found_section:
        if out and out[-1].strip():
            out.append("")
        out.append("[FlowStations]")
        out.append(f"{station_key}={display_name}")

    flow_path.write_text("\n".join(out) + "\n", encoding="utf-8")


def merge_profile_staging(
    device_id: str,
    station_key: str,
    file_overrides: dict[str, str] | None = None,
    delete_paths: list[str] | None = None,
    merged_by: str | None = None,
) -> dict:
    """
    将草稿合入 current/profiles/{displayName}/。
    合入前快照工作区，写入合入记录，支持后续撤销。
    """
    meta = get_profile_staging(device_id, station_key)
    display_name = str(meta.get("displayName") or station_key).strip() or station_key
    display_name = _safe_segment(display_name, field="displayName", allow_cjk=True)

    files_dir = _staging_dir(device_id, station_key) / "files"
    if not files_dir.is_dir():
        raise FileNotFoundError("草稿文件缺失")

    root = _root()
    profiles = root / PROFILES_DIR_NAME
    profiles.mkdir(parents=True, exist_ok=True)
    dest = profiles / display_name

    # 正式 ProfileVersion 由服务端在合入时统一递增，避免各电脑本地自增撞号
    old_version = 0
    if dest.is_dir():
        old_ini = dest / "profile.ini"
        if old_ini.is_file():
            old_version = _parse_profile_version(
                _read_ini_value(old_ini.read_text(encoding="utf-8", errors="replace"), "Profile", "ProfileVersion")
            )
    staging_version = _parse_profile_version(str(meta.get("profileVersion") or ""))
    assigned_version = max(old_version, staging_version) + 1

    # 合入前快照（供撤销）
    merge_id = _new_merge_id(display_name)
    history_dir = _merge_history_root() / merge_id
    before_dir = history_dir / "before"
    after_dir = history_dir / "after"
    history_dir.mkdir(parents=True, exist_ok=False)
    had_before = dest.is_dir()
    if had_before:
        shutil.copytree(dest, before_dir)
    else:
        before_dir.mkdir(parents=True, exist_ok=True)

    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(files_dir, dest)

    overrides = file_overrides or {}
    for rel, text in overrides.items():
        rel_norm = str(rel).replace("\\", "/").lstrip("/")
        if not _is_safe_rel_path(rel_norm) or not rel_norm.lower().endswith(".ini"):
            raise ValueError(f"非法覆盖路径: {rel}")
        target = dest / rel_norm
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text if text is not None else "", encoding="utf-8")

    for rel in delete_paths or []:
        rel_norm = str(rel).replace("\\", "/").lstrip("/")
        if not _is_safe_rel_path(rel_norm):
            continue
        target = dest / rel_norm
        if target.is_file():
            target.unlink()

    _write_ini_value(dest / "profile.ini", "Profile", "ProfileVersion", str(assigned_version))

    _upsert_flow_station(str(meta.get("stationKey") or station_key), display_name)
    _refresh_manifest_files()

    # 合入后快照（便于对照记录）
    if after_dir.exists():
        shutil.rmtree(after_dir)
    shutil.copytree(dest, after_dir)

    record = {
        "mergeId": merge_id,
        "deviceId": meta.get("deviceId"),
        "stationKey": meta.get("stationKey") or station_key,
        "displayName": display_name,
        "hostName": meta.get("hostName") or "",
        "source": meta.get("source") or "upload",
        "remark": str(meta.get("remark") or "").strip(),
        "mergedAt": _utc_now_iso(),
        "mergedBy": (merged_by or "").strip(),
        "beforeProfileVersion": str(old_version) if had_before and old_version else ("—" if not had_before else "0"),
        "afterProfileVersion": str(assigned_version),
        "hadBefore": had_before,
        "fileCountBefore": len(list(before_dir.rglob("*.ini"))) if had_before else 0,
        "fileCountAfter": len(list(dest.rglob("*.ini"))),
        "overrideCount": len(overrides),
        "undone": False,
        "undoneAt": None,
        "undoneBy": "",
    }
    (history_dir / "meta.json").write_text(
        json.dumps(record, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    _prune_merge_history()

    try:
        delete_profile_staging(device_id, station_key)
    except FileNotFoundError:
        pass

    return {
        "deviceId": meta.get("deviceId"),
        "stationKey": meta.get("stationKey"),
        "displayName": display_name,
        "profileVersion": str(assigned_version),
        "mergedPath": f"{PROFILES_DIR_NAME}/{display_name}",
        "fileCount": len(list(dest.rglob("*.ini"))),
        "bundleVersion": read_manifest().get("bundleVersion"),
        "overrideCount": len(overrides),
        "mergeId": merge_id,
    }


def _new_merge_id(display_name: str) -> str:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    safe = re.sub(r"[^\w.\-]+", "_", display_name, flags=re.UNICODE)[:40] or "station"
    # 同秒多次合入时加短哈希区分
    suffix = hashlib.md5(f"{stamp}:{display_name}:{_utc_now_iso()}".encode("utf-8")).hexdigest()[:6]
    return f"{stamp}_{safe}_{suffix}"


def _read_merge_meta(merge_id: str) -> dict:
    merge_id = _safe_segment(merge_id, field="mergeId", allow_cjk=True)
    meta_path = _merge_history_root() / merge_id / "meta.json"
    if not meta_path.is_file():
        raise FileNotFoundError("合入记录不存在")
    try:
        data = json.loads(meta_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise FileNotFoundError("合入记录损坏") from exc
    if not isinstance(data, dict):
        raise FileNotFoundError("合入记录损坏")
    data["mergeId"] = merge_id
    return data


def _write_merge_meta(merge_id: str, meta: dict) -> None:
    merge_id = _safe_segment(merge_id, field="mergeId", allow_cjk=True)
    meta_path = _merge_history_root() / merge_id / "meta.json"
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")


def list_merge_history(limit: int = 50) -> list[dict]:
    """合入记录列表（新→旧）。"""
    root = _merge_history_root()
    items: list[dict] = []
    for child in root.iterdir():
        if not child.is_dir():
            continue
        meta_path = child / "meta.json"
        if not meta_path.is_file():
            continue
        try:
            data = json.loads(meta_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(data, dict):
            continue
        data["mergeId"] = data.get("mergeId") or child.name
        # 仅最新一条未撤销记录对该工站可撤销
        items.append(data)
    items.sort(key=lambda x: str(x.get("mergedAt") or ""), reverse=True)

    # 标注 canUndo：同 displayName 下最新一条未撤销记录
    seen_active: set[str] = set()
    for item in items:
        key = str(item.get("displayName") or item.get("stationKey") or "")
        can = False
        if key and not item.get("undone") and key not in seen_active:
            can = True
            seen_active.add(key)
        item["canUndo"] = can

    return items[: max(1, min(int(limit or 50), 200))]


def _prune_merge_history() -> None:
    root = _merge_history_root()
    dirs = [p for p in root.iterdir() if p.is_dir()]
    if len(dirs) <= MERGE_HISTORY_KEEP:
        return
    dirs.sort(key=lambda p: p.name, reverse=True)
    for old in dirs[MERGE_HISTORY_KEEP:]:
        shutil.rmtree(old, ignore_errors=True)


def undo_merge(merge_id: str, undone_by: str | None = None) -> dict:
    """
    撤销合入：将工作区该工站恢复为合入前快照。
    仅允许撤销该工站最新一条未撤销记录，避免旧记录覆盖新编辑。
    """
    meta = _read_merge_meta(merge_id)
    if meta.get("undone"):
        raise ValueError("该合入已撤销")

    display_name = str(meta.get("displayName") or "").strip()
    if not display_name:
        raise ValueError("合入记录缺少工站名")
    display_name = _safe_segment(display_name, field="displayName", allow_cjk=True)

    # 必须是该工站最新未撤销记录
    latest = None
    for item in list_merge_history(limit=200):
        if str(item.get("displayName") or "") == display_name and not item.get("undone"):
            latest = item
            break
    if not latest or latest.get("mergeId") != meta.get("mergeId"):
        raise ValueError("只能撤销该工站最近一次合入；若之后又有合入，请先撤销更新的记录")

    history_dir = _merge_history_root() / meta["mergeId"]
    before_dir = history_dir / "before"
    dest = _root() / PROFILES_DIR_NAME / display_name

    if dest.exists():
        shutil.rmtree(dest)

    had_before = bool(meta.get("hadBefore")) and before_dir.is_dir() and any(before_dir.iterdir())
    if had_before:
        shutil.copytree(before_dir, dest)
    # 若合入前不存在该工站，撤销后删除目录即可（上面已 rmtree）

    _refresh_manifest_files()

    meta["undone"] = True
    meta["undoneAt"] = _utc_now_iso()
    meta["undoneBy"] = (undone_by or "").strip()
    _write_merge_meta(meta["mergeId"], meta)

    return {
        "mergeId": meta["mergeId"],
        "displayName": display_name,
        "stationKey": meta.get("stationKey"),
        "restored": had_before,
        "beforeProfileVersion": meta.get("beforeProfileVersion"),
        "undoneAt": meta["undoneAt"],
    }


def _list_ini_rel_paths(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    out: set[str] = set()
    for path in root.rglob("*"):
        if path.is_file() and path.name.lower().endswith(".ini"):
            out.add(path.relative_to(root).as_posix())
    return out


def _read_text_or_empty(path: Path) -> str:
    if not path.is_file():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def diff_staging_against_current(device_id: str, station_key: str) -> dict:
    """
    合入前预览：草稿 vs 工作区当前 profiles/{displayName}/。
    附带各文件全文，供左右对比与编辑最终内容。
    """
    from app.services.versioning import _compute_line_diff

    meta = get_profile_staging(device_id, station_key)
    display_name = str(meta.get("displayName") or station_key).strip() or station_key
    display_name = _safe_segment(display_name, field="displayName", allow_cjk=True)

    staging_files = _staging_dir(device_id, station_key) / "files"
    if not staging_files.is_dir():
        raise FileNotFoundError("草稿文件缺失")

    current_dir = _root() / PROFILES_DIR_NAME / display_name
    if not current_dir.is_dir():
        try:
            resolved, _, resolved_name = resolve_published_profile_dir(str(meta.get("stationKey") or station_key))
            current_dir = resolved
            display_name = resolved_name or display_name
        except FileNotFoundError:
            pass

    old_paths = _list_ini_rel_paths(current_dir)
    new_paths = _list_ini_rel_paths(staging_files)
    all_paths = sorted(old_paths | new_paths)

    file_diffs: list[dict] = []
    changed_diffs: dict[str, list] = {}
    contents: dict[str, dict[str, str]] = {}
    for rel in all_paths:
        old_text = _read_text_or_empty(current_dir / rel)
        new_text = _read_text_or_empty(staging_files / rel)
        contents[rel] = {"current": old_text, "staging": new_text}

        in_old = rel in old_paths
        in_new = rel in new_paths
        if in_old and not in_new:
            file_diffs.append({"path": rel, "status": "removed"})
            continue
        if in_new and not in_old:
            file_diffs.append({"path": rel, "status": "added"})
            continue
        if old_text == new_text:
            file_diffs.append({"path": rel, "status": "unchanged"})
        else:
            file_diffs.append({"path": rel, "status": "changed"})
            changed_diffs[rel] = _compute_line_diff(old_text, new_text)

    current_version = "—"
    if current_dir.is_dir() and (current_dir / "profile.ini").is_file():
        current_version = (
            _read_ini_value(_read_text_or_empty(current_dir / "profile.ini"), "Profile", "ProfileVersion") or "—"
        )

    return {
        "meta": meta,
        "displayName": display_name,
        "remark": str(meta.get("remark") or "").strip(),
        "currentExists": current_dir.is_dir(),
        "currentPath": f"{PROFILES_DIR_NAME}/{display_name}" if current_dir.is_dir() else "",
        "currentProfileVersion": current_version,
        "stagingProfileVersion": meta.get("profileVersion"),
        "addedCount": sum(1 for f in file_diffs if f["status"] == "added"),
        "removedCount": sum(1 for f in file_diffs if f["status"] == "removed"),
        "changedCount": sum(1 for f in file_diffs if f["status"] == "changed"),
        "unchangedCount": sum(1 for f in file_diffs if f["status"] == "unchanged"),
        "files": file_diffs,
        "fileDiffs": changed_diffs,
        "contents": contents,
    }
