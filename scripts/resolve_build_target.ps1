# 计算当日上位机 TARGET：首个为 new_production_yyyyMMdd，同日后续为 -1/-2/…
param(
    [string]$BinDir = ""
)

if ([string]::IsNullOrWhiteSpace($BinDir)) {
    $BinDir = Join-Path (Get-Location) "bin"
}

$buildDay = Get-Date -Format "yyyyMMdd"
$prefix = "new_production_$buildDay"
$maxSeq = -1

if (Test-Path -LiteralPath $BinDir) {
    Get-ChildItem -LiteralPath $BinDir -Filter "${prefix}*.exe" -ErrorAction SilentlyContinue | ForEach-Object {
        $name = $_.BaseName
        if ($name -eq $prefix) {
            if ($maxSeq -lt 0) { $maxSeq = 0 }
        } elseif ($name -match "^${prefix}-(\d+)$") {
            $v = [int]$matches[1]
            if ($v -gt $maxSeq) { $maxSeq = $v }
        }
    }
}

$suffix = if ($maxSeq -lt 0) { "" } else { "-$($maxSeq + 1)" }
Write-Output "${prefix}${suffix}"
