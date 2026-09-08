#Requires -Version 5.1
<#
.SYNOPSIS
    在飞书应用云空间创建文档，并自动为指定邮箱授予可管理权限。

.PARAMETER Title
    文档标题。

.PARAMETER Content
    Markdown 正文。

.PARAMETER File
    从 Markdown 文件创建。

.PARAMETER Email
    被授权邮箱，默认 joy.he@rootglobal.net。

.PARAMETER SkipLinkShare
    跳过组织内链接可读设置。

.PARAMETER GrantAll
    创建成功后，对应用云空间全部文件重新授权（而不只是新文档）。

.EXAMPLE
    .\scripts\创建飞书文档.ps1 -Title "周报" -Content "# 本周工作`n- 事项一"

.EXAMPLE
    .\scripts\创建飞书文档.ps1 -Title "周报" -File .\docs\周报\周报_2026-09-07_2026-09-08.md
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Title,
    [string] $Content = "",
    [string] $File = "",
    [string] $Email = $(if ($env:FEISHU_GRANT_EMAIL) { $env:FEISHU_GRANT_EMAIL } else { "joy.he@rootglobal.net" }),
    [switch] $SkipLinkShare,
    [switch] $GrantAll
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$GrantScript = Join-Path $ScriptDir "飞书应用云空间授权.ps1"

if (-not (Get-Command feishu-docx -ErrorAction SilentlyContinue)) {
    throw "未找到 feishu-docx，请先安装：uv tool install feishu-docx"
}

$createArgs = @("create", $Title)
if ($File) {
    if (-not (Test-Path $File)) {
        throw "Markdown 文件不存在：$File"
    }
    $createArgs += @("-f", (Resolve-Path $File).Path)
} elseif ($Content) {
    $createArgs += @("-c", $Content)
}

Write-Host "> 正在创建飞书文档：$Title" -ForegroundColor Cyan
$createOutput = & feishu-docx @createArgs 2>&1 | Out-String
Write-Host $createOutput

if ($LASTEXITCODE -ne 0) {
    throw "feishu-docx create 失败（exit $LASTEXITCODE）"
}

$docId = $null
if ($createOutput -match "docx/([A-Za-z0-9]+)") {
    $docId = $Matches[1]
} elseif ($createOutput -match "文档 ID:\s*([A-Za-z0-9]+)") {
    $docId = $Matches[1]
}

if (-not $docId) {
    throw "无法从 feishu-docx 输出中解析文档 ID，请手动运行授权脚本。"
}

Write-Host "> 文档 ID: $docId" -ForegroundColor Cyan

$grantParams = @{
    Email = $Email
}
if ($SkipLinkShare) {
    $grantParams.SkipLinkShare = $true
}
if ($GrantAll) {
    Write-Host "> 正在为应用云空间全部文件授权..." -ForegroundColor Cyan
} else {
    $grantParams.DocToken = $docId
    $grantParams.Type = "docx"
    Write-Host "> 正在为新文档授权..." -ForegroundColor Cyan
}

& $GrantScript @grantParams
if ($LASTEXITCODE -ne 0) {
    throw "授权脚本执行失败"
}

$url = "https://feishu.cn/docx/$docId"
Write-Host ""
Write-Host "文档已创建并授权：" -ForegroundColor Green
Write-Host $url
