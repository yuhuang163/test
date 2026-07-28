@echo off
REM Pack remote_agent.exe (onefile) -> ..\build\...\bin\remote_agent\
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_exe.ps1" %*
set EXITCODE=%ERRORLEVEL%
endlocal
exit /b %EXITCODE%
