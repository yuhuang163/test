#Requires -Version 5.1
<#
.SYNOPSIS
    为指定邮箱批量授予飞书「应用云空间」文件的可管理权限。

.DESCRIPTION
    读取 ~/.feishu-docx/tenant_token.json 中的应用 token，
    扫描应用云空间根目录下全部文件，逐个添加 full_access 协作者。
    对 docx/doc/sheet 等文档类型，可选开启组织内链接可读。

    注意：飞书 tenant 身份无法给文件夹批量授权，只能逐文件处理。

.PARAMETER Email
    被授权邮箱，默认 joy.he@rootglobal.net，也可用环境变量 FEISHU_GRANT_EMAIL 覆盖。

.PARAMETER DocToken
    仅处理单个文件 token（新建文档后只授权这一份时使用）。

.PARAMETER Type
    与 DocToken 配合使用，文件类型，默认 docx。

.PARAMETER SkipLinkShare
    跳过 link_share_entity=tenant_readable 设置。

.PARAMETER TokenPath
    tenant token 缓存路径，默认 %USERPROFILE%\.feishu-docx\tenant_token.json

.EXAMPLE
    .\scripts\飞书应用云空间授权.ps1

.EXAMPLE
    .\scripts\飞书应用云空间授权.ps1 -DocToken U4x9dhOWQoi4ZXxVVwVc6Gzrngd

.EXAMPLE
    $env:FEISHU_GRANT_EMAIL = "someone@example.com"
    .\scripts\飞书应用云空间授权.ps1
#>
[CmdletBinding()]
param(
    [string] $Email = $(if ($env:FEISHU_GRANT_EMAIL) { $env:FEISHU_GRANT_EMAIL } else { "joy.he@rootglobal.net" }),
    [string] $DocToken = "",
    [string] $Type = "docx",
    [switch] $SkipLinkShare,
    [string] $TokenPath = $(Join-Path $env:USERPROFILE ".feishu-docx\tenant_token.json")
)

$ErrorActionPreference = "Stop"

$DocTypesWithLinkShare = @("docx", "doc", "sheet", "bitable", "mindnote", "slides")

function Get-FeishuTenantToken {
    param([string] $Path)
    if (-not (Test-Path $Path)) {
        throw ("未找到 tenant token：{0}。请先运行：feishu-docx config set --app-id ... --app-secret ..." -f $Path)
    }
    $cached = Get-Content $Path -Raw | ConvertFrom-Json
    if (-not $cached.token) {
        throw "token 文件无效：$Path"
    }
    return $cached.token
}

function Invoke-FeishuApi {
    param(
        [string] $Method,
        [string] $Uri,
        [hashtable] $Headers,
        [string] $Body = $null
    )
    $params = @{
        Method  = $Method
        Uri     = $Uri
        Headers = $Headers
    }
    if ($Body) {
        $params.Body = $Body
    }
    return Invoke-RestMethod @params
}

function Grant-FeishuFileAccess {
    param(
        [string] $AccessToken,
        [hashtable] $Headers,
        [string] $FileToken,
        [string] $FileType,
        [string] $FileName,
        [string] $GrantEmail,
        [bool] $EnableLinkShare
    )

    $result = [ordered]@{
        Name = $FileName
        Type = $FileType
        Token = $FileToken
        Perm = ""
        Link = ""
    }

    $memberBody = (@{
        member_type = "email"
        member_id   = $GrantEmail
        perm        = "full_access"
        perm_type   = "container"
    } | ConvertTo-Json -Compress)

    try {
        $permResp = Invoke-FeishuApi -Method Post `
            -Uri "https://open.feishu.cn/open-apis/drive/v1/permissions/$FileToken/members?type=$FileType" `
            -Headers $Headers -Body $memberBody
        if ($permResp.code -eq 0) {
            $result.Perm = "full_access"
        } else {
            $result.Perm = "失败: $($permResp.msg)"
        }
    } catch {
        $result.Perm = "失败: $($_.Exception.Message)"
    }

    if ($EnableLinkShare -and ($FileType -in $DocTypesWithLinkShare)) {
        $publicBody = (@{
            type               = $FileType
            link_share_entity  = "tenant_readable"
        } | ConvertTo-Json -Compress)
        try {
            $linkResp = Invoke-FeishuApi -Method Patch `
                -Uri "https://open.feishu.cn/open-apis/drive/v1/permissions/$FileToken/public?type=$FileType" `
                -Headers $Headers -Body $publicBody
            if ($linkResp.code -eq 0) {
                $result.Link = "tenant_readable"
            } else {
                $result.Link = "失败: $($linkResp.msg)"
            }
        } catch {
            $result.Link = "失败: $($_.Exception.Message)"
        }
    }

    return [pscustomobject] $result
}

$tenantToken = Get-FeishuTenantToken -Path $TokenPath
$headers = @{
    Authorization = "Bearer $tenantToken"
    "Content-Type" = "application/json; charset=utf-8"
}

$files = @()
if ($DocToken) {
    $files += [pscustomobject]@{
        token = $DocToken
        type  = $Type
        name  = $DocToken
    }
} else {
    Write-Host "> 正在列出应用云空间文件..." -ForegroundColor Cyan
    $listResp = Invoke-FeishuApi -Method Get `
        -Uri "https://open.feishu.cn/open-apis/drive/v1/files?page_size=200" `
        -Headers @{ Authorization = "Bearer $tenantToken" }
    if ($listResp.code -ne 0) {
        throw "列出文件失败: $($listResp.msg)"
    }
    $files = @($listResp.data.files)
    if ($files.Count -eq 0) {
        Write-Host "应用云空间暂无文件。" -ForegroundColor Yellow
        exit 0
    }
}

Write-Host "> 正在为 $Email 授权 $($files.Count) 个文件..." -ForegroundColor Cyan
$enableLinkShare = -not $SkipLinkShare
$results = @()
foreach ($file in $files) {
    $results += Grant-FeishuFileAccess `
        -AccessToken $tenantToken `
        -Headers $headers `
        -FileToken $file.token `
        -FileType $file.type `
        -FileName $file.name `
        -GrantEmail $Email `
        -EnableLinkShare $enableLinkShare
}

$results | Format-Table -AutoSize

$permOk = @($results | Where-Object { $_.Perm -eq "full_access" }).Count
Write-Host ""
Write-Host "完成：$permOk / $($results.Count) 个文件已授予可管理权限。" -ForegroundColor Green
if ($permOk -lt $results.Count) {
    exit 1
}
