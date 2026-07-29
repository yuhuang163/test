# Pack remote_agent as onedir (fast start; onefile unpack costs ~10s+ each launch)
#   powershell -ExecutionPolicy Bypass -File .\build_exe.ps1
#
# Layout:
#   1) this folder = source (Git)
#   2) ..\build\...\bin\remote_agent\remote_agent.exe + _internal\ = runtime
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
$stageName = "_remote_agent_build"
$stageDir = Join-Path $binDir $stageName
$workDir = Join-Path $PSScriptRoot "_pyi_work"
$hooksDir = Join-Path $PSScriptRoot "pyi_hooks"

if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }

Write-Host "[remote_agent] pyinstaller onedir -> stage $stageDir"
# hook-dxcam: ship .pyd as binary; never archive Cython .c as Python source
$pyiArgs = @(
    "--noconfirm",
    "--clean",
    "--onedir",
    "--name", $stageName,
    "--console",
    "--distpath", $binDir,
    "--workpath", $workDir,
    "--specpath", $workDir,
    "--additional-hooks-dir", $hooksDir,
    "--collect-all", "aiortc",
    "--collect-all", "av",
    "--collect-all", "comtypes",
    "--hidden-import", "cv2",
    "--hidden-import", "dxcam",
    "--hidden-import", "mss",
    "--hidden-import", "PIL",
    "--hidden-import", "numpy",
    "--hidden-import", "websockets",
    "main.py"
)
& $venvPy -m PyInstaller @pyiArgs

$stageExe = Join-Path $stageDir ($stageName + ".exe")
$stageInternal = Join-Path $stageDir "_internal"
if (-not (Test-Path $stageExe)) { throw "build failed: $stageExe missing" }
if (-not (Test-Path $stageInternal)) { throw "build failed: $stageInternal missing" }

$stagedAgentExe = Join-Path $stageDir "remote_agent.exe"
if (Test-Path $stagedAgentExe) { Remove-Item $stagedAgentExe -Force }
Rename-Item $stageExe "remote_agent.exe"

$dxcamDir = Join-Path $stageInternal "dxcam"
if (Test-Path $dxcamDir) {
    Get-ChildItem $dxcamDir -Recurse -Include *.c, *.pyx -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
}
$pyd = Get-ChildItem $dxcamDir -Recurse -Filter "_numpy_kernels*.pyd" -ErrorAction SilentlyContinue
if (-not $pyd) {
    throw "build failed: dxcam _numpy_kernels.pyd missing (DXGI would fall back to mss)"
}

if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
Get-ChildItem $outDir -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
cmd /c "robocopy `"$stageDir`" `"$outDir`" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul"
if ($LASTEXITCODE -ge 8) { throw "robocopy failed code=$LASTEXITCODE" }
Remove-Item $stageDir -Recurse -Force -ErrorAction SilentlyContinue
if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }

$exe = Join-Path $outDir "remote_agent.exe"
$internal = Join-Path $outDir "_internal"
if (-not (Test-Path $exe)) { throw "install failed: $exe missing" }
if (-not (Test-Path $internal)) { throw "install failed: $internal missing" }

# Do not seed encoder_cache.json: each PC probes HW encoders on first run
$cache = Join-Path $outDir "encoder_cache.json"
if (Test-Path $cache) { Remove-Item $cache -Force -ErrorAction SilentlyContinue }

$sizeMb = [math]::Round(((Get-ChildItem $outDir -Recurse -File | Measure-Object Length -Sum).Sum) / 1MB, 1)
Write-Host "[remote_agent] OK: $exe (folder $sizeMb MB, onedir)"
Write-Host "[remote_agent] deploy whole bin\remote_agent\ folder (exe + _internal)"
