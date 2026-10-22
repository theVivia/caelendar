// ocr.cpp - 按需调用 Windows 自带 OCR（Windows.Media.Ocr），不打包模型
#include "app.h"
#include <winhttp.h>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

std::wstring Quote(const std::wstring& s) {
    return L"\"" + s + L"\"";
}

const char kOcrScript[] = R"PS(param([string]$Image,[string]$Out,[string]$Lang)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Runtime.WindowsRuntime
[Windows.Storage.StorageFile,Windows.Storage,ContentType=WindowsRuntime] | Out-Null
[Windows.Storage.Streams.IRandomAccessStream,Windows.Storage.Streams,ContentType=WindowsRuntime] | Out-Null
[Windows.Graphics.Imaging.BitmapDecoder,Windows.Graphics.Imaging,ContentType=WindowsRuntime] | Out-Null
[Windows.Media.Ocr.OcrEngine,Windows.Foundation,ContentType=WindowsRuntime] | Out-Null
[Windows.Globalization.Language,Windows.Globalization,ContentType=WindowsRuntime] | Out-Null
$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq "AsTask" -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq "IAsyncOperation``1" })[0]
function Await($t, $type) {
  $m = $asTaskGeneric.MakeGenericMethod($type)
  $task = $m.Invoke($null, @($t))
  $task.Wait(-1) | Out-Null
  return $task.Result
}
$file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($Image)) ([Windows.Storage.StorageFile])
$stream = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
$decoder = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
$bitmap = Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
$engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage([Windows.Globalization.Language]::new($Lang))
if (-not $engine) { $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages() }
if (-not $engine) { throw "Windows OCR engine unavailable" }
$result = Await ($engine.RecognizeAsync($bitmap)) ([Windows.Media.Ocr.OcrResult])
[System.IO.File]::WriteAllText($Out, $result.Text, (New-Object System.Text.UTF8Encoding($false)))
)PS";

}  // namespace

bool WindowsOcrImage(const std::wstring& imagePath, const std::wstring& lang, std::wstring& outText, std::wstring& err) {
    outText.clear();
    err.clear();
    EnsureDir(GetDataDir());
    std::wstring script = PathJoin(GetDataDir(), L"ocr-helper.ps1");
    std::wstring outPath = PathJoin(GetDataDir(), L"ocr-out.txt");
    std::string scriptBytes = kOcrScript;
    if (!WriteFileBytesAtomic(script, scriptBytes)) { err = L"无法创建 OCR 脚本"; return false; }
    DeleteFileW(outPath.c_str());
    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File " +
                       Quote(script) + L" -Image " + Quote(imagePath) + L" -Out " + Quote(outPath) +
                       L" -Lang " + Quote(lang.empty() ? L"zh-Hans-CN" : lang);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(0);
    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        err = L"无法启动 Windows OCR（powershell.exe）";
        return false;
    }
    DWORD wait = WaitForSingleObject(pi.hProcess, 30000);
    bool ok = (wait == WAIT_OBJECT_0);
    if (!ok) TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (!ok) { err = L"OCR 超时（30 秒）"; return false; }
    bool fileOk = false;
    std::string raw = ReadFileBytes(outPath, &fileOk);
    DeleteFileW(outPath.c_str());
    if (!fileOk || raw.empty()) { err = L"OCR 没有识别出文字（或系统未安装中文 OCR 语言包）"; return false; }
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        raw = raw.substr(3);
    outText = Utf8ToWide(raw);
    DebugLog(L"OCR 完成：%s 字", Fmt(L"%d", (int)outText.size()).c_str());
    return !outText.empty();
}

void OpenOcrResult() {
    app.overlay = OV_OCR;
    UIInvalidate();
}

void DrawOcrResultOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(540);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Text(g, L"OCR 识别结果", S(17), true, th.text, RectF(x + S(24), y + S(16), S(300), S(26)));
    Text(g, app.import.ocrSource.empty() ? L"Windows 自带 OCR（按需加载）" : app.import.ocrSource,
         S(9), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        app.import.open = false;
        UIInvalidate();
    });

    Text(g, L"识别出的文字可直接修改；确认后点「解析并预览」走智能导入（课表/日程都能识别）。",
         S(9), false, th.dim2, RectF(x + S(24), y + S(72), pw - S(48), S(18)));
    RectF box(x + S(24), y + S(96), pw - S(48), S(330));
    FillRoundRect(g, box, S(10), WithAlpha(th.panel2, 180));
    StrokeRoundRect(g, box, S(10), th.borderSoft, S(1));
    UIEdit(9950, RectF(box.X + S(12), box.Y + S(10), box.Width - S(24), box.Height - S(20)),
           &app.import.ocrText, true, S(11));

    Text(g, L"提示：截图越清晰、表格越规整，OCR 越准；识别后可在上面手动修正。", S(8.5f), false, th.dim2,
         RectF(x + S(24), y + ph - S(96), pw - S(250), S(18)));
    float by = y + ph - S(54);
    UI::Btn(g, RectF(x + pw - S(24) - S(90), by, S(90), S(34)), CMD_IMPORT_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        app.import.open = false;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(10) - S(130), by, S(130), S(34)), CMD_IMPORT_OK,
            L"解析并预览", 0, ICO_SEARCH, []() {
        std::wstring t = app.import.ocrText;
        if (t.empty()) { ShowToast(L"OCR 文本为空"); return; }
        LoadSmartImport(L"", t, true, app.import.ocrSource.empty() ? L"OCR 识别文本" : app.import.ocrSource);
    });
}
