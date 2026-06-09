@echo off
REM new_product_test Release 编译入口（双击或 cmd 均可，内部调用 编译Release版本.ps1）
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0编译Release版本.ps1" %*
set EXITCODE=%ERRORLEVEL%
endlocal
exit /b %EXITCODE%
