"""生产/测试统一启动入口（兼容旧版 uvicorn CLI）。"""

from __future__ import annotations

import inspect
import os

import uvicorn


def main() -> None:
    port = int(os.environ.get("FACTORY_API_PORT", os.environ.get("PORT", "8800")))
    kwargs = {
        "app": "app.main:app",
        "host": "0.0.0.0",
        "port": port,
        "reload": False,
    }
    # ARR 不支持 permessage-deflate；必须关闭，否则浏览器经 IIS 会 101 后立刻断
    if "ws_per_message_deflate" in inspect.signature(uvicorn.Config).parameters:
        kwargs["ws_per_message_deflate"] = False
        print("[API] ws_per_message_deflate=False (ARR compatible)")
    else:
        print("[API] WARN: 当前 uvicorn 无 ws_per_message_deflate，请升级 uvicorn>=0.29")
    uvicorn.run(**kwargs)


if __name__ == "__main__":
    main()
