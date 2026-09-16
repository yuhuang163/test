"""使用 lark-whiteboard DSL 渲染 + 上传 PNG 到飞书画板（保证内容与本地预览一致）。"""
import json
import pathlib
import re
import subprocess
import time
from typing import Any, Dict, List, Optional

import httpx

BASE = "https://open.feishu.cn/open-apis"
TOKEN_PATH = pathlib.Path.home() / ".feishu-docx" / "tenant_token.json"
SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
DIAGRAM_DIR = SCRIPT_DIR / "diagrams" / "new_product_architecture"
DIAGRAM_JSON = DIAGRAM_DIR / "diagram.json"
DIAGRAM_PNG = DIAGRAM_DIR / "diagram.png"
OPENAPI_JSON = DIAGRAM_DIR / "diagram.openapi.json"
WHITEBOARD_CLI = "@larksuite/whiteboard-cli@0.2.13"


def load_token() -> str:
    if not TOKEN_PATH.exists():
        raise RuntimeError(f"未找到 token：{TOKEN_PATH}")
    return json.loads(TOKEN_PATH.read_text(encoding="utf-8"))["token"]


def api(method: str, url: str, token: str, **kwargs):
    headers = {"Authorization": f"Bearer {token}"}
    if "json" in kwargs:
        headers["Content-Type"] = "application/json; charset=utf-8"
    data = httpx.request(method, url, headers=headers, timeout=120, **kwargs).json()
    if data.get("code") != 0:
        raise RuntimeError(f"{method} {url} failed: {data}")
    return data["data"]


def run(cmd: list[str], cwd: Optional[pathlib.Path] = None) -> str:
    kwargs: Dict[str, Any] = {
        "capture_output": True,
        "text": True,
        "encoding": "utf-8",
        "errors": "replace",
    }
    if cwd:
        kwargs["cwd"] = str(cwd)
    if cmd and cmd[0] in ("npx", "feishu-docx"):
        kwargs["shell"] = True
        proc = subprocess.run(subprocess.list2cmdline(cmd), **kwargs)
    else:
        proc = subprocess.run(cmd, **kwargs)
    text = (proc.stdout or "") + (proc.stderr or "")
    if proc.returncode != 0:
        raise RuntimeError(f"命令失败 {' '.join(cmd)}:\n{text}")
    return text


def create_doc(title: str) -> str:
    cmd = [
        "feishu-docx",
        "create",
        title,
        "-c",
        (
            f"# {title}\n\n"
            "由 **lark-whiteboard DSL** 渲染为高清图写入画板，内容与本地 `diagram.png` 一致。\n\n"
            "源文件：`scripts/diagrams/new_product_architecture/diagram.json`"
        ),
    ]
    text = run(cmd)
    m = re.search(r"docx/([A-Za-z0-9]+)", text) or re.search(r"文档 ID:\s*([A-Za-z0-9]+)", text)
    if not m:
        raise RuntimeError(f"无法解析文档 ID：{text}")
    return m.group(1)


def render_dsl_png() -> Dict[str, Any]:
    if not DIAGRAM_JSON.exists():
        raise RuntimeError(f"缺少 DSL：{DIAGRAM_JSON}")
    text = run(
        ["npx", "-y", WHITEBOARD_CLI, "-i", str(DIAGRAM_JSON), "-o", str(DIAGRAM_PNG), "-F", "json"],
        cwd=DIAGRAM_DIR,
    )
    meta = json.loads(text)
    return meta.get("data", meta)


def insert_board_block(token: str, doc_id: str, width: int, height: int) -> str:
    data = api(
        "POST",
        f"{BASE}/docx/v1/documents/{doc_id}/blocks/{doc_id}/children",
        token,
        params={"document_revision_id": -1},
        json={
            "index": 0,
            "children": [{"block_type": 43, "board": {"align": 2, "width": width, "height": height}}],
        },
    )
    for block in data.get("children", []):
        if block.get("block_type") == 43:
            wb = block.get("board", {}).get("token")
            if wb:
                return wb
    raise RuntimeError("未找到画板 token")


def upload_whiteboard_image(token: str, whiteboard_id: str, doc_id: str, png_path: pathlib.Path) -> str:
    size = png_path.stat().st_size
    extra = json.dumps({"drive_route_token": doc_id}, ensure_ascii=False)
    with png_path.open("rb") as f:
        resp = httpx.post(
            f"{BASE}/drive/v1/medias/upload_all",
            headers={"Authorization": f"Bearer {token}"},
            data={
                "file_name": png_path.name,
                "parent_type": "whiteboard",
                "parent_node": whiteboard_id,
                "size": str(size),
                "extra": extra,
            },
            files={"file": (png_path.name, f, "image/png")},
            timeout=120,
        ).json()
    if resp.get("code") != 0:
        raise RuntimeError(f"上传画板图片失败：{resp}")
    return resp["data"]["file_token"]


def write_image_node(token: str, whiteboard_id: str, media_token: str, width: int, height: int):
    body = {
        "nodes": [
            {
                "type": "image",
                "x": 0,
                "y": 0,
                "width": width,
                "height": height,
                "image": {"token": media_token},
                "z_index": 1,
            }
        ],
        "overwrite": True,
    }
    api("POST", f"{BASE}/board/v1/whiteboards/{whiteboard_id}/nodes", token, json=body)


def normalize_text(node: Dict[str, Any]):
    text = node.get("text")
    if not isinstance(text, dict):
        return
    text["text_color_type"] = 1
    if not text.get("text_color"):
        text["text_color"] = "#1f2329"
    text["theme_text_color_code"] = -1
    text["theme_text_background_color_code"] = -1


def normalize_nodes(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    for node in nodes:
        if node.get("type") in ("svg", "sticky_note"):
            continue
        n = json.loads(json.dumps(node, ensure_ascii=False))
        normalize_text(n)
        if n.get("type") == "connector":
            cap = n.get("connector", {}).get("captions", {}).get("data", [])
            for c in cap:
                if isinstance(c, dict):
                    c["text_color_type"] = 1
                    c.setdefault("text_color", "#1f2329")
        out.append(n)

    def is_bg(n: Dict[str, Any]) -> bool:
        if n.get("type") != "composite_shape":
            return False
        tx = (n.get("text") or {}).get("text", "")
        return not str(tx).strip() and (n.get("width", 0) >= 500 or n.get("height", 0) >= 90)

    backgrounds = [n for n in out if is_bg(n)]
    contents = [n for n in out if n not in backgrounds and n.get("type") != "connector"]
    connectors = [n for n in out if n.get("type") == "connector"]

    z = 1
    for n in backgrounds:
        n["z_index"] = z
        z += 1
    for n in contents:
        n["z_index"] = z
        z += 1
    for n in connectors:
        n["z_index"] = z
        z += 1
    return backgrounds + contents + connectors


def convert_dsl_to_openapi() -> dict:
    run(["npx", "-y", WHITEBOARD_CLI, "-i", str(DIAGRAM_JSON), "-o", str(DIAGRAM_PNG)], cwd=DIAGRAM_DIR)
    run(
        [
            "npx",
            "-y",
            WHITEBOARD_CLI,
            "-i",
            str(DIAGRAM_JSON),
            "--to",
            "openapi",
            "--format",
            "json",
            "-o",
            str(OPENAPI_JSON),
        ],
        cwd=DIAGRAM_DIR,
    )
    payload = json.loads(OPENAPI_JSON.read_text(encoding="utf-8"))
    if isinstance(payload, dict) and "nodes" in payload:
        return payload
    raise RuntimeError("OpenAPI 转换失败")


def write_native_nodes(token: str, whiteboard_id: str, openapi_payload: dict):
    nodes = normalize_nodes(openapi_payload.get("nodes", []))
    api(
        "POST",
        f"{BASE}/board/v1/whiteboards/{whiteboard_id}/nodes",
        token,
        json={"nodes": nodes, "overwrite": True},
    )


def publish(title: str, doc_id: Optional[str] = None, mode: str = "image") -> str:
    feishu_token = load_token()
    if not doc_id:
        doc_id = create_doc(title)
        print(f"已创建文档：{doc_id}")

    print("渲染 DSL...")
    meta = render_dsl_png()
    width = int(meta.get("metadata", {}).get("width", 1232))
    height = int(meta.get("metadata", {}).get("height", 1348))
    print(f"画布：{width} x {height}")

    board_w = min(960, width)
    board_h = max(720, int(height * board_w / width) + 40)
    whiteboard_id = insert_board_block(feishu_token, doc_id, board_w, board_h)
    print(f"画板 ID：{whiteboard_id}")
    time.sleep(0.5)

    if mode == "native":
        print("写入原生节点（已修正 text_color_type / z_index）...")
        openapi = convert_dsl_to_openapi()
        write_native_nodes(feishu_token, whiteboard_id, openapi)
    else:
        print("上传 PNG 到画板...")
        media_token = upload_whiteboard_image(feishu_token, whiteboard_id, doc_id, DIAGRAM_PNG)
        write_image_node(feishu_token, whiteboard_id, media_token, board_w - 20, board_h - 20)

    url = f"https://feishu.cn/docx/{doc_id}"
    print(f"完成：{url}")
    return doc_id


def main():
    import argparse

    parser = argparse.ArgumentParser(description="发布飞书画板（DSL → PNG 或原生节点）")
    parser.add_argument("--title", "-t", default="new_product_test 上位机代码框架（画板版）")
    parser.add_argument("--doc-id", help="已有文档 ID")
    parser.add_argument("--mode", choices=("image", "native"), default="image", help="image=PNG整图（推荐）")
    args = parser.parse_args()

    doc_id = publish(args.title, args.doc_id, args.mode)
    print(doc_id)


if __name__ == "__main__":
    main()
