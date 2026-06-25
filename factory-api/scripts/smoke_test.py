"""简单联调脚本：登录 + 上传测试 zip。"""

import io
import json
import urllib.request
import zipfile

BASE = "http://127.0.0.1:8800/api/factory-tool"


def main() -> None:
    login_req = urllib.request.Request(
        f"{BASE}/auth/login",
        data=json.dumps({"username": "admin", "password": "admin123", "hostName": "web-console"}).encode(),
        headers={"Content-Type": "application/json"},
    )
    login_body = json.loads(urllib.request.urlopen(login_req).read().decode())
    token = login_body["data"]["accessToken"]
    print("login ok", login_body["code"])

    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        zf.writestr("上位机log/test.txt", "hello factory cloud log")
    zip_bytes = buf.getvalue()

    boundary = "----boundary123"
    parts: list[bytes] = []
    for name, val in [("factoryName", "hz"), ("deviceId", "DESKTOP-TEST"), ("station", "DEFAULT")]:
        parts.append(
            f"--{boundary}\r\nContent-Disposition: form-data; name=\"{name}\"\r\n\r\n{val}\r\n".encode()
        )
    parts.append(
        f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"test.zip\"\r\n"
        f"Content-Type: application/zip\r\n\r\n".encode()
    )
    parts.append(zip_bytes)
    parts.append(f"\r\n--{boundary}--\r\n".encode())
    body = b"".join(parts)

    upload_req = urllib.request.Request(f"{BASE}/logs/upload", data=body, method="POST")
    upload_req.add_header("Content-Type", f"multipart/form-data; boundary={boundary}")
    upload_req.add_header("Authorization", f"Bearer {token}")
    upload_body = json.loads(urllib.request.urlopen(upload_req).read().decode())
    print("upload ok", upload_body)

    list_req = urllib.request.Request(f"{BASE}/logs?page=1&pageSize=10")
    list_req.add_header("Authorization", f"Bearer {token}")
    list_body = json.loads(urllib.request.urlopen(list_req).read().decode())
    print("list total", list_body["data"]["total"])

    manifest_req = urllib.request.Request(f"{BASE}/test-cases/manifest")
    manifest_req.add_header("Authorization", f"Bearer {token}")
    manifest_body = json.loads(urllib.request.urlopen(manifest_req).read().decode())
    print("manifest 用例包版本", manifest_body["data"]["bundleVersion"])


if __name__ == "__main__":
    main()
