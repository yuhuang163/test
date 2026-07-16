@echo off

echo [STOP] Lute Factory API (test)...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

set "ROOT=%~dp0"

call "%ROOT%scripts\port-test.bat"

for /f "tokens=5" %%a in ('netstat -ano 2^>nul ^| findstr ":%FACTORY_API_PORT% " ^| findstr "LISTENING"') do (

    echo Kill port %FACTORY_API_PORT% PID=%%a

    taskkill /F /T /PID %%a >nul 2>&1

)

taskkill /F /FI "WINDOWTITLE eq Lute-API-Test*" >nul 2>&1

ping 127.0.0.1 -n 2 >nul

echo Done.

ping 127.0.0.1 -n 3 >nul

endlocal

exit /b 0
