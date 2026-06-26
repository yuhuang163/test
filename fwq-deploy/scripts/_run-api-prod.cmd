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

curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/health >nul 2>&1

if not errorlevel 1 (

    echo [API-PROD] already running: http://127.0.0.1:%PORT%/docs

    goto stay

)

if exist ".\.venv\Scripts\python.exe" (

    .\.venv\Scripts\python.exe -c "import sys" >nul 2>&1

    if errorlevel 1 (

        echo [API-PROD] venv invalid, recreating...

        rmdir /s /q .venv

    )

)

if not exist ".\.venv\Scripts\python.exe" (

    echo [API-PROD] first run: creating venv, pip install 1-3 min...

    py -3 -m venv .venv

    if errorlevel 1 (

        echo ERROR: Python 3 not found. Install Python 3.10+ and add to PATH.

        goto stay

    )

    set PYTHONUTF8=1

    set PYTHONIOENCODING=utf-8

    .\.venv\Scripts\python.exe -m pip install --upgrade pip

    .\.venv\Scripts\python.exe -m pip install -r requirements.txt

    if errorlevel 1 (

        echo ERROR: pip install failed

        goto stay

    )

)

if not exist ".\.env" (

    if exist ".\.env.production.example" (

        copy /Y .\.env.production.example .\.env >nul

    ) else (

        copy /Y .\.env.example .\.env >nul

    )

    echo [API-PROD] created .env - edit CORS_ORIGINS and SECRET_KEY before public access

)

echo [API-PROD] listen 0.0.0.0:%PORT%  (LAN / remote)

echo [API-PROD] docs  http://127.0.0.1:%PORT%/docs

echo [API-PROD] IIS admin site proxies /api -^> this port

echo.

.\.venv\Scripts\uvicorn.exe app.main:app --host 0.0.0.0 --port %PORT%

if errorlevel 1 echo [API-PROD] start failed

:stay

echo.

echo Close this window to stop API.

pause
