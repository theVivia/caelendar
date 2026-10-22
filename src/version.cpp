// version.cpp - 版本快照与回滚（数据 + exe 成对保存）
#include "app.h"
#include <algorithm>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

struct VersionEntry {
    std::wstring exePath;
    std::wstring dataPath;
    std::wstring version;
    std::wstring timeText;
    long long size = 0;
    bool hasData = false;
};

std::wstring VersionDir() { return PathJoin(GetDataDir(), L"versions"); }

std::wstring TimeStamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return Fmt(L"%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

std::wstring BaseName(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? path : path.substr(p + 1);
}

std::wstring Stem(const std::wstring& path) {
    std::wstring n = BaseName(path);
    size_t p = n.find_last_of(L'.');
    return (p == std::wstring::npos) ? n : n.substr(0, p);
}

bool NameStartsWith(const std::wstring& path, const std::wstring& prefix) {
    return BaseName(path).rfind(prefix, 0) == 0;
}

ULONGLONG FileTimeOf(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA ad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &ad)) return 0;
    return ((ULONGLONG)ad.ftLastWriteTime.dwHighDateTime << 32) | ad.ftLastWriteTime.dwLowDateTime;
}

long long FileSizeOf(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA ad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &ad)) return 0;
    return ((long long)ad.nFileSizeHigh << 32) | ad.nFileSizeLow;
}

std::vector<VersionEntry> ListVersions() {
    std::vector<VersionEntry> out;
    auto files = ListFiles(VersionDir(), L".exe");
    for (auto& f : files) {
        std::wstring stem = Stem(f);
        if (stem.rfind(L"XingLi-", 0) != 0) continue;
        VersionEntry e;
        e.exePath = f;
        e.size = FileSizeOf(f);
        std::wstring rest = stem.substr(7);   // 去掉 XingLi-
        size_t dash = rest.find(L'-');
        e.version = (dash == std::wstring::npos) ? rest : rest.substr(0, dash);
        std::wstring tail = (dash == std::wstring::npos) ? L"" : rest.substr(dash + 1);
        e.timeText = tail;
        if (e.timeText.size() >= 15)
            e.timeText = e.timeText.substr(0, 4) + L"-" + e.timeText.substr(4, 2) + L"-" + e.timeText.substr(6, 2) +
                         L" " + e.timeText.substr(9, 2) + L":" + e.timeText.substr(11, 2);
        std::wstring data = f.substr(0, f.size() - 4) + L".json";
        if (FileExists(data)) { e.dataPath = data; e.hasData = true; }
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const VersionEntry& a, const VersionEntry& b) {
        return FileTimeOf(a.exePath) > FileTimeOf(b.exePath);
    });
    return out;
}

void PruneVersions() {
    auto files = ListFiles(VersionDir(), L".exe");
    // 删除 rollback-* 临时目录中的旧文件不在此列；只保留 XingLi-* 最近 10 个
    std::vector<std::wstring> vers;
    for (auto& f : files) if (NameStartsWith(f, L"XingLi-")) vers.push_back(f);
    std::sort(vers.begin(), vers.end(), [](const std::wstring& a, const std::wstring& b) {
        return FileTimeOf(a) > FileTimeOf(b);
    });
    for (size_t i = 10; i < vers.size(); ++i) {
        std::wstring data = vers[i].substr(0, vers[i].size() - 4) + L".json";
        DeleteFileW(vers[i].c_str());
        if (FileExists(data)) DeleteFileW(data.c_str());
    }
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

void Frame(Graphics& g, float W, float H, float pw, float ph, float& px, float& py) {
    const Theme& th = TH();
    pw = std::min(pw, W - S(40));
    ph = std::min(ph, H - S(30));
    px = (W - pw) / 2;
    py = (H - ph) / 2;
    RectF r(px, py, pw, ph);
    Shadow(g, r, S(18), 8, 26);
    FillRoundRect(g, r, S(18), th.card);
    StrokeRoundRect(g, r, S(18), WithAlpha(th.border, 255), S(1));
    FillVGradient(g, RectF(px, py, pw, S(60)), WithAlpha(th.accent, 10), WithAlpha(th.accent, 0));
}

}  // namespace

void EnsureVersionSnapshot() {
    EnsureDir(VersionDir());
    wchar_t exe[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (!exe[0]) return;
    std::wstring prefix = Fmt(L"XingLi-%s-", kAppVersion);
    auto files = ListFiles(VersionDir(), L".exe");
    for (auto& f : files) {
        if (NameStartsWith(f, prefix)) {
            // 该版本已有 exe 快照，更新数据快照
            std::wstring data = f.substr(0, f.size() - 4) + L".json";
            if (FileExists(GetCalendarPath())) CopyFileSimple(GetCalendarPath(), data);
            return;
        }
    }
    std::wstring ts = TimeStamp();
    std::wstring dst = PathJoin(VersionDir(), Fmt(L"XingLi-%s-%s.exe", kAppVersion, ts.c_str()));
    if (!CopyFileSimple(exe, dst)) {
        DebugLog(L"版本快照失败: %s", dst.c_str());
        return;
    }
    std::wstring data = dst.substr(0, dst.size() - 4) + L".json";
    if (FileExists(GetCalendarPath())) CopyFileSimple(GetCalendarPath(), data);
    DebugLog(L"版本快照: %s", BaseName(dst).c_str());
    PruneVersions();
}

void OpenVersionsOverlay() {
    app.overlay = OV_VERSIONS;
    UIInvalidate();
}

void RollbackToVersion(const std::wstring& exePath, const std::wstring& dataPath, const std::wstring& label) {
    if (exePath.empty() || !FileExists(exePath)) { ShowToast(L"版本文件不存在"); return; }
    DoSaveStore();
    EnsureDir(PathJoin(GetDataDir(), L"backup"));
    EnsureDir(VersionDir());
    std::wstring ts = TimeStamp();
    std::wstring preData = PathJoin(GetDataDir(), L"backup/manual-before-rollback-" + ts + L".json");
    CopyFileSimple(GetCalendarPath(), preData);
    EnsureVersionSnapshot();

    wchar_t curExe[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, curExe, MAX_PATH);
    std::wstring aside = PathJoin(VersionDir(), L"rollback-" + std::wstring(kAppVersion) + L"-" + ts + L".exe");
    if (!MoveFileExW(curExe, aside.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        ShowToast(L"回滚失败：无法重命名当前程序，请关闭后重试");
        return;
    }
    if (!CopyFileSimple(exePath, curExe)) {
        MoveFileExW(aside.c_str(), curExe, MOVEFILE_REPLACE_EXISTING);
        ShowToast(L"回滚失败：无法写入新版本程序");
        return;
    }
    bool restoredData = false;
    if (!dataPath.empty() && FileExists(dataPath)) {
        restoredData = CopyFileSimple(dataPath, GetCalendarPath());
    }
    // 关键：恢复数据后父进程退出时绝不能再保存，否则会用旧内存覆盖刚恢复的数据
    g_noSave = true;
    DebugLog(L"回滚到 %s，数据快照=%d", label.c_str(), restoredData ? 1 : 0);
    if (g_singleMutex) {
        ReleaseMutex(g_singleMutex);
        CloseHandle(g_singleMutex);
        g_singleMutex = nullptr;
    }
    std::wstring cmd = std::wstring(L"\"") + curExe + L"\" --restarted";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(curExe, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    DebugLog(L"回滚启动: ok=%d cmd=%s", ok ? 1 : 0, cmd.c_str());
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    g_quitting = true;
    if (g_hwnd) DestroyWindow(g_hwnd);
}

void DrawVersionsOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(560);
    float x, y;
    Frame(g, W, H, pw, ph, x, y);
    Txt(g, L"版本历史", S(18), true, th.text, RectF(x + S(24), y + S(16), S(200), S(28)));
    Txt(g, Fmt(L"当前版本 %s · 回滚会同时恢复对应数据快照，当前数据先备份", kAppVersion),
        S(9), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_VER_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_ABOUT;
        UIInvalidate();
    });

    auto entries = ListVersions();
    float listY = y + S(82);
    if (entries.empty()) {
        Txt(g, L"还没有历史版本。当前版本首次正常启动后会自动备份 exe + 数据快照。",
            S(10.5f), false, th.dim2, RectF(x + S(24), listY + S(20), pw - S(48), S(40)), ALIGN_CENTER);
    } else {
        Txt(g, L"版本", S(9), true, th.dim2, RectF(x + S(24), listY, S(120), S(16)));
        Txt(g, L"时间", S(9), true, th.dim2, RectF(x + S(150), listY, S(180), S(16)));
        Txt(g, L"数据快照", S(9), true, th.dim2, RectF(x + S(350), listY, S(100), S(16)));
        int shown = 0;
        for (auto& e : entries) {
            if (shown >= 8) break;
            float ry = listY + S(24) + shown * S(48);
            RectF row(x + S(20), ry, pw - S(40), S(42));
            FillRoundRect(g, row, S(8), WithAlpha(th.panel2, 180));
            Txt(g, L"v" + e.version, S(11), true, th.text, RectF(row.X + S(12), row.Y, S(110), row.Height), ALIGN_VCENTER);
            Txt(g, e.timeText, S(9.5f), false, th.dim, RectF(row.X + S(130), row.Y, S(190), row.Height), ALIGN_VCENTER);
            Txt(g, e.hasData ? L"有" : L"无", S(9.5f), false, e.hasData ? th.success : th.warn,
                RectF(row.X + S(330), row.Y, S(70), row.Height), ALIGN_VCENTER);
            UI::Btn(g, RectF(RX(row) - S(100), row.Y + S(7), S(86), S(28)), CMD_VER_ROLLBACK_BASE + shown,
                    L"回滚", 1, -1, [e]() {
                        OpenConfirm(L"回滚到 v" + e.version,
                                    Fmt(L"将把主程序替换为 v%s，并恢复它保存的数据快照。\n当前 exe 和当前数据都会先备份到 data\\versions 与 data\\backup。恢复后程序会重启。",
                                        e.version.c_str()),
                                    [e]() { RollbackToVersion(e.exePath, e.dataPath, L"v" + e.version); }, L"回滚", L"取消");
                    });
            ++shown;
        }
    }
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(50), S(90), S(32)), CMD_VER_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_ABOUT;
        UIInvalidate();
    });
}