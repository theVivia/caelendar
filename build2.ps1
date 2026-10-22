# build2.ps1 - incremental parallel build for XingLi (ASCII only)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
New-Item -ItemType Directory -Force -Path bin, obj | Out-Null
$cxx = "g++"
$flags = @("-std=c++17", "-O2", "-municode", "-mwindows",
           "-DUNICODE", "-D_UNICODE", "-DNOMINMAX", "-D_WIN32_WINNT=0x0A00", "-DWINVER=0x0A00",
           "-I", "src")
$libs = @("-lgdiplus", "-lcomctl32", "-lshlwapi", "-lshell32", "-luser32", "-lgdi32",
          "-lole32", "-luuid", "-loleacc", "-loleaut32", "-lmsimg32", "-ldwmapi", "-lwinmm", "-lcomdlg32", "-lbcrypt", "-lwinhttp", "-limm32", "-lws2_32")
$ldflags = @("-municode", "-mwindows", "-static", "-static-libgcc", "-static-libstdc++", "-s")

$startTime = Get-Date
$headers = Get-ChildItem src\*.h, src\*.inc -ErrorAction SilentlyContinue
$newestHeader = ($headers | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime

# 找到需要重新编译的源文件
$targets = @()
foreach ($f in (Get-ChildItem src\*.cpp | Sort-Object Name)) {
    $obj = Join-Path "obj" ($f.BaseName + ".o")
    $need = $true
    if (Test-Path $obj) {
        $objTime = (Get-Item $obj).LastWriteTime
        $need = ($f.LastWriteTime -gt $objTime) -or ($newestHeader -gt $objTime)
    }
    if ($need) { $targets += $f }
}

$failed = $false
if ($targets.Count -gt 0) {
    $jobs = [Math]::Max(1, [Math]::Min($env:NUMBER_OF_PROCESSORS - 1, 6))
    if ($jobs -lt 1) { $jobs = 1 }
    Write-Host ("CC  {0} files, {1} parallel" -f $targets.Count, $jobs)
    if ($PSVersionTable.PSVersion.Major -ge 7 -and $jobs -gt 1) {
        $results = $targets | ForEach-Object -Parallel {
            $f = $_
            $cxx = $using:cxx
            $flags = $using:flags
            $obj = Join-Path "obj" ($f.BaseName + ".o")
            $out = & $cxx @flags -c $f.FullName -o $obj 2>&1
            [pscustomobject]@{ Name = $f.Name; Code = $LASTEXITCODE; Out = $out }
        } -ThrottleLimit $jobs
        foreach ($r in $results) {
            if ($r.Code -ne 0) {
                Write-Host ("FAIL {0}" -f $r.Name)
                $r.Out | ForEach-Object { Write-Host $_ }
                $failed = $true
            } else {
                Write-Host ("  ok {0}" -f $r.Name)
            }
        }
    } else {
        foreach ($f in $targets) {
            Write-Host ("CC  " + $f.Name)
            & $cxx @flags -c $f.FullName -o (Join-Path "obj" ($f.BaseName + ".o")) 2>&1 | ForEach-Object { Write-Host $_ }
            if ($LASTEXITCODE -ne 0) { $failed = $true; break }
        }
    }
} else {
    Write-Host "CC  up to date"
}
if ($failed) { Write-Host "BUILD FAILED"; exit 1 }

# 资源（图标）
$objs = @()
foreach ($f in (Get-ChildItem src\*.cpp | Sort-Object Name)) { $objs += (Join-Path "obj" ($f.BaseName + ".o")) }
if (Test-Path "res\xingli.rc") {
    $windres = (Get-Command windres -ErrorAction SilentlyContinue).Source
    if ($windres) {
        Write-Host "RC  res\xingli.rc"
        Push-Location res
        & $windres "xingli.rc" -O coff -o "..\obj\xingli_res.o" 2>&1 | ForEach-Object { Write-Host $_ }
        Pop-Location
        if ($LASTEXITCODE -ne 0) { Write-Host "RC FAILED"; exit 1 }
        $objs += "obj\xingli_res.o"
    }
}

# 没编译、没资源变化时，exe 比所有 obj 新就跳过链接
$needLink = $true
if ((Test-Path "bin\XingLi.exe") -and $targets.Count -eq 0) {
    $exeTime = (Get-Item "bin\XingLi.exe").LastWriteTime
    $newestObj = (Get-ChildItem obj\*.o | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
    if ($exeTime -ge $newestObj -and $exeTime -ge (Get-Item "res\xingli.rc").LastWriteTime) { $needLink = $false }
}
if ($needLink) {
    Write-Host "LD  bin\XingLi.exe"
    & $cxx @objs -o "bin\XingLi.exe" @libs @ldflags 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { Write-Host "LINK FAILED"; exit 1 }
} else {
    Write-Host "LD  up to date"
}
$size = (Get-Item "bin\XingLi.exe").Length
$elapsed = [int]((Get-Date) - $startTime).TotalSeconds
Write-Host ("BUILD OK  {0:N0} KB  {1}s" -f ($size / 1KB), $elapsed)
