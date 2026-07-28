# 产线 WebRTC 远控 Agent

## 目录（只两处）

| 位置 | 作用 |
|------|------|
| 仓库 `remote_agent/` | **源码**（Git） |
| `build/.../bin/remote_agent/remote_agent.exe` | **运行时**（单个 exe） |

qmake / `.pro` **不**再拷贝 Agent。发运行包时拷这一个 exe 即可（无需 `_internal`）。

## 打包（产线无 Python）

```bat
cd remote_agent
powershell -ExecutionPolicy Bypass -File .\build_exe.ps1
```

产物：`bin\remote_agent\remote_agent.exe`（onefile，首次启动会稍慢，属正常）。

## 开发调试（有 Python）

```bat
cd remote_agent
py -3 -m venv .venv
.venv\Scripts\pip install -r requirements.txt
run.bat --config env:REMOTE_AGENT_CONFIG --log-dir .
```

联调上位机前建议先跑一次 `build_exe.ps1`。

## 配置（不落盘）

上位机通过环境变量 `REMOTE_AGENT_CONFIG` 传 JSON。
