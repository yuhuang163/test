@echo off

echo [API] starting...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

title Lute-API



set "SCRIPTS=%~dp0"

call "%SCRIPTS%port.bat"

set "PORT=%FACTORY_API_PORT%"

set "ROOT=%SCRIPTS%.."

cd /d "%ROOT%\factory-api"



curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/health >nul 2>&1

if not errorlevel 1 (

    echo [API] already running: http://127.0.0.1:%PORT%/docs

    goto stay

)



if not exist ".\.venv\Scripts\python.exe" (

    echo [API] first run: creating venv, pip install 1-3 min...

    py -3 -m venv .venv

    if errorlevel 1 (

        echo ERROR: Python 3 not found

        goto stay

    )

    .\.venv\Scripts\python.exe -m pip install -r requirements.txt

    if errorlevel 1 (

        echo ERROR: pip install failed

        goto stay

    )

)



if not exist ".\.env" (

    copy /Y .\.env.example .\.env >nul

    echo [API] created .env

)



echo [API] http://127.0.0.1:%PORT%

echo [API] docs  http://127.0.0.1:%PORT%/docs

echo.



.\.venv\Scripts\uvicorn.exe app.main:app --host 127.0.0.1 --port %PORT%

if errorlevel 1 echo [API] start failed



:stay

echo.

echo Close this window to stop API.

pause

