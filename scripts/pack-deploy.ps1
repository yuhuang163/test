# Pack production deploy zip (no .venv / node_modules / local data)
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$Root = Split-Path $PSScriptRoot -Parent
$AdminDir = Join-Path $Root "factory-admin"
$ApiDir = Join-Path $Root "factory-api"
$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$Stage = Join-Path $env:TEMP "fwq-deploy-$Stamp"
$OutZip = Join-Path $Root "fwq-deploy-$Stamp.zip"

function Resolve-NpmCmd {
    $cmd = Get-Command npm.cmd -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        (Join-Path $env:ProgramFiles "nodejs\npm.cmd"),
        (Join-Path ${env:ProgramFiles(x86)} "nodejs\npm.cmd"),
        (Join-Path $env:LocalAppData "Programs\nodejs\npm.cmd")
    )
    foreach ($path in $candidates) {
        if ($path -and (Test-Path $path)) {
            $nodeDir = Split-Path $path -Parent
            $env:Path = "$nodeDir;$env:Path"
            return $path
        }
    }
    throw "npm not found. Install Node.js LTS from https://nodejs.org/ and reopen CMD."
}

if (-not $SkipBuild) {
    $npmCmd = Resolve-NpmCmd
    Write-Host "[pack] using $npmCmd"
    Push-Location $AdminDir
    try {
        if (-not (Test-Path "node_modules")) {
            Write-Host "[pack] npm install ..."
            & $npmCmd install
            if ($LASTEXITCODE -ne 0) { throw "npm install failed" }
        }
        Write-Host "[pack] npm run build ..."
        & $npmCmd run build
        if ($LASTEXITCODE -ne 0) { throw "npm run build failed" }
    }
    finally {
        Pop-Location
    }
}

$DistDir = Join-Path $AdminDir "dist"
if (-not (Test-Path (Join-Path $DistDir "index.html"))) {
    throw "factory-admin\dist\index.html missing. Run npm run build first."
}

Write-Host "[pack] staging -> $Stage"
if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage -Force | Out-Null

$ApiStage = Join-Path $Stage "factory-api"
$null = New-Item -ItemType Directory -Path $ApiStage -Force
robocopy $ApiDir $ApiStage /E /XD .venv __pycache__ data /XF .env *.db /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy factory-api failed: $LASTEXITCODE" }

$DataStage = Join-Path $ApiStage "data\storage"
New-Item -ItemType Directory -Path $DataStage -Force | Out-Null

$DistStage = Join-Path $Stage "factory-admin\dist"
robocopy $DistDir $DistStage /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy dist failed: $LASTEXITCODE" }

$ScriptsStage = Join-Path $Stage "scripts"
New-Item -ItemType Directory -Path $ScriptsStage -Force | Out-Null
Copy-Item (Join-Path $Root "scripts\port.bat") $ScriptsStage -Force
Copy-Item (Join-Path $Root "scripts\_run-api-prod.cmd") $ScriptsStage -Force
Copy-Item (Join-Path $Root "scripts\iis-setup.txt") $ScriptsStage -Force
Copy-Item (Join-Path $Root "scripts\coturn-setup.txt") $ScriptsStage -Force
Copy-Item (Join-Path $Root "scripts\remote-desktop-verify.txt") $ScriptsStage -Force

$AgentSrc = Join-Path $Root "remote-agent"
if (Test-Path $AgentSrc) {
    $AgentStage = Join-Path $Stage "remote-agent"
    robocopy $AgentSrc $AgentStage /E /XD .venv __pycache__ /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy remote-agent failed: $LASTEXITCODE" }
}

foreach ($bat in @("start-api-prod.bat", "stop-api-prod.bat")) {
    Copy-Item (Join-Path $Root $bat) $Stage -Force
}

if (Test-Path $OutZip) { Remove-Item $OutZip -Force }
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $OutZip -Force
Remove-Item $Stage -Recurse -Force

Write-Host ""
Write-Host "[pack] OK: $OutZip"
Write-Host "[pack] 1) Copy zip to server, extract to C:\inetpub\lute-factory (not Desktop)"
Write-Host "[pack] 2) On server run start-api-prod.bat"
Write-Host "[pack] 3) IIS -> factory-admin\dist (see scripts\iis-setup.txt)"
