@echo off

echo [API-PROD] starting...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

title Lute-API-Prod

set "SCRIPTS=%~dp0"

call "%SCRIPTS%port.bat"

set "PORT=%FACTORY_API_PORT%"

set "ROOT=%SCRIPTS%.."

cd /d "%ROOT%\factory-api"

where py >nul 2>&1

if errorlevel 1 (

    echo ERROR: 未找到 Python 3。请安装 Python 3.10+ 并勾选 Add to PATH。

    goto stay

)

curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/health >nul 2>&1

if not errorlevel 1 (

    echo [API-PROD] 端口 %PORT% 已有 API 在运行: http://127.0.0.1:%PORT%/docs

    echo [API-PROD] 若这是「启动管理平台」开发环境，请先 stop-api-prod.bat 或关闭 Lute-API 窗口后再启本生产包。

    goto stay

)

if exist ".\.venv\Scripts\python.exe" (

    .\.venv\Scripts\python.exe -c "import sys" >nul 2>&1

    if errorlevel 1 (

        echo [API-PROD] venv 无效（可能从其他电脑复制），正在重建...

        rmdir /s /q .venv

    )

)

if not exist ".\.venv\Scripts\python.exe" (

    echo [API-PROD] 首次运行：创建虚拟环境并 pip install，约 1-3 分钟...

    py -3 -m venv .venv

    if errorlevel 1 (

        echo ERROR: 创建 venv 失败，请确认已安装 Python 3。

        goto stay

    )

    set PYTHONUTF8=1

    set PYTHONIOENCODING=utf-8

    .\.venv\Scripts\python.exe -m pip install --upgrade pip

    .\.venv\Scripts\python.exe -m pip install -r requirements.txt

    if errorlevel 1 (

        echo ERROR: pip install 失败，请检查网络或 requirements.txt。

        goto stay

    )

)

if not exist ".\.env" (

    if exist ".\.env.production.example" (

        copy /Y .\.env.production.example .\.env >nul

    ) else (

        copy /Y .\.env.example .\.env >nul

    )

    echo [API-PROD] 已生成 .env（数据库默认 factory-api\data\）

)

if not exist ".\data\storage" mkdir ".\data\storage" 2>nul

echo [API-PROD] listen 0.0.0.0:%PORT%  (LAN / remote)

echo [API-PROD] docs  http://127.0.0.1:%PORT%/docs

echo [API-PROD] IIS 管理端见 scripts\iis-setup.txt

echo.

.\.venv\Scripts\uvicorn.exe app.main:app --host 0.0.0.0 --port %PORT%

if errorlevel 1 (

    echo [API-PROD] 启动失败。常见原因：端口 %PORT% 被占用 — 运行 stop-api-prod.bat 后重试。

)

:stay

echo.

echo 关闭本窗口即停止 API。

pause
