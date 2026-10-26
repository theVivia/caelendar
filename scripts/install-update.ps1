$ErrorActionPreference = 'Continue'
$log = 'E:\myCalendar\logs\update-install.log'
function W($m) { try { ("[" + (Get-Date -Format 'HH:mm:ss') + "] " + $m) | Out-File $log -Encoding UTF8 -Append } catch {} }
try {
  W "=== 搜索更新 ==="
  $session = New-Object -ComObject Microsoft.Update.Session
  $searcher = $session.CreateUpdateSearcher()
  $result = $searcher.Search("IsInstalled=0 and Type='Software'")
  W ("候选: " + $result.Updates.Count)
  $target = $null
  for ($i = 0; $i -lt $result.Updates.Count; $i++) {
    $u = $result.Updates.Item($i)
    $title = $u.Title
    if ($title -match 'KB5127648') { $target = $u; W ("命中: " + $title) }
  }
  if (-not $target) { W "未找到 KB5127648"; exit 1 }
  $coll = New-Object -ComObject Microsoft.Update.UpdateColl
  $coll.Add($target) | Out-Null
  W "=== 下载 ==="
  $downloader = $session.CreateUpdateDownloader()
  $downloader.Updates = $coll
  $dr = $downloader.Download()
  W ("下载结果: " + $dr.ResultCode)
  W "=== 安装 ==="
  $installer = $session.CreateUpdateInstaller()
  $installer.Updates = $coll
  $ir = $installer.Install()
  W ("安装结果: " + $ir.ResultCode + " 重启需要: " + $ir.RebootRequired)
  W "=== 完成 ==="
} catch {
  W ("异常: " + $_.Exception.Message)
}
