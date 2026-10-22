// sync.cpp - 多设备同步基础：设备 ID、操作日志、删除墓碑、加密换机迁移包
#include "app.h"
#include "json.h"
#include <objbase.h>
#include <winhttp.h>
#include <ctime>
#include <algorithm>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

struct DeviceInfo {
    std::wstring id;
    std::wstring name;
    std::wstring created;
};
DeviceInfo g_device;
std::vector<std::string> g_oplog;      // 每行一个 JSON
std::vector<JValue> g_tombstones;
bool g_inited = false;
std::string g_lastHash;

std::wstring DevicePath() { return PathJoin(GetDataDir(), L"device.json"); }
std::wstring SyncDir() { return PathJoin(GetDataDir(), L"sync"); }
std::wstring OplogPath() { return PathJoin(SyncDir(), L"oplog.jsonl"); }
std::wstring TombPath() { return PathJoin(SyncDir(), L"tombstones.json"); }

std::wstring FnvHash(const std::string& s) {
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return Fmt(L"%016llX", h);
}

void SaveOplog() {
    EnsureDir(SyncDir());
    std::string out;
    for (auto& l : g_oplog) { if (!out.empty()) out += "\n"; out += l; }
    if (!out.empty()) out += "\n";
    WriteFileBytesAtomic(OplogPath(), out);
}

void LoadOplog() {
    g_oplog.clear();
    bool ok = false;
    std::string raw = ReadFileBytes(OplogPath(), &ok);
    if (!ok || raw.empty()) return;
    std::string cur;
    for (char c : raw) {
        if (c == '\n') {
            if (!cur.empty()) g_oplog.push_back(cur);
            cur.clear();
        } else cur += c;
    }
    if (!cur.empty()) g_oplog.push_back(cur);
    while (g_oplog.size() > 100) g_oplog.erase(g_oplog.begin());
}

void SaveTombstones() {
    EnsureDir(SyncDir());
    JValue root = JValue::Obj();
    root.Set(L"version") = JValue(1);
    JValue arr = JValue::Arr();
    for (auto& t : g_tombstones) arr.Push(t);
    root.Set(L"items") = arr;
    WriteFileBytesAtomic(TombPath(), JsonDump(root, true));
}

void LoadTombstones() {
    g_tombstones.clear();
    bool ok = false;
    std::string raw = ReadFileBytes(TombPath(), &ok);
    if (!ok || raw.empty()) return;
    std::wstring err;
    JValue root = JsonParse(raw, &err);
    if (!root.IsObj()) return;
    for (auto& t : root.Get(L"items").arr) if (t.IsObj()) g_tombstones.push_back(t);
    while (g_tombstones.size() > 300) g_tombstones.erase(g_tombstones.begin());
}

void SaveDevice() {
    JValue root = JValue::Obj();
    root.Set(L"version") = JValue(1);
    root.Set(L"deviceId") = JValue(g_device.id);
    root.Set(L"deviceName") = JValue(g_device.name);
    root.Set(L"createdAt") = JValue(g_device.created);
    WriteFileBytesAtomic(DevicePath(), JsonDump(root, true));
}

void LoadDevice() {
    bool ok = false;
    std::string raw = ReadFileBytes(DevicePath(), &ok);
    if (ok && !raw.empty()) {
        std::wstring err;
        JValue root = JsonParse(raw, &err);
        if (root.IsObj()) {
            g_device.id = root.Get(L"deviceId").AsStr();
            g_device.name = root.Get(L"deviceName").AsStr();
            g_device.created = root.Get(L"createdAt").AsStr();
        }
    }
    if (g_device.id.empty()) {
        GUID guid{};
        if (SUCCEEDED(CoCreateGuid(&guid))) {
            g_device.id = Fmt(L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                              guid.Data1, guid.Data2, guid.Data3,
                              guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                              guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        } else {
            g_device.id = Fmt(L"%llX-%llX", (unsigned long long)time(nullptr), (unsigned long long)GetTickCount64());
        }
    }
    if (g_device.name.empty()) {
        wchar_t name[256] = { 0 };
        DWORD n = 256;
        if (GetComputerNameW(name, &n)) g_device.name = name;
        else g_device.name = L"Windows";
    }
    if (g_device.created.empty()) g_device.created = ISODate(NowDT()) + L"T" + TimeStr(NowDT());
    SaveDevice();
}

std::string WideJson(const JValue& v) { return JsonDump(v, false); }

bool SaveDialog(std::wstring& path) {
    wchar_t file[1024] = { 0 };
    std::wstring name = Fmt(L"星历换机包-%d%02d%02d.xlbak", app.now.y, app.now.mo, app.now.d);
    wcsncpy_s(file, name.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"星历换机包 (*.xlbak)\0*.xlbak\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrDefExt = L"xlbak";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    path = file;
    if (path.size() < 6 || path.substr(path.size() - 6) != L".xlbak") path += L".xlbak";
    return true;
}

bool OpenDialog(std::wstring& path) {
    wchar_t file[1024] = { 0 };
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"星历换机包 (*.xlbak)\0*.xlbak\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;
    path = file;
    return true;
}

bool PeekSummary(const std::string& plain, std::wstring& summary, std::wstring& err) {
    std::wstring perr;
    JValue root = JsonParse(plain, &perr);
    if (!root.IsObj()) { err = L"迁移包内容无效：" + perr; return false; }
    if (root.Get(L"format").AsStr() == L"caelendar-migration") {
        const JValue& dev = root.Get(L"device");
        int events = (int)root.Get(L"store").Get(L"events").arr.size();
        int oplogN = (int)root.Get(L"oplog").arr.size();
        int tombN = (int)root.Get(L"tombstones").arr.size();
        summary = Fmt(L"来自设备「%s」\n导出时间：%s\n日程 %d 条 · 操作日志 %d 条 · 删除墓碑 %d 条\n包内数据格式版本：%d",
                      dev.Get(L"name").AsStr(L"未知").c_str(),
                      root.Get(L"exportedAt").AsStr(L"未知").c_str(),
                      events, oplogN, tombN, root.Get(L"dataFormat").AsInt(1));
        return true;
    }
    // 兼容旧加密备份包：明文就是 Store JSON
    int events = (int)root.Get(L"events").arr.size();
    if (events == 0 && !root.Get(L"tags").arr.size()) { err = L"这不像星历迁移包或旧加密备份"; return false; }
    summary = Fmt(L"旧版加密备份：%d 条日程\n导入后会覆盖当前数据（导入前自动备份）。", events);
    return true;
}

void ApplyMigration(const std::string& plain) {
    std::wstring perr;
    JValue root = JsonParse(plain, &perr);
    if (!root.IsObj()) { ShowToast(L"迁移包解析失败"); return; }
    std::string storeJson;
    bool migration = root.Get(L"format").AsStr() == L"caelendar-migration";
    if (migration) {
        const JValue& st = root.Get(L"store");
        storeJson = WideJson(st);
    } else {
        storeJson = plain;
    }
    Store tmp;
    std::wstring err;
    if (!tmp.FromJsonStr(storeJson, &err)) { ShowToast(L"迁移包里的日程数据校验失败"); return; }
    EnsureDir(PathJoin(GetDataDir(), L"backup"));
    std::wstring pre = PathJoin(GetDataDir(), L"backup/manual-before-migration-" + ISODate(app.now) + L"-" +
                                              Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
    CopyFileSimple(GetCalendarPath(), pre);
    if (!WriteFileBytesAtomic(GetCalendarPath(), storeJson)) { ShowToast(L"写入导入数据失败"); return; }
    app.store = tmp;
    // 合并操作日志与墓碑（只追加去重，不做危险覆盖）
    if (migration) {
        for (auto& l : root.Get(L"oplog").arr) {
            std::string line = WideJson(l);
            if (std::find(g_oplog.begin(), g_oplog.end(), line) == g_oplog.end()) g_oplog.push_back(line);
        }
        while (g_oplog.size() > 100) g_oplog.erase(g_oplog.begin());
        SaveOplog();
        for (auto& t : root.Get(L"tombstones").arr) {
            std::wstring id = t.Get(L"id").AsStr();
            bool dup = false;
            for (auto& old : g_tombstones) if (old.Get(L"id").AsStr() == id) { dup = true; break; }
            if (!dup) g_tombstones.push_back(t);
        }
        while (g_tombstones.size() > 300) g_tombstones.erase(g_tombstones.begin());
        SaveTombstones();
    }
    ApplyThemeFromSettings();
    ApplyFontSettings();
    InvalidateOccs();
    app.selected.clear();
    app.store.Touch();
    DoSaveStore();
    app.syncUi.summary = Fmt(L"已导入迁移包：%d 条日程", (int)app.store.events.size());
    ShowToast(L"换机迁移包已导入（导入前已自动备份）", 5000);
    DebugLog(L"迁移包导入完成: device=%s events=%d", g_device.id.c_str(), (int)app.store.events.size());
}

}  // namespace

void SyncInit() {
    if (g_inited) return;
    g_inited = true;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LoadDevice();
    LoadOplog();
    LoadTombstones();
    if (!g_oplog.empty()) {
        std::wstring perr;
        JValue last = JsonParse(g_oplog.back(), &perr);
        g_lastHash = WideToUtf8(last.Get(L"hash").AsStr());
    }
    DebugLog(L"同步基础: device=%s oplog=%d tombstones=%d", g_device.id.c_str(), (int)g_oplog.size(), (int)g_tombstones.size());
}

const std::wstring& SyncDeviceId() { SyncInit(); return g_device.id; }
const std::wstring& SyncDeviceName() { SyncInit(); return g_device.name; }
int SyncOplogCount() { SyncInit(); return (int)g_oplog.size(); }
int SyncTombstoneCount() { SyncInit(); return (int)g_tombstones.size(); }

std::wstring SyncLastOpText() {
    SyncInit();
    if (g_oplog.empty()) return L"暂无操作记录";
    std::wstring perr;
    JValue last = JsonParse(g_oplog.back(), &perr);
    return last.Get(L"at").AsStr(L"未知") + L" · " + Fmt(L"%d", last.Get(L"events").AsInt(0)) + L" 条";
}

void SyncRecordSave(const std::string& storeJson) {
    SyncInit();
    std::wstring h = FnvHash(storeJson);
    if (!g_lastHash.empty() && WideToUtf8(h) == g_lastHash) return;
    g_lastHash = WideToUtf8(h);
    std::wstring perr;
    JValue root = JsonParse(storeJson, &perr);
    JValue line = JValue::Obj();
    line.Set(L"version") = JValue(1);
    line.Set(L"device") = JValue(g_device.id);
    line.Set(L"at") = JValue(ISODate(NowDT()) + L"T" + TimeStr(NowDT()));
    line.Set(L"events") = JValue((int)root.Get(L"events").arr.size());
    line.Set(L"tags") = JValue((int)root.Get(L"tags").arr.size());
    line.Set(L"hash") = JValue(h);
    g_oplog.push_back(WideJson(line));
    while (g_oplog.size() > 100) g_oplog.erase(g_oplog.begin());
    SaveOplog();
}

void SyncRecordTombstone(const std::string& id, const std::wstring& uid, const std::wstring& title) {
    SyncInit();
    JValue t = JValue::Obj();
    t.Set(L"id") = JValue(Utf8ToWide(id));
    t.Set(L"uid") = JValue(uid);
    t.Set(L"title") = JValue(title);
    t.Set(L"deletedAt") = JValue(ISODate(NowDT()) + L"T" + TimeStr(NowDT()));
    t.Set(L"device") = JValue(g_device.id);
    g_tombstones.push_back(t);
    while (g_tombstones.size() > 300) g_tombstones.erase(g_tombstones.begin());
    SaveTombstones();
    DebugLog(L"墓碑: %s（%s）", title.c_str(), Utf8ToWide(id).c_str());
}

std::wstring SyncBuildMigrationPackage() {
    SyncInit();
    JValue root = JValue::Obj();
    root.Set(L"format") = JValue(L"caelendar-migration");
    root.Set(L"version") = JValue(2);
    root.Set(L"exportedAt") = JValue(ISODate(NowDT()) + L"T" + TimeStr(NowDT()));
    root.Set(L"appVersion") = JValue(kAppVersion);
    root.Set(L"dataFormat") = JValue(kDataFormatVersion);
    JValue dev = JValue::Obj();
    dev.Set(L"id") = JValue(g_device.id);
    dev.Set(L"name") = JValue(g_device.name);
    dev.Set(L"createdAt") = JValue(g_device.created);
    root.Set(L"device") = dev;
    root.Set(L"store") = JsonParse(app.store.ToJsonStr());
    JValue oplog = JValue::Arr();
    for (auto& l : g_oplog) {
        std::wstring perr;
        JValue v = JsonParse(l, &perr);
        if (v.IsObj()) oplog.Push(v);
    }
    root.Set(L"oplog") = oplog;
    JValue tombs = JValue::Arr();
    for (auto& t : g_tombstones) tombs.Push(t);
    root.Set(L"tombstones") = tombs;
    return Utf8ToWide(JsonDump(root, true));
}

bool SyncImportMigrationPackage(const std::string& plain, std::wstring& err) {
    std::wstring summary;
    if (!PeekSummary(plain, summary, err)) return false;
    ApplyMigration(plain);
    return true;
}

// ------------------------- UI -------------------------
void OpenSyncCenter() {
    SyncInit();
    app.syncUi.error.clear();
    app.overlay = OV_SYNC;
    UIInvalidate();
}

void DrawSyncCenterOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(600);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Text(g, L"换机迁移 / 同步基础", S(17), true, th.text, RectF(x + S(24), y + S(16), S(360), S(26)));
    Text(g, L"本地优先：无账号、无后台服务；迁移包加密携带设备信息、操作日志与删除墓碑。",
         S(9), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    RectF st(x + S(24), y + S(80), pw - S(48), S(132));
    FillRoundRect(g, st, S(10), WithAlpha(th.bg, 170));
    StrokeRoundRect(g, st, S(10), th.borderSoft, S(1));
    Text(g, L"设备「" + SyncDeviceName() + L"」", S(13.5f), true, th.text,
         RectF(st.X + S(16), st.Y + S(12), st.Width - S(32), S(22)));
    Text(g, L"设备 ID：" + SyncDeviceId(), S(9), false, th.dim,
         RectF(st.X + S(16), st.Y + S(40), st.Width - S(32), S(18)), ALIGN_LEFT, false, true);
    Text(g, Fmt(L"本地日程 %d 条 · 操作日志 %d 条 · 删除墓碑 %d 条",
                (int)app.store.events.size(), SyncOplogCount(), SyncTombstoneCount()),
         S(9.5f), false, th.dim, RectF(st.X + S(16), st.Y + S(62), st.Width - S(32), S(18)));
    Text(g, L"最近操作：" + SyncLastOpText(), S(9), false, th.dim2,
         RectF(st.X + S(16), st.Y + S(84), st.Width - S(32), S(18)));
    Text(g, L"同步服务：关闭（本地优先；不会在后台联网）", S(9), false, th.success,
         RectF(st.X + S(16), st.Y + S(106), st.Width - S(32), S(18)));

    Text(g, L"换机迁移包密码", S(10), false, th.dim2, RectF(x + S(24), y + S(232), S(240), S(18)));
    RectF passBox(x + S(24), y + S(252), pw - S(48), S(36));
    FillRoundRect(g, passBox, S(8), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, passBox, S(8), th.borderSoft, S(1));
    UIEdit(9970, RectF(passBox.X + S(10), passBox.Y + S(8), passBox.Width - S(20), S(20)),
           &app.syncUi.password, false, S(11));
    Text(g, L"密码用于 AES-256-GCM 加密整包；迁移包不含明文密码，请自己记住。", S(8.5f), false, th.dim2,
         RectF(x + S(24), y + S(294), pw - S(48), S(16)));

    float by = y + S(326);
    UI::Btn(g, RectF(x + S(24), by, S(160), S(38)), CMD_IMPORT_OK, L"导出迁移包…", 0, ICO_COPY, []() {
        if (app.syncUi.password.empty()) { ShowToast(L"先设置迁移包密码"); return; }
        std::wstring path;
        if (!SaveDialog(path)) return;
        std::wstring pkg = SyncBuildMigrationPackage();
        std::wstring err;
        if (!EncryptBackupFile(path, WideToUtf8(pkg), app.syncUi.password, err)) {
            ShowToast(err.empty() ? L"导出失败" : err, 5000);
            return;
        }
        app.syncUi.lastExport = path;
        DebugLog(L"换机迁移包导出: %s", path.c_str());
        ShowToast(L"换机迁移包已导出");
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + S(194), by, S(160), S(38)), CMD_IMPORT_OK + 1, L"导入迁移包…", 1, ICO_FOLDER, []() {
        if (app.syncUi.password.empty()) { ShowToast(L"先输入迁移包密码"); return; }
        std::wstring path;
        if (!OpenDialog(path)) return;
        std::string plain;
        std::wstring err;
        if (!DecryptBackupFile(path, app.syncUi.password, plain, err)) {
            app.syncUi.error = err;
            UIInvalidate();
            return;
        }
        std::wstring summary;
        if (!PeekSummary(plain, summary, err)) { app.syncUi.error = err; UIInvalidate(); return; }
        OpenConfirm(L"导入换机迁移包", summary + L"\n\n导入会覆盖当前数据；导入前会自动备份当前 JSON。", [plain]() {
            std::wstring err;
            if (!SyncImportMigrationPackage(plain, err)) { ShowToast(err.empty() ? L"导入失败" : err, 6000); return; }
            app.overlay = OV_SYNC;
            UIInvalidate();
        }, L"导入", L"取消");
    });
    UI::Btn(g, RectF(x + S(364), by, S(140), S(38)), CMD_IMPORT_OK + 2, L"打开数据目录", 1, ICO_FOLDER, []() {
        ShellExecuteW(nullptr, L"open", GetDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    });

    if (!app.syncUi.lastExport.empty())
        Text(g, L"上次导出：" + app.syncUi.lastExport, S(8.5f), false, th.dim2,
             RectF(x + S(24), y + S(378), pw - S(48), S(16)), ALIGN_LEFT, false, true);
    if (!app.syncUi.summary.empty())
        Text(g, app.syncUi.summary, S(9.5f), false, th.success, RectF(x + S(24), y + S(402), pw - S(48), S(18)));
    if (!app.syncUi.error.empty())
        Text(g, app.syncUi.error, S(9), false, th.danger, RectF(x + S(24), y + S(424), pw - S(48), S(18)));

    Text(g, L"说明：迁移包 = 日程/标签/设置 + 设备 ID/名称 + 操作日志（最近 100 条）+ 删除墓碑（最近 300 条）。",
         S(8.5f), false, th.dim2, RectF(x + S(24), y + S(458), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    Text(g, L"云同步尚未接入；未来同步层会单独模块化，关闭时完全不跑后台服务。",
         S(8.5f), false, th.dim2, RectF(x + S(24), y + S(478), pw - S(48), S(18)), ALIGN_LEFT, false, true);

    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}


// ---------------- 账号同步（M1）客户端 ----------------
static bool SyncHttpJson(const std::wstring& method, const std::wstring& server, const std::wstring& path,
                         const std::string& body, std::string& resp, std::wstring& err) {
    std::wstring u = server;
    if (u.rfind(L"http://", 0) == 0) u = u.substr(7);
    else if (u.rfind(L"https://", 0) == 0) { err = L"暂不支持 HTTPS，先用 http"; return false; }
    std::wstring hostport = u;
    size_t slash = hostport.find(L'/');
    if (slash != std::wstring::npos) hostport = hostport.substr(0, slash);
    std::wstring host = hostport;
    int port = 17890;
    size_t colon = hostport.find(L':');
    if (colon != std::wstring::npos) {
        host = hostport.substr(0, colon);
        port = _wtoi(hostport.substr(colon + 1).c_str());
    }
    if (host.empty() || port <= 0 || port > 65535) { err = L"服务器地址无效"; return false; }

    HINTERNET hs = WinHttpOpen(L"CAELENDAR/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hs) { err = L"WinHTTP 初始化失败"; return false; }
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    HINTERNET hr = hc ? WinHttpOpenRequest(hc, method.c_str(), path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0) : nullptr;
    bool ok = false;
    if (hr) {
        std::wstring hdr = L"Content-Type: application/json\r\n";
        BOOL sent = WinHttpSendRequest(hr, hdr.c_str(), (DWORD)-1L,
                                       body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                       (DWORD)body.size(), (DWORD)body.size(), 0);
        if (sent && WinHttpReceiveResponse(hr, nullptr)) {
            DWORD status = 0, sz = sizeof(status);
            WinHttpQueryHeaders(hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
            resp.clear();
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(hr, &avail) && avail > 0) {
                std::string chunk(avail, '\0');
                DWORD got = 0;
                if (!WinHttpReadData(hr, &chunk[0], avail, &got) || got == 0) break;
                chunk.resize(got);
                resp += chunk;
                if (resp.size() > 4 * 1024 * 1024) break;
            }
            if (status >= 200 && status < 300) ok = true;
            else err = Fmt(L"HTTP %d", (int)status);
        } else {
            err = L"无法连接服务器";
        }
    } else {
        err = L"请求创建失败";
    }
    if (hr) WinHttpCloseHandle(hr);
    if (hc) WinHttpCloseHandle(hc);
    WinHttpCloseHandle(hs);
    return ok;
}

static bool SyncApiCall(const std::wstring& method, const std::wstring& server, const std::wstring& path,
                        const std::string& body, JValue& out, std::wstring& err) {
    std::string resp;
    if (!SyncHttpJson(method, server, path, body, resp, err)) {
        JValue r = JsonParse(resp);
        if (r.IsObj() && r.Has(L"err")) err = r.Get(L"err").AsStr();
        if (err.empty()) err = L"请求失败";
        return false;
    }
    out = JsonParse(resp);
    if (!out.IsObj()) { err = L"服务器返回无效数据"; return false; }
    if (!out.Has(L"ok") || !out.Get(L"ok").AsBool()) {
        err = out.Has(L"err") ? out.Get(L"err").AsStr() : L"请求失败";
        return false;
    }
    return true;
}

bool SyncAccountRegister(const std::wstring& server, const std::wstring& user, const std::wstring& pass,
                         std::wstring& err) {
    JValue req = JValue::Obj();
    req.Set(L"user") = JValue(user);
    req.Set(L"pass") = JValue(pass);
    JValue out;
    if (!SyncApiCall(L"POST", server, L"/api/auth/register", JsonDump(req, false), out, err)) return false;
    app.syncUi.server = server;
    app.syncUi.user = user;
    app.syncUi.token = out.Get(L"token").AsStr();
    app.syncUi.loggedIn = !app.syncUi.token.empty();
    app.syncUi.status = L"已注册并登录";
    return true;
}
bool SyncAccountLogin(const std::wstring& server, const std::wstring& user, const std::wstring& pass,
                      std::wstring& err) {
    JValue req = JValue::Obj();
    req.Set(L"user") = JValue(user);
    req.Set(L"pass") = JValue(pass);
    JValue out;
    if (!SyncApiCall(L"POST", server, L"/api/auth/login", JsonDump(req, false), out, err)) return false;
    app.syncUi.server = server;
    app.syncUi.user = user;
    app.syncUi.token = out.Get(L"token").AsStr();
    app.syncUi.loggedIn = !app.syncUi.token.empty();
    app.syncUi.status = L"已登录";
    return true;
}
bool SyncPushStore(const std::wstring& server, const std::wstring& token, const std::string& store,
                   std::wstring& err) {
    if (token.empty()) { err = L"未登录"; return false; }
    JValue sv = JsonParse(store);
    if (!sv.IsObj()) { err = L"本地数据格式错误"; return false; }
    JValue req = JValue::Obj();
    req.Set(L"store") = sv;
    JValue out;
    std::wstring path = L"/api/sync/push?token=" + token;
    return SyncApiCall(L"POST", server, path, JsonDump(req, true), out, err);
}
bool SyncPullStore(const std::wstring& server, const std::wstring& token, std::string& store,
                   std::wstring& err) {
    if (token.empty()) { err = L"未登录"; return false; }
    JValue out;
    std::wstring path = L"/api/sync/pull?token=" + token;
    if (!SyncApiCall(L"GET", server, path, "", out, err)) return false;
    if (!out.Has(L"store") || !out.Get(L"store").IsObj()) { err = L"服务器没有数据"; return false; }
    store = JsonDump(out.Get(L"store"), true);
    return true;
}
