# 将本仓库的 Git 钩子目录设为 .githooks（提交说明校验等）。
# 每个克隆/工作副本只需执行一次；之后 git pull 无需再执行。
# 用法：
#   - 推荐（绕过执行策略）：在仓库根目录执行
#       powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup-git-hooks.ps1
#     或双击/运行：  scripts\setup-git-hooks.cmd
#   - 若直接执行 .\scripts\setup-git-hooks.ps1 报「禁止运行脚本」，说明本机 ExecutionPolicy 较严，请用上面两种方式之一。
#   - 在「命令提示符 cmd」里勿直接输入 .ps1（易被当成用默认程序打开），请用：  scripts\setup-git-hooks.cmd
#   - 仅当前 PowerShell 窗口放宽（可选）：  Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel 2>$null
if (-not $root) {
    Write-Error "请在 Git 仓库内执行本脚本。"
}
Set-Location $root
git config core.hooksPath .githooks
$val = git config --get core.hooksPath
if ($val -ne ".githooks") {
    Write-Error "设置失败，当前 core.hooksPath=$val"
}
Write-Host "已设置 core.hooksPath=.githooks，提交时将运行 .githooks/commit-msg"
