@echo off
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-git-hooks.ps1"
exit /b %ERRORLEVEL%
