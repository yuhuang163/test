@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo [remote_agent] creating venv ...
  py -3 -m venv .venv
  ".venv\Scripts\python.exe" -m pip install -r requirements.txt
) else (
  REM 已有 venv 时补装 dxcam（同机自控防浏览器截黑）
  ".venv\Scripts\python.exe" -c "import dxcam,cv2" >nul 2>nul
  if errorlevel 1 (
    echo [remote_agent] installing screen deps ^(dxcam^) ...
    ".venv\Scripts\python.exe" -m pip install -r requirements.txt
  )
)
".venv\Scripts\python.exe" main.py %*
