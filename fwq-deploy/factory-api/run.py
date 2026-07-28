import uvicorn

if __name__ == "__main__":
    # Windows 下 reload 多进程易卡住，本地开发默认单进程
    # 压缩扩展由 app.main 中间件剥离，兼容旧版 uvicorn（无 --no-ws-per-message-deflate）
    uvicorn.run("app.main:app", host="0.0.0.0", port=8800, reload=False)
