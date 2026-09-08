#Requires -Version 5.1
<#
.SYNOPSIS
    从协议 Markdown 发布到飞书（native 转换 + 表格列宽 + 高亮块），并自动授权。

.EXAMPLE
    .\scripts\创建飞书协议文档.ps1 -File .\docs\协议文档\ASD9026A协议.md

.EXAMPLE
    .\scripts\创建飞书协议文档.ps1 -DocId UFhddGCTGo7ibsxjpBzc3hX5n4b
#>
[CmdletBinding()]
param(
    [string] $File = "",
    [string] $Title = "",
    [string] $DocId = "",
    [string] $Email = $(if ($env:FEISHU_GRANT_EMAIL) { $env:FEISHU_GRANT_EMAIL } else { "joy.he@rootglobal.net" })
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$PublishPy = Join-Path $ScriptDir "发布飞书协议文档.py"
$GrantScript = Join-Path $ScriptDir "飞书应用云空间授权.ps1"

if (-not (Get-Command py -ErrorAction SilentlyContinue)) {
    throw "未找到 Python（py 命令）"
}

$resolvedDocId = $DocId
if ($File) {
    if (-not (Test-Path $File)) {
        throw "Markdown 文件不存在：$File"
    }
    $resolvedTitle = if ($Title) { $Title } else { [System.IO.Path]::GetFileNameWithoutExtension($File) }
    Write-Host "> 正在发布协议文档：$resolvedTitle" -ForegroundColor Cyan
    $pubOut = & py -3 $PublishPy --file (Resolve-Path $File).Path --title $resolvedTitle 2>&1 | Out-String
    Write-Host $pubOut
    if ($LASTEXITCODE -ne 0) {
        throw "发布脚本失败"
    }
    if ($pubOut -match "docx/([A-Za-z0-9]+)") {
        $resolvedDocId = $Matches[1]
    }
} elseif ($DocId) {
    Write-Host "> 正在优化已有文档：$DocId" -ForegroundColor Cyan
    & py -3 $PublishPy $DocId
    if ($LASTEXITCODE -ne 0) {
        throw "发布脚本失败"
    }
} else {
    throw "请指定 -File 或 -DocId"
}

if (-not $resolvedDocId) {
    throw "未能解析文档 ID"
}

Write-Host "> 正在授权..." -ForegroundColor Cyan
& $GrantScript -DocToken $resolvedDocId -Email $Email
if ($LASTEXITCODE -ne 0) {
    throw "授权失败"
}

Write-Host ""
Write-Host "完成：https://feishu.cn/docx/$resolvedDocId" -ForegroundColor Green
