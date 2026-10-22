# 发布脚本：构建两端产物 → 打 tag → 发到 GitHub/Gitee Release
# 前置：git remote 已配置；GitHub 需要 gh CLI 且已 gh auth login（Gitee 用 API token）
# 用法：
#   pwsh scripts/release.ps1 -Tag v3.36 -Title "星历 3.36" -Notes "更新内容..."
#   pwsh scripts/release.ps1 -Tag v3.36 -Title "星历 3.36" -Notes "..." -SkipBuild

param(
    [Parameter(Mandatory = $true)][string]$Tag,
    [Parameter(Mandatory = $true)][string]$Title,
    [string]$Notes = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Fail($msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red; exit 1 }

# --- 0) 环境检查 ---
if (-not (git remote 2>$null)) { Fail "没有配置 git remote，先 git remote add origin <仓库地址>" }
$hasGh = Get-Command gh -ErrorAction SilentlyContinue
if (-not $hasGh) { Fail "没装 gh CLI（GitHub Release 用）：winget install GitHub.cli 然后 gh auth login" }
gh auth status 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { Fail "gh 未登录：gh auth login" }

# --- 1) 构建桌面 ---
if (-not $SkipBuild) {
    Write-Host "[1/4] 构建桌面端..." -ForegroundColor Cyan
    pwsh -NoProfile -ExecutionPolicy Bypass -File build2.ps1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail "桌面构建失败" }
}

# --- 2) 构建 Android ---
if (-not $SkipBuild) {
    Write-Host "[2/4] 构建 Android..." -ForegroundColor Cyan
    Push-Location android
    $env:ANDROID_HOME = 'E:\AndroidSdk'
    $env:ANDROID_SDK_ROOT = 'E:\AndroidSdk'
    $env:GRADLE_USER_HOME = 'E:\GradleHome'
    & 'C:\Gradle\gradle-7.6\bin\gradle.bat' --no-daemon --console=plain -g E:\GradleHome --offline assembleRelease
    $code = $LASTEXITCODE
    Pop-Location
    if ($code -ne 0) { Fail "Android 构建失败" }
}

# --- 3) 收集产物 ---
Write-Host "[3/4] 收集产物..." -ForegroundColor Cyan
$exe = Join-Path $root "bin\XingLi.exe"
if (-not (Test-Path $exe)) { Fail "找不到 bin\XingLi.exe" }
$apk = Get-ChildItem (Join-Path $root "android\app\build\outputs\apk\release\app-release.apk") -ErrorAction SilentlyContinue
if (-not $apk) { Fail "找不到 release APK" }

$verLine = Select-String -Path (Join-Path $root "src\app.h") -Pattern 'kAppVersion = L"([^"]+)"'
$desktopVer = if ($verLine) { $verLine.Matches[0].Groups[1].Value } else { "unknown" }
$gradle = Get-Content (Join-Path $root "android\app\build.gradle.kts") -Raw
$androidVer = if ($gradle -match 'versionName = "([^"]+)"') { $Matches[1] } else { "unknown" }
$code = if ($gradle -match 'versionCode = (\d+)') { $Matches[1] } else { "0" }

$releaseDir = Join-Path $root "release"
New-Item -ItemType Directory -Force $releaseDir | Out-Null
$exeOut = Join-Path $releaseDir "星历-$desktopVer.exe"
$apkOut = Join-Path $releaseDir "星历-Android-v$code-$androidVer.apk"
Copy-Item $exe $exeOut -Force
Copy-Item $apk.FullName $apkOut -Force
Write-Host "  桌面: $exeOut ($([math]::Round((Get-Item $exeOut).Length/1MB,2)) MB)"
Write-Host "  Android: $apkOut ($([math]::Round((Get-Item $apkOut).Length/1MB,2)) MB)"

# --- 4) tag + 推送 + Release ---
Write-Host "[4/4] 打 tag、推送、创建 Release..." -ForegroundColor Cyan
$fullNotes = if ($Notes) { $Notes } else { "星历 $desktopVer 桌面版 + Android $androidVer（versionCode $code）" }
git add -A
git diff --cached --quiet
if ($LASTEXITCODE -ne 0) { git commit -m "release: $Tag" | Out-Null }
git tag -f $Tag
git push origin HEAD 2>&1 | Out-Null
git push origin $Tag -f 2>&1 | Out-Null
gh release create $Tag $exeOut $apkOut --title $Title --notes $fullNotes
if ($LASTEXITCODE -ne 0) { Fail "Release 创建失败" }

Write-Host "[OK] Release 已发布：$Tag" -ForegroundColor Green
