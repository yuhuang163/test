@echo off

echo [START] Lute Factory API (production)...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

set "ROOT=%~dp0"

call "%ROOT%scripts\port.bat"

set "PORT=%FACTORY_API_PORT%"

curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/health >nul 2>&1

if not errorlevel 1 (

    echo.

    echo 端口 %PORT% 已有 API 在运行。

    echo   文档: http://127.0.0.1:%PORT%/docs

    echo.

    echo 若需重启「本目录」生产 API，请先运行 stop-api-prod.bat

    echo （8800 也可能被「启动管理平台」开发环境占用）

    goto done

)

echo 正在打开 Lute-API-Prod 窗口...

start "Lute-API-Prod" /D "%ROOT%scripts" cmd /k _run-api-prod.cmd

echo.

echo 首次运行请在 Lute-API-Prod 窗口等待 pip 安装完成（约 1-3 分钟）。

echo 请看任务栏「Lute-API-Prod」，安装结束后会显示 Uvicorn running。

echo.

echo   API 端口: %PORT%  (0.0.0.0，可局域网访问)

echo   管理端:   IIS 指向 factory-admin\dist  (见 scripts\iis-setup.txt)

echo   停止:     stop-api-prod.bat

echo.

:done

echo 本窗口可关闭；服务在 Lute-API-Prod 窗口中运行。

ping 127.0.0.1 -n 4 >nul

endlocal

exit /b 0
