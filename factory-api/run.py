import uvicorn

if __name__ == "__main__":
    # Windows 下 reload 多进程易卡住，本地开发默认单进程
    # ARR 不支持 permessage-deflate：关闭后浏览器即使声明扩展也不会协商压缩
    uvicorn.run(
        "app.main:app",
        host="0.0.0.0",
        port=8800,
        reload=False,
        ws_per_message_deflate=False,
    )
