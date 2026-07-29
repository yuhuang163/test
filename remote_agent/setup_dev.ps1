# Dev one-shot: create .venv and install deps (no _internal in git).
#   .\setup_dev.bat
# Host falls back to this folder when bin\remote_agent\exe is missing.
# Do not put non-ASCII literals in this script (ANSI mis-decode on Windows).

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$venvPy = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
if (-not (Test-Path $venvPy)) {
    Write-Host "[remote_agent] create .venv ..."
    py -3 -m venv .venv
}
Write-Host "[remote_agent] pip install -r requirements.txt ..."
& $venvPy -m pip install -U pip
& $venvPy -m pip install -r requirements.txt

Write-Host "[remote_agent] setup_dev OK"
Write-Host "[remote_agent] next: start host app; Agent dir fallback = this folder"
Write-Host "[remote_agent] for factory package: run .\build_exe.bat"
