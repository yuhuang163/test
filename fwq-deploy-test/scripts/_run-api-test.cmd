@echo off

echo [API-TEST] starting...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

title Lute-API-Test

set "SCRIPTS=%~dp0"

call "%SCRIPTS%port-test.bat"

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

    echo [API-TEST] 端口 %PORT% 已有 API 在运行: http://127.0.0.1:%PORT%/docs

    echo [API-TEST] 若需重启，请先运行 stop-api-test.bat

    goto stay

)

if exist ".\.venv\Scripts\python.exe" (

    .\.venv\Scripts\python.exe -c "import sys" >nul 2>&1

    if errorlevel 1 (

        echo [API-TEST] venv 无效（可能从其他电脑复制），正在重建...

        rmdir /s /q .venv

    )

)

if not exist ".\.venv\Scripts\python.exe" (

    echo [API-TEST] 首次运行：创建虚拟环境并 pip install，约 1-3 分钟...

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

    if exist ".\.env.test.example" (

        copy /Y .\.env.test.example .\.env >nul

    ) else (

        copy /Y .\.env.example .\.env >nul

    )

    echo [API-TEST] 已生成 .env（测试库 factory-api\data\factory-test.db）

)

if not exist ".\data\storage-test" mkdir ".\data\storage-test" 2>nul

echo [API-TEST] listen 0.0.0.0:%PORT%  (LAN / remote)

echo [API-TEST] docs  http://127.0.0.1:%PORT%/docs

echo [API-TEST] IIS 测试站见 scripts\iis-setup-test.txt

echo.

.\.venv\Scripts\python.exe run_server.py

if errorlevel 1 (

    echo [API-TEST] 启动失败。常见原因：端口 %PORT% 被占用 — 运行 stop-api-test.bat 后重试。

)

:stay

echo.

echo 关闭本窗口即停止测试 API。

pause
