from flask import Flask, request, jsonify
import copy
import datetime
import json
import re
from pathlib import Path
from typing import Any, Optional
from urllib.parse import unquote

app = Flask(__name__)

_LOG_URL_MAX = 4096


def _short(s: str, limit: int) -> str:
    if len(s) <= limit:
        return s
    return f"{s[:limit]}...(共{len(s)}字符)"


def _parse_param_block(param_decoded: str) -> dict:
    """解析 MES 松散 param：{KEY:VAL,...}；键值对之间可为逗号或分号（Complete 常见 CLIENT_ID:1;remark:备注）。"""
    out = {}
    if not param_decoded:
        return out
    inner = param_decoded.strip()
    if inner.startswith("{") and inner.endswith("}"):
        inner = inner[1:-1]
    for part in re.split(r"[,;]", inner):
        part = part.strip()
        if ":" not in part:
            continue
        k, v = part.split(":", 1)
        out[k.strip()] = v.strip()
    return out


def _parse_param(param_decoded: str) -> Any:
    """优先按 JSON 解析（含 TEST_DATA_LIST 数组）；失败则回退到松散键值。"""
    s = (param_decoded or "").strip()
    if not s:
        return {}
    if s.startswith("{") and s.endswith("}"):
        try:
            v = json.loads(s)
            return v if isinstance(v, dict) else {}
        except json.JSONDecodeError:
            return _parse_param_block(param_decoded)
    return _parse_param_block(param_decoded)


def _testdata_collect_summary(payload: dict) -> dict:
    """回包中只带摘要，避免把整个 TEST_DATA_LIST 再塞回去过大。"""
    lst = payload.get("TEST_DATA_LIST")
    n = len(lst) if isinstance(lst, list) else 0
    keys = (
        "SN",
        "SHOPORDER_NO",
        "TEST_RESULT",
        "CLIENT_ID",
        "LOGIN_ID",
        "TDS_NAME",
        "STATION_ID",
        "PROJECT_NAME",
        "TEST_STATION",
    )
    out: dict[str, Any] = {k: payload[k] for k in keys if k in payload}
    out["TEST_DATA_LIST_COUNT"] = n
    return out


def _ok_json(method: str, fields: Optional[dict] = None, **extra: Any):
    """统一成功 JSON；extra 可覆盖字段。"""
    payload = {"RESULT": "PASS", "CODE": "0", "MSG": "OK", "METHOD": method}
    if fields:
        payload["PARAM"] = fields
    payload.update(extra)
    return jsonify(payload)


_GET_SFC_KEY_BY_SFC_TEMPLATE: Optional[dict] = None


def _load_get_sfc_key_by_sfc_template() -> dict:
    path = Path(__file__).with_name("get_sfc_key_by_sfc_response.json")
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def _json_get_sfc_key_by_sfc(param_fields: Any):
    """GetSfcKeyBySfc：返回与现网一致的 RESULT/DATA 结构；若 param 含 SFC，则仅替换 DATA 内各条 sfc 字段。"""
    global _GET_SFC_KEY_BY_SFC_TEMPLATE
    if _GET_SFC_KEY_BY_SFC_TEMPLATE is None:
        _GET_SFC_KEY_BY_SFC_TEMPLATE = _load_get_sfc_key_by_sfc_template()
    body = copy.deepcopy(_GET_SFC_KEY_BY_SFC_TEMPLATE)
    sfc = ""
    if isinstance(param_fields, dict):
        sfc = str(param_fields.get("SFC", "") or "").strip()
    if sfc:
        for row in body.get("DATA") or []:
            if isinstance(row, dict):
                row["sfc"] = sfc
    return jsonify(body)

@app.route("/Service.action", methods=["GET", "POST"])
def service_action():
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    method = request.values.get("method", "")
    param_raw = request.values.get("param", "")
    param_decoded = unquote(param_raw) if param_raw else ""
    fields = _parse_param(param_decoded)

    full_url = request.url
    print(f"\n===== Service.action | {now} =====", flush=True)
    print(f"请求URL: {_short(full_url, _LOG_URL_MAX)}", flush=True)
    print(f"请求方法: {request.method}  客户端: {request.remote_addr}", flush=True)
    print(f"method: {method}", flush=True)
    # print(f"param(raw): {param_raw[:800]}{'...' if len(param_raw) > 800 else ''}", flush=True)
    print(f"param(解码): {param_decoded[:800]}{'...' if len(param_decoded) > 800 else ''}", flush=True)
    if isinstance(fields, dict) and "TEST_DATA_LIST" in fields:
        snap = {k: fields[k] for k in fields if k != "TEST_DATA_LIST"}
        lst = fields.get("TEST_DATA_LIST")
        snap["TEST_DATA_LIST"] = (
            f"<{len(lst)} items>" if isinstance(lst, list) else lst
        )
        print(f"param(解析): {snap}", flush=True)
    else:
        print(f"param(解析): {fields}", flush=True)
    print("=" * 50, flush=True)

    # MES 常见写法：PascalCase；兼容小写 start / complete
    m = method
    if m == "start":
        m = "Start"
    elif m == "complete":
        m = "Complete"

    if m == "TestDataCollect2MainChild" and isinstance(fields, dict):
        tdl = fields.get("TEST_DATA_LIST")
        if tdl is not None:
            print("----- param TEST_DATA_LIST（完整）-----", flush=True)
            try:
                print(
                    json.dumps(tdl, ensure_ascii=False, indent=2),
                    flush=True,
                )
            except (TypeError, ValueError):
                print(repr(tdl), flush=True)
            print("----- END TEST_DATA_LIST -----", flush=True)

    if m == "GetSfcKeyBySfc":
        return _json_get_sfc_key_by_sfc(fields)
    if m == "TestDataCollect2MainChild":
        if isinstance(fields, dict):
            return _ok_json(m, _testdata_collect_summary(fields))
        return _ok_json(m, {})

    if m in ("Start", "Complete", "NcComplete"):
        return _ok_json(m, fields if isinstance(fields, dict) else {})

    return jsonify(RESULT="FAIL", CODE="-1", MSG=f"未模拟的 method: {method}"), 200


# ==================== 华庄 MES 模拟 ====================
# 内存存储：已添加的计划列表
_planned_pcbs: set = set()  # 已添加计划的 pcbSeq
_routed_pcbs: set = set()   # 已过站的 pcbSeq
_pcb_fields: dict = {}      # pcbSeq -> {fieldName: value}


def _mrs_log(route: str, params: dict):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"\n===== MRS {route} | {now} =====", flush=True)
    print(f"客户端: {request.remote_addr}", flush=True)
    print(f"参数: {params}", flush=True)
    print("=" * 50, flush=True)


@app.route("/mrs/createRoute", methods=["GET", "POST"])
def mrs_create_route():
    """模拟华庄 MES 过站"""
    params = dict(request.values)
    _mrs_log("createRoute", params)

    pcb_seq = params.get("pcbSeq", "").strip()

    if pcb_seq not in _planned_pcbs:
        return jsonify({"msgId": 1, "msgStr": "不存在未关闭的计划序号！", "mpcbSeq": "", "snList": [""]})

    _routed_pcbs.add(pcb_seq)
    return jsonify({"msgId": 0, "msgStr": "", "mpcbSeq": "", "snList": [""]})


@app.route("/mrs/checkRoute", methods=["GET", "POST"])
def mrs_check_route():
    """模拟华庄 MES 路由检查"""
    params = dict(request.values)
    _mrs_log("checkRoute", params)

    pcb_seq = params.get("pcbSeq", "").strip()

    if pcb_seq not in _planned_pcbs:
        return jsonify({"msgId": 1, "msgStr": "条码号不存在！", "mpcbSeq": "", "snList": [""]})

    return jsonify({"msgId": 0, "msgStr": "", "mpcbSeq": "", "snList": [""]})


@app.route("/mrs/addPlan", methods=["GET", "POST"])
def mrs_add_plan():
    """添加计划（管理接口），添加后 createRoute/checkRoute 才会成功"""
    params = dict(request.values)
    _mrs_log("addPlan", params)

    pcb_seq = params.get("pcbSeq", "").strip()
    prod_no = params.get("prodNo", "").strip()

    if not pcb_seq:
        return jsonify({"msgId": 1, "msgStr": "pcbSeq 不能为空"})

    _planned_pcbs.add(pcb_seq)
    print(f"  -> 已添加计划: pcbSeq={pcb_seq}, prodNo={prod_no}", flush=True)
    return jsonify({"msgId": 0, "msgStr": "计划已添加", "pcbSeq": pcb_seq, "prodNo": prod_no})


@app.route("/mrs/status", methods=["GET"])
def mrs_status():
    """查看当前已添加的计划和已过站列表"""
    return jsonify({
        "plannedPcbs": sorted(_planned_pcbs),
        "routedPcbs": sorted(_routed_pcbs),
        "plannedCount": len(_planned_pcbs),
        "routedCount": len(_routed_pcbs),
        "pcbFields": _pcb_fields,
    })


@app.route("/mrs/getField", methods=["GET", "POST"])
def mrs_get_field():
    """根据pcbSeq和fieldName返回字段值（字段名大小写不敏感）"""
    params = dict(request.values)
    _mrs_log("getField", params)

    pcb_seq = params.get("pcbSeq", "").strip()
    field_name = params.get("fieldName", "").strip()

    if not pcb_seq or not field_name:
        return ""

    fields = _pcb_fields.get(pcb_seq, {})
    
    # 先精确匹配，再大小写不敏感匹配
    if field_name in fields:
        return fields[field_name]
    
    # 大小写不敏感查找
    field_name_lower = field_name.lower()
    for key in fields:
        if key.lower() == field_name_lower:
            return fields[key]
    
    return ""


@app.route("/mrs/setField", methods=["GET", "POST"])
def mrs_set_field():
    """设置pcbSeq的字段值（管理接口）"""
    params = dict(request.values)
    _mrs_log("setField", params)

    pcb_seq = params.get("pcbSeq", "").strip()
    field_name = params.get("fieldName", "").strip()
    field_value = params.get("value", "").strip()

    if not pcb_seq or not field_name:
        return jsonify({"msgId": 1, "msgStr": "pcbSeq 和 fieldName 不能为空"})

    if pcb_seq not in _pcb_fields:
        _pcb_fields[pcb_seq] = {}
    _pcb_fields[pcb_seq][field_name] = field_value
    
    print(f"  -> 已设置字段: pcbSeq={pcb_seq}, {field_name}={field_value}", flush=True)
    return jsonify({"msgId": 0, "msgStr": "字段已设置", "pcbSeq": pcb_seq, "fieldName": field_name, "value": field_value})

@app.route("/api/device/groupTest", methods=["GET", "POST"])
def api_device_group_test():
    """工序过站校验"""
    if request.method == "POST":
        data = request.get_json(silent=True) or {}
    else:
        data = dict(request.values)
    
    print(f"\n===== API groupTest =====", flush=True)
    print(f"数据: {data}", flush=True)
    
    return jsonify({
        "resultCode": "0000",
        "resultMsg": "TRUE",
        "data": None,
        "count": 0,
        "extend": {}
    })


@app.route("/api/device/wipTest", methods=["GET", "POST"])
def api_device_wip_test():
    """工序过站"""
    if request.method == "POST":
        data = request.get_json(silent=True) or {}
    else:
        data = dict(request.values)
    
    print(f"\n===== API wipTest =====", flush=True)
    print(f"数据: {data}", flush=True)
    
    return jsonify({
        "resultCode": "0000",
        "resultMsg": "TRUE",
        "data": None,
        "count": 0,
        "extend": {}
    })

# 其它路径：日志 + 简单应答
@app.route("/", defaults={"path": ""})
@app.route("/<path:path>")
def catch_all(path):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    full_url = request.url
    get_params = request.args.to_dict()

    print(f"\n===== 收到请求 | {now} =====", flush=True)
    print(f"路径: /{path}", flush=True)
    print(f"请求URL: {_short(full_url, _LOG_URL_MAX)}", flush=True)
    print(f"请求方法: {request.method}  客户端: {request.remote_addr}", flush=True)
    print(f"参数: {get_params}", flush=True)
    print("=" * 50, flush=True)

    return "请求已收到\n"


if __name__ == "__main__":
    # 0.0.0.0 局域网可访问。MES 里若写死 http://192.168.11.157/... 无端口，则需本机 80 端口或反向代理到本服务端口。
    app.run(host="0.0.0.0", port=8888, debug=False)