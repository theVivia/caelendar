// focus.cpp - 番茄钟 / 专注计时（与学习统计联动）
#include "app.h"
#include "json.h"
#include <ctime>
#include <algorithm>

using namespace Gdiplus;
using namespace gfx;

namespace {

std::wstring FocusPath() { return PathJoin(GetDataDir(), L"focus.json"); }

void SaveSessions() {
    EnsureDir(GetDataDir());
    JValue root = JValue::Obj();
    root.Set(L"version") = JValue(1);
    JValue arr = JValue::Arr();
    for (auto& s : app.focus.sessions) {
        JValue o = JValue::Obj();
        o.Set(L"id") = JValue(Utf8ToWide(s.id));
        o.Set(L"start") = JValue(ISODT(s.start));
        o.Set(L"end") = JValue(ISODT(s.end));
        o.Set(L"minutes") = JValue(s.minutes);
        o.Set(L"label") = JValue(s.label);
        arr.Push(o);
    }
    root.Set(L"sessions") = arr;
    WriteFileBytesAtomic(FocusPath(), JsonDump(root, true));
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

int DayIndex(const DateTime& d) { return DaySerial(d.y, d.mo, d.d); }

void EnsureRemaining() {
    if (app.focus.remainingSec <= 0) app.focus.remainingSec = app.focus.minutes * 60;
}

}  // namespace

void FocusInit() {
    app.focus.sessions.clear();
    bool ok = false;
    std::string raw = ReadFileBytes(FocusPath(), &ok);
    if (!ok || raw.empty()) return;
    std::wstring err;
    JValue root = JsonParse(raw, &err);
    if (!root.IsObj()) return;
    for (auto& v : root.Get(L"sessions").arr) {
        if (!v.IsObj()) continue;
        FocusSession s;
        s.id = WideToUtf8(v.Get(L"id").AsStr());
        DateInputParse(v.Get(L"start").AsStr(), Today(), s.start);
        DateInputParse(v.Get(L"end").AsStr(), Today(), s.end);
        s.minutes = std::max(0, std::min(24 * 60, v.Get(L"minutes").AsInt(0)));
        s.label = v.Get(L"label").AsStr();
        if (s.minutes <= 0) continue;
        app.focus.sessions.push_back(s);
    }
    while (app.focus.sessions.size() > 500) app.focus.sessions.erase(app.focus.sessions.begin());
}

void FocusRecordSession(int minutes, const std::wstring& label) {
    if (minutes <= 0) return;
    FocusSession s;
    s.id = WideToUtf8(Fmt(L"fc%lld", (long long)time(nullptr)));
    s.end = NowDT();
    s.start = AddMinutesDT(s.end, -minutes);
    s.minutes = std::min(minutes, 24 * 60);
    s.label = label;
    app.focus.sessions.push_back(s);
    while (app.focus.sessions.size() > 500) app.focus.sessions.erase(app.focus.sessions.begin());
    SaveSessions();
    DebugLog(L"专注记录: %d 分钟（%s）", s.minutes, label.c_str());
}

void FocusStart() {
    EnsureRemaining();
    app.focus.running = true;
    app.focus.lastTick = GetTickCount64();
    DebugLog(L"专注开始: %d 秒", app.focus.remainingSec);
}

void FocusPause() {
    if (!app.focus.running) return;
    unsigned long long now = GetTickCount64();
    if (now > app.focus.lastTick) {
        int elapsed = (int)((now - app.focus.lastTick) / 1000);
        app.focus.remainingSec = std::max(0, app.focus.remainingSec - elapsed);
    }
    app.focus.lastTick = now;
    app.focus.running = false;
    DebugLog(L"专注暂停: 剩余 %d 秒", app.focus.remainingSec);
}

void FocusFinish(bool record) {
    int total = app.focus.minutes * 60;
    int usedSec = total - std::max(0, app.focus.remainingSec);
    if (app.focus.running) {
        unsigned long long now = GetTickCount64();
        if (now > app.focus.lastTick) {
            usedSec += (int)((now - app.focus.lastTick) / 1000);
        }
    }
    app.focus.running = false;
    int usedMin = usedSec / 60;
    if (record && usedMin >= 1) {
        FocusRecordSession(usedMin, app.focus.label);
        ShowToast(Fmt(L"已记录专注 %d 分钟", usedMin), 3000);
    }
    app.focus.remainingSec = app.focus.minutes * 60;
    app.focus.lastTick = 0;
    UIInvalidate();
}

void FocusReset() {
    app.focus.running = false;
    app.focus.lastTick = 0;
    app.focus.remainingSec = app.focus.minutes * 60;
    UIInvalidate();
}

void FocusTick() {
    if (!app.focus.running) return;
    unsigned long long now = GetTickCount64();
    if (now <= app.focus.lastTick) return;
    int elapsed = (int)((now - app.focus.lastTick) / 1000);
    if (elapsed <= 0) return;
    app.focus.lastTick += (unsigned long long)elapsed * 1000;
    app.focus.remainingSec = std::max(0, app.focus.remainingSec - elapsed);
    if (app.focus.remainingSec == 0) {
        app.focus.running = false;
        FocusRecordSession(app.focus.minutes, app.focus.label);
        MessageBeep(MB_ICONASTERISK);
        ShowToast(Fmt(L"专注完成：%d 分钟", app.focus.minutes), 4000);
        app.focus.remainingSec = app.focus.minutes * 60;
        app.focus.lastTick = 0;
        if (app.overlay == OV_FOCUS) UIInvalidate();
    } else if (app.overlay == OV_FOCUS && elapsed > 0) {
        UIInvalidate();
    }
}

int FocusTodayMinutes() {
    int today = DayIndex(Today()), sum = 0;
    for (auto& s : app.focus.sessions)
        if (DayIndex(s.start) == today) sum += s.minutes;
    return sum;
}

int FocusWeekMinutes() {
    DateTime today = Today();
    int wd = DayOfWeek(today.y, today.mo, today.d);   // 0=Sun
    int offset = app.store.settings.weekStart == 1 ? ((wd + 6) % 7) : wd;
    DateTime start = AddDaysDT(today, -offset);
    int from = DayIndex(start), sum = 0;
    for (auto& s : app.focus.sessions)
        if (DayIndex(s.start) >= from) sum += s.minutes;
    return sum;
}

int FocusWeekCount() {
    DateTime today = Today();
    int wd = DayOfWeek(today.y, today.mo, today.d);
    int offset = app.store.settings.weekStart == 1 ? ((wd + 6) % 7) : wd;
    DateTime start = AddDaysDT(today, -offset);
    int from = DayIndex(start), n = 0;
    for (auto& s : app.focus.sessions)
        if (DayIndex(s.start) >= from) ++n;
    return n;
}

bool FocusProbeRun(std::wstring& err) {
    FocusInit();
    int before = FocusWeekMinutes();
    FocusRecordSession(25, L"高数");
    FocusRecordSession(45, L"英语");
    int after = FocusWeekMinutes();
    bool file = FileExists(FocusPath());
    DebugLog(L"probe-focus: before=%d after=%d count=%d file=%d", before, after, FocusWeekCount(), file ? 1 : 0);
    err.clear();
    return after >= before + 70;
}

void OpenFocusPanel() {
    EnsureRemaining();
    app.overlay = OV_FOCUS;
    UIInvalidate();
}

void DrawFocusPanelOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(620), ph = S(560);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Txt(g, L"番茄钟 / 专注计时", S(17), true, th.text, RectF(x + S(24), y + S(16), S(320), S(26)));
    Txt(g, L"记录保存到本地 focus.json；本周专注会显示在学习统计里。", S(9), false, th.dim2,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    // 预设
    float cy = y + S(80);
    const int presets[4] = { 15, 25, 45, 60 };
    for (int i = 0; i < 4; ++i) {
        RectF r(x + S(24) + i * S(86), cy, S(78), S(28));
        int preset = presets[i];
        UI::Chip(g, r, CMD_IMPORT_MODE_BASE + 40 + i, Fmt(L"%d 分钟", preset),
                 app.focus.minutes == preset, th.accent, [preset]() {
                     app.focus.minutes = preset;
                     app.focus.running = false;
                     app.focus.remainingSec = preset * 60;
                     app.focus.lastTick = 0;
                     UIInvalidate();
                 }, 10);
    }
    Txt(g, L"本次专注内容", S(9), false, th.dim2, RectF(x + S(24), cy + S(44), S(200), S(16)));
    RectF labBox(x + S(24), cy + S(62), pw - S(48), S(32));
    FillRoundRect(g, labBox, S(8), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, labBox, S(8), th.borderSoft, S(1));
    UIEdit(9980, RectF(labBox.X + S(10), labBox.Y + S(6), labBox.Width - S(20), S(20)), &app.focus.label, false, S(11));

    // 圆环计时
    int total = std::max(1, app.focus.minutes * 60);
    float frac = 1.0f - (float)app.focus.remainingSec / total;
    float cx = x + pw / 2, cyc = y + S(250), radius = S(84);
    Pen ring(WithAlpha(th.border, 90), S(12));
    g.DrawEllipse(&ring, cx - radius, cyc - radius, radius * 2, radius * 2);
    if (frac > 0) {
        Pen arc(th.accent, S(12));
        arc.SetStartCap(LineCapRound);
        arc.SetEndCap(LineCapRound);
        g.DrawArc(&arc, cx - radius, cyc - radius, radius * 2, radius * 2, -90, 360.0f * frac);
    }
    int mm = app.focus.remainingSec / 60, ss = app.focus.remainingSec % 60;
    Txt(g, Fmt(L"%02d:%02d", mm, ss), S(40), true, app.focus.running ? th.accent : th.text,
        RectF(cx - S(120), cyc - S(30), S(240), S(60)), ALIGN_CENTER | ALIGN_VCENTER, true);
    Txt(g, app.focus.running ? L"专注中…" : (app.focus.remainingSec == app.focus.minutes * 60 ? L"准备开始" : L"已暂停"),
        S(9.5f), false, th.dim2, RectF(cx - S(100), cyc + S(38), S(200), S(18)), ALIGN_CENTER);

    // 按钮
    float by = y + S(368);
    UI::Btn(g, RectF(x + S(24), by, S(130), S(38)), CMD_IMPORT_OK,
            app.focus.running ? L"暂停" : L"开始专注", 0, app.focus.running ? ICO_PAUSE : ICO_PLAY, []() {
                if (app.focus.running) FocusPause();
                else FocusStart();
                UIInvalidate();
            });
    UI::Btn(g, RectF(x + S(164), by, S(120), S(38)), CMD_IMPORT_OK + 1, L"结束并记录", 1, ICO_CHECK, []() {
        FocusFinish(true);
    });
    UI::Btn(g, RectF(x + S(294), by, S(100), S(38)), CMD_IMPORT_OK + 2, L"重置", 1, ICO_REFRESH, []() {
        FocusFinish(false);
    });

    // 统计
    RectF st(x + S(24), y + S(424), pw - S(48), S(56));
    FillRoundRect(g, st, S(10), WithAlpha(th.panel2, 150));
    Txt(g, Fmt(L"今日专注 %d 分钟", FocusTodayMinutes()), S(11), true, th.accent,
        RectF(st.X + S(16), st.Y + S(8), S(220), S(20)));
    Txt(g, Fmt(L"本周 %d 分钟 · %d 次", FocusWeekMinutes(), FocusWeekCount()), S(9.5f), false, th.dim,
        RectF(st.X + S(16), st.Y + S(30), S(320), S(18)));
    Txt(g, L"番茄完成会自动记录；也可随时「结束并记录」。", S(8.5f), false, th.dim2,
        RectF(st.X + S(300), st.Y + S(30), st.Width - S(316), S(18)), ALIGN_RIGHT);
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
