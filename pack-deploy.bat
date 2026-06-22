@echo off

echo [PACK] Build frontend and create deploy zip...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

set "ROOT=%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%scripts\pack-deploy.ps1" %*

if errorlevel 1 (

    echo.

    echo ERROR: pack failed

    pause

    exit /b 1

)

echo.

pause

endlocal

exit /b 0
