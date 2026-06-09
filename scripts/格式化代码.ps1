#Requires -Version 5.1
# 中文名入口，转发到 ASCII 脚本（避免 cmd/.bat 中文路径乱码）
& "$PSScriptRoot\format-code.ps1" @args
exit $LASTEXITCODE
