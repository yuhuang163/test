# 产线 WebRTC 远控 Agent

由上位机收到 `start_remote_desktop` 命令后拉起，向管理端推送主屏画面并接收键鼠。

## 本机准备

```bat
cd D:\code\fwq\remote-agent
py -3 -m venv .venv
.venv\Scripts\pip install -r requirements.txt
```

部署时把整个 `remote_agent` 目录拷到上位机 `bin\remote_agent\`（与 `new_production.exe` 同级）。

## 手动调试

由上位机写入 `session.json` 后：

```bat
run.bat --config session.json --log-dir .
```

`signalingUrl` 示例：

`ws://127.0.0.1:8800/api/factory-tool/remote-desktop/ws?sessionId=...&role=agent&token=...`

（HTTPS 站点用 `wss://`）
