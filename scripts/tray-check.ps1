# 重启后托盘自检（由 DSH 部署）
$log = 'E:\myCalendar\logs\after-reboot-check.txt'
Start-Sleep -Seconds 15
"=== $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') 开机自检 ===" | Out-File $log -Encoding UTF8 -Append
if (Test-Path 'E:\myCalendar\logs\traytest.exe') {
  Remove-Item 'E:\myCalendar\logs\traytest-result.txt' -Force -ErrorAction SilentlyContinue
  Start-Process -FilePath 'E:\myCalendar\logs\traytest.exe' -Wait
  Start-Sleep -Seconds 1
  if (Test-Path 'E:\myCalendar\logs\traytest-result.txt') {
    ('独立托盘测试: ' + (Get-Content 'E:\myCalendar\logs\traytest-result.txt' -Raw)) | Out-File $log -Encoding UTF8 -Append
  } else { '独立托盘测试: 无结果文件' | Out-File $log -Encoding UTF8 -Append }
} else { 'traytest.exe 不存在' | Out-File $log -Encoding UTF8 -Append }
$before = (Get-Content 'E:\myCalendar\data\debug.log' -Raw -ErrorAction SilentlyContinue).Length
Start-Process -FilePath 'E:\myCalendar\星历.exe' -WorkingDirectory 'E:\myCalendar'
Start-Sleep -Seconds 6
$after = Get-Content 'E:\myCalendar\data\debug.log' -Raw -ErrorAction SilentlyContinue
if ($after.Length -gt $before) {
  $new = $after.Substring($before)
  $tray = ($new -split "`n") | Select-String 'tray' | Select-Object -First 5
  if ($tray) { $tray | Out-File $log -Encoding UTF8 -Append } else { '星历日志: 无 tray 记录' | Out-File $log -Encoding UTF8 -Append }
} else { '星历日志: 无新内容' | Out-File $log -Encoding UTF8 -Append }
"=== 自检结束 ===" | Out-File $log -Encoding UTF8 -Append
Remove-Item "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\tray-check.lnk" -Force -ErrorAction SilentlyContinue
