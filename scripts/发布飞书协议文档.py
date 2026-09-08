"""发布飞书协议文档：native Markdown + 表格列宽 + 高亮块 + 分隔线。"""
import json
import pathlib
import re
import subprocess
import time
from typing import List, Optional

import httpx

BASE = "https://open.feishu.cn/open-apis"
DEFAULT_TABLE_WIDTH = 815
TOKEN_PATH = pathlib.Path.home() / ".feishu-docx" / "tenant_token.json"


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


def list_root_children(token: str, doc_id: str):
    data = api(
        "GET",
        f"{BASE}/docx/v1/documents/{doc_id}/blocks",
        token,
        params={"document_revision_id": -1, "page_size": 500},
    )
    items = data.get("items", [])
    bmap = {b["block_id"]: b for b in items}
    root = bmap.get(doc_id) or next((b for b in items if b.get("block_type") == 1), None)
    if not root:
        raise RuntimeError("未找到文档根 block")
    children = []
    for cid in root.get("children", []):
        if cid in bmap:
            children.append(bmap[cid])
    return children, bmap


def block_plain_text(block: dict) -> str:
    for key in ("text", "heading1", "heading2", "heading3", "heading4"):
        payload = block.get(key)
        if not payload:
            continue
        parts = []
        for el in payload.get("elements", []):
            parts.append(el.get("text_run", {}).get("content", ""))
        return "".join(parts).strip()
    return ""


def cell_plain_text(cell_block: dict, bmap: dict) -> str:
    parts = []
    for cid in cell_block.get("children", []):
        child = bmap.get(cid, {})
        parts.append(block_plain_text(child))
    return re.sub(r"\s+", " ", " ".join(parts)).strip()


def compute_column_widths(table_block: dict, bmap: dict, target_width=DEFAULT_TABLE_WIDTH):
    prop = table_block.get("table", {}).get("property", {})
    rows = prop.get("row_size", 0)
    cols = prop.get("column_size", 0)
    if cols <= 0:
        return []
    cell_ids = table_block.get("table", {}).get("cells", [])
    max_lens = [2] * cols
    for r in range(rows):
        for c in range(cols):
            idx = r * cols + c
            if idx >= len(cell_ids):
                continue
            cell = bmap.get(cell_ids[idx], {})
            max_lens[c] = max(max_lens[c], len(cell_plain_text(cell, bmap)))
    raw = [max(72, min(480, n * 15 + 40)) for n in max_lens]
    total = sum(raw) or 1
    scale = target_width / total
    return [max(72, int(w * scale)) for w in raw]


def patch_table(token: str, doc_id: str, table_block: dict, bmap: dict):
    bid = table_block["block_id"]
    widths = compute_column_widths(table_block, bmap)
    if not widths:
        return
    api(
        "PATCH",
        f"{BASE}/docx/v1/documents/{doc_id}/blocks/{bid}",
        token,
        params={"document_revision_id": -1},
        json={"update_table_property": {"header_row": True}},
    )
    time.sleep(0.35)
    for idx, width in enumerate(widths):
        api(
            "PATCH",
            f"{BASE}/docx/v1/documents/{doc_id}/blocks/{bid}",
            token,
            params={"document_revision_id": -1},
            json={"update_table_property": {"column_index": idx, "column_width": width}},
        )
        time.sleep(0.35)


def patch_all_tables(token: str, doc_id: str) -> int:
    data = api(
        "GET",
        f"{BASE}/docx/v1/documents/{doc_id}/blocks",
        token,
        params={"document_revision_id": -1, "page_size": 500},
    )
    items = data.get("items", [])
    bmap = {b["block_id"]: b for b in items}
    count = 0
    for b in items:
        if b.get("block_type") == 31:
            patch_table(token, doc_id, b, bmap)
            count += 1
    return count


def insert_descendant(token: str, doc_id: str, index: int, descendants: list, root_id: Optional[str] = None):
    root_id = root_id or doc_id
    root_temp = descendants[-1]["block_id"]
    body = {
        "index": index,
        "children_id": [root_temp],
        "descendants": descendants,
    }
    api(
        "POST",
        f"{BASE}/docx/v1/documents/{doc_id}/blocks/{root_id}/descendant",
        token,
        params={"document_revision_id": -1},
        json=body,
    )
    time.sleep(0.35)


def make_text_block(temp_id: str, content: str):
    return {
        "block_id": temp_id,
        "block_type": 2,
        "text": {"elements": [{"text_run": {"content": content}}]},
        "children": [],
    }


def insert_callout(token: str, doc_id: str, index: int, emoji: str, bg: int, border: int, lines: List[str]):
    line_ids = [f"co_line_{i}" for i in range(len(lines))]
    callout_id = "co_root"
    descendants = [make_text_block(line_ids[i], line) for i, line in enumerate(lines)]
    descendants.append(
        {
            "block_id": callout_id,
            "block_type": 19,
            "callout": {"emoji_id": emoji, "background_color": bg, "border_color": border},
            "children": line_ids,
        }
    )
    insert_descendant(token, doc_id, index, descendants)


def insert_divider(token: str, doc_id: str, index: int):
    insert_descendant(
        token,
        doc_id,
        index,
        [{"block_id": "div_root", "block_type": 22, "divider": {}, "children": []}],
    )


def enrich_protocol_doc(token: str, doc_id: str):
    children, bmap = list_root_children(token, doc_id)
    has_callout = any(c.get("block_type") == 19 for c in children)
    if not has_callout:
        insert_callout(
            token,
            doc_id,
            1,
            "bulb",
            5,
            5,
            [
                "设备说明：ASD9026A 为自由工站（一拖多）双通道模拟电池 / 治具程控电源。",
                "规范来源：ASD9026A双通道模拟电池协议.xlsx；本文按上位机实现整理。",
            ],
        )
        children, _ = list_root_children(token, doc_id)

    # 在「1.4 校验和 CRC16」「1.5 字节序说明」前插入提示高亮块（若尚未存在 warning callout）
    warning_text = "注意：CRC 低字节在前；配置帧电压/限流为大端，读模块状态电压为小端。"
    has_warning = False
    for c in children:
        if c.get("block_type") == 19:
            for cid in c.get("children", []):
                sub = bmap.get(cid, {})
                if warning_text[:8] in block_plain_text(sub):
                    has_warning = True
                    break
    if not has_warning:
        for i, child in enumerate(children):
            title = block_plain_text(child)
            if title.startswith("1.4") or title.startswith("1.5"):
                insert_callout(
                    token,
                    doc_id,
                    i,
                    "warning",
                    3,
                    3,
                    [warning_text],
                )
                break

    tables = patch_all_tables(token, doc_id)
    return tables


def create_from_markdown(title: str, md_path: pathlib.Path) -> str:
    cmd = ["feishu-docx", "create", title, "-f", str(md_path), "--native"]
    out = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    text = (out.stdout or "") + (out.stderr or "")
    if out.returncode != 0:
        raise RuntimeError(f"feishu-docx create 失败：{text}")
    m = re.search(r"docx/([A-Za-z0-9]+)", text) or re.search(r"文档 ID:\s*([A-Za-z0-9]+)", text)
    if not m:
        raise RuntimeError(f"无法解析文档 ID：{text}")
    return m.group(1)


def prepare_markdown(source: pathlib.Path) -> pathlib.Path:
    text = source.read_text(encoding="utf-8")
    if not text.startswith("# "):
        raise RuntimeError("协议 Markdown 须以 # 标题 开头")
    # 确保大章节之间有分隔线，便于 native 转换成分割线 block
    text = re.sub(r"\n(?=## \d+\.)", "\n\n---\n\n", text)
    out = source.with_name(f"_feishu_{source.name}")
    out.write_text(text, encoding="utf-8")
    return out


def main():
    import argparse

    parser = argparse.ArgumentParser(description="发布飞书协议文档（表格列宽 + 高亮块）")
    parser.add_argument("doc_id", nargs="?", help="已有文档 ID，仅做排版增强")
    parser.add_argument("--file", "-f", help="源 Markdown 路径，创建新文档")
    parser.add_argument("--title", "-t", help="新文档标题（配合 --file）")
    args = parser.parse_args()

    token = load_token()
    doc_id = args.doc_id

    if args.file:
        src = pathlib.Path(args.file).resolve()
        title = args.title or src.stem
        md = prepare_markdown(src)
        try:
            doc_id = create_from_markdown(title, md)
            print(f"已创建文档：{doc_id}")
        finally:
            if md.exists():
                md.unlink()

    if not doc_id:
        parser.error("请提供 doc_id 或 --file")

    tables = enrich_protocol_doc(token, doc_id)
    print(f"已优化 {tables} 个表格列宽，并插入高亮块/分隔线。")
    print(f"https://feishu.cn/docx/{doc_id}")


if __name__ == "__main__":
    main()
