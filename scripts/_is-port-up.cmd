@echo off
rem curl exit 0 = port/service up
set "PORT=%~1"
if "%PORT%"=="" exit /b 1
if "%PORT%"=="8800" (
    curl.exe -s -o NUL -m 2 http://127.0.0.1:8800/health
    exit /b %errorlevel%
)
curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/
exit /b %errorlevel%

