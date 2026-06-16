@echo off

echo [Admin] starting...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

title Lute-Admin



set "SCRIPTS=%~dp0"

call "%SCRIPTS%port.bat"

set "ADMIN_PORT=%FACTORY_ADMIN_PORT%"

set "ROOT=%SCRIPTS%.."

cd /d "%ROOT%\factory-admin"



set "NPM_CMD="

where npm >nul 2>&1

if not errorlevel 1 (

    set "NPM_CMD=npm"

) else if exist "%ProgramFiles%\nodejs\npm.cmd" (

    set "PATH=%ProgramFiles%\nodejs;%PATH%"

    set "NPM_CMD=%ProgramFiles%\nodejs\npm.cmd"

) else if exist "%LocalAppData%\Programs\nodejs\npm.cmd" (

    set "PATH=%LocalAppData%\Programs\nodejs;%PATH%"

    set "NPM_CMD=%LocalAppData%\Programs\nodejs\npm.cmd"

)



if not defined NPM_CMD (

    echo ERROR: npm not found. Install Node.js LTS.

    goto stay

)



curl.exe -s -o NUL -m 2 http://127.0.0.1:%ADMIN_PORT%/ >nul 2>&1

if not errorlevel 1 (

    echo [Admin] already running: http://127.0.0.1:%ADMIN_PORT%

    goto stay

)



if not exist ".\node_modules" (

    echo [Admin] first run: npm install 1-2 min...

    call "%NPM_CMD%" install

    if errorlevel 1 (

        echo ERROR: npm install failed

        goto stay

    )

)



echo [Admin] http://127.0.0.1:%ADMIN_PORT%
echo [Admin] use 127.0.0.1 not localhost if browser fails

echo.



call "%NPM_CMD%" run dev

if errorlevel 1 echo [Admin] start failed



:stay

echo.

echo Close this window to stop Admin.

pause

