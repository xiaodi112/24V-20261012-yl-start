param(
    [string]$Project = "Pressure_Regulating_Box/Projects/Wakeup_box_V0.0.1/MDK-ARM/GD32L233R_EVAL.uvprojx"
)

# 命令行全量构建脚本: 清空 Objects 后调用 Keil UV4, 轮询等待构建完成,
# 并以日志中的 "0 Error(s), 0 Warning(s)" 判定结果 (供 CI 使用).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$log  = Join-Path $root "build_verify.log"
$obj  = Join-Path $root "Pressure_Regulating_Box/Projects/Wakeup_box_V0.0.1/MDK-ARM/Objects"
$uv4  = "D:/keil/core/UV4/UV4.exe"

if(-not (Test-Path $uv4)) { throw "UV4 not found: $uv4" }

if(Test-Path $obj) {
    Get-ChildItem $obj -File | Remove-Item -Force
}

Write-Host "[build] clean rebuild: $Project"
& $uv4 -b (Join-Path $root $Project) -o $log -j0

# UV4 异步返回, 轮询日志直到出现 "Build Time Elapsed"
$deadline = (Get-Date).AddSeconds(240)
do {
    Start-Sleep -Milliseconds 1500
    $done = (Test-Path $log) -and (Select-String -Path $log -Pattern 'Build Time Elapsed' -Quiet)
} while(-not $done -and (Get-Date) -lt $deadline)

if(-not $done) { throw "Build timeout after 240s" }

$summary = Select-String -Path $log -Pattern 'Error\(s\)' | Select-Object -Last 1
if(-not $summary) { throw "No build summary in log" }
Write-Host $summary.Line
if($summary.Line -notmatch '0 Error\(s\), 0 Warning\(s\)') {
    Write-Error "Build failed: $($summary.Line)"
    exit 1
}
Write-Host "[build] OK"
