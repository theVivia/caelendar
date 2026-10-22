# 修复 GitHub 访问：清理 hosts 中指向 127.0.0.1 的 github 域名
# 前置：火绒「暂停防护」中（否则 hosts 修改会被实时回滚）
# 用法（管理员 PowerShell）：pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/fix-github-hosts.ps1

$ErrorActionPreference = "Stop"
$hosts = "$env:SystemRoot\System32\drivers\etc\hosts"

# 1) 备份
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = "$env:SystemRoot\System32\drivers\etc\hosts.bak-$stamp"
Copy-Item $hosts $backup -Force
Write-Host "[1/4] 已备份到 $backup" -ForegroundColor Cyan

# 2) 删除 github 相关行（只删指向 127.0.0.1 的；其它域名保留）
$lines = Get-Content $hosts
$githubLines = $lines | Where-Object { $_ -match '(^|\s)127\.0\.0\.1\s+.*(github|githubusercontent|gist\.github|githubapp|githubassets)' }
Write-Host "[2/4] 将删除 $($githubLines.Count) 行："
$githubLines | ForEach-Object { Write-Host "      $_" }
$kept = $lines | Where-Object { $_ -notmatch '127\.0\.0\.1\s+.*(github|githubusercontent|gist\.github|githubapp|githubassets)' }
Set-Content -Path $hosts -Value $kept -Encoding ASCII -Force

# 3) 刷新 DNS
ipconfig /flushdns | Out-Null
Write-Host "[3/4] DNS 缓存已刷新" -ForegroundColor Cyan

# 4) 验证（等 3 秒确认没有被守护进程回滚）
Start-Sleep -Seconds 3
$remaining = (Get-Content $hosts | Select-String -Pattern 'github' | Measure-Object).Count
if ($remaining -gt 0) {
    Write-Host "[WARN] 还有 $remaining 行 github 条目，说明 hosts 仍被保护（火绒防护没暂停？）" -ForegroundColor Yellow
} else {
    Write-Host "[4/4] hosts 已清理干净，尝试访问 GitHub..." -ForegroundColor Cyan
    try {
        $r = Invoke-WebRequest -Uri 'https://github.com' -TimeoutSec 15 -UseBasicParsing -MaximumRedirection 2
        Write-Host "[OK] GitHub 可访问，status=$($r.StatusCode)" -ForegroundColor Green
    } catch {
        Write-Host "[FAIL] hosts 干净了但 GitHub 仍连不上：$($_.Exception.Message)" -ForegroundColor Red
        Write-Host "       可能需要再查 DNS 劫持（换 8.8.8.8/223.5.5.5）或运营商封锁" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "完成后记得把火绒防护恢复：托盘右键 → 恢复防护" -ForegroundColor Yellow
Write-Host "如需还原 hosts：Copy-Item '$backup' '$hosts' -Force" -ForegroundColor DarkGray
