#Requires -Version 5.1
<#
.SYNOPSIS
    new_product_test Release build (Qt 5.15.2 MSVC2019 x64 + jom)

.PARAMETER SkipQmake
    Skip qmake when .pro unchanged.

.PARAMETER Jobs
    jom parallel jobs. 0 = use logical CPU count (default).

.PARAMETER Clean
    Run jom clean before build.
#>
[CmdletBinding()]
param(
    [switch] $SkipQmake,
    [int] $Jobs = 0,
    [switch] $Clean
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ProFile = Join-Path $RepoRoot "new_production.pro"
$BuildDirName = "Desktop_Qt_5_15_2_MSVC2019_64bit-Release"
$BuildDir = Join-Path $RepoRoot "build\$BuildDirName"

$QtDir = if ($env:NEW_PRODUCT_QT_DIR) { $env:NEW_PRODUCT_QT_DIR } else { "D:\Qt\5.15.2\msvc2019_64" }
$Qmake = Join-Path $QtDir "bin\qmake.exe"
$Jom = if ($env:NEW_PRODUCT_JOM) { $env:NEW_PRODUCT_JOM } else { "D:\Qt\Tools\QtCreator\bin\jom\jom.exe" }

function Find-VcVars64 {
    if ($env:NEW_PRODUCT_VCVARS) {
        if (Test-Path $env:NEW_PRODUCT_VCVARS) { return $env:NEW_PRODUCT_VCVARS }
        throw "NEW_PRODUCT_VCVARS not found: $($env:NEW_PRODUCT_VCVARS)"
    }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere not found; set NEW_PRODUCT_VCVARS to vcvars64.bat"
    }
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if (-not $install) {
        $install = & $vswhere -latest -products * -property installationPath 2>$null
    }
    if (-not $install) { throw "Visual Studio not found" }
    $vcvars = Join-Path $install "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }
    return $vcvars
}

function Show-CompileErrors([string]$LogPath) {
    if (-not (Test-Path $LogPath)) { return }
    $errors = Select-String -Path $LogPath -Pattern "error C\d+:|fatal error C\d+:|: error LNK\d+:|jom: .*Error" -ErrorAction SilentlyContinue
    if ($errors) {
        Write-Host ""
        Write-Host "---------- compile errors (first 40) ----------" -ForegroundColor Red
        $errors | Select-Object -First 40 | ForEach-Object { Write-Host $_.Line }
        if ($errors.Count -gt 40) {
            Write-Host "... see log: $LogPath" -ForegroundColor Yellow
        }
    }
}

function Stop-RunningNewProduction {
    # 避免 LNK1104：正在运行的 exe 占用导致链接失败
    $procs = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -like "new_production*"
    })
    if ($procs.Count -eq 0) { return }
    foreach ($p in $procs) {
        Write-Host "kill running: $($p.ProcessName) (pid $($p.Id))" -ForegroundColor Yellow
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Milliseconds 500
}

if (-not (Test-Path $ProFile)) { throw "Missing: $ProFile" }
if (-not (Test-Path $Qmake)) { throw "Missing qmake: $Qmake (set NEW_PRODUCT_QT_DIR)" }
if (-not (Test-Path $Jom)) { throw "Missing jom: $Jom (set NEW_PRODUCT_JOM)" }

$VcVars = Find-VcVars64
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$binDir = Join-Path $BuildDir "bin"
New-Item -ItemType Directory -Force -Path $binDir | Out-Null
# TARGET 固定；可用 NEW_PRODUCT_BUILD_TARGET 覆盖（一般无需）
if (-not $env:NEW_PRODUCT_BUILD_TARGET) {
    $env:NEW_PRODUCT_BUILD_TARGET = "new_production"
}

$LogDir = Join-Path $RepoRoot "build\logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$LogFile = Join-Path $LogDir ("build_{0:yyyyMMdd_HHmmss}.log" -f (Get-Date))

if ($Jobs -le 0) {
    $Jobs = [int]((Get-CimInstance Win32_Processor |
        Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum)
    if ($Jobs -lt 1) { $Jobs = 8 }
}

Write-Host "repo:  $RepoRoot"
Write-Host "build: $BuildDir"
Write-Host "target: $env:NEW_PRODUCT_BUILD_TARGET"
Write-Host "qt:    $QtDir"
Write-Host "jobs:  $Jobs"
Write-Host "log:   $LogFile"
Write-Host ""
Stop-RunningNewProduction

function Get-StationEnableBlock([string]$ProPath) {
    $block = New-Object System.Collections.Generic.List[string]
    foreach ($line in Get-Content -LiteralPath $ProPath -Encoding UTF8) {
        if ($line -match '^ENABLE_STATION_') {
            [void]$block.Add($line.TrimEnd())
        }
    }
    return ($block -join "`n")
}

$forceQmake = $false
$stationBlock = Get-StationEnableBlock $ProFile
$stampPath = Join-Path $BuildDir ".enable_station_stamp"
$prevBlock = ""
if (Test-Path -LiteralPath $stampPath) {
    $prevBlock = Get-Content -LiteralPath $stampPath -Raw -Encoding UTF8
}
if ($stationBlock -ne $prevBlock) {
    Write-Host "[build] ENABLE_STATION_* changed: invalidate PCH and run qmake (avoid MSVC C4651)" -ForegroundColor Yellow
    $forceQmake = $true
    $relDir = Join-Path $BuildDir "release"
    foreach ($name in @("new_production_pch.pch", "new_production_pch.obj")) {
        $p = Join-Path $relDir $name
        if (Test-Path -LiteralPath $p) {
            Remove-Item -LiteralPath $p -Force
        }
    }
    Set-Content -LiteralPath $stampPath -Value $stationBlock -Encoding UTF8
}

Write-Host "========== Release build ==========" -ForegroundColor Cyan

# 必须与 Qt Creator Release 的 Effective qmake 一致，否则 Creator 点运行会判定
# Makefile 参数不匹配而重新 qmake，看起来像「脚本编过一遍、三角形又编一遍」。
# Creator 当前命令：qmake -o Makefile <pro> -spec win32-msvc "CONFIG+=qtquickcompiler"
$QmakeArgs = @(
    "-o", "Makefile",
    $ProFile,
    "-spec", "win32-msvc",
    "CONFIG+=qtquickcompiler"
)

if ($SkipQmake -and -not $forceQmake) {
    $makefilePath = Join-Path $BuildDir "Makefile"
    if (-not (Test-Path -LiteralPath $makefilePath)) {
        Write-Host "[build] Makefile missing: run qmake (Creator-compatible)" -ForegroundColor Yellow
        $forceQmake = $true
    } else {
        $makefileHead = Get-Content -LiteralPath $makefilePath -TotalCount 12 -ErrorAction SilentlyContinue
        $cmdLine = ($makefileHead | Where-Object { $_ -match '^\s*#\s*Command:' } | Select-Object -First 1)
        if ($cmdLine -and ($cmdLine -notmatch 'CONFIG\+=qtquickcompiler')) {
            Write-Host "[build] Makefile qmake args != Qt Creator; regenerating to match" -ForegroundColor Yellow
            $forceQmake = $true
        }
    }
}

$batLines = New-Object System.Collections.Generic.List[string]
[void]$batLines.Add("@echo off")
[void]$batLines.Add("setlocal")
[void]$batLines.Add("call `"$VcVars`" >nul 2>&1")
[void]$batLines.Add("if errorlevel 1 exit /b 1")
[void]$batLines.Add("cd /d `"$BuildDir`"")
if ((-not $SkipQmake) -or $forceQmake) {
    $qmakeCmd = "`"$Qmake`" " + (($QmakeArgs | ForEach-Object {
        if ($_ -match '[\s"]') { '"{0}"' -f ($_ -replace '"', '\"') } else { $_ }
    }) -join " ")
    [void]$batLines.Add($qmakeCmd)
    [void]$batLines.Add("if errorlevel 1 exit /b 1")
}
if ($Clean) {
    [void]$batLines.Add("`"$Jom`" clean")
    [void]$batLines.Add("if errorlevel 1 exit /b 1")
}
[void]$batLines.Add("`"$Jom`" -j$Jobs")
[void]$batLines.Add("exit /b %ERRORLEVEL%")

$batFile = Join-Path $env:TEMP ("new_product_build_{0}.cmd" -f [guid]::NewGuid().ToString("N"))
$batLines | Set-Content -Path $batFile -Encoding ASCII

$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    # jom/cl 会向 stderr 打进度，不能当作 PowerShell 异常
    $output = & cmd.exe /c "`"$batFile`""
    $exitCode = $LASTEXITCODE
    $output | Tee-Object -FilePath $LogFile
} finally {
    $ErrorActionPreference = $prevEap
    Remove-Item -Path $batFile -Force -ErrorAction SilentlyContinue
}

Show-CompileErrors -LogPath $LogFile

if ($exitCode -ne 0) {
    Write-Host ""
    Write-Host "BUILD FAILED (exit $exitCode)" -ForegroundColor Red
    exit $exitCode
}

$binDir = Join-Path $BuildDir "bin"
$targetName = if ($env:NEW_PRODUCT_BUILD_TARGET) { $env:NEW_PRODUCT_BUILD_TARGET } else { "new_production" }
$exePath = Join-Path $binDir ($targetName + ".exe")
if (Test-Path -LiteralPath $exePath) {
    $exe = Get-Item -LiteralPath $exePath
    Write-Host ""
    Write-Host "BUILD OK: $($exe.FullName)" -ForegroundColor Green
    Write-Host "time:   $($exe.LastWriteTime)"
} else {
    Write-Host ""
    Write-Host "BUILD OK but missing $exePath" -ForegroundColor Yellow
}

exit 0
