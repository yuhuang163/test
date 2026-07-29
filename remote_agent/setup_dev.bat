REM 用途：开发环境一键准备（创建 .venv 并安装依赖，不生成 _internal）
@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_dev.ps1" %*
set EXITCODE=%ERRORLEVEL%
endlocal
exit /b %EXITCODE%
