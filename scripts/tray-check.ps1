# 重启后托盘自检（由 DSH 部署，带容错）
$log = 'E:\myCalendar\logs\after-reboot-check.txt'
function W($msg) { try { ("[" + (Get-Date -Format 'HH:mm:ss') + "] " + $msg) | Out-File $log -Encoding UTF8 -Append } catch {} }
try {
  W "=== 开机自检开始 ==="
  Start-Sleep -Seconds 20
  W "1) 独立托盘测试"
  if (Test-Path 'E:\myCalendar\logs\traytest.exe') {
    Remove-Item 'E:\myCalendar\logs\traytest-result.txt' -Force -ErrorAction SilentlyContinue
    try {
      $p = Start-Process -FilePath 'E:\myCalendar\logs\traytest.exe' -PassThru
      $p.WaitForExit(30000) | Out-Null
    } catch { W ("traytest 启动失败: " + $_.Exception.Message) }
    Start-Sleep -Seconds 1
    if (Test-Path 'E:\myCalendar\logs\traytest-result.txt') {
      W ("traytest 结果: " + (Get-Content 'E:\myCalendar\logs\traytest-result.txt' -Raw).Trim())
    } else { W "traytest 无结果文件" }
  } else { W "traytest.exe 不存在" }
  W "2) 启动星历并读取托盘日志"
  $before = 0
  if (Test-Path 'E:\myCalendar\data\debug.log') { $before = (Get-Content 'E:\myCalendar\data\debug.log' -Raw).Length }
  Start-Process -FilePath 'E:\myCalendar\星历.exe' -WorkingDirectory 'E:\myCalendar'
  Start-Sleep -Seconds 8
  if (Test-Path 'E:\myCalendar\data\debug.log') {
    $after = Get-Content 'E:\myCalendar\data\debug.log' -Raw
    if ($after.Length -gt $before) {
      $new = $after.Substring($before)
      $tray = ($new -split "`n") | Select-String 'tray' | Select-Object -First 6
      if ($tray) { foreach ($t in $tray) { W ("星历: " + $t.Line.Trim()) } } else { W "星历日志: 无 tray 记录" }
    } else { W "星历日志: 无新内容" }
  }
  W "=== 自检结束 ==="
} catch {
  W ("异常: " + $_.Exception.Message)
}
try { Remove-Item "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\tray-check.lnk" -Force -ErrorAction SilentlyContinue } catch {}