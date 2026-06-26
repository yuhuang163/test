@echo off

echo [START] Lute Factory API (production)...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

set "ROOT=%~dp0"

call "%ROOT%scripts\port.bat"

set "PORT=%FACTORY_API_PORT%"

curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/health >nul 2>&1

if not errorlevel 1 (

    echo API already running on port %PORT%.

    echo   docs: http://127.0.0.1:%PORT%/docs

    goto done

)

echo Opening API window (Lute-API-Prod)...

start "Lute-API-Prod" /D "%ROOT%scripts" cmd /k _run-api-prod.cmd

echo.

echo   API port: %PORT%  (0.0.0.0, remote accessible)

echo   Admin UI: configure IIS -^> factory-admin\dist  (see scripts\iis-setup.txt)

echo   Stop:     stop-api-prod.bat

echo.

:done

ping 127.0.0.1 -n 3 >nul

endlocal

exit /b 0
