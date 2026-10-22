// subs.cpp - ICS 只读订阅：下载、缓存、自动更新（只新增，不修改本地日程）
#include "app.h"
#include <winhttp.h>
#include <ctime>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

std::wstring SubsPath() { return PathJoin(GetDataDir(), L"subscriptions.json"); }
std::wstring CacheDir() { return PathJoin(GetDataDir(), L"subscriptions"); }

std::string MakeId() {
    static int counter = 0;
    return Fmt(L"sub%lld%02d", (long long)time(nullptr), counter++).empty()
               ? "sub"
               : WideToUtf8(Fmt(L"sub%lld%02d", (long long)time(nullptr), counter));
}

bool HttpDownload(const std::wstring& url, std::string& out, std::wstring& err) {
    out.clear();
    wchar_t host[256] = { 0 }, path[2048] = { 0 }, extra[1024] = { 0 };
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { err = L"订阅地址格式不对"; return false; }
    std::wstring fullPath = std::wstring(path) + extra;
    bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
    HINTERNET session = WinHttpOpen(L"CAELENDAR/1.0 (Windows)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { err = L"无法初始化网络"; return false; }
    WinHttpSetTimeouts(session, 8000, 8000, 8000, 15000);
    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (!connect) { WinHttpCloseHandle(session); err = L"无法连接订阅服务器"; return false; }
    HINTERNET req = WinHttpOpenRequest(connect, L"GET", fullPath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!req) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); err = L"无法创建请求"; return false; }
    bool ok = false;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, len = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
        if (status >= 200 && status < 300) {
            DWORD avail = 0;
            do {
                avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail)) break;
                if (avail == 0) { ok = true; break; }
                size_t old = out.size();
                out.resize(old + avail);
                DWORD read = 0;
                if (!WinHttpReadData(req, &out[old], avail, &read)) { ok = false; break; }
                out.resize(old + read);
                if (out.size() > 8 * 1024 * 1024) { err = L"订阅文件超过 8MB"; ok = false; break; }
            } while (avail > 0);
            if (!ok) err = L"下载订阅内容失败";
        } else {
            err = Fmt(L"订阅服务器返回 HTTP %u", (unsigned)status);
        }
    } else {
        err = L"网络请求失败（超时或无法访问）";
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

}  // namespace

void LoadSubscriptions() {
    app.subscriptions.clear();
    bool ok = false;
    std::string raw = ReadFileBytes(SubsPath(), &ok);
    if (!ok || raw.empty()) return;
    std::wstring err;
    JValue root = JsonParse(raw, &err);
    if (!root.IsObj()) return;
    const auto& arr = root.Get(L"subs").arr;
    for (auto& v : arr) {
        if (!v.IsObj()) continue;
        Subscription s;
        s.id = WideToUtf8(v.Get(L"id").AsStr());
        s.name = v.Get(L"name").AsStr();
        s.url = v.Get(L"url").AsStr();
        s.lastSync = (long long)v.Get(L"lastSync").AsInt(0);
        s.enabled = v.Get(L"enabled").AsBool(true);
        if (s.id.empty() || s.url.empty()) continue;
        if (s.name.empty()) s.name = L"订阅";
        app.subscriptions.push_back(s);
    }
}

void SaveSubscriptions() {
    EnsureDir(GetDataDir());
    JValue root = JValue::Obj();
    root.Set(L"version") = JValue(1);
    JValue arr = JValue::Arr();
    for (auto& s : app.subscriptions) {
        JValue o = JValue::Obj();
        o.Set(L"id") = JValue(Utf8ToWide(s.id));
        o.Set(L"name") = JValue(s.name);
        o.Set(L"url") = JValue(s.url);
        o.Set(L"lastSync") = JValue((double)s.lastSync);
        o.Set(L"enabled") = JValue(s.enabled);
        arr.Push(o);
    }
    root.Set(L"subs") = arr;
    WriteFileBytesAtomic(SubsPath(), JsonDump(root, true));
}

bool AddSubscription(const std::wstring& url, const std::wstring& name, std::wstring& err) {
    std::wstring u = Trim(url);
    if (u.rfind(L"http://", 0) != 0 && u.rfind(L"https://", 0) != 0) {
        err = L"订阅地址必须以 http:// 或 https:// 开头";
        return false;
    }
    for (auto& s : app.subscriptions) {
        if (s.url == u) { err = L"这个订阅已经存在"; return false; }
    }
    Subscription s;
    s.id = MakeId();
    s.url = u;
    s.name = Trim(name);
    if (s.name.empty()) {
        size_t p = u.find(L"://");
        s.name = p == std::wstring::npos ? L"订阅" : u.substr(p + 3);
    }
    s.lastSync = 0;
    s.enabled = true;
    app.subscriptions.push_back(s);
    SaveSubscriptions();
    return true;
}

void DeleteSubscription(int index) {
    if (index < 0 || index >= (int)app.subscriptions.size()) return;
    app.subscriptions.erase(app.subscriptions.begin() + index);
    SaveSubscriptions();
}

int SyncSubscriptionsDue(bool force, bool userVisible) {
    LoadSubscriptions();
    if (app.subscriptions.empty()) {
        if (userVisible) ShowToast(L"还没有订阅，先添加一个只读 ICS 地址");
        return 0;
    }
    EnsureDir(CacheDir());
    long long now = (long long)time(nullptr);
    int totalAdded = 0;
    int updatedSubs = 0;
    bool anyChange = false;
    std::wstring lastErr;
    for (auto& s : app.subscriptions) {
        if (!s.enabled) continue;
        if (!force && s.lastSync > 0 && now - s.lastSync < 6 * 3600) continue;
        std::string bytes;
        std::wstring err;
        if (!HttpDownload(s.url, bytes, err)) {
            lastErr = err;
            DebugLog(L"订阅下载失败 %s: %s", s.name.c_str(), err.c_str());
            continue;
        }
        std::wstring cacheFile = PathJoin(CacheDir(), Utf8ToWide(s.id) + L".ics");
        WriteFileBytesAtomic(cacheFile, bytes);
        IcsParseResult pr = ParseIcs(bytes);
        int added = 0;
        for (auto& e : pr.events) {
            bool exists = false;
            for (auto& old : app.store.events) {
                if (!e.uid.empty() && old.uid == e.uid) { exists = true; break; }
                if (old.title == e.title && CompareDT(old.start, e.start) == 0) { exists = true; break; }
            }
            if (exists) continue;
            if (!anyChange) { app.store.Snapshot(); anyChange = true; }
            Event copy = e;
            copy.source = L"订阅:" + s.name;
            app.store.AddEvent(copy);
            ++added;
            ++totalAdded;
        }
        s.lastSync = now;
        ++updatedSubs;
        DebugLog(L"订阅更新 %s：新增 %d，解析 %d", s.name.c_str(), added, (int)pr.events.size());
    }
    SaveSubscriptions();
    if (anyChange) {
        DoSaveStore();
        InvalidateOccs();
        UIInvalidate();
    }
    if (userVisible) {
        if (totalAdded > 0) ShowToast(Fmt(L"订阅更新完成：新增 %d 条（只新增，不改本地日程）", totalAdded), 5000);
        else if (updatedSubs > 0) ShowToast(L"订阅已是最新，没有新日程");
        else if (!lastErr.empty()) ShowToast(lastErr, 5000);
    }
    return totalAdded;
}

void OpenSubscriptions() {
    LoadSubscriptions();
    app.subUi.sel = -1;
    app.subUi.error.clear();
    app.overlay = OV_SUBS;
    UIInvalidate();
}

void DrawSubscriptionsOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(560);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Txt(g, L"ICS 只读订阅", S(17), true, th.text, RectF(x + S(24), y + S(16), S(300), S(26)));
    Txt(g, L"定时拉取 ICS 并只新增日程；绝不修改、覆盖或删除你本地的日程。每 6 小时自动更新一次。",
        S(9), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    float ly = y + S(78);
    Txt(g, Fmt(L"已有 %d 个订阅", (int)app.subscriptions.size()), S(10), false, th.accent,
        RectF(x + S(24), ly, S(300), S(18)));
    ly += S(24);
    float rowH = S(52);
    for (int i = 0; i < (int)app.subscriptions.size() && i < 5; ++i) {
        auto& s = app.subscriptions[i];
        RectF row(x + S(24), ly + i * rowH, pw - S(48), rowH - S(6));
        FillRoundRect(g, row, S(8), i == app.subUi.sel ? WithAlpha(th.accent, 24) : WithAlpha(th.panel2, 170));
        StrokeRoundRect(g, row, S(8), WithAlpha(th.borderSoft, 220), S(1));
        Txt(g, s.name, S(10.5f), true, th.text, RectF(row.X + S(12), row.Y + S(6), S(150), S(18)), ALIGN_LEFT, false, true);
        Txt(g, s.url, S(8.5f), false, th.dim2, RectF(row.X + S(12), row.Y + S(26), row.Width - S(190), S(16)), ALIGN_LEFT, false, true);
        std::wstring last = s.lastSync > 0 ? Fmt(L"上次更新 %lld", s.lastSync) : L"尚未更新";
        Txt(g, last, S(8), false, th.dim2, RectF(row.X + S(170), row.Y + S(6), S(140), S(18)));
        UI::Btn(g, RectF(RX(row) - S(118), row.Y + S(10), S(48), S(24)), CMD_TS_CLOSE + 500 + i, L"更新", 1, -1, [i]() {
            for (auto& sub : app.subscriptions) sub.lastSync = 0;
            SyncSubscriptionsDue(true, true);
            UIInvalidate();
        });
        UI::Btn(g, RectF(RX(row) - S(64), row.Y + S(10), S(48), S(24)), CMD_TS_CLOSE + 600 + i, L"删除", 1, -1, [i]() {
            DeleteSubscription(i);
            app.subUi.sel = -1;
            UIInvalidate();
        });
    }
    if (app.subscriptions.empty()) {
        Txt(g, L"还没有订阅。在下面粘贴一个 ICS 订阅地址（Google/Outlook 日历的「秘密地址」均可）。",
            S(10), false, th.dim2, RectF(x + S(24), ly + S(8), pw - S(48), S(20)));
    }

    float fy = y + S(356);
    Txt(g, L"添加订阅", S(10.5f), false, th.dim2, RectF(x + S(24), fy, S(120), S(18)));
    RectF urlBox(x + S(24), fy + S(22), pw - S(48), S(34));
    FillRoundRect(g, urlBox, S(8), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, urlBox, S(8), th.borderSoft, S(1));
    UIEdit(9960, RectF(urlBox.X + S(10), urlBox.Y + S(7), urlBox.Width - S(20), S(20)), &app.subUi.newUrl, false, S(11));
    RectF nameBox(x + S(24), fy + S(64), S(250), S(32));
    FillRoundRect(g, nameBox, S(8), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, nameBox, S(8), th.borderSoft, S(1));
    UIEdit(9961, RectF(nameBox.X + S(10), nameBox.Y + S(6), nameBox.Width - S(20), S(20)), &app.subUi.newName, false, S(11));
    Txt(g, L"名称（可留空）", S(8), false, th.dim2, RectF(nameBox.X, nameBox.Y - S(18), S(200), S(16)));
    UI::Btn(g, RectF(x + S(286), fy + S(64), S(120), S(32)), CMD_IMPORT_OK, L"添加并更新", 0, ICO_PLUS, []() {
        std::wstring err;
        if (!AddSubscription(app.subUi.newUrl, app.subUi.newName, err)) {
            app.subUi.error = err;
            UIInvalidate();
            return;
        }
        app.subUi.newUrl.clear();
        app.subUi.newName.clear();
        app.subUi.error.clear();
        SyncSubscriptionsDue(true, true);
        UIInvalidate();
    });
    Txt(g, L"示例：https://calendar.google.com/calendar/ical/.../basic.ics", S(8.5f), false, th.dim2,
        RectF(x + S(24), fy + S(104), pw - S(48), S(16)));
    if (!app.subUi.error.empty())
        Txt(g, app.subUi.error, S(9), false, th.danger, RectF(x + S(24), fy + S(124), pw - S(48), S(18)));

    float by = y + ph - S(54);
    UI::Btn(g, RectF(x + pw - S(24) - S(90), by, S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(10) - S(140), by, S(140), S(34)), CMD_IMPORT_OK, L"立即更新全部", 1, ICO_REFRESH, []() {
        for (auto& s : app.subscriptions) s.lastSync = 0;
        SyncSubscriptionsDue(true, true);
        UIInvalidate();
    });
}
