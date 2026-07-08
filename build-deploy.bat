@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set OUT=%ROOT%\fwq-deploy

echo ================================
echo Building...
echo Output: %OUT%
echo ================================

echo [1/2] npm run build ...
pushd "%ROOT%\factory-admin"
call npm run build
if errorlevel 1 (
    popd
    echo ERROR: frontend build failed
    pause
    exit /b 1
)
popd

echo [2/2] copy files ...

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

echo ================================
echo Done! Output: %OUT%
echo ================================
pause
