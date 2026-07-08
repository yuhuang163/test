@echo off
echo [STOP] Lute Factory Platform...
chcp 65001 >nul 2>&1
setlocal EnableExtensions

set "ROOT=%~dp0"
call "%ROOT%scripts\port.bat"

for %%P in (%FACTORY_API_PORT% %FACTORY_ADMIN_PORT%) do (
    for /f "tokens=5" %%a in ('netstat -ano 2^>nul ^| findstr ":%%P " ^| findstr "LISTENING"') do (
        echo Kill port %%P PID=%%a
        taskkill /F /T /PID %%a >nul 2>&1
    )
)

taskkill /F /FI "WINDOWTITLE eq Lute-API*" >nul 2>&1
taskkill /F /FI "WINDOWTITLE eq Lute-Admin*" >nul 2>&1

ping 127.0.0.1 -n 2 >nul
echo Done.
ping 127.0.0.1 -n 3 >nul
endlocal
exit /b 0
