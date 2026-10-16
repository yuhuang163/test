@echo off
REM new_product_test Release 编译（纯 cmd，不经过 PowerShell）
REM 用法：
REM   scripts\编译Release版本.bat
REM   scripts\编译Release版本.bat -SkipQmake
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

set "SKIP_QMAKE=0"
set "DO_CLEAN=0"
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="-SkipQmake" set "SKIP_QMAKE=1"
if /i "%~1"=="/SkipQmake" set "SKIP_QMAKE=1"
if /i "%~1"=="-Clean" set "DO_CLEAN=1"
if /i "%~1"=="/Clean" set "DO_CLEAN=1"
shift
goto parse_args
:args_done

set "REPO=%CD%"
set "PRO=%REPO%\new_production.pro"
set "BUILD_DIR=%REPO%\build\Desktop_Qt_5_15_2_MSVC2019_64bit-Release"
set "LOG_DIR=%REPO%\build\logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
REM 日志时间戳：不用 PowerShell（本机常卡）
set "STAMP=%DATE:~0,4%%DATE:~5,2%%DATE:~8,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%"
set "STAMP=%STAMP: =0%"
set "STAMP=%STAMP:/=%"
set "STAMP=%STAMP::=%"
set "LOG=%LOG_DIR%\build_%STAMP%.log"

REM ---- Qt ----
if defined NEW_PRODUCT_QT_DIR (
  set "QT_DIR=%NEW_PRODUCT_QT_DIR%"
) else if exist "F:\qt5152\5.15.2\msvc2019_64\bin\qmake.exe" (
  set "QT_DIR=F:\qt5152\5.15.2\msvc2019_64"
) else if exist "D:\Qt\5.15.2\msvc2019_64\bin\qmake.exe" (
  set "QT_DIR=D:\Qt\5.15.2\msvc2019_64"
) else if exist "C:\Qt\5.15.2\msvc2019_64\bin\qmake.exe" (
  set "QT_DIR=C:\Qt\5.15.2\msvc2019_64"
) else if exist "E:\Qt\5.15.2\msvc2019_64\bin\qmake.exe" (
  set "QT_DIR=E:\Qt\5.15.2\msvc2019_64"
) else (
  echo [ERROR] qmake not found. Set NEW_PRODUCT_QT_DIR
  exit /b 1
)
set "QMAKE=%QT_DIR%\bin\qmake.exe"

REM ---- jom ----
if defined NEW_PRODUCT_JOM (
  set "JOM=%NEW_PRODUCT_JOM%"
) else if exist "F:\Qt19\bin\jom\jom.exe" (
  set "JOM=F:\Qt19\bin\jom\jom.exe"
) else if exist "D:\Qt\Tools\QtCreator\bin\jom\jom.exe" (
  set "JOM=D:\Qt\Tools\QtCreator\bin\jom\jom.exe"
) else if exist "C:\Qt\Tools\QtCreator\bin\jom\jom.exe" (
  set "JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe"
) else if exist "E:\Qt\Tools\QtCreator\bin\jom\jom.exe" (
  set "JOM=E:\Qt\Tools\QtCreator\bin\jom\jom.exe"
) else (
  echo [ERROR] jom not found. Set NEW_PRODUCT_JOM
  exit /b 1
)

REM ---- vcvars64 ----
if defined NEW_PRODUCT_VCVARS (
  set "VCVARS=%NEW_PRODUCT_VCVARS%"
) else (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  set "VCVARS="
  if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
      if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)
if not defined VCVARS (
  echo [ERROR] vcvars64.bat not found. Set NEW_PRODUCT_VCVARS
  exit /b 1
)

if not exist "%PRO%" (
  echo [ERROR] missing %PRO%
  exit /b 1
)
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BUILD_DIR%\bin" mkdir "%BUILD_DIR%\bin"
if not defined NEW_PRODUCT_BUILD_TARGET set "NEW_PRODUCT_BUILD_TARGET=new_production"

set "JOBS=%NUMBER_OF_PROCESSORS%"
if not defined JOBS set "JOBS=8"
if "%JOBS%"=="0" set "JOBS=8"

echo repo:  %REPO%
echo build: %BUILD_DIR%
echo qt:    %QT_DIR%
echo jom:   %JOM%
echo jobs:  %JOBS%
echo skipq: %SKIP_QMAKE%
echo log:   %LOG%
echo.

REM 避免 LNK1104：杀掉占用中的 exe（静默）
taskkill /F /IM new_production.exe >nul 2>&1
for /f "tokens=2 delims=," %%P in ('tasklist /FO CSV /NH 2^>nul ^| findstr /I "new_production"') do (
  taskkill /F /PID %%~P >nul 2>&1
)

set "NEED_QMAKE=1"
if "%SKIP_QMAKE%"=="1" (
  if exist "%BUILD_DIR%\Makefile" (
    set "NEED_QMAKE=0"
  ) else (
    echo [build] Makefile missing: will run qmake
  )
)

echo ========== Release build (cmd) ==========
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] vcvars64 failed
  exit /b 1
)

cd /d "%BUILD_DIR%"
if "!NEED_QMAKE!"=="1" (
  echo [build] qmake ...
  "%QMAKE%" -o Makefile "%PRO%" -spec win32-msvc "CONFIG+=qtquickcompiler" >>"%LOG%" 2>&1
  if errorlevel 1 (
    echo BUILD FAILED: qmake
    type "%LOG%"
    exit /b 1
  )
) else (
  echo [build] skip qmake
)

if "%DO_CLEAN%"=="1" (
  echo [build] jom clean ...
  "%JOM%" clean >>"%LOG%" 2>&1
  if errorlevel 1 (
    echo BUILD FAILED: jom clean
    exit /b 1
  )
)

echo [build] jom -j%JOBS% ...
"%JOM%" -j%JOBS% >>"%LOG%" 2>&1
set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo ---------- compile errors ----------
  findstr /I /C:"error C" /C:"fatal error C" /C:"error LNK" /C:"jom:" "%LOG%" 2>nul
  echo.
  echo BUILD FAILED (exit !EC!)
  echo log: %LOG%
  exit /b !EC!
)

set "EXE=%BUILD_DIR%\bin\%NEW_PRODUCT_BUILD_TARGET%.exe"
if exist "%EXE%" (
  echo.
  echo BUILD OK: %EXE%
  for %%A in ("%EXE%") do echo time:   %%~tA
) else (
  echo.
  echo BUILD OK but missing %EXE%
)
exit /b 0
