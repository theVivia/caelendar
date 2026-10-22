// reminder.cpp - 日程提醒 / 喝水提醒弹窗与调度
#include "app.h"
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

using namespace Gdiplus;
using namespace gfx;

struct PopupItem {
    std::wstring title;
    std::wstring body;
    int kind;          // 0=日程提醒 1=喝水
    std::string eventId;
    DateTime origStart;
    DateTime occStart;
    DateTime occEnd;
};

static std::deque<PopupItem> g_queue;
static std::vector<std::pair<PopupItem, DateTime>> g_snoozed;
static HWND g_popup = nullptr;
static PopupItem g_current;
static std::map<std::wstring, DateTime> g_fired;
static DateTime g_nextWater{ 0,0,0,0,0 };
static DateTime g_waterSnooze{ 0,0,0,0,0 };
struct MissedItem { PopupItem p; DateTime missedAt; };
static std::vector<MissedItem> g_missed;

static void AddMissed(const PopupItem& p) {
    if (p.kind != 0 || p.eventId.empty()) return;
    for (auto& m : g_missed)
        if (m.p.eventId == p.eventId && SameDay(m.p.occStart, p.occStart)) return;
    g_missed.push_back(MissedItem{ p, NowDT() });
    if (g_missed.size() > 100) g_missed.erase(g_missed.begin());
    DebugLog(L"提醒进入错过列表：%s", p.title.c_str());
}

static bool IsFocusImportant(const PopupItem& p) {
    const Event* e = app.store.Find(p.eventId);
    if (!e) return true;
    for (auto& t : e->tags) if (t == "homework" || t == "exam") return true;
    const std::wstring& s = e->title;
    static const wchar_t* keys[] = { L"作业", L"考试", L"期中", L"期末", L"截止", L"DDL", L"提交", L"测验" };
    for (auto* k : keys) if (s.find(k) != std::wstring::npos) return true;
    return false;
}
static bool g_engineOn = false;
static UINT_PTR g_soundTimeout = 0;

enum { RBTN_OK = 1, RBTN_SNOOZE, RBTN_DRINK, RBTN_CLOSE, RBTN_DONE, RBTN_EDIT };
struct Btn { RectF r; int id; std::wstring label; };
static std::vector<Btn> g_btns;
static int g_hoverBtn = 0;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}
static void PlaySoundSafe(bool enabled) {
    if (!enabled) return;
    const std::wstring& f = app.store.settings.notify.soundFile;
    if (!f.empty() && GetFileAttributesW(f.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (PlaySoundW(f.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
    }
    PlaySoundW(L"SystemAsterisk", nullptr, SND_ALIAS | SND_ASYNC);
}

static float PopupK() {
    int s = std::max(80, std::min(160, app.store.settings.widget.scale));
    return s / 100.0f;
}

static void DrawPopupBg(Graphics& g, float Wd, float Hd, const Theme& th, Color accent) {
    std::wstring skin = app.store.settings.widget.skin;
    float rad = S(16);
    RectF full(0, 0, Wd, Hd);
    if (skin == L"solid") {
        FillRoundRect(g, full, rad, Color(244, 11, 14, 22));
        StrokeRoundRect(g, RectF(0.5f, 0.5f, Wd - 1, Hd - 1), rad, WithAlpha(accent, 90), S(1));
    } else if (skin == L"neon") {
        FillRoundRect(g, full, rad, Color(240, 5, 7, 12));
        FillVGradient(g, RectF(0, 0, Wd, S(52)), WithAlpha(accent, 32), WithAlpha(accent, 0));
        StrokeRoundRect(g, RectF(0.8f, 0.8f, Wd - 1.6f, Hd - 1.6f), rad, WithAlpha(accent, 215), S(1.6f));
        StrokeRoundRect(g, RectF(S(2.5f), S(2.5f), Wd - S(5), Hd - S(5)), rad, WithAlpha(th.accent2, 60), S(1));
    } else if (skin == L"glass") {
        FillRoundRect(g, full, rad, Color(212, 22, 26, 38));
        FillVGradient(g, RectF(0, 0, Wd, S(52)), WithAlpha(accent, 24), WithAlpha(accent, 0));
        StrokeRoundRect(g, RectF(0.5f, 0.5f, Wd - 1, Hd - 1), rad, WithAlpha(th.border, 235), S(1));
    } else {
        FillRoundRect(g, full, rad, th.card);
        FillVGradient(g, RectF(0, 0, Wd, S(52)), WithAlpha(accent, 16), WithAlpha(accent, 0));
        StrokeRoundRect(g, RectF(0.5f, 0.5f, Wd - 1, Hd - 1), rad, th.border, S(1));
    }
}

static void RenderPopup() {
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    if (!g_popup) return;
    RECT rc; GetClientRect(g_popup, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) return;
    float k = PopupK();
    float Wd = W / k, Hd = H / k;
    HDC screen = GetDC(g_popup);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    {
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.ScaleTransform(k, k);
        const Theme& th = TH();
        Color accent = g_current.kind == 1 ? th.accent2 : th.accent;
        DrawPopupBg(g, Wd, Hd, th, accent);
        // 左侧强调条
        FillRoundRect(g, RectF(0, S(16), S(3.5f), Hd - S(32)), S(2), accent);
        float x = S(22), y = S(14);
        int icon = g_current.kind == 1 ? ICO_DROP : ICO_BELL;
        Icon(g, icon, RectF(x, y, S(18), S(18)), accent, 1.7f);
        Txt(g, g_current.kind == 1 ? L"该喝水啦" : L"日程提醒", S(10), false, th.dim2,
            RectF(x + S(26), y + S(1), S(220), S(18)));
        Txt(g, g_current.title, S(14.5f), true, th.text, RectF(x, y + S(22), Wd - x - S(20), S(24)), ALIGN_LEFT, false, true);
        Txt(g, g_current.body, S(10.5f), false, th.dim, RectF(x, y + S(48), Wd - x - S(20), S(38)), ALIGN_LEFT, false, true);
        // 按钮
        g_btns.clear();
        float by = Hd - S(46);
        float bx = Wd - S(16);
        auto addBtn = [&](const std::wstring& label, int id, bool primary) {
            SizeF sz; MeasureText(g, label, S(11), false, sz);
            float bw = sz.Width + S(26);
            bx -= bw;
            RectF r(bx, by, bw, S(30));
            bool hover = g_hoverBtn == id;
            if (primary) {
                FillRoundRect(g, r, S(7), hover ? accent : WithAlpha(accent, 220));
                Txt(g, label, S(11), true, th.onAccent, r, ALIGN_CENTER | ALIGN_VCENTER);
            } else {
                FillRoundRect(g, r, S(7), hover ? th.cardHover : WithAlpha(th.panel2, 220));
                StrokeRoundRect(g, r, S(7), th.borderSoft, S(1));
                Txt(g, label, S(11), false, hover ? th.text : th.dim, r, ALIGN_CENTER | ALIGN_VCENTER);
            }
            g_btns.push_back(Btn{ r, id, label });
            bx -= S(8);
        };
        if (g_current.kind == 1) {
            addBtn(L"喝了一杯 +1", RBTN_DRINK, true);
            addBtn(L"10 分钟后", RBTN_SNOOZE, false);
        } else {
            addBtn(L"知道了", RBTN_OK, true);
            if (!g_current.eventId.empty()) {
                addBtn(L"编辑", RBTN_EDIT, false);
                addBtn(L"完成", RBTN_DONE, false);
            }
            addBtn(Fmt(L"稍后 %d 分钟", app.store.settings.notify.snoozeMin), RBTN_SNOOZE, false);
        }
        // 关闭小叉
        Txt(g, L"×", S(14), false, WithAlpha(th.dim2, 200), RectF(Wd - S(30), S(8), S(20), S(20)), ALIGN_CENTER | ALIGN_VCENTER);
        g_btns.push_back(Btn{ RectF(Wd - S(32), S(6), S(24), S(24)), RBTN_CLOSE, L"" });
    }
    HDC wdc = GetDC(g_popup);
    BitBlt(wdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    ReleaseDC(g_popup, wdc);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(g_popup, screen);
}

static void PositionPopup() {
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    RECT wr; GetWindowRect(g_popup, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    int x = work.right - w - (int)S(24);
    int y = work.bottom - h - (int)S(20);
    SetWindowPos(g_popup, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
static void ShowNextPopup();
static void HidePopup() {
    if (g_popup) ShowWindow(g_popup, SW_HIDE);
    if (!g_queue.empty()) ShowNextPopup();
}
static void ShowNextPopup() {
    if (!g_popup || g_queue.empty()) return;
    g_current = g_queue.front();
    g_queue.pop_front();
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    float k = PopupK();
    int w = (int)std::lround(S(400) * k), h = (int)std::lround(S(160) * k);
    SetWindowPos(g_popup, HWND_TOPMOST, 0, 0, w, h, SWP_NOMOVE | SWP_NOACTIVATE);
    PositionPopup();
    int rad = (int)std::lround(S(32) * k);
    SetWindowRgn(g_popup, CreateRoundRectRgn(0, 0, w + 1, h + 1, rad, rad), TRUE);
    int alpha = std::max(30, std::min(100, app.store.settings.widget.opacity));
    SetLayeredWindowAttributes(g_popup, 0, (BYTE)(alpha * 255 / 100), LWA_ALPHA);
    RenderPopup();
    ShowWindow(g_popup, SW_SHOWNOACTIVATE);
    if (g_current.kind == 0) PlaySoundSafe(app.store.settings.notify.sound);
    else PlaySoundSafe(app.store.settings.water.sound);
    SetTimer(g_popup, 1, 45000, nullptr);
}

static LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        RenderPopup();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (wParam == 1) {
            KillTimer(hwnd, 1);
            if (g_current.kind == 0) AddMissed(g_current);
            HidePopup();
        }
        return 0;
    case WM_MOUSEMOVE: {
        float x = (float)GET_X_LPARAM(lParam), y = (float)GET_Y_LPARAM(lParam);
        int hover = 0;
        for (auto& b : g_btns) if (x >= b.r.X && x <= b.r.X + b.r.Width && y >= b.r.Y && y <= b.r.Y + b.r.Height) hover = b.id;
        if (hover != g_hoverBtn) { g_hoverBtn = hover; RenderPopup(); SetCursor(LoadCursorW(nullptr, hover ? IDC_HAND : IDC_ARROW)); }
        return 0;
    }
    case WM_LBUTTONUP: {
        float x = (float)GET_X_LPARAM(lParam), y = (float)GET_Y_LPARAM(lParam);
        for (auto& b : g_btns) {
            if (x >= b.r.X && x <= b.r.X + b.r.Width && y >= b.r.Y && y <= b.r.Y + b.r.Height) {
                KillTimer(hwnd, 1);
                if (b.id == RBTN_EDIT) {
                    std::string eid = g_current.eventId;
                    DateTime orig = g_current.origStart;
                    HidePopup();
                    if (g_hwnd) {
                        ShowWindow(g_hwnd, SW_SHOW);
                        ShowWindow(g_hwnd, SW_RESTORE);
                        SetForegroundWindow(g_hwnd);
                    }
                    if (!eid.empty()) OpenEditorExisting(eid, orig);
                    UIInvalidate();
                    return 0;
                }
                if (b.id == RBTN_CLOSE && g_current.kind == 0) AddMissed(g_current);
                if (b.id == RBTN_DONE) {
                    if (!g_current.eventId.empty()) {
                        app.store.Snapshot();
                        if (app.store.SetOccurrenceDone(g_current.eventId, g_current.origStart,
                                                        g_current.occStart, g_current.occEnd, true)) {
                            DoSaveStore();
                            InvalidateOccs();
                            ShowToast(Fmt(L"已完成：%s", g_current.title.c_str()));
                            DebugLog(L"popup done toggle series=%s", Utf8ToWide(g_current.eventId).c_str());
                        }
                        UIInvalidate();
                    }
                } else if (b.id == RBTN_DRINK) {
                    app.store.Snapshot();
                    app.store.AddWaterCup();
                    DoSaveStore();
                    g_nextWater = AddMinutesDT(NowDT(), app.store.settings.water.intervalMin);
                    ShowToast(L"已记一杯水");
                    UIInvalidate();
                } else if (b.id == RBTN_SNOOZE) {
                    if (g_current.kind == 1) {
                        g_nextWater = AddMinutesDT(NowDT(), 10);
                    } else {
                        // 稍后再提醒：进入延时队列
                        PopupItem p = g_current;
                        DateTime when = AddMinutesDT(NowDT(), app.store.settings.notify.snoozeMin);
                        g_snoozed.push_back({ p, when });
                    }
                }
                HidePopup();
                UIInvalidate();
                return 0;
            }
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void StartReminderEngine() {
    g_engineOn = true;
    if (!g_popup) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PopupProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"XingLiReminder";
        RegisterClassExW(&wc);
        g_popup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, L"XingLiReminder", L"CAELENDAR 提醒",
                                  WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    }
    DateTime now = NowDT();
    g_nextWater = AddMinutesDT(now, app.store.settings.water.intervalMin);
    g_waterSnooze = DateTime{ 0,0,0,0,0 };
}

void StopReminderEngine() {
    g_engineOn = false;
    if (g_popup) {
        KillTimer(g_popup, 1);
        DestroyWindow(g_popup);
        g_popup = nullptr;
    }
    g_queue.clear();
}

void ShowTestReminder() {
    PopupItem p;
    p.kind = 0;
    p.title = L"高等数学";
    p.body = L"10月20日 周二 08:00 - 09:40 · 提前 10 分钟";
    if (!app.store.events.empty()) {
        p.eventId = app.store.events[0].id;
        p.origStart = app.store.events[0].start;
        p.occStart = app.store.events[0].start;
        p.occEnd = app.store.events[0].end;
    }
    if (g_popup && IsWindowVisible(g_popup)) g_queue.push_back(p);
    else { g_queue.push_back(p); ShowNextPopup(); }
}

void OnTimerTick() {
    if (!g_engineOn) return;
    DateTime now = NowDT();
    Settings& s = app.store.settings;
    // 到点的“稍后提醒”
    for (auto it = g_snoozed.begin(); it != g_snoozed.end();) {
        if (CompareDT(now, it->second) >= 0) {
            g_queue.push_back(it->first);
            it = g_snoozed.erase(it);
        } else ++it;
    }
    if (!g_queue.empty() && (!g_popup || !IsWindowVisible(g_popup))) ShowNextPopup();
    // 日程提醒
    if (s.notify.enabled) {
        auto dues = UpcomingAlarms(app.store, now, 1);
        for (auto& a : dues) {
            if (g_fired.count(a.key)) continue;
            g_fired[a.key] = a.fireAt;
            PopupItem p;
            p.kind = 0;
            p.title = a.title;
            std::wstring t = a.fireAt.h == 0 && a.fireAt.mi == 0 ? L"" : TimeStr(a.fireAt);
            p.body = Fmt(L"%s%s", DateTimeHuman(a.fireAt).c_str(), a.body.empty() ? L"" : (L" · " + a.body).c_str());
            p.eventId = a.seriesId;
            p.origStart = a.occurrenceStart;
            p.occStart = a.occStart;
            p.occEnd = a.occEnd;
            if (s.notify.focusMode && !IsFocusImportant(p)) continue;
            if (g_popup && IsWindowVisible(g_popup)) {
                if (g_queue.size() < 5) g_queue.push_back(p);
                else AddMissed(p);
            } else {
                g_queue.push_back(p);
                ShowNextPopup();
            }
        }
        // 清理 1 天前的 fired 记录
        if (g_fired.size() > 200) {
            for (auto it = g_fired.begin(); it != g_fired.end();) {
                if (CompareDT(it->second, AddDaysDT(now, -1)) < 0) it = g_fired.erase(it);
                else ++it;
            }
        }
    }
    // 喝水提醒
    if (s.water.enabled) {
        int nowMin = now.h * 60 + now.mi;
        bool inWindow = nowMin >= s.water.fromMin && nowMin <= s.water.toMin;
        if (inWindow) {
            if (g_nextWater.y == 0) g_nextWater = AddMinutesDT(now, s.water.intervalMin);
            if (CompareDT(now, g_nextWater) >= 0) {
                if (app.store.WaterToday() < s.water.targetCups) {
                    PopupItem p;
                    p.kind = 1;
                    p.title = L"喝水时间到";
                    p.body = Fmt(L"今天已喝 %d/%d 杯 · 起身动一动，喝口水吧", app.store.WaterToday(), s.water.targetCups);
                    if (g_popup && IsWindowVisible(g_popup)) {
                        if (g_queue.size() < 5) g_queue.push_back(p);
                    } else {
                        g_queue.push_back(p);
                        ShowNextPopup();
                    }
                }
                g_nextWater = AddMinutesDT(now, s.water.intervalMin);
            }
        } else {
            // 非活跃时段：把下次提醒推到下一个活跃窗口开始
            DateTime next{ now.y, now.mo, now.d, s.water.fromMin / 60, s.water.fromMin % 60 };
            if (nowMin > s.water.toMin) next = AddDaysDT(next, 1);
            g_nextWater = next;
        }
    }
}
// ---------------- 提醒中心（错过提醒） ----------------
int MissedReminderCount() { return (int)g_missed.size(); }

void OpenReminderCenter() {
    app.reminderScroll = 0;
    app.overlay = OV_REMINDERS;
    UIInvalidate();
}

static void ReminderFrame(Graphics& g, float W, float H, float pw, float ph, float& px, float& py) {
    const Theme& th = TH();
    pw = std::min(pw, W - S(40));
    ph = std::min(ph, H - S(30));
    px = (W - pw) / 2;
    py = (H - ph) / 2;
    RectF r(px, py, pw, ph);
    Shadow(g, r, S(18), 8, 24);
    FillRoundRect(g, r, S(18), th.card);
    StrokeRoundRect(g, r, S(18), WithAlpha(th.border, 255), S(1));
    FillVGradient(g, RectF(px, py, pw, S(60)), WithAlpha(th.accent, 10), WithAlpha(th.accent, 0));
}

void DrawReminderCenterOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(560);
    float x, y;
    ReminderFrame(g, W, H, pw, ph, x, y);
    Txt(g, L"提醒中心", S(18), true, th.text, RectF(x + S(24), y + S(16), S(200), S(28)));
    Txt(g, L"这里收集被自动关闭 / 错过的提醒；完成会同步到日历", S(9), false, th.dim2,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_RM_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    float listY = y + S(80);
    float rowH = S(54);
    int maxRows = (int)((ph - S(160)) / rowH);
    maxRows = std::max(1, maxRows);
    int total = (int)g_missed.size();
    int maxScroll = std::max(0, total - maxRows);
    app.reminderScroll = std::max(0, std::min(app.reminderScroll, maxScroll));
    int start = total - 1 - app.reminderScroll;   // 从最新一条开始
    if (total == 0) {
        Txt(g, L"没有错过的提醒。被自动关闭的日程提醒会出现在这里。", S(11), false, th.dim2,
            RectF(x + S(24), listY + S(24), pw - S(48), S(40)), ALIGN_CENTER);
    } else {
        int shown = 0;
        for (int idx = start; idx >= 0 && shown < maxRows; --idx, ++shown) {
            auto& it = g_missed[idx];
            float ry = listY + shown * rowH;
            RectF row(x + S(20), ry, pw - S(40), rowH - S(6));
            FillRoundRect(g, row, S(8), WithAlpha(th.panel2, 180));
            Icon(g, ICO_BELL, RectF(row.X + S(12), row.Y + S(10), S(15), S(15)), th.accent, 1.4f);
            Txt(g, it.p.title, S(10.5f), true, th.text, RectF(row.X + S(36), row.Y + S(4), row.Width - S(230), S(18)), ALIGN_LEFT, false, true);
            std::wstring when = RelativeDayLabel(it.missedAt) + L" " + TimeStr(it.missedAt);
            Txt(g, Fmt(L"%s · 错过于 %s", it.p.body.c_str(), when.c_str()), S(8.5f), false, th.dim2,
                RectF(row.X + S(36), row.Y + S(24), row.Width - S(230), S(16)), ALIGN_LEFT, false, true);
            float bx = RX(row) - S(8);
            auto itemBtn = [&](const wchar_t* label, int cmd, int index) {
                SizeF sz; MeasureText(g, label, S(9), false, sz);
                float bw = sz.Width + S(16);
                bx -= bw;
                UI::Btn(g, RectF(bx, row.Y + S(10), bw, S(24)), cmd, label, 1, -1, [index, cmd]() {
                    if (index < 0 || index >= (int)g_missed.size()) return;
                    MissedItem mi = g_missed[index];
                    if (cmd == CMD_RM_DONE_BASE + index) {
                        app.store.Snapshot();
                        if (app.store.SetOccurrenceDone(mi.p.eventId, mi.p.origStart, mi.p.occStart, mi.p.occEnd, true)) {
                            DoSaveStore();
                            InvalidateOccs();
                            ShowToast(Fmt(L"已完成：%s", mi.p.title.c_str()));
                        }
                    } else if (cmd == CMD_RM_EDIT_BASE + index) {
                        HidePopup();
                        if (g_hwnd) {
                            ShowWindow(g_hwnd, SW_SHOW);
                            ShowWindow(g_hwnd, SW_RESTORE);
                            SetForegroundWindow(g_hwnd);
                        }
                        OpenEditorExisting(mi.p.eventId, mi.p.origStart);
                    }
                    g_missed.erase(g_missed.begin() + index);
                    UIInvalidate();
                });
                bx -= S(6);
            };
            itemBtn(L"忽略", CMD_RM_DISMISS_BASE + idx, idx);
            itemBtn(L"编辑", CMD_RM_EDIT_BASE + idx, idx);
            itemBtn(L"完成", CMD_RM_DONE_BASE + idx, idx);
        }
        if (total > maxRows)
            Txt(g, Fmt(L"共 %d 条 · 滚轮查看更多", total), S(8.5f), false, th.dim2,
                RectF(x + S(24), y + ph - S(100), S(300), S(16)));
    }

    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_RM_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    if (total > 0) {
        UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(100), y + ph - S(52), S(90), S(34)),
                CMD_RM_CLEAR, L"全部忽略", 1, -1, []() {
                    g_missed.clear();
                    app.reminderScroll = 0;
                    UIInvalidate();
                    ShowToast(L"已清空错过的提醒");
                });
    }
}
void SeedReminderCenterDemo() {
    int n = 0;
    for (auto& e : app.store.events) {
        if (n >= 3) break;
        PopupItem p;
        p.kind = 0;
        p.title = e.title;
        p.body = Fmt(L"%s · 示例错过提醒", DateTimeHuman(e.start).c_str());
        p.eventId = e.id;
        p.origStart = e.start;
        p.occStart = e.start;
        p.occEnd = e.end;
        AddMissed(p);
        ++n;
    }
}