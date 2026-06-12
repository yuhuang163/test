#Requires -Version 5.1
<#
.SYNOPSIS
    使用 clang-format 批量格式化 C/C++ 源码（排除第三方与生成代码）。

.PARAMETER DryRun
    仅列出将格式化的文件，不写入。

.PARAMETER Paths
    限定目录或文件；默认扫描整个仓库。
#>
[CmdletBinding()]
param(
    [switch] $DryRun,
    [string[]] $Paths = @()
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-ClangFormatPath {
    if ($env:NEW_PRODUCT_CLANG_FORMAT -and (Test-Path $env:NEW_PRODUCT_CLANG_FORMAT)) {
        return $env:NEW_PRODUCT_CLANG_FORMAT
    }

    $candidates = @(
        "D:\Qt\Tools\QtCreator\bin\clang\bin\clang-format.exe",
        "C:\Qt\Tools\QtCreator\bin\clang\bin\clang-format.exe",
        "C:\Program Files\LLVM\bin\clang-format.exe",
        "C:\Program Files (x86)\LLVM\bin\clang-format.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    $cmd = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source -and (Test-Path $cmd.Source)) {
        return $cmd.Source
    }

    foreach ($qtRoot in @("C:\Qt", "D:\Qt")) {
        if (-not (Test-Path $qtRoot)) { continue }
        $found = Get-ChildItem -Path $qtRoot -Filter "clang-format.exe" -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if ($found) {
            return $found
        }
    }

    return $null
}

$ClangFormat = Resolve-ClangFormatPath

$ExcludeDirNames = @("build", ".git", "Python39", "node_modules", ".cursor")
$ExcludePathContains = @(
    "agreement/factory_protocol/protocol/qpb/Python39",
    "advance/xlsx",
    "lib/qcustomplot",
    "lib/libusb",
    "agreement/factory_protocol/protocol/qpb/ble_protocol",
    "agreement/factory_protocol/protocol/qpb/factory_protocol"
)
$Extensions = @(".cpp", ".h", ".hpp", ".cc", ".cxx", ".c")

function Test-ExcludedPath([string]$RelPosix) {
    foreach ($part in $ExcludePathContains) {
        if ($RelPosix -like "*$part*") {
            return $true
        }
    }
    return $false
}

function Get-FormatTargets([string[]]$InputPaths) {
    $targets = New-Object System.Collections.Generic.List[string]

    if ($InputPaths -and $InputPaths.Count -gt 0) {
        foreach ($inputPath in $InputPaths) {
            $full = if ([System.IO.Path]::IsPathRooted($inputPath)) { $inputPath } else { Join-Path $RepoRoot $inputPath }
            if (-not (Test-Path $full)) {
                throw "Path not found: $inputPath"
            }
            if (Test-Path $full -PathType Leaf) {
                $rel = (Resolve-Path $full).Path.Substring($RepoRoot.Length + 1).Replace("\", "/")
                if (-not (Test-ExcludedPath $rel)) {
                    $targets.Add($full)
                }
                continue
            }
            Get-ChildItem -Path $full -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
                $rel = $_.FullName.Substring($RepoRoot.Length + 1).Replace("\", "/")
                if ((Test-ExcludedPath $rel)) { return }
                if ($Extensions -contains $_.Extension.ToLower()) {
                    $targets.Add($_.FullName)
                }
            }
        }
        return $targets | Sort-Object -Unique
    }

    Get-ChildItem -Path $RepoRoot -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
        $rel = $_.FullName.Substring($RepoRoot.Length + 1).Replace("\", "/")
        if ((Test-ExcludedPath $rel)) { return }
        foreach ($dir in $ExcludeDirNames) {
            if ($rel -like "$dir/*" -or $rel -like "*/$dir/*") { return }
        }
        if ($Extensions -contains $_.Extension.ToLower()) {
            $_.FullName
        }
    } | Sort-Object -Unique
}

if (-not $ClangFormat) {
    throw @"
Missing clang-format.
Install LLVM or Qt Creator (with bundled clang-format), or set:
  NEW_PRODUCT_CLANG_FORMAT=C:\path\to\clang-format.exe
"@
}

$styleFile = Join-Path $RepoRoot ".clang-format"
if (-not (Test-Path $styleFile)) {
    throw "Missing .clang-format at repo root"
}

$files = @(Get-FormatTargets -InputPaths $Paths)
Write-Host "repo:          $RepoRoot"
Write-Host "clang-format:  $ClangFormat"
Write-Host "targets:       $($files.Count)"
Write-Host ""

if ($files.Count -eq 0) {
    Write-Host "No files to format."
    exit 0
}

if ($DryRun) {
    $files | ForEach-Object { Write-Host $_ }
    exit 0
}

$formatted = 0
foreach ($file in $files) {
    & $ClangFormat -i -style=file $file
    if ($LASTEXITCODE -ne 0) {
        throw "clang-format failed: $file (exit $LASTEXITCODE)"
    }
    $formatted++
}

Write-Host "Formatted $formatted file(s)."

$Utf8Script = Get-ChildItem -Path (Join-Path $RepoRoot "scripts") -Filter "*.py" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match "UTF8|utf8" } |
    Select-Object -First 1 -ExpandProperty FullName
if ($Utf8Script -and (Test-Path $Utf8Script)) {
    Write-Host "Ensuring UTF-8 (no BOM) for text sources..."
    $python = $null
    if (Get-Command py -ErrorAction SilentlyContinue) {
        $python = "py"
        $pythonArgs = @("-3", $Utf8Script, "--no-bom")
    } elseif (Get-Command python -ErrorAction SilentlyContinue) {
        $python = "python"
        $pythonArgs = @($Utf8Script, "--no-bom")
    }
    if ($python) {
        & $python @pythonArgs | Out-Host
    } else {
        Write-Warning "Python not found; skip UTF-8 normalization."
    }
}

Write-Host "Done."
