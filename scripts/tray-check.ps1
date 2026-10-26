$log = 'E:\myCalendar\logs\after-reboot-check.txt'
function W($msg) { try { ("[" + (Get-Date -Format 'HH:mm:ss') + "] " + $msg) | Out-File $log -Encoding UTF8 -Append } catch {} }
try {
  W "=== 自检开始 ==="
  if (Test-Path 'E:\myCalendar\logs\traytest.exe') {
    Remove-Item 'E:\myCalendar\logs\traytest-result.txt' -Force -ErrorAction SilentlyContinue
    Start-Process -FilePath 'E:\myCalendar\logs\traytest.exe' | Out-Null
    Start-Sleep -Seconds 6
    Get-Process traytest -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    if (Test-Path 'E:\myCalendar\logs\traytest-result.txt') {
      W ("traytest: " + (Get-Content 'E:\myCalendar\logs\traytest-result.txt' -Raw).Trim())
    } else { W "traytest: 无结果" }
  }
  W "启动星历"
  Start-Process -FilePath 'E:\myCalendar\星历.exe' -WorkingDirectory 'E:\myCalendar' | Out-Null
  Start-Sleep -Seconds 8
  if (Test-Path 'E:\myCalendar\data\debug.log') {
    $lines = Get-Content 'E:\myCalendar\data\debug.log' -Tail 20
    $tray = $lines | Select-String 'tray' | Select-Object -First 6
    if ($tray) { foreach ($t in $tray) { W ("星历: " + $t.Line.Trim()) } } else { W "星历: 无 tray 记录" }
  }
  W "=== 自检结束 ==="
} catch { W ("异常: " + $_.Exception.Message) }