@echo off
REM new_product_test C/C++ 格式化入口（双击或 cmd 均可，内部调用 格式化代码.ps1）
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0format-code.ps1" %*
set EXITCODE=%ERRORLEVEL%
endlocal
exit /b %EXITCODE%
