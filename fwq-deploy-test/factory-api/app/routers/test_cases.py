"""测试用例：上位机按工站上传草稿 / 下载正式包 + 管理端合并发布 + 版本管理。"""

from typing import Annotated, Any

from fastapi import APIRouter, Body, Depends, File, Form, Query, UploadFile
from fastapi.responses import PlainTextResponse, Response

from app.deps import get_current_user
from app.models import User
from app.response import fail, ok
from app.services import device_runtime
from app.services import test_cases as test_case_service
from app.services import versioning as versioning_service

router = APIRouter(prefix="/test-cases", tags=["test-cases"])
admin_router = APIRouter(prefix="/admin/test-cases", tags=["test-cases-admin"])
device_router = APIRouter(prefix="/device", tags=["device-runtime"])


def _require_engineer_or_admin(user: User) -> None:
    roles = (user.roles or "").split(",")
    if "admin" not in roles and "engineer" not in roles:
        fail(403, "仅 engineer / admin 可访问", 403)


@router.get("/manifest")
def client_manifest(user: Annotated[User, Depends(get_current_user)]):
    """上位机同步：查询当前正式用例包版本与文件清单。"""
    return ok(test_case_service.read_manifest())


@router.get("/bundle")
def client_bundle(user: Annotated[User, Depends(get_current_user)]):
    """上位机同步：下载当前正式用例包（zip，非 JSON 包络）。"""
    zip_bytes = test_case_service.build_bundle_zip()
    if not zip_bytes:
        fail(404, "用例包为空", 404)
    manifest = test_case_service.read_manifest()
    version = manifest.get("bundleVersion") or "bundle"
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="test_case_{version}.zip"'},
    )


@router.post("/upload")
async def client_upload_bundle_deprecated(user: Annotated[User, Depends(get_current_user)]):
    """旧整包上传已停用：请改用按工站 Profile 上传草稿接口。"""
    del user
    fail(
        400,
        "整包上传已停用。请使用 POST /test-cases/profiles/{stationKey}/upload 上传当前工站草稿，"
        "再由网页合入并发布。",
        400,
    )


@router.post("/profiles/{station_key}/upload")
async def client_upload_profile(
    station_key: str,
    user: Annotated[User, Depends(get_current_user)],
    file: Annotated[UploadFile, File()],
    deviceId: Annotated[str | None, Form()] = None,
    hostName: Annotated[str | None, Form()] = None,
    displayName: Annotated[str | None, Form()] = None,
    profileVersion: Annotated[str | None, Form()] = None,
    source: Annotated[str | None, Form()] = "upload",
    remark: Annotated[str | None, Form()] = None,
):
    """上位机上传当前工站 Profile 到 staging 草稿（不自动发布）。"""
    _require_engineer_or_admin(user)
    content = await file.read()
    device_id = (deviceId or "").strip()
    if not device_id:
        fail(400, "deviceId 不能为空", 400)
    try:
        meta = test_case_service.save_profile_staging(
            device_id=device_id,
            station_key=station_key,
            zip_bytes=content,
            display_name=displayName,
            host_name=hostName,
            profile_version=profileVersion,
            source=(source or "upload"),
            remark=remark,
        )
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(meta, message="工站用例草稿已上传，待网页合入发布")


@router.get("/profiles")
def client_list_profiles(user: Annotated[User, Depends(get_current_user)]):
    """上位机下载：列出已发布正式包中可下载的工站。"""
    del user
    bundle = test_case_service.read_manifest()
    return ok(
        {
            "bundleVersion": bundle.get("bundleVersion"),
            "items": test_case_service.list_published_profiles(),
        }
    )


@router.get("/profiles/{station_key}/manifest")
def client_profile_manifest(
    station_key: str,
    user: Annotated[User, Depends(get_current_user)],
    displayName: Annotated[str | None, Query()] = None,
):
    """上位机下载：查询已发布正式包中某工站版本（可带 displayName 消歧）。"""
    del user
    try:
        data = test_case_service.read_profile_manifest(station_key, display_name=displayName)
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(data)


@router.get("/profiles/{station_key}/bundle")
def client_profile_bundle(
    station_key: str,
    user: Annotated[User, Depends(get_current_user)],
    displayName: Annotated[str | None, Query()] = None,
):
    """上位机下载：仅下载已发布正式包中某工站 Profile zip。"""
    del user
    try:
        zip_bytes = test_case_service.build_profile_bundle_zip(station_key, display_name=displayName)
        meta = test_case_service.read_profile_manifest(station_key, display_name=displayName)
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    version = meta.get("profileVersion") or "1"
    name = meta.get("stationKey") or station_key
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="profile_{name}_v{version}.zip"'},
    )


@router.post("/steps/upload")
async def client_upload_steps(
    user: Annotated[User, Depends(get_current_user)],
    file: Annotated[UploadFile, File()],
    deviceId: Annotated[str | None, Form()] = None,
    hostName: Annotated[str | None, Form()] = None,
    libraryVersion: Annotated[str | None, Form()] = None,
    source: Annotated[str | None, Form()] = "upload",
    remark: Annotated[str | None, Form()] = None,
):
    """上位机上传共享步骤库（test_case/steps）到 staging 草稿（不自动发布）。"""
    _require_engineer_or_admin(user)
    content = await file.read()
    device_id = (deviceId or "").strip()
    if not device_id:
        fail(400, "deviceId 不能为空", 400)
    try:
        meta = test_case_service.save_steps_staging(
            device_id=device_id,
            zip_bytes=content,
            host_name=hostName,
            library_version=libraryVersion,
            source=(source or "upload"),
            remark=remark,
        )
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(meta, message="用例库草稿已上传，待网页合入发布")


@router.get("/steps/manifest")
def client_steps_manifest(user: Annotated[User, Depends(get_current_user)]):
    """上位机下载：查询已发布正式包中的共享步骤库版本与文件清单。"""
    del user
    return ok(test_case_service.read_steps_manifest())


@router.get("/steps/bundle")
def client_steps_bundle(user: Annotated[User, Depends(get_current_user)]):
    """上位机下载：仅下载已发布正式包中的共享步骤库 zip。"""
    del user
    try:
        zip_bytes = test_case_service.build_steps_bundle_zip()
        meta = test_case_service.read_steps_manifest()
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    version = meta.get("libraryVersion") or "1"
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="steps_library_v{version}.zip"'},
    )


@device_router.post("/heartbeat")
def device_heartbeat(
    body: dict[str, Any],
    user: Annotated[User, Depends(get_current_user)],
):
    """产线机心跳：更新在线状态。"""
    del user
    device_id = str(body.get("deviceId") or "").strip()
    if not device_id:
        fail(400, "deviceId 不能为空", 400)
    try:
        data = device_runtime.heartbeat(
            device_id,
            host_name=str(body.get("hostName") or "") or None,
            station_key=str(body.get("stationKey") or "") or None,
            station_name=str(body.get("stationName") or "") or None,
            app_version=str(body.get("appVersion") or "") or None,
            stations=body.get("stations") if isinstance(body.get("stations"), list) else None,
            remote_desktop=body.get("remoteDesktop") if "remoteDesktop" in body else None,
        )
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(data)


@device_router.get("/commands")
def device_poll_commands(
    user: Annotated[User, Depends(get_current_user)],
    deviceId: Annotated[str, Query()],
):
    """产线机领取待执行命令（领取即消费）。"""
    del user
    try:
        commands = device_runtime.poll_commands(deviceId)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok({"items": commands})


@admin_router.get("/files")
def list_files(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok(test_case_service.read_manifest())


@admin_router.get("/bundle")
def admin_download_bundle(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    zip_bytes = test_case_service.build_bundle_zip()
    if not zip_bytes:
        fail(404, "用例包为空", 404)
    manifest = test_case_service.read_manifest()
    version = manifest.get("bundleVersion") or "bundle"
    return Response(
        content=zip_bytes,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="test_case_{version}.zip"'},
    )


@admin_router.get("/files/{path:path}")
def get_file(path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        text = test_case_service.read_file_text(path)
    except ValueError:
        fail(400, "非法路径", 400)
    except FileNotFoundError:
        fail(404, "文件不存在", 404)
    return PlainTextResponse(text, media_type="text/plain; charset=utf-8")


@admin_router.put("/files/{path:path}")
def save_file(path: str, content: str = Body(""), user: Annotated[User, Depends(get_current_user)] = None):
    _require_engineer_or_admin(user)
    try:
        test_case_service.write_file_text(path, content)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(message="已保存")


@admin_router.delete("/files/{path:path}")
def remove_file(path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        test_case_service.delete_file(path)
    except ValueError:
        fail(400, "非法路径", 400)
    except FileNotFoundError:
        fail(404, "文件不存在", 404)
    return ok(message="已删除")


@admin_router.get("/staging")
def list_staging(user: Annotated[User, Depends(get_current_user)]):
    """产线上报的工站 Profile 草稿列表。"""
    _require_engineer_or_admin(user)
    return ok({"items": test_case_service.list_profile_staging()})


@admin_router.get("/staging/diff")
def staging_diff(
    user: Annotated[User, Depends(get_current_user)],
    deviceId: Annotated[str, Query()],
    stationKey: Annotated[str, Query()],
):
    """合入前预览：草稿相对工作区当前工站目录的文件/行级差异。"""
    _require_engineer_or_admin(user)
    try:
        data = test_case_service.diff_staging_against_current(deviceId, stationKey)
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(data)


@admin_router.post("/staging/merge")
def merge_staging(body: dict[str, Any], user: Annotated[User, Depends(get_current_user)]):
    """将指定草稿合入 current 工作区（不升正式版本）。可带文件最终内容覆盖。"""
    _require_engineer_or_admin(user)
    device_id = str(body.get("deviceId") or "").strip()
    station_key = str(body.get("stationKey") or "").strip()
    if not device_id or not station_key:
        fail(400, "deviceId 与 stationKey 不能为空", 400)
    overrides_raw = body.get("fileOverrides") or {}
    file_overrides: dict[str, str] = {}
    if isinstance(overrides_raw, dict):
        for k, v in overrides_raw.items():
            file_overrides[str(k)] = "" if v is None else str(v)
    delete_raw = body.get("deletePaths") or []
    delete_paths = [str(x) for x in delete_raw] if isinstance(delete_raw, list) else []
    try:
        data = test_case_service.merge_profile_staging(
            device_id,
            station_key,
            file_overrides=file_overrides or None,
            delete_paths=delete_paths or None,
            merged_by=user.username,
        )
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(data, message="已合入工作区，请确认后发布")


@admin_router.get("/merge-history")
def list_merge_history(
    user: Annotated[User, Depends(get_current_user)],
    limit: Annotated[int, Query()] = 50,
):
    """合入记录列表（含可否撤销）。"""
    _require_engineer_or_admin(user)
    return ok({"items": test_case_service.list_merge_history(limit=limit)})


@admin_router.get("/merge-history/{merge_id}/diff")
def merge_history_diff(merge_id: str, user: Annotated[User, Depends(get_current_user)]):
    """合入记录详情：合入前/后快照的文件与行级差异（只读）。"""
    _require_engineer_or_admin(user)
    try:
        data = test_case_service.diff_merge_history(merge_id)
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(data)


@admin_router.post("/merge-history/{merge_id}/undo")
def undo_merge(merge_id: str, user: Annotated[User, Depends(get_current_user)]):
    """撤销合入：恢复该工站合入前工作区快照。"""
    _require_engineer_or_admin(user)
    try:
        data = test_case_service.undo_merge(merge_id, undone_by=user.username)
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(data, message="已撤销合入，工作区已恢复")


@admin_router.delete("/staging")
def clear_staging(
    user: Annotated[User, Depends(get_current_user)],
    deviceId: Annotated[str, Query()],
    stationKey: Annotated[str, Query()],
):
    """清除产线草稿（无差异或不需要合入时）。"""
    _require_engineer_or_admin(user)
    try:
        data = test_case_service.delete_profile_staging(deviceId, stationKey)
    except FileNotFoundError as exc:
        fail(404, str(exc), 404)
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(data, message="草稿已清除")


@admin_router.get("/online-devices")
def list_online_devices(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok({"items": device_runtime.list_online_devices()})


@admin_router.post("/pull-profile")
def pull_profile(body: dict[str, Any], user: Annotated[User, Depends(get_current_user)]):
    """向在线产线机下发拉取 Profile 命令。"""
    _require_engineer_or_admin(user)
    device_id = str(body.get("deviceId") or "").strip()
    station_key = str(body.get("stationKey") or "").strip()
    if not device_id:
        fail(400, "deviceId 不能为空", 400)
    if not device_runtime.is_online(device_id):
        fail(400, "设备不在线或心跳已过期", 400)
    try:
        cmd = device_runtime.enqueue_command(
            device_id,
            "pull_test_profile",
            {
                "stationKey": station_key,
                "displayName": str(body.get("displayName") or "").strip(),
            },
        )
    except ValueError as exc:
        fail(400, str(exc), 400)
    return ok(cmd, message="已下发拉取命令，等待产线机回传草稿")


@admin_router.get("/versions")
def list_versions(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    return ok(versioning_service.list_versions())


@admin_router.get("/versions/{version}/files")
def get_version_files(version: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        files = versioning_service.get_version_files(version)
    except ValueError as exc:
        fail(404, str(exc), 404)
    return ok({"version": version, "files": files})


@admin_router.get("/versions/{version}/files/{path:path}")
def get_version_file(version: str, path: str, user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    try:
        text = versioning_service.read_version_file(version, path)
    except ValueError:
        fail(400, "非法路径", 400)
    except FileNotFoundError:
        fail(404, "文件不存在", 404)
    return PlainTextResponse(text, media_type="text/plain; charset=utf-8")


@admin_router.get("/versions/diff")
def diff_versions(
    from_: Annotated[str, Query(..., alias="from")],
    to: Annotated[str, Query(...)],
    user: Annotated[User, Depends(get_current_user)],
):
    _require_engineer_or_admin(user)
    try:
        data = versioning_service.diff_versions(from_, to)
    except ValueError as exc:
        fail(404, str(exc), 404)
    return ok(data)


@admin_router.post("/publish")
def publish_bundle(user: Annotated[User, Depends(get_current_user)]):
    _require_engineer_or_admin(user)
    data = test_case_service.publish_bundle()
    return ok({"bundleVersion": data["bundleVersion"]}, message="发布成功")
