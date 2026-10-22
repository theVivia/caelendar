// bot.cpp - Telegram 机器人（M1）：getUpdates 轮询、/today /week /add /help、聊天绑定
#include "app.h"
#include "json.h"
#include "quick.h"
#include <winhttp.h>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

CRITICAL_SECTION g_botCs;
bool g_botCsInit = false;
bool g_botRun = false;
HANDLE g_botThread = nullptr;
long long g_updateOffset = 0;
std::vector<std::pair<long long, std::wstring>> g_pending;
std::wstring g_botLastError;
bool g_botPolling = false;

std::wstring BotConfigPath() { return PathJoin(GetDataDir(), L"bot.json"); }

void BotLock() { if (g_botCsInit) EnterCriticalSection(&g_botCs); }
void BotUnlock() { if (g_botCsInit) LeaveCriticalSection(&g_botCs); }

bool BotStartsWith(const std::wstring& s, const wchar_t* p) {
    size_t n = wcslen(p);
    return s.size() >= n && s.compare(0, n, p) == 0;
}

void LoadBotConfig() {
    app.bot.token.clear();
    app.bot.chatId = 0;
    app.bot.enabled = false;
    bool ok = false;
    std::string raw = ReadFileBytes(BotConfigPath(), &ok);
    if (ok && !raw.empty()) {
        JValue root = JsonParse(raw);
        if (root.IsObj()) {
            app.bot.enabled = root.Get(L"enabled").AsBool(false);
            app.bot.token = root.Get(L"token").AsStr(L"");
            app.bot.chatId = (long long)root.Get(L"chatId").AsNum(0);
        }
    }
}

void SaveBotConfig() {
    JValue root = JValue::Obj();
    root.Set(L"enabled") = JValue(app.bot.enabled);
    root.Set(L"token") = JValue(app.bot.token);
    root.Set(L"chatId") = JValue((double)app.bot.chatId);
    WriteFileBytesAtomic(BotConfigPath(), JsonDump(root, true));
}

bool BotHttp(const std::wstring& method, const std::wstring& path, const std::string& body,
             std::string& resp, std::wstring& err) {
    if (app.bot.token.empty()) { err = L"未填写 Bot Token"; return false; }
    HINTERNET hs = WinHttpOpen(L"CAELENDAR-Bot/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hs) { err = L"WinHTTP 初始化失败"; return false; }
    HINTERNET hc = WinHttpConnect(hs, L"api.telegram.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hr = hc ? WinHttpOpenRequest(hc, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                      : nullptr;
    bool ok = false;
    if (hr) {
        DWORD timeout = 8000;
        WinHttpSetOption(hr, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
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
                if (resp.size() > 2 * 1024 * 1024) break;
            }
            if (status >= 200 && status < 300) ok = true;
            else err = Fmt(L"HTTP %d", (int)status);
        } else {
            err = L"无法连接 api.telegram.org";
        }
    } else {
        err = L"请求创建失败";
    }
    if (hr) WinHttpCloseHandle(hr);
    if (hc) WinHttpCloseHandle(hc);
    WinHttpCloseHandle(hs);
    return ok;
}

std::wstring WebhookPath(const wchar_t* method) {
    return L"/bot" + app.bot.token + L"/" + method;
}

bool BotPollOnce() {
    std::wstring err;
    std::string resp;
    std::wstring path = WebhookPath(L"getUpdates") + L"?offset=" + std::to_wstring(g_updateOffset) + L"&timeout=0";
    bool ok = BotHttp(L"GET", path, "", resp, err);
    BotLock();
    g_botLastError = ok ? L"" : err;
    g_botPolling = ok;
    BotUnlock();
    if (!ok) return false;
    JValue root = JsonParse(resp);
    if (!root.IsObj() || !root.Get(L"ok").AsBool(false)) return false;
    if (!root.Has(L"result") || !root.Get(L"result").IsArr()) return true;
    for (auto& up : root.Get(L"result").arr) {
        if (!up.IsObj()) continue;
        long long uid = (long long)up.Get(L"update_id").AsNum(0);
        if (uid >= g_updateOffset) g_updateOffset = uid + 1;
        if (!up.Has(L"message")) continue;
        const JValue& msg = up.Get(L"message");
        std::wstring text = msg.Has(L"text") ? msg.Get(L"text").AsStr() : L"";
        long long chat = 0;
        if (msg.Has(L"chat") && msg.Get(L"chat").Has(L"id"))
            chat = (long long)msg.Get(L"chat").Get(L"id").AsNum(0);
        if (text.empty() || chat == 0) continue;
        BotLock();
        g_pending.push_back({ chat, text });
        BotUnlock();
    }
    return true;
}

DWORD WINAPI BotThread(LPVOID) {
    while (g_botRun) {
        BotPollOnce();
        for (int i = 0; i < 25 && g_botRun; ++i) Sleep(100);
    }
    return 0;
}

void BotStartThread() {
    if (!g_botCsInit) { InitializeCriticalSection(&g_botCs); g_botCsInit = true; }
    g_botRun = true;
    g_botThread = CreateThread(nullptr, 0, BotThread, nullptr, 0, nullptr);
}

void BotStopThread() {
    g_botRun = false;
    if (g_botThread) {
        WaitForSingleObject(g_botThread, 3000);
        CloseHandle(g_botThread);
        g_botThread = nullptr;
    }
}

bool BotSendMessage(long long chatId, const std::wstring& text) {
    if (app.bot.token.empty()) return false;
    std::wstring t = text;
    if (t.size() > 3500) t = t.substr(0, 3500) + L"…";
    JValue req = JValue::Obj();
    req.Set(L"chat_id") = JValue((double)chatId);
    req.Set(L"text") = JValue(t);
    std::string resp;
    std::wstring err;
    bool ok = BotHttp(L"POST", WebhookPath(L"sendMessage"), JsonDump(req, false), resp, err);
    if (!ok) {
        BotLock();
        g_botLastError = err;
        BotUnlock();
    }
    return ok;
}

std::wstring FormatOccList(int days) {
    DateTime from = StartOfDay(Today());
    DateTime to = AddDaysDT(from, days);
    auto occs = ExpandOccurrences(app.store, from, to);
    std::wstring out;
    int n = 0;
    int lastDay = -1;
    app.bot.lastList.clear();
    for (auto& o : occs) {
        if (o.ev.allDay) continue;
        int ds = DaySerial(o.start.y, o.start.mo, o.start.d);
        if (ds != lastDay) {
            out += (out.empty() ? L"" : L"\n") + Fmt(L"%d/%d %s", o.start.mo, o.start.d, WeekdayCN(DayOfWeek(o.start.y, o.start.mo, o.start.d)));
            lastDay = ds;
        }
        out += Fmt(L"\n%02d:%02d %s", o.start.h, o.start.mi, o.ev.title.c_str());
        if (!o.ev.location.empty()) out += L" @" + o.ev.location;
        app.bot.lastList.push_back(OccKey(o));
        if (++n >= 20) { out += L"\n…"; break; }
    }
    if (out.empty()) out = L"这段时间没有日程。";
    return out;
}

void BotCheckReminders() {
    if (!app.bot.enabled || app.bot.chatId == 0) return;
    DateTime now = NowDT();
    DateTime from = AddDaysDT(StartOfDay(now), -1);
    DateTime to = AddDaysDT(StartOfDay(now), 2);
    auto occs = ExpandOccurrences(app.store, from, to);
    int pushed = 0;
    for (auto& o : occs) {
        if (o.ev.allDay || o.ev.done) continue;
        for (auto& al : o.ev.alarms) {
            DateTime at = AddMinutesDT(o.start, -al.offsetMin);
            if (CompareDT(at, now) > 0) continue;
            if (CompareDT(AddMinutesDT(at, 5), now) < 0) continue;   // 只推 5 分钟内到点的
            std::wstring key = OccKey(o) + L"|" + std::to_wstring(al.offsetMin);
            bool seen = false;
            for (auto& s2 : app.bot.sentReminders) if (s2 == key) { seen = true; break; }
            if (seen) continue;
            app.bot.sentReminders.push_back(key);
            while (app.bot.sentReminders.size() > 200) app.bot.sentReminders.erase(app.bot.sentReminders.begin());
            std::wstring msg = Fmt(L"⏰ 提醒：%s\n%s", o.ev.title.c_str(), TimeStr(o.start).c_str());
            if (!o.ev.location.empty()) msg += L" @" + o.ev.location;
            if (!app.bot.token.empty()) BotSendMessage(app.bot.chatId, msg);
            DebugLog(L"bot reminder due: %s", key.c_str());
            if (++pushed >= 5) break;
        }
        if (pushed >= 5) break;
    }
}

std::wstring BotHandleCommand(long long chatId, const std::wstring& raw) {
    std::wstring text = Trim(raw);
    if (app.bot.chatId == 0) {
        app.bot.chatId = chatId;
        SaveBotConfig();
    } else if (app.bot.chatId != chatId) {
        return L"";
    }
    if (BotStartsWith(text, L"/start")) {
        return L"已绑定当前聊天。\n/today 今日日程\n/week 未来 7 天\n/next 接下来 3 条\n/search 关键词\n/done 序号\n/add 明天 9:00 开会 #会议\n/help 帮助";
    }
    if (BotStartsWith(text, L"/help")) {
        return L"命令：\n/today 今日日程\n/week 未来 7 天\n/next 接下来 3 条\n/search <关键词> 搜索\n/done <序号> 标记完成（先 /today 或 /week）\n/add <自然语言> 新建日程\n/help 帮助\n到点提醒会主动推送。只响应当前绑定的聊天。";
    }
    if (BotStartsWith(text, L"/today")) return FormatOccList(1);
    if (BotStartsWith(text, L"/week")) return FormatOccList(7);
    if (BotStartsWith(text, L"/next")) {
        DateTime from = NowDT();
        DateTime to = AddDaysDT(from, 30);
        auto occs = ExpandOccurrences(app.store, from, to);
        std::wstring out;
        int n = 0;
        for (auto& o : occs) {
            if (o.ev.allDay) continue;
            if (CompareDT(o.start, from) < 0) continue;
            out += Fmt(L"%s\n%d/%d %02d:%02d %s\n", n == 0 ? L"" : L"\n", o.start.mo, o.start.d, o.start.h, o.start.mi, o.ev.title.c_str());
            if (++n >= 3) break;
        }
        return out.empty() ? L"未来 30 天没有日程。" : out;
    }
    if (BotStartsWith(text, L"/search ")) {
        std::wstring q = Trim(text.substr(8));
        if (q.empty()) return L"用法：/search 关键词";
        DateTime from = StartOfDay(Today());
        DateTime to = AddDaysDT(from, 60);
        auto occs = ExpandOccurrences(app.store, from, to);
        std::wstring out;
        int n = 0;
        for (auto& o : occs) {
            if (o.ev.title.find(q) == std::wstring::npos && o.ev.location.find(q) == std::wstring::npos &&
                o.ev.desc.find(q) == std::wstring::npos) continue;
            out += Fmt(L"\n%d/%d %02d:%02d %s", o.start.mo, o.start.d, o.start.h, o.start.mi, o.ev.title.c_str());
            if (++n >= 10) break;
        }
        return out.empty() ? (L"没找到包含「" + q + L"」的日程。") : (L"搜索「" + q + L"」：" + out);
    }
    if (BotStartsWith(text, L"/done")) {
        std::wstring arg = Trim(text.size() > 5 ? text.substr(5) : L"");
        int idx = _wtoi(arg.c_str());
        if (idx <= 0) return L"用法：先 /today 或 /week，再 /done 序号";
        if (app.bot.lastList.empty()) return L"先发 /today 或 /week 获取列表，再 /done 序号。";
        if (idx > (int)app.bot.lastList.size()) return Fmt(L"序号超出范围（当前列表 %d 条）", (int)app.bot.lastList.size());
        std::wstring key = app.bot.lastList[idx - 1];
        // 找到对应 occurrence 并标记完成
        DateTime df = StartOfDay(Today());
        auto occs = ExpandOccurrences(app.store, AddDaysDT(df, -1), AddDaysDT(df, 30));
        for (auto& o : occs) {
            if (OccKey(o) != key) continue;
            app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, true);
            DoSaveStore();
            InvalidateOccs();
            app.bot.lastList.erase(app.bot.lastList.begin() + (idx - 1));
            return L"已完成：" + o.ev.title;
        }
        return L"这条日程已经找不到了。";
    }
    if (BotStartsWith(text, L"/add ")) {
        std::wstring arg = Trim(text.substr(5));
        if (arg.empty()) return L"用法：/add 明天 9:00 开会 #会议";
        QuickResult q = ParseQuick(arg, Today(), &app.store);
        if (!q.ok) return L"没看懂：" + q.error;
        for (auto& kv : q.tagNames) {
            if (!app.store.FindTag(kv.first)) app.store.AddTag(kv.second, L"#6EA8FF", L"tag");
        }
        app.store.Snapshot();
        app.store.AddEvent(q.ev);
        InvalidateOccs();
        DoSaveStore();
        return L"已添加：" + (q.summary.empty() ? q.ev.title : q.summary);
    }
    return L"未知命令，发 /help 查看。";
}

} // namespace

// ---------------- 对外接口 ----------------
void BotInit() {
    LoadBotConfig();
    if (!g_botCsInit) { InitializeCriticalSection(&g_botCs); g_botCsInit = true; }
    if (app.bot.enabled && !app.bot.token.empty()) {
        BotStartThread();
        app.bot.status = L"轮询中";
    } else {
        app.bot.status = app.bot.token.empty() ? L"未配置 Token" : L"未启用";
    }
}

void BotShutdown() {
    BotStopThread();
}

void BotTick() {
    if (!g_botCsInit) return;
    std::vector<std::pair<long long, std::wstring>> msgs;
    BotLock();
    msgs.swap(g_pending);
    std::wstring err = g_botLastError;
    bool polling = g_botPolling;
    BotUnlock();
    for (auto& m : msgs) {
        std::wstring reply = BotHandleCommand(m.first, m.second);
        if (!reply.empty()) BotSendMessage(m.first, reply);
    }
    if (app.bot.enabled) {
        BotCheckReminders();
        if (!err.empty()) app.bot.status = L"错误：" + err;
        else app.bot.status = polling ? (app.bot.chatId ? L"轮询中 · 已绑定" : L"轮询中 · 等待 /start 绑定") : L"连接中";
    }
}

std::wstring BotTestCommand(const std::wstring& raw) {
    return BotHandleCommand(app.bot.chatId ? app.bot.chatId : 1, raw);
}

void BotSetEnabled(bool on) {
    app.bot.enabled = on;
    SaveBotConfig();
    if (on && !app.bot.token.empty()) {
        BotStopThread();
        BotStartThread();
        app.bot.status = L"轮询中";
    } else {
        BotStopThread();
        app.bot.status = on ? L"未配置 Token" : L"未启用";
    }
}

void BotSaveToken(const std::wstring& token) {
    app.bot.token = Trim(token);
    SaveBotConfig();
    if (app.bot.enabled && !app.bot.token.empty()) {
        BotStopThread();
        BotStartThread();
        app.bot.status = L"轮询中";
    }
}
