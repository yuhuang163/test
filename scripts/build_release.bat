@echo off
REM new_product_test Release 编译入口（双击或 cmd 均可）
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_release.ps1" %*
set EXITCODE=%ERRORLEVEL%
endlocal
exit /b %EXITCODE%
