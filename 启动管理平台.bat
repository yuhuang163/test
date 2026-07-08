@echo off
echo [START] Lute Factory Platform...
chcp 65001 >nul 2>&1
setlocal EnableExtensions

set "ROOT=%~dp0"
call "%ROOT%scripts\port.bat"
set "PORT=%FACTORY_API_PORT%"
set "ADMIN_PORT=%FACTORY_ADMIN_PORT%"

echo.
echo ========================================
echo   Lute Factory Platform
echo ========================================
echo.

set "API_UP=0"
set "ADMIN_UP=0"

curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/health >nul 2>&1
if not errorlevel 1 set "API_UP=1"

curl.exe -s -o NUL -m 2 http://127.0.0.1:%ADMIN_PORT%/ >nul 2>&1
if not errorlevel 1 set "ADMIN_UP=1"

if "%API_UP%"=="1" if "%ADMIN_UP%"=="1" (
    echo Services already running.
    echo   Web: http://127.0.0.1:%ADMIN_PORT%
    echo   API: http://127.0.0.1:%PORT%/docs
    start "" http://127.0.0.1:%ADMIN_PORT%
    goto done
)

if "%API_UP%"=="0" (
    echo Opening API window ^(Lute-API^)...
    start "Lute-API" /D "%ROOT%scripts" cmd /k _run-api.cmd
)

if "%ADMIN_UP%"=="0" (
    echo Opening Admin window ^(Lute-Admin^)...
    start "Lute-Admin" /D "%ROOT%scripts" cmd /k _run-admin.cmd
)

echo.
echo Check taskbar for Lute-API and Lute-Admin windows.
echo First run may take 1-3 min ^(pip/npm install^).
echo.
echo   Web: http://127.0.0.1:%ADMIN_PORT%
echo   API: http://127.0.0.1:%PORT%/docs
echo   Login: admin / admin123
echo.
echo Stop: 停止管理平台.bat
echo ========================================

ping 127.0.0.1 -n 4 >nul
start "" http://127.0.0.1:%ADMIN_PORT%

:done
echo.
echo This window can be closed. Services run in Lute-API / Lute-Admin.
ping 127.0.0.1 -n 3 >nul
endlocal
exit /b 0
