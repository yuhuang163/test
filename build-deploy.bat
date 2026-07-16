@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set OUT=%ROOT%\fwq-deploy
set OUT_TEST=%ROOT%\fwq-deploy-test

echo ================================
echo Building...
echo Production: %OUT%
echo Test:       %OUT_TEST%
echo ================================

echo [1/3] npm run build ...
pushd "%ROOT%\factory-admin"
call npm run build
if errorlevel 1 (
    popd
    echo ERROR: frontend build failed
    pause
    exit /b 1
)
popd

echo [2/3] production deploy folder ...

if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%\factory-admin\dist" 2>nul
mkdir "%OUT%\factory-api\data\storage" 2>nul
mkdir "%OUT%\scripts" 2>nul

xcopy "%ROOT%\factory-admin\dist" "%OUT%\factory-admin\dist" /E /I /Q /Y >nul

robocopy "%ROOT%\factory-api" "%OUT%\factory-api" /E /XD .venv __pycache__ data /XF .env *.db /NFL /NDL /NJH /NJS /nc /ns /np >nul

copy "%ROOT%\scripts\port.bat" "%OUT%\scripts\" >nul
copy "%ROOT%\scripts\_run-api-prod.cmd" "%OUT%\scripts\" >nul
copy "%ROOT%\scripts\iis-setup.txt" "%OUT%\scripts\" >nul
if exist "%ROOT%\start-api-prod.bat" copy "%ROOT%\start-api-prod.bat" "%OUT%\" >nul
if exist "%ROOT%\stop-api-prod.bat" copy "%ROOT%\stop-api-prod.bat" "%OUT%\" >nul

echo [3/3] test deploy folder (server) ...

if exist "%OUT_TEST%" rmdir /s /q "%OUT_TEST%"
mkdir "%OUT_TEST%\factory-admin\dist" 2>nul
mkdir "%OUT_TEST%\factory-api\data\storage-test" 2>nul
mkdir "%OUT_TEST%\scripts" 2>nul

xcopy "%ROOT%\factory-admin\dist" "%OUT_TEST%\factory-admin\dist" /E /I /Q /Y >nul
copy /Y "%ROOT%\scripts\web.config.test" "%OUT_TEST%\factory-admin\dist\web.config" >nul

robocopy "%ROOT%\factory-api" "%OUT_TEST%\factory-api" /E /XD .venv __pycache__ data /XF .env *.db /NFL /NDL /NJH /NJS /nc /ns /np >nul

copy "%ROOT%\scripts\port-test.bat" "%OUT_TEST%\scripts\" >nul
copy "%ROOT%\scripts\_run-api-test.cmd" "%OUT_TEST%\scripts\" >nul
copy "%ROOT%\scripts\iis-setup-test.txt" "%OUT_TEST%\scripts\" >nul
copy "%ROOT%\scripts\start-api-test.bat" "%OUT_TEST%\start-api-test.bat" >nul
copy "%ROOT%\scripts\stop-api-test.bat" "%OUT_TEST%\stop-api-test.bat" >nul

copy "%ROOT%\fwq-deploy\factory-api\.gitignore" "%OUT_TEST%\factory-api\.gitignore" >nul 2>&1
if not exist "%OUT_TEST%\factory-api\.gitignore" (
    copy "%ROOT%\factory-api\.gitignore" "%OUT_TEST%\factory-api\.gitignore" >nul 2>&1
)

echo ================================
echo Done!
echo   Production: %OUT%
echo   Test:       %OUT_TEST%
echo.
echo Test server:
echo   API  start-api-test.bat  -^> port 8801
echo   IIS  factory-admin\dist  -^> see scripts\iis-setup-test.txt
echo ================================
pause
