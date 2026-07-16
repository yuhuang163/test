@echo off

echo [START] Lute Factory API (test)...

chcp 65001 >nul 2>&1

setlocal EnableExtensions

set "ROOT=%~dp0"

call "%ROOT%scripts\port-test.bat"

set "PORT=%FACTORY_API_PORT%"

curl.exe -s -o NUL -m 2 http://127.0.0.1:%PORT%/health >nul 2>&1

if not errorlevel 1 (

    echo.

    echo 端口 %PORT% 已有测试 API 在运行。

    echo   文档: http://127.0.0.1:%PORT%/docs

    echo.

    echo 若需重启，请先运行 stop-api-test.bat

    goto done

)

echo 正在打开 Lute-API-Test 窗口...

start "Lute-API-Test" /D "%ROOT%scripts" cmd /k _run-api-test.cmd

echo.

echo 首次运行请在 Lute-API-Test 窗口等待 pip 安装完成（约 1-3 分钟）。

echo.

echo   API 端口: %PORT%  (0.0.0.0)

echo   管理端:   IIS 指向 factory-admin\dist  (见 scripts\iis-setup-test.txt)

echo   停止:     stop-api-test.bat

echo.

:done

echo 本窗口可关闭；服务在 Lute-API-Test 窗口中运行。

ping 127.0.0.1 -n 4 >nul

endlocal

exit /b 0
