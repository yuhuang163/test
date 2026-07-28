# Pack single remote_agent.exe (no system Python, no _internal folder)
#   powershell -ExecutionPolicy Bypass -File .\build_exe.ps1
#
# Layout:
#   1) this folder = source (Git)
#   2) ..\build\...\bin\remote_agent\remote_agent.exe = runtime
# Do not put non-ASCII literals in this script (ANSI mis-decode on Windows).

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$venvPy = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
if (-not (Test-Path $venvPy)) {
    Write-Host "[remote_agent] create .venv ..."
    py -3 -m venv .venv
    & $venvPy -m pip install -U pip
    & $venvPy -m pip install -r requirements.txt
}
& $venvPy -m pip install -q "pyinstaller>=6.0"

$binDir = Join-Path $PSScriptRoot "..\build\Desktop_Qt_5_15_2_MSVC2019_64bit-Release\bin"
if (-not (Test-Path $binDir)) {
    throw "bin not found: $binDir (build host Release first)"
}
$outDir = Join-Path $binDir "remote_agent"
$workDir = Join-Path $PSScriptRoot "_pyi_work"

if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
# runtime folder keeps only the single exe (wipe old py/venv/_internal leftovers)
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "[remote_agent] pyinstaller onefile -> $outDir"
& $venvPy -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --name remote_agent `
    --console `
    --distpath $outDir `
    --workpath $workDir `
    --specpath $workDir `
    --collect-all aiortc `
    --collect-all av `
    --collect-all cv2 `
    --hidden-import dxcam `
    --hidden-import mss `
    --hidden-import PIL `
    --hidden-import numpy `
    --hidden-import websockets `
    main.py

$exe = Join-Path $outDir "remote_agent.exe"
if (-not (Test-Path $exe)) {
    throw "build failed: $exe missing"
}

$internal = Join-Path $outDir "_internal"
if (Test-Path $internal) { Remove-Item $internal -Recurse -Force }

if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }

$sizeMb = [math]::Round((Get-Item $exe).Length / 1MB, 1)
Write-Host "[remote_agent] OK: $exe ($sizeMb MB)"
Write-Host "[remote_agent] deploy only this one exe under bin\remote_agent\"
