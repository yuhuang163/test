param(
    [string]$BaseUrl = "https://fctp.luteos.com",
    [string]$Token = "",
    [string]$Username = "",
    [string]$Password = ""
)

function Log {
    param([string]$Text)
    $ts = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
    $line = "$ts`t$Text"
    Write-Host $line
    Add-Content -Path $LogFile -Value $line
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$LogFile = Join-Path $ScriptDir 'verify_endpoints.log'
Remove-Item $LogFile -ErrorAction SilentlyContinue

Log "Verify script started. BaseUrl=$BaseUrl"

function Try-Invoke {
    param($ScriptBlock, [string]$ErrMsg)
    try {
        & $ScriptBlock
    } catch {
        Log "$ErrMsg : $($_.Exception.Message)"
        return $false
    }
    return $true
}

# 1. Health
Log "== Health Check =="
Try-Invoke { $h = Invoke-RestMethod -Uri "$BaseUrl/health" -Method Get -UseBasicParsing } "Health check failed"
if ($null -ne $h) { Log "Health response: $($h | ConvertTo-Json -Depth 3)" }

# 2. Docs (status)
Log "== Docs =="
try {
    $r = Invoke-WebRequest -Uri "$BaseUrl/docs" -Method Get -UseBasicParsing -ErrorAction Stop
    Log "Docs HTTP status: $($r.StatusCode)"
} catch {
    Log "Docs request failed: $($_.Exception.Message)"
}

# 3. Login (if needed)
if ([string]::IsNullOrWhiteSpace($Token) -and -not [string]::IsNullOrWhiteSpace($Username)) {
    Log "== Login =="
    $loginBody = @{ username = $Username; password = $Password; hostName = $env:COMPUTERNAME } | ConvertTo-Json
    try {
        $resp = Invoke-RestMethod -Uri "$BaseUrl/api/factory-tool/auth/login" -Method Post -Body $loginBody -ContentType 'application/json' -UseBasicParsing
        if ($resp -and $resp.data -and $resp.data.accessToken) {
            $Token = $resp.data.accessToken
            Log "Login success, token obtained"
        } else {
            Log "Login response: $($resp | ConvertTo-Json -Depth 3)"
        }
    } catch {
        Log "Login failed: $($_.Exception.Message)"
    }
}

# 4. auth/me (if token present)
if (-not [string]::IsNullOrWhiteSpace($Token)) {
    Log "== auth/me =="
    try {
        $hdr = @{ Authorization = "Bearer $Token" }
        $me = Invoke-RestMethod -Uri "$BaseUrl/api/factory-tool/auth/me" -Method Get -Headers $hdr -UseBasicParsing
        Log "auth/me: $($me | ConvertTo-Json -Depth 3)"
    } catch {
        Log "auth/me failed: $($_.Exception.Message)"
    }
} else {
    Log "No token provided; skipping auth/me"
}

# 5. meta/factories
Log "== meta/factories =="
try {
    $fact = Invoke-RestMethod -Uri "$BaseUrl/api/factory-tool/admin/meta/factories" -Method Get -UseBasicParsing
    Log "factories: $($fact | ConvertTo-Json -Depth 3)"
} catch {
    Log "meta/factories failed: $($_.Exception.Message)"
}

# 6. prepare test zip
Log "== Prepare test zip =="
$tmp = [IO.Path]::GetTempPath()
$txt = Join-Path $tmp "verify_test.txt"
$zip = Join-Path $tmp "verify_test.zip"
Set-Content -Path $txt -Value "verify $(Get-Date -Format o)"
try {
    Compress-Archive -Path $txt -DestinationPath $zip -Force -ErrorAction Stop
    Log "Created zip: $zip"
} catch {
    Log "Compress-Archive failed: $($_.Exception.Message)"
}

# Use a specific existing zip for upload (uncomment and adjust if needed)
$userProvidedZip = $false
$provided = 'D:/code/test_30mb.zip'
if (Test-Path $provided) {
    $zip = $provided
    $userProvidedZip = $true
    Log "Using provided upload zip: $zip"
}

# 7. upload test (prefer curl.exe for verbose multipart)
Log "== Upload test =="
$uploadUrl = "$BaseUrl/api/factory-tool/logs/upload"
$curl = Get-Command curl.exe -ErrorAction SilentlyContinue
if ($curl) {
    $curlArgs = @('-v')
    if (-not [string]::IsNullOrWhiteSpace($Token)) { $curlArgs += ('-H'); $curlArgs += "Authorization: Bearer $Token" }
    $curlArgs += ('-F'); $curlArgs += "factoryName=DEMO"
    $curlArgs += ('-F'); $curlArgs += "deviceId=VERIFY-PC"
    $curlArgs += ('-F'); $curlArgs += "station=DEFAULT"
    $curlArgs += ('-F'); $curlArgs += "file=@$zip"
    $curlArgs += $uploadUrl
    Log "Running curl: $($curl.Path) $($curlArgs -join ' ')"
    try {
        # Use direct invocation with splatting to avoid PowerShell alias/array invocation issues
        $out = & $curl.Path @curlArgs 2>&1
        Add-Content -Path $LogFile -Value $out
        Log "curl output written to log"
    } catch {
        Log "curl invocation failed: $($_.Exception.Message)"
        # fallback: try Start-Process and capture output files
        try {
            $tmpOut = Join-Path $ScriptDir 'curl_out.txt'
            $tmpErr = Join-Path $ScriptDir 'curl_err.txt'
            Start-Process -FilePath $curl.Path -ArgumentList $curlArgs -NoNewWindow -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr -Wait
            $o = Get-Content $tmpOut -Raw -ErrorAction SilentlyContinue
            $e = Get-Content $tmpErr -Raw -ErrorAction SilentlyContinue
            Add-Content -Path $LogFile -Value "-- curl stdout --"; Add-Content -Path $LogFile -Value $o
            Add-Content -Path $LogFile -Value "-- curl stderr --"; Add-Content -Path $LogFile -Value $e
            Log "curl Start-Process fallback output written to log"
        } catch {
            Log "curl Start-Process fallback failed: $($_.Exception.Message)"
        }
    }
} else {
    Log "curl.exe not found, trying Invoke-RestMethod multipart (PowerShell 7+)"
    $hdr = @{}
    if (-not [string]::IsNullOrWhiteSpace($Token)) { $hdr['Authorization'] = "Bearer $Token" }
    try {
        $form = @{ factoryName='DEMO'; deviceId='VERIFY-PC'; station='DEFAULT'; file = Get-Item $zip }
        $resp = Invoke-RestMethod -Uri $uploadUrl -Method Post -Form $form -Headers $hdr -TimeoutSec 300 -UseBasicParsing
        Log "Upload response: $($resp | ConvertTo-Json -Depth 3)"
    } catch {
        Log "Upload failed (Invoke-RestMethod): $($_.Exception.Message)"
    }
}

Log "== Done =="
Log "Log file: $LogFile"

try {
    Remove-Item $txt -ErrorAction SilentlyContinue
    if (-not $userProvidedZip) { Remove-Item $zip -ErrorAction SilentlyContinue }
} catch {}
