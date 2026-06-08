#Requires -Version 5.1
<#
.SYNOPSIS
    new_product_test Release build (Qt 5.15.2 MSVC2019 x64 + jom)

.PARAMETER SkipQmake
    Skip qmake when .pro unchanged.

.PARAMETER Jobs
    jom parallel jobs (default 8).

.PARAMETER Clean
    Run jom clean before build.
#>
[CmdletBinding()]
param(
    [switch] $SkipQmake,
    [int] $Jobs = 8,
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

if (-not (Test-Path $ProFile)) { throw "Missing: $ProFile" }
if (-not (Test-Path $Qmake)) { throw "Missing qmake: $Qmake (set NEW_PRODUCT_QT_DIR)" }
if (-not (Test-Path $Jom)) { throw "Missing jom: $Jom (set NEW_PRODUCT_JOM)" }

$VcVars = Find-VcVars64
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$LogDir = Join-Path $RepoRoot "build\logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$LogFile = Join-Path $LogDir ("build_{0:yyyyMMdd_HHmmss}.log" -f (Get-Date))

Write-Host "repo:  $RepoRoot"
Write-Host "build: $BuildDir"
Write-Host "qt:    $QtDir"
Write-Host "log:   $LogFile"
Write-Host ""
Write-Host "========== Release build ==========" -ForegroundColor Cyan

$batLines = New-Object System.Collections.Generic.List[string]
[void]$batLines.Add("@echo off")
[void]$batLines.Add("setlocal")
[void]$batLines.Add("call `"$VcVars`" >nul 2>&1")
[void]$batLines.Add("if errorlevel 1 exit /b 1")
[void]$batLines.Add("cd /d `"$BuildDir`"")
if (-not $SkipQmake) {
    [void]$batLines.Add("`"$Qmake`" `"$ProFile`" -spec win32-msvc CONFIG+=release")
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
$exes = @(Get-ChildItem -Path $binDir -Filter "new_production_*.exe" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
if ($exes.Count -gt 0) {
    Write-Host ""
    Write-Host "BUILD OK: $($exes[0].FullName)" -ForegroundColor Green
    Write-Host "time:   $($exes[0].LastWriteTime)"
} else {
    Write-Host ""
    Write-Host "BUILD OK but no new_production_*.exe in bin" -ForegroundColor Yellow
}

exit 0
