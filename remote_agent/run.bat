REM 用途：启动远控 Agent（优先 exe，否则用本目录 .venv 跑 main.py）
@echo off
setlocal
cd /d "%~dp0"

REM 产线优先：已打包的独立 exe，无需本机 Python
if exist "remote_agent.exe" (
  "remote_agent.exe" %*
  exit /b %ERRORLEVEL%
)

if not exist ".venv\Scripts\python.exe" (
  echo [remote_agent] creating venv ...
  where py >nul 2>nul
  if errorlevel 1 (
    echo [remote_agent] 本机无 Python，且缺少 remote_agent.exe。
    echo [remote_agent] 请在开发机执行 build_exe.bat 后，把产物拷到本目录。
    exit /b 1
  )
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
