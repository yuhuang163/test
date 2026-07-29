# 产线 WebRTC 远控 Agent

## 原则

- **Git**：只提交本目录源码，不提交 `bin/remote_agent/`（含 `_internal`）
- **产线**：发版时把打包好的 `bin/remote_agent/` 整目录和上位机 exe 一起拷走

## 脚本（只留 3 个 bat）

| 脚本 | 谁用 | 做什么 |
|------|------|--------|
| `setup_dev.bat` | 开发 | 创建 `.venv`、装依赖 |
| `build_exe.bat` | 发版 | 生成 `bin/remote_agent/`（exe + `_internal`） |
| `run.bat` | 调试 | 手动启动 Agent |

开发：

```bat
cd remote_agent
setup_dev.bat
```

发版：

```bat
cd remote_agent
build_exe.bat
```

然后拷贝整个 `build/.../bin/remote_agent/`（必须含 `_internal`）到上位机 exe 同级。

> 用 onedir，不要 onefile（onefile 每次启动解压很慢）。

## 配置

上位机通过环境变量 `REMOTE_AGENT_CONFIG` 传 JSON（不落盘）。
