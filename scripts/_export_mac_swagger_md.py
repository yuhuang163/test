# -*- coding: utf-8 -*-
"""从内网 Swagger /v3/api-docs 导出 mac-address-controller 为 Markdown。"""
from __future__ import annotations

import json
from pathlib import Path
from urllib.request import urlopen

URL = "http://192.168.200.140:8080/v3/api-docs"
ROOT = Path(__file__).resolve().parents[1]
OUT_MD = ROOT / "docs" / "开发参考资料" / "mac-address-controller接口协议.md"
OUT_JSON = ROOT / "docs" / "开发参考资料" / "_swagger_v3_mac.json"


def ref_name(ref: str) -> str:
    return ref.split("/")[-1] if ref else ""


def main() -> None:
    raw = urlopen(URL, timeout=30).read()
    data = json.loads(raw.decode("utf-8"))
    schemas = data.get("components", {}).get("schemas", {})

    def resolve_schema(sch, depth=0):
        if not sch or depth > 6:
            return {}
        if "$ref" in sch:
            name = ref_name(sch["$ref"])
            nested = resolve_schema(schemas.get(name, {}), depth + 1)
            return {"$ref": name, **nested}
        out = {}
        for k in ("type", "format", "enum", "description", "example", "required"):
            if k in sch:
                out[k] = sch[k]
        if "items" in sch:
            out["items"] = resolve_schema(sch["items"], depth + 1)
        if "properties" in sch:
            out["properties"] = {k: resolve_schema(v, depth + 1) for k, v in sch["properties"].items()}
        if "additionalProperties" in sch and isinstance(sch["additionalProperties"], dict):
            out["additionalProperties"] = resolve_schema(sch["additionalProperties"], depth + 1)
        return out

    def schema_table(sch, prefix=""):
        rows = []
        if not sch:
            return rows
        if "$ref" in sch and "properties" not in sch:
            rows.append((prefix or sch["$ref"], "object", "", f"见 Schema `{sch['$ref']}`"))
            return rows
        props = sch.get("properties") or {}
        req = set(sch.get("required") or [])
        for name, p in props.items():
            path = f"{prefix}.{name}" if prefix else name
            typ = p.get("type") or p.get("$ref") or "object"
            if p.get("format"):
                typ = f"{typ}({p['format']})"
            if "enum" in p:
                typ = f"{typ} enum{p['enum']}"
            if typ == "array" or p.get("type") == "array":
                it = p.get("items") or {}
                it_t = it.get("type") or it.get("$ref") or "?"
                typ = f"array<{it_t}>"
            desc = p.get("description") or ""
            if "example" in p:
                desc = (desc + f" 示例=`{p['example']}`").strip()
            rows.append((path, typ, "是" if name in req else "否", desc))
            if p.get("properties"):
                rows.extend(schema_table(p, path))
            if p.get("type") == "array" and isinstance(p.get("items"), dict):
                items = p["items"]
                if items.get("properties"):
                    rows.extend(schema_table(items, path + "[]"))
                elif items.get("$ref"):
                    nested = resolve_schema({"$ref": f"#/components/schemas/{items['$ref']}"})
                    if nested.get("properties"):
                        rows.extend(schema_table(nested, path + "[]"))
        return rows

    ops = []
    for path, methods in sorted(data.get("paths", {}).items()):
        if not path.startswith("/api/mac-addresses"):
            continue
        for method, op in methods.items():
            if method.startswith("x-") or not isinstance(op, dict):
                continue
            ops.append((path, method.upper(), op))

    slim = {
        "openapi": data.get("openapi"),
        "info": data.get("info"),
        "servers": data.get("servers"),
        "paths": {
            p: {m.lower(): op for p2, m, op in ops if p2 == p}
            for p in sorted({p for p, _, _ in ops})
        },
        "components": {"schemas": schemas},
    }
    OUT_JSON.write_text(json.dumps(slim, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = []
    lines.append("# mac-address-controller 接口协议 — 三元组 / MAC 服务")
    lines.append("")
    lines.append(
        "> **一句话**：从内网 OpenAPI（Swagger）导出的 `/api/mac-addresses` 全量接口说明"
        "（含你打开的 `create_1`）。  "
    )
    lines.append(
        "> **读者**：上位机 / 烧录工具对接开发。"
        "**前提**：可达 `192.168.200.140:8080`；多数接口需 Basic 鉴权。  "
    )
    lines.append(
        "> **来源**：`GET /v3/api-docs`；Swagger UI："
        "`/swagger-ui/index.html#/mac-address-controller/create_1`。"
    )
    lines.append("")
    lines.append("## 快速参考")
    lines.append("")
    lines.append("| 项 | 值 |")
    lines.append("|----|-----|")
    lines.append("| BaseUrl | `http://192.168.200.140:8080` |")
    lines.append("| OpenAPI | `GET /v3/api-docs` |")
    lines.append("| Swagger UI | `http://192.168.200.140:8080/swagger-ui/index.html` |")
    lines.append("| 锚点接口 create_1 | `POST /api/mac-addresses` |")
    lines.append("| 贴片 PCBA SN | `POST /api/mac-addresses/submit-smt-pcba-sn` |")
    lines.append("| 贴片 MAC | `POST /api/mac-addresses/submit-smt-mac` |")
    lines.append("| 领三元组 | `GET /api/mac-addresses/applyTupleByMac` |")
    lines.append("| 原始切片 JSON | `docs/开发参考资料/_swagger_v3_mac.json` |")
    lines.append("")
    lines.append("## 1. 范围")
    lines.append("")
    lines.append("### 包含")
    lines.append("- 路径前缀 `/api/mac-addresses` 下全部方法（Swagger `mac-address-controller`）")
    lines.append("- 参数、Body Schema、响应码与字段表")
    lines.append("")
    lines.append("### 不包含（边界）")
    lines.append("- product-config / batch / 用户管理等其它 controller")
    lines.append("- 服务端贴片卡控实现（仅契约；业务 `message` 以接口返回为准）")
    lines.append("")
    lines.append("## 2. 接口一览")
    lines.append("")
    lines.append("| Method | Path | operationId | summary |")
    lines.append("|--------|------|-------------|---------|")
    for path, method, op in ops:
        oid = op.get("operationId") or ""
        summary = (op.get("summary") or op.get("description") or "").replace("\r\n", " ")
        mark = " ← **create_1**" if oid == "create_1" else ""
        lines.append(f"| `{method}` | `{path}` | `{oid}`{mark} | {summary} |")
    lines.append("")

    lines.append("## 3. 接口详情")
    lines.append("")
    ops_sorted = sorted(
        ops, key=lambda x: (0 if x[2].get("operationId") == "create_1" else 1, x[0], x[1])
    )

    for path, method, op in ops_sorted:
        oid = op.get("operationId") or ""
        summary = op.get("summary") or ""
        desc = op.get("description") or ""
        lines.append(f"### `{method} {path}`")
        lines.append("")
        lines.append(f"- **operationId**：`{oid}`")
        if summary:
            lines.append(f"- **summary**：{summary}")
        if desc and desc != summary:
            lines.append(f"- **description**：{desc}")
        if op.get("deprecated"):
            lines.append("- **deprecated**：是")
        lines.append("")

        params = op.get("parameters") or []
        if params:
            lines.append("#### 参数")
            lines.append("")
            lines.append("| 位置 | 名称 | 类型 | 必填 | 说明 |")
            lines.append("|------|------|------|------|------|")
            for p in params:
                schema = p.get("schema") or {}
                if "$ref" in schema:
                    typ = ref_name(schema["$ref"])
                else:
                    typ = schema.get("type", "")
                    if schema.get("format"):
                        typ = f"{typ}({schema['format']})"
                lines.append(
                    f"| {p.get('in', '')} | `{p.get('name', '')}` | {typ} | "
                    f"{'是' if p.get('required') else '否'} | "
                    f"{(p.get('description') or '').replace('|', '/')} |"
                )
            lines.append("")

        rb = op.get("requestBody")
        if rb:
            lines.append("#### 请求体")
            lines.append("")
            lines.append(f"- **必填**：{'是' if rb.get('required') else '否'}")
            content = rb.get("content") or {}
            for ctype, cobj in content.items():
                lines.append(f"- **Content-Type**：`{ctype}`")
                sch = resolve_schema(cobj.get("schema") or {})
                if sch.get("$ref"):
                    lines.append(f"- **Schema**：`{sch['$ref']}`")
                rows = schema_table(sch)
                if rows:
                    lines.append("")
                    lines.append("| 字段 | 类型 | 必填 | 说明 |")
                    lines.append("|------|------|------|------|")
                    for name, typ, req, d in rows:
                        lines.append(f"| `{name}` | {typ} | {req} | {d.replace('|', '/')} |")
                ex = cobj.get("example")
                if ex is None:
                    for _, ev in (cobj.get("examples") or {}).items():
                        if isinstance(ev, dict) and "value" in ev:
                            ex = ev["value"]
                            break
                if ex is not None:
                    lines.append("")
                    lines.append("```json")
                    lines.append(json.dumps(ex, ensure_ascii=False, indent=2))
                    lines.append("```")
            lines.append("")

        responses = op.get("responses") or {}
        if responses:
            lines.append("#### 响应")
            lines.append("")
            lines.append("| HTTP | 说明 | Schema |")
            lines.append("|------|------|--------|")
            for code, resp in responses.items():
                rdesc = (resp.get("description") or "").replace("|", "/")
                sch_name = ""
                for ctype, cobj in (resp.get("content") or {}).items():
                    sch = cobj.get("schema") or {}
                    if "$ref" in sch:
                        sch_name = ref_name(sch["$ref"])
                    elif sch.get("type"):
                        sch_name = sch.get("type")
                lines.append(f"| `{code}` | {rdesc} | {sch_name} |")
            for code in ("200", "201"):
                resp = responses.get(code)
                if not resp:
                    continue
                for ctype, cobj in (resp.get("content") or {}).items():
                    sch = resolve_schema(cobj.get("schema") or {})
                    rows = schema_table(sch)
                    if not rows:
                        continue
                    lines.append("")
                    lines.append(f"**{code} 响应字段**（`{ctype}`）")
                    lines.append("")
                    lines.append("| 字段 | 类型 | 必填 | 说明 |")
                    lines.append("|------|------|------|------|")
                    for name, typ, req, d in rows:
                        lines.append(f"| `{name}` | {typ} | {req} | {d.replace('|', '/')} |")
            lines.append("")

    used_refs = set()

    def collect_refs(sch):
        if not isinstance(sch, dict):
            return
        if "$ref" in sch:
            used_refs.add(ref_name(sch["$ref"]))
        for v in sch.values():
            if isinstance(v, dict):
                collect_refs(v)
            elif isinstance(v, list):
                for i in v:
                    if isinstance(i, dict):
                        collect_refs(i)

    for path, method, op in ops:
        collect_refs(op.get("requestBody") or {})
        for resp in (op.get("responses") or {}).values():
            collect_refs(resp)
        for p in op.get("parameters") or []:
            collect_refs(p.get("schema") or {})

    changed = True
    while changed:
        changed = False
        for name in list(used_refs):
            blob = json.dumps(schemas.get(name) or {}, ensure_ascii=False)
            for other in schemas:
                token = f"#/components/schemas/{other}"
                if token in blob and other not in used_refs:
                    used_refs.add(other)
                    changed = True

    lines.append("## 4. Schema 定义（本控制器引用）")
    lines.append("")
    for name in sorted(used_refs):
        sch = resolve_schema(schemas.get(name, {}))
        lines.append(f"### `{name}`")
        lines.append("")
        if schemas.get(name, {}).get("description"):
            lines.append(schemas[name]["description"])
            lines.append("")
        rows = schema_table(sch)
        if rows:
            lines.append("| 字段 | 类型 | 必填 | 说明 |")
            lines.append("|------|------|------|------|")
            for n, typ, req, d in rows:
                lines.append(f"| `{n}` | {typ} | {req} | {d.replace('|', '/')} |")
        else:
            lines.append("```json")
            lines.append(json.dumps(schemas.get(name, {}), ensure_ascii=False, indent=2))
            lines.append("```")
        lines.append("")

    lines.append("## 5. 验证")
    lines.append("")
    lines.append(
        "1. 浏览器打开 Swagger UI，展开 `mac-address-controller` → "
        "`POST /api/mac-addresses`（operationId=`create_1`）。"
    )
    lines.append('2. `GET /v3/api-docs` 中检索 `"operationId":"create_1"`。')
    lines.append("3. 贴片：检索 `submit-smt-pcba-sn` / `submit-smt-mac`。")
    lines.append(
        "4. 领三元组：`GET /api/mac-addresses/applyTupleByMac`"
        "（上位机 `QTupleService::applyTupleByMacImpl`）。"
    )
    lines.append("")
    lines.append("## 6. 常见问题")
    lines.append("")
    lines.append("| 现象 | 原因 | 处理 |")
    lines.append("|------|------|------|")
    lines.append(
        "| 提示与贴片 / PCBA SN 相关 | 业务校验写在服务端返回 `message` |"
        " 查 `applyTupleByMac` / `submit-smt-pcba-sn` 响应 |"
    )
    lines.append("| 401 | 未带 Basic 鉴权 | 先调 `GET /api/mac-addresses/auth` |")
    lines.append("| Swagger 中文乱码 | 页面编码 | 以本 md / UTF-8 JSON 为准 |")
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append(f"*由 `scripts/_export_mac_swagger_md.py` 从 `{URL}` 导出；共 {len(ops)} 个接口。*")
    lines.append("")

    text = "\r\n".join(lines)
    OUT_MD.write_bytes(text.replace("\r\n", "\r\n").encode("utf-8"))
    print("wrote", OUT_MD)
    print("ops", len(ops), "schemas", len(used_refs))
    for path, method, op in ops:
        if op.get("operationId") == "create_1":
            print("create_1", method, path, "summary=", op.get("summary"))


if __name__ == "__main__":
    main()
