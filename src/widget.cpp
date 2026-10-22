// widget.cpp - 桌面挂件 / 右下角提示窗（可自定义皮肤、大小、透明度、布局 + 一键完成最近日程）
#include "app.h"
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

static HWND g_widget = nullptr;
static bool g_dragging = false;
static POINT g_dragStart{};
static POINT g_winStart{};
static bool g_moved = false;

// 命中区（设计坐标 = dpi 坐标，渲染时按 widget.scale 整体缩放）
static RectF g_doneHit{};
static bool g_doneHitValid = false;
static RectF g_waterHit{};
static bool g_waterHitValid = false;
static bool g_hoverDone = false;
static HWND TopmostFlag();
static void ClampWidgetToMonitor();

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

static bool IsMode(const wchar_t* m) { return app.store.settings.widget.mode == m; }
static float WidgetK() {
    int s = std::max(80, std::min(160, app.store.settings.widget.scale));
    return s / 100.0f;
}
static void WidgetSize(int& w, int& h) {
    float bw = 340, bh = 182;
    if (IsMode(L"bar")) { bw = 340; bh = 56; }
    else if (IsMode(L"list")) { bw = 340; bh = 252; }
    else if (IsMode(L"clock")) { bw = 272; bh = 150; }
    else if (IsMode(L"week")) { bw = 340; bh = 200; }
    float k = WidgetK();
    w = (int)std::lround(S(bw) * k);
    h = (int)std::lround(S(bh) * k);
}

static void FindNext(const DateTime& now, Occurrence& next, bool& ongoing, int& todayRemain) {
    ongoing = false;
    todayRemain = 0;
    DateTime from = StartOfDay(now);
    auto occs = ExpandOccurrences(app.store, from, AddDaysDT(from, 3));
    bool found = false;
    for (auto& o : occs) {
        if (o.ev.done || o.ev.status == ST_CANCELLED) continue;
        if (SameDay(o.start, now)) ++todayRemain;
        if (CompareDT(o.start, now) <= 0 && CompareDT(o.end, now) > 0) {
            next = o; ongoing = true; found = true;
            break;
        }
        if (!found && CompareDT(o.start, now) > 0) {
            next = o; found = true;
        }
    }
    if (!found) {
        for (auto& o : occs) {
            if (o.ev.done || o.ev.status == ST_CANCELLED) continue;
            if (CompareDT(o.start, now) > 0) { next = o; break; }
        }
    }
}

static void FindUpcoming(const DateTime& now, std::vector<Occurrence>& list, int maxN = 3) {
    list.clear();
    DateTime from = StartOfDay(now);
    auto occs = ExpandOccurrences(app.store, from, AddDaysDT(from, 3));
    for (auto& o : occs) {
        if (o.ev.done || o.ev.status == ST_CANCELLED) continue;
        bool ongoing = CompareDT(o.start, now) <= 0 && CompareDT(o.end, now) > 0;
        if (ongoing || CompareDT(o.start, now) > 0) {
            list.push_back(o);
            if ((int)list.size() >= maxN) break;
        }
    }
}


static bool IsTaskEvent(const Occurrence& o) {
    for (auto& t : o.ev.tags) if (t == "homework" || t == "exam") return true;
    const std::wstring& s = o.ev.title;
    static const wchar_t* keys[] = { L"作业", L"考试", L"截止", L"DDL", L"提交", L"论文", L"报告", L"报名" };
    for (auto* k : keys) if (s.find(k) != std::wstring::npos) return true;
    return false;
}

static int TodayTaskCount(const DateTime& now) {
    int n = 0;
    DateTime from = StartOfDay(now);
    auto occs = ExpandOccurrences(app.store, from, AddDaysDT(from, 1));
    for (auto& o : occs) {
        if (o.ev.done || o.ev.status == ST_CANCELLED) continue;
        if (IsTaskEvent(o) && CompareDT(o.start, now) >= 0) ++n;
    }
    return n;
}

static Color NextColor(const Occurrence& o) {
    if (!o.ev.color.empty()) return Hex(o.ev.color);
    if (!o.ev.tags.empty()) return TagColorOf(o.ev.tags[0]);
    return Color(255, 148, 163, 184);
}

static std::wstring RelativeWhen(const DateTime& now, const Occurrence& o, bool ongoing) {
    if (o.ev.allDay) return ongoing ? L"全天 · 进行中" : L"全天";
    std::wstring s = FormatTimeRange(o);
    if (ongoing) s += L" · 进行中 · 还剩 " + HumanRemainCN(o.end, now);
    else s += L" · " + HumanFutureCN(o.start, now);
    return s;
}

static void DrawDoneButton(Graphics& g, RectF r, Color accent) {
    float cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;
    float rad = S(10);
    FillCircle(g, cx, cy, rad, g_hoverDone ? WithAlpha(accent, 70) : WithAlpha(TH().panel2, 190));
    StrokeCircle(g, cx, cy, rad, WithAlpha(accent, g_hoverDone ? 230 : 120), S(1.2f));
    Pen p(WithAlpha(accent, g_hoverDone ? 255 : 200), S(1.8f));
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawLine(&p, cx - S(4), cy, cx - S(1), cy + S(3));
    g.DrawLine(&p, cx - S(1), cy + S(3), cx + S(4.5f), cy - S(4));
}

static void DrawWidgetBg(Graphics& g, float Wd, float Hd) {
    const Theme& th = TH();
    std::wstring skin = app.store.settings.widget.skin;
    Color accent = th.accent;
    float rad = S(14);
    RectF full(0, 0, Wd, Hd);
    if (skin == L"solid") {
        FillRoundRect(g, full, rad, Color(242, 11, 14, 22));
        StrokeRoundRect(g, RectF(0.5f, 0.5f, Wd - 1, Hd - 1), rad, WithAlpha(accent, 70), S(1));
    } else if (skin == L"neon") {
        FillRoundRect(g, full, rad, Color(238, 5, 7, 12));
        FillVGradient(g, RectF(0, 0, Wd, S(72)), WithAlpha(accent, 30), WithAlpha(accent, 0));
        StrokeRoundRect(g, RectF(0.8f, 0.8f, Wd - 1.6f, Hd - 1.6f), rad, WithAlpha(accent, 210), S(1.5f));
        StrokeRoundRect(g, RectF(S(2.5f), S(2.5f), Wd - S(5), Hd - S(5)), rad, WithAlpha(th.accent2, 60), S(1));
    } else if (skin == L"glass") {
        FillRoundRect(g, full, rad, Color(208, 22, 26, 38));
        FillVGradient(g, RectF(0, 0, Wd, S(72)), WithAlpha(accent, 22), WithAlpha(accent, 0));
        StrokeRoundRect(g, RectF(0.5f, 0.5f, Wd - 1, Hd - 1), rad, WithAlpha(th.border, 235), S(1));
    } else {
        FillRoundRect(g, full, rad, th.card);
        FillVGradient(g, RectF(0, 0, Wd, S(72)), WithAlpha(accent, 12), WithAlpha(accent, 0));
        StrokeRoundRect(g, RectF(0.5f, 0.5f, Wd - 1, Hd - 1), rad, th.border, S(1));
    }
}

static void RenderWidget(HWND hwnd) {
    gfx::FontPartScope fontPart(gfx::FP_WIDGET);
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) return;
    float k = WidgetK();
    float Wd = W / k, Hd = H / k;
    HDC screen = GetDC(hwnd);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    g_doneHitValid = false;
    g_waterHitValid = false;
    {
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.ScaleTransform(k, k);   // 整体缩放：窗口 = 设计尺寸 × scale
        const Theme& th = TH();
        DateTime now = NowDT();
        DrawWidgetBg(g, Wd, Hd);
        Occurrence next;
        bool ongoing = false;
        int todayRemain = 0;
        FindNext(now, next, ongoing, todayRemain);
        Color nextColor = NextColor(next);
        bool hasNext = !next.ev.title.empty();
        int cups = app.store.WaterToday();
        int target = app.store.settings.water.targetCups;
        bool showDate = app.store.settings.widget.showDate;
        bool showWater = app.store.settings.widget.showWater;
        bool showToday = app.store.settings.widget.showToday;
        bool showTasks = app.store.settings.widget.showTasks;

        if (IsMode(L"bar")) {
            // 细条：时间 / 日期 / 下一项 / 倒计时 / ✓
            Txt(g, Fmt(L"%02d:%02d", now.h, now.mi), S(19), true, th.accent, RectF(S(14), 0, S(70), Hd), ALIGN_VCENTER, true);
            float tx = S(80);
            if (showDate) {
                Txt(g, Fmt(L"%d/%d %s", now.mo, now.d, WeekdayShortCN(DayOfWeek(now.y, now.mo, now.d))),
                    S(9.5f), false, th.dim2, RectF(S(80), 0, S(48), Hd), ALIGN_VCENTER);
                tx = S(132);
            }
            RectF doneR(Wd - S(40), Hd / 2 - S(15), S(30), S(30));
            g_doneHit = doneR;
            g_doneHitValid = hasNext;
            if (hasNext) {
                FillCircle(g, tx + S(3), Hd / 2, S(3), nextColor);
                Txt(g, DisplayEventTitle(next), S(11.5f), true, th.text,
                    RectF(tx + S(12), 0, Wd - tx - S(80), Hd), ALIGN_VCENTER);
                std::wstring cd;
                if (ongoing) cd = L"进行中";
                else cd = HumanFutureShortCN(next.start, now);
                Txt(g, cd, S(9.5f), true, nextColor, RectF(Wd - S(122), 0, S(74), Hd), ALIGN_RIGHT | ALIGN_VCENTER);
                DrawDoneButton(g, doneR, nextColor);
                g_waterHit = RectF(Wd - S(122), 0, S(74), Hd);
                g_waterHitValid = showWater;
            } else {
                Txt(g, L"今天没有日程了", S(11), false, th.dim2, RectF(tx, 0, Wd - tx - S(20), Hd), ALIGN_VCENTER);
            }
        } else if (IsMode(L"list")) {
            // 今日清单：时间 + 接下来 3 条
            Txt(g, Fmt(L"%02d:%02d", now.h, now.mi), S(24), true, th.text, RectF(S(16), S(6), S(100), S(32)), ALIGN_LEFT, true);
            if (showDate) {
                Txt(g, Fmt(L"%d月%d日 · %s", now.mo, now.d, WeekdayCN(DayOfWeek(now.y, now.mo, now.d))),
                    S(9.5f), false, th.dim, RectF(S(120), S(10), Wd - S(136), S(18)), ALIGN_RIGHT);
            }
            Line(g, S(16), S(52), Wd - S(16), S(52), WithAlpha(th.borderSoft, 200), S(1));
            std::vector<Occurrence> list;
            FindUpcoming(now, list, 3);
            float rowY = S(62);
            float rowH = S(42);
            for (int i = 0; i < (int)list.size(); ++i) {
                auto& o = list[i];
                Color c = NextColor(o);
                if (i == 0) {
                    RectF doneR(Wd - S(44), rowY + S(6), S(28), S(28));
                    g_doneHit = doneR;
                    g_doneHitValid = true;
                    DrawDoneButton(g, doneR, c);
                }
                FillCircle(g, S(24), rowY + S(12), S(3.2f), c);
                Txt(g, DisplayEventTitle(o), S(11.5f), true, th.text, RectF(S(34), rowY, Wd - S(90), S(22)), ALIGN_LEFT, false, true);
                std::wstring sub = RelativeWhen(now, o, CompareDT(o.start, now) <= 0 && CompareDT(o.end, now) > 0);
                Txt(g, sub, S(9), false, WithAlpha(c, 235), RectF(S(34), rowY + S(20), Wd - S(60), S(18)), ALIGN_LEFT, false, true);
                rowY += rowH;
            }
            if (list.empty()) {
                Txt(g, L"今天没有更多日程", S(11.5f), false, th.dim2, RectF(S(16), S(70), Wd - S(32), S(22)));
            }
            Line(g, S(16), Hd - S(52), Wd - S(16), Hd - S(52), WithAlpha(th.borderSoft, 160), S(1));
            if (showWater) {
                Icon(g, ICO_DROP, RectF(S(16), Hd - S(42), S(13), S(13)), th.accent2, 1.3f);
                Txt(g, Fmt(L"水 %d/%d", cups, target), S(9.5f), false, th.accent2,
                    RectF(S(33), Hd - S(45), S(70), S(18)), ALIGN_VCENTER);
                g_waterHit = RectF(S(12), Hd - S(48), S(96), S(24));
                g_waterHitValid = true;
            }
            if (showToday) {
                Txt(g, Fmt(L"今日剩 %d 项", todayRemain), S(9.5f), false, th.dim2,
                    RectF(S(110), Hd - S(45), S(120), S(18)), ALIGN_VCENTER);
            }
            Txt(g, L"点击打开", S(8.5f), false, WithAlpha(th.dim2, 170),
                RectF(Wd - S(110), Hd - S(45), S(94), S(18)), ALIGN_RIGHT | ALIGN_VCENTER);
        } else if (IsMode(L"clock")) {
            // 极简时钟
            Txt(g, Fmt(L"%02d:%02d", now.h, now.mi), S(40), true, th.accent, RectF(S(18), S(4), S(170), S(52)), ALIGN_LEFT, true);
            if (showDate) {
                Txt(g, Fmt(L"%d月%d日 · %s", now.mo, now.d, WeekdayCN(DayOfWeek(now.y, now.mo, now.d))),
                    S(9.5f), false, th.dim, RectF(S(20), S(52), Wd - S(40), S(18)));
            }
            if (showWater) {
                Icon(g, ICO_DROP, RectF(Wd - S(80), S(12), S(12), S(12)), th.accent2, 1.2f);
                Txt(g, Fmt(L"%d/%d", cups, target), S(9), false, th.accent2, RectF(Wd - S(64), S(10), S(50), S(16)));
                g_waterHit = RectF(Wd - S(84), S(6), S(74), S(26));
                g_waterHitValid = true;
            }
            Line(g, S(18), S(80), Wd - S(18), S(80), WithAlpha(th.borderSoft, 190), S(1));
            if (hasNext) {
                Txt(g, ongoing ? L"进行中" : L"接下来", S(8.5f), false, th.dim2, RectF(S(20), S(86), S(60), S(14)));
                Txt(g, DisplayEventTitle(next), S(11.5f), true, th.text, RectF(S(20), S(100), Wd - S(70), S(20)), ALIGN_LEFT, false, true);
                Txt(g, RelativeWhen(now, next, ongoing), S(8.5f), false, WithAlpha(nextColor, 235),
                    RectF(S(20), S(120), Wd - S(70), S(16)), ALIGN_LEFT, false, true);
                RectF doneR(Wd - S(46), S(94), S(30), S(30));
                g_doneHit = doneR;
                g_doneHitValid = true;
                DrawDoneButton(g, doneR, nextColor);
            } else {
                Txt(g, L"今天没有更多日程", S(11), false, th.dim2, RectF(S(20), S(92), Wd - S(40), S(20)));
            }
        } else if (IsMode(L"week")) {
            // 迷你周视图：顶部下一节 + 7 天事件分布 + 底部任务/喝水
            Txt(g, Fmt(L"%02d:%02d", now.h, now.mi), S(22), true, th.text, RectF(S(14), S(4), S(90), S(28)), ALIGN_LEFT, true);
            if (showDate)
                Txt(g, Fmt(L"%d月%d日 · %s", now.mo, now.d, WeekdayCN(DayOfWeek(now.y, now.mo, now.d))),
                    S(9.5f), false, th.dim, RectF(S(110), S(8), Wd - S(124), S(16)), ALIGN_RIGHT);
            Line(g, S(14), S(38), Wd - S(14), S(38), WithAlpha(th.borderSoft, 200), S(1));
            if (hasNext) {
                Txt(g, ongoing ? L"进行中" : L"下一节", S(8.5f), false, th.dim2, RectF(S(14), S(42), S(46), S(16)));
                Txt(g, DisplayEventTitle(next), S(10.5f), true, th.text, RectF(S(62), S(42), Wd - S(196), S(18)), ALIGN_LEFT, false, true);
                std::wstring cd;
                if (ongoing) cd = L"剩 " + HumanRemainCN(next.end, now);
                else cd = HumanFutureShortCN(next.start, now);
                Txt(g, cd, S(9), true, nextColor, RectF(Wd - S(132), S(42), S(82), S(18)), ALIGN_RIGHT, false, true);
                RectF doneR(Wd - S(34), S(40), S(26), S(26));
                g_doneHit = doneR;
                g_doneHitValid = true;
                DrawDoneButton(g, doneR, nextColor);
            } else {
                Txt(g, L"今天没有更多日程", S(10), false, th.dim2, RectF(S(14), S(44), Wd - S(28), S(16)), ALIGN_CENTER);
            }

            DateTime ws = StartOfWeek(now, app.store.settings.weekStart);
            auto weekOccs = ExpandOccurrences(app.store, ws, AddDaysDT(ws, 7));
            int counts[7] = { 0 };
            for (auto& o : weekOccs) {
                if (o.ev.done || o.ev.status == ST_CANCELLED) continue;
                int off = DaySerial(o.start.y, o.start.mo, o.start.d) - DaySerial(ws.y, ws.mo, ws.d);
                if (off >= 0 && off < 7) ++counts[off];
            }
            static const wchar_t* wdNames[7] = { L"一", L"二", L"三", L"四", L"五", L"六", L"日" };
            float colW = (Wd - S(28)) / 7.0f;
            for (int i = 0; i < 7; ++i) {
                DateTime day = AddDaysDT(ws, i);
                bool isToday = SameDay(day, now);
                RectF cr(S(14) + i * colW, S(74), colW - S(3), S(62));
                FillRoundRect(g, cr, S(7), WithAlpha(isToday ? th.accent : th.panel2, isToday ? 26 : 120));
                Txt(g, wdNames[(DayOfWeek(day.y, day.mo, day.d) + 6) % 7], S(8), false,
                    isToday ? th.accent : th.dim2, RectF(cr.X, cr.Y + S(3), cr.Width, S(13)), ALIGN_CENTER);
                Txt(g, Fmt(L"%d", day.d), S(12), isToday, isToday ? th.accent : th.text,
                    RectF(cr.X, cr.Y + S(16), cr.Width, S(18)), ALIGN_CENTER, true);
                if (counts[i] > 0) {
                    float barH = std::min(S(16), S(5) + counts[i] * S(2.5f));
                    FillRoundRect(g, RectF(cr.X + S(7), BY(cr) - S(5) - barH, S(8), barH), S(3),
                                  WithAlpha(th.accent2, 220));
                    Txt(g, Fmt(L"%d", counts[i]), S(8), false, th.dim2,
                        RectF(cr.X + S(17), BY(cr) - S(16), cr.Width - S(19), S(12)), ALIGN_RIGHT);
                }
            }
            Line(g, S(14), S(144), Wd - S(14), S(144), WithAlpha(th.borderSoft, 160), S(1));
            if (showWater) {
                Icon(g, ICO_DROP, RectF(S(14), S(152), S(12), S(12)), th.accent2, 1.2f);
                Txt(g, Fmt(L"水 %d/%d", cups, target), S(9), false, th.accent2, RectF(S(30), S(149), S(70), S(16)));
                g_waterHit = RectF(S(10), S(146), S(94), S(22));
                g_waterHitValid = true;
            }
            if (showTasks) {
                Txt(g, Fmt(L"今日任务 %d", TodayTaskCount(now)), S(9), false, th.dim,
                    RectF(S(106), S(149), S(110), S(16)));
            } else {
                Txt(g, Fmt(L"今日剩 %d 项", todayRemain), S(9), false, th.dim2,
                    RectF(S(106), S(149), S(110), S(16)));
            }
            Txt(g, L"周视图", S(8), false, WithAlpha(th.dim2, 150),
                RectF(Wd - S(64), S(169), S(54), S(14)), ALIGN_RIGHT);
        } else {
            // 卡片（默认）
            Txt(g, Fmt(L"%02d:%02d", now.h, now.mi), S(28), true, th.text, RectF(S(16), S(8), S(100), S(36)), ALIGN_LEFT, true);
            if (showDate) {
                Txt(g, Fmt(L"%d月%d日 · %s", now.mo, now.d, WeekdayCN(DayOfWeek(now.y, now.mo, now.d))),
                    S(9.5f), false, th.dim, RectF(S(16), S(44), S(190), S(16)));
            }
            Icon(g, ICO_STAR, RectF(Wd - S(30), S(14), S(15), S(15)), WithAlpha(th.accent, 200), 1.3f);
            Line(g, S(16), S(68), Wd - S(16), S(68), WithAlpha(th.borderSoft, 200), S(1));
            if (hasNext) {
                Txt(g, ongoing ? L"进行中" : L"接下来", S(9), false, th.dim2, RectF(S(16), S(74), S(60), S(14)));
                FillCircle(g, S(22), S(108), S(3.4f), nextColor);
                Txt(g, DisplayEventTitle(next), S(13), true, th.text, RectF(S(32), S(96), Wd - S(96), S(22)), ALIGN_LEFT, false, true);
                Txt(g, RelativeWhen(now, next, ongoing), S(9.5f), false, WithAlpha(nextColor, 240),
                    RectF(S(32), S(118), Wd - S(86), S(16)), ALIGN_LEFT, false, true);
                RectF doneR(Wd - S(48), S(94), S(30), S(30));
                g_doneHit = doneR;
                g_doneHitValid = true;
                DrawDoneButton(g, doneR, nextColor);
            } else {
                Txt(g, L"今天没有更多日程", S(12), false, th.dim2, RectF(S(16), S(96), Wd - S(32), S(20)));
            }
            Line(g, S(16), S(142), Wd - S(16), S(142), WithAlpha(th.borderSoft, 160), S(1));
            float sx = S(16);
            if (showWater) {
                Icon(g, ICO_DROP, RectF(sx, S(151), S(13), S(13)), th.accent2, 1.3f);
                Txt(g, Fmt(L"水 %d/%d", cups, target), S(9.5f), false, th.accent2,
                    RectF(sx + S(17), S(148), S(70), S(18)), ALIGN_VCENTER);
                g_waterHit = RectF(sx - S(4), S(144), S(96), S(24));
                g_waterHitValid = true;
                sx += S(92);
            }
            if (showToday) {
                Txt(g, Fmt(L"今日剩 %d 项", todayRemain), S(9.5f), false, th.dim2,
                    RectF(sx, S(148), S(120), S(18)), ALIGN_VCENTER);
            }
            Txt(g, L"点击打开", S(8.5f), false, WithAlpha(th.dim2, 170),
                RectF(Wd - S(110), S(148), S(94), S(18)), ALIGN_RIGHT | ALIGN_VCENTER);
        }
    }
    HDC wdc = GetDC(hwnd);
    BitBlt(wdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    ReleaseDC(hwnd, wdc);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, screen);
}

static void SaveWidgetPos() {
    RECT wr; GetWindowRect(g_widget, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    HMONITOR mon = MonitorFromWindow(g_widget, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(mon, &mi)) {
        RECT wa = mi.rcWork;
        wr.left = (LONG)std::max<long>(wa.left, std::min<long>(wr.left, wa.right - w));
        wr.top = (LONG)std::max<long>(wa.top, std::min<long>(wr.top, wa.bottom - h));
        SetWindowPos(g_widget, nullptr, wr.left, wr.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    app.store.settings.widget.x = wr.left;
    app.store.settings.widget.y = wr.top;
    app.store.Touch();
    DoSaveStore();
}

static void ToggleNextDone(HWND hwnd) {
    DateTime now = NowDT();
    Occurrence next;
    bool ongoing = false;
    int remain = 0;
    FindNext(now, next, ongoing, remain);
    if (next.ev.title.empty()) return;
    app.store.Snapshot();
    bool ok = app.store.SetOccurrenceDone(next.seriesId, next.origStart, next.start, next.end, true);
    if (!ok) return;
    DoSaveStore();
    InvalidateOccs();
    UIInvalidate();
    ShowToast(Fmt(L"已完成：%s（Ctrl+Z 可撤销）", next.ev.title.c_str()), 3000);
    DebugLog(L"widget done toggle series=%s", Utf8ToWide(next.seriesId).c_str());
    RenderWidget(hwnd);
}

static void ApplyAndRecreate() {
    app.store.Touch();
    DoSaveStore();
    RecreateWidgetWindow();
    UIInvalidate();
}

bool g_widgetMenuOpen = false;

static void WidgetMenu() {
    POINT pt; GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    enum {
        M_OPEN = 1, M_LAYOUT0 = 10, M_SKIN0 = 20, M_SCALE_DEC = 30, M_SCALE_100, M_SCALE_INC,
        M_OP_DEC = 40, M_OP_INC, M_UNDO = 50, M_APPEAR, M_HIDE, M_CLICK = 60, M_LOW
    };
    AppendMenuW(menu, MF_STRING, M_OPEN, L"打开 CAELENDAR");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    // 布局
    const wchar_t* layoutKeys[5] = { L"card", L"bar", L"list", L"clock", L"week" };
    AppendMenuW(menu, MF_STRING, M_LAYOUT0 + 0, L"布局：卡片");
    AppendMenuW(menu, MF_STRING, M_LAYOUT0 + 1, L"布局：细条");
    AppendMenuW(menu, MF_STRING, M_LAYOUT0 + 2, L"布局：今日清单");
    AppendMenuW(menu, MF_STRING, M_LAYOUT0 + 3, L"布局：极简时钟");
    AppendMenuW(menu, MF_STRING, M_LAYOUT0 + 4, L"布局：迷你周视图");
    int curLayout = 0;
    for (int i = 0; i < 5; ++i) if (app.store.settings.widget.mode == layoutKeys[i]) curLayout = i;
    CheckMenuRadioItem(menu, M_LAYOUT0, M_LAYOUT0 + 4, M_LAYOUT0 + curLayout, MF_BYCOMMAND);
    // 皮肤
    const wchar_t* skins[4] = { L"跟随主题", L"玻璃", L"纯黑", L"霓虹" };
    const wchar_t* skinKeys[4] = { L"theme", L"glass", L"solid", L"neon" };
    AppendMenuW(menu, MF_STRING, M_SKIN0 + 0, L"皮肤：跟随主题");
    AppendMenuW(menu, MF_STRING, M_SKIN0 + 1, L"皮肤：玻璃");
    AppendMenuW(menu, MF_STRING, M_SKIN0 + 2, L"皮肤：纯黑");
    AppendMenuW(menu, MF_STRING, M_SKIN0 + 3, L"皮肤：霓虹");
    int curSkin = 0;
    for (int i = 0; i < 4; ++i) if (app.store.settings.widget.skin == skinKeys[i]) curSkin = i;
    CheckMenuRadioItem(menu, M_SKIN0, M_SKIN0 + 3, M_SKIN0 + curSkin, MF_BYCOMMAND);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, M_SCALE_DEC, L"缩小 10%");
    AppendMenuW(menu, MF_STRING, M_SCALE_100, Fmt(L"大小恢复 100%%（当前 %d%%）", app.store.settings.widget.scale).c_str());
    AppendMenuW(menu, MF_STRING, M_SCALE_INC, L"放大 10%");
    AppendMenuW(menu, MF_STRING, M_OP_DEC, L"更透明 10%");
    AppendMenuW(menu, MF_STRING, M_OP_INC, L"更不透明 10%");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, M_UNDO, L"撤销上一步");
    AppendMenuW(menu, MF_STRING | (app.store.settings.widget.clickThrough ? MF_CHECKED : 0), M_CLICK,
                L"鼠标穿透（游戏时不挡鼠标）");
    AppendMenuW(menu, MF_STRING | (app.store.settings.widget.lowPower ? MF_CHECKED : 0), M_LOW,
                L"省电模式（5 秒刷新）");
    AppendMenuW(menu, MF_STRING, M_APPEAR, L"挂件外观…");
    AppendMenuW(menu, MF_STRING, M_HIDE, L"隐藏挂件");
    // WS_EX_NOACTIVATE 挂件无法成为前台，TrackPopupMenu 会弹出即消失：菜单期间临时允许激活
    HWND prevFg = GetForegroundWindow();
    LONG_PTR exStyle = GetWindowLongPtrW(g_widget, GWL_EXSTYLE);
    g_widgetMenuOpen = true;
    SetWindowLongPtrW(g_widget, GWL_EXSTYLE, exStyle & ~WS_EX_NOACTIVATE);
    SetWindowPos(g_widget, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    BOOL fgOk = SetForegroundWindow(g_widget);
    DWORD t0 = GetTickCount();
    int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                                  pt.x, pt.y, 0, g_widget, nullptr);
    g_widgetMenuOpen = false;
    PostMessageW(g_widget, WM_NULL, 0, 0);
    SetWindowLongPtrW(g_widget, GWL_EXSTYLE, exStyle);
    SetWindowPos(g_widget, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (prevFg && prevFg != g_widget && cmd != M_OPEN) SetForegroundWindow(prevFg);
    DestroyMenu(menu);
    auto& w = app.store.settings.widget;
    if (cmd >= M_LAYOUT0 && cmd <= M_LAYOUT0 + 4) {
        w.mode = layoutKeys[cmd - M_LAYOUT0];
        ApplyAndRecreate();
    } else if (cmd >= M_SKIN0 && cmd <= M_SKIN0 + 3) {
        w.skin = skinKeys[cmd - M_SKIN0];
        ApplyAndRecreate();
    } else switch (cmd) {
    case M_OPEN:
        if (g_hwnd) { ShowWindow(g_hwnd, SW_SHOW); ShowWindow(g_hwnd, SW_RESTORE); SetForegroundWindow(g_hwnd); }
        break;
    case M_SCALE_DEC:
        w.scale = std::max(80, w.scale - 10);
        ApplyAndRecreate();
        break;
    case M_SCALE_100:
        w.scale = 100;
        ApplyAndRecreate();
        break;
    case M_SCALE_INC:
        w.scale = std::min(160, w.scale + 10);
        ApplyAndRecreate();
        break;
    case M_OP_DEC:
        w.opacity = std::max(30, w.opacity - 10);
        ApplyAndRecreate();
        break;
    case M_OP_INC:
        w.opacity = std::min(100, w.opacity + 10);
        ApplyAndRecreate();
        break;
    case M_UNDO: UndoAction(); RenderWidget(g_widget); break;
    case M_CLICK:
        w.clickThrough = !w.clickThrough;
        ApplyAndRecreate();
        ShowToast(w.clickThrough ? L"挂件已开启鼠标穿透（可从托盘菜单关闭）" : L"挂件已关闭鼠标穿透");
        break;
    case M_LOW:
        w.lowPower = !w.lowPower;
        ApplyAndRecreate();
        break;
    case M_APPEAR:
        if (g_hwnd) { ShowWindow(g_hwnd, SW_SHOW); ShowWindow(g_hwnd, SW_RESTORE); SetForegroundWindow(g_hwnd); }
        OpenWidgetSettings();
        break;
    case M_HIDE:
        w.enabled = false;
        app.store.Touch();
        DoSaveStore();
        DestroyWidgetWindow();
        break;
    default: break;
    }
}

static LRESULT CALLBACK WidgetProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        RenderWidget(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (wParam == 1) {
            if (!IsWindowVisible(hwnd)) return 0;
            static int lastMin = -1;
            static int lastRev = -1;
            DateTime n = NowDT();
            int m = n.h * 60 + n.mi;
            if (m != lastMin || lastRev != app.store.revision) {
                lastMin = m;
                lastRev = app.store.revision;
                RenderWidget(hwnd);
            }
        }
        return 0;
    case WM_DISPLAYCHANGE:
        ClampWidgetToMonitor();
        return 0;
    case WM_MOUSEMOVE: {
        if (g_dragging) {
            POINT pt; GetCursorPos(&pt);
            int dx = pt.x - g_dragStart.x, dy = pt.y - g_dragStart.y;
            if (abs(dx) + abs(dy) > 3) g_moved = true;
            if (g_moved) SetWindowPos(hwnd, TopmostFlag(), g_winStart.x + dx, g_winStart.y + dy, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            return 0;
        }
        float k = WidgetK();
        float px = GET_X_LPARAM(lParam) / k, py = GET_Y_LPARAM(lParam) / k;
        bool hd = g_doneHitValid && px >= g_doneHit.X && px <= g_doneHit.X + g_doneHit.Width &&
                  py >= g_doneHit.Y && py <= g_doneHit.Y + g_doneHit.Height;
        bool hw = g_waterHitValid && px >= g_waterHit.X && px <= g_waterHit.X + g_waterHit.Width &&
                  py >= g_waterHit.Y && py <= g_waterHit.Y + g_waterHit.Height;
        if (hd != g_hoverDone) { g_hoverDone = hd; RenderWidget(hwnd); }
        SetCursor(LoadCursorW(nullptr, (hd || hw) ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_dragging = true;
        g_moved = false;
        GetCursorPos(&g_dragStart);
        RECT wr; GetWindowRect(hwnd, &wr);
        g_winStart = POINT{ wr.left, wr.top };
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_dragging) {
            ReleaseCapture();
            g_dragging = false;
            if (g_moved) { SaveWidgetPos(); return 0; }
            float k = WidgetK();
            float px = GET_X_LPARAM(lParam) / k, py = GET_Y_LPARAM(lParam) / k;
            if (g_doneHitValid && px >= g_doneHit.X && px <= g_doneHit.X + g_doneHit.Width &&
                py >= g_doneHit.Y && py <= g_doneHit.Y + g_doneHit.Height) {
                ToggleNextDone(hwnd);
                return 0;
            }
            if (g_waterHitValid && px >= g_waterHit.X && px <= g_waterHit.X + g_waterHit.Width &&
                py >= g_waterHit.Y && py <= g_waterHit.Y + g_waterHit.Height) {
                app.store.Snapshot();
                app.store.AddWaterCup();
                DoSaveStore();
                ShowToast(L"已记一杯水");
                RenderWidget(hwnd);
                UIInvalidate();
                return 0;
            }
            if (g_hwnd) {
                ShowWindow(g_hwnd, SW_SHOW);
                ShowWindow(g_hwnd, SW_RESTORE);
                SetForegroundWindow(g_hwnd);
            }
        }
        return 0;
    }
    case WM_RBUTTONUP:
        WidgetMenu();
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HWND TopmostFlag() {
    return app.store.settings.widget.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
}

static void ApplyWidgetStyle() {
    LONG ex = GetWindowLongW(g_widget, GWL_EXSTYLE);
    if (app.store.settings.widget.alwaysOnTop) ex |= WS_EX_TOPMOST;
    else ex &= ~WS_EX_TOPMOST;
    if (app.store.settings.widget.clickThrough) ex |= WS_EX_TRANSPARENT;
    else ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongW(g_widget, GWL_EXSTYLE, ex);
    int alpha = std::max(30, std::min(100, app.store.settings.widget.opacity));
    SetLayeredWindowAttributes(g_widget, 0, (BYTE)(alpha * 255 / 100), LWA_ALPHA);
    SetWindowPos(g_widget, TopmostFlag(), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// 把挂件位置限制在当前(或最近)显示器的工作区内；完全在屏幕外则回到主屏右下角
static void ClampWidgetToMonitor() {
    if (!g_widget) return;
    RECT wr;
    GetWindowRect(g_widget, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    POINT center{ wr.left + w / 2, wr.top + h / 2 };
    HMONITOR mon = MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(mon, &mi)) return;
    RECT wa = mi.rcWork;
    int x = wr.left, y = wr.top;
    if (x + w < wa.left + 20 || x > wa.right - 20 || y + h < wa.top + 20 || y > wa.bottom - 20) {
        // 完全或大部分在屏幕外：回到主屏右下角
        MONITORINFO primary{ sizeof(primary) };
        if (GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &primary)) wa = primary.rcWork;
        x = wa.right - w - (int)S(28);
        y = wa.bottom - h - (int)S(60);
    } else {
        x = std::max<long>(wa.left, std::min<long>(x, wa.right - w));
        y = std::max<long>(wa.top, std::min<long>(y, wa.bottom - h));
    }
    if (x != wr.left || y != wr.top) {
        SetWindowPos(g_widget, TopmostFlag(), x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        app.store.settings.widget.x = x;
        app.store.settings.widget.y = y;
        app.store.Touch();
        DoSaveStore();
    }
}

void UpdateWidgetZOrder();

void CreateWidgetWindow() {
    if (g_widget && !IsWindow(g_widget)) g_widget = nullptr;
    if (g_widget) { ShowWindow(g_widget, SW_SHOWNOACTIVATE); UpdateWidgetFullscreenVisibility(); return; }
    if (!app.store.settings.widget.enabled) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WidgetProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"XingLiWidget";
    RegisterClassExW(&wc);
    int w, h;
    WidgetSize(w, h);
    int x = app.store.settings.widget.x;
    int y = app.store.settings.widget.y;
    if (x < 0 || y < 0) {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        x = work.right - w - (int)S(28);
        y = work.bottom - h - (int)S(60);
    }
    g_widget = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"XingLiWidget", L"CAELENDAR 挂件",
                               WS_POPUP, x, y, w, h, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_widget) return;
    int rad = (int)(S(28) * WidgetK());
    SetWindowRgn(g_widget, CreateRoundRectRgn(0, 0, w + 1, h + 1, rad, rad), TRUE);
    ApplyWidgetStyle();
    SetTimer(g_widget, 1, app.store.settings.widget.lowPower ? 5000 : 1000, nullptr);
    ShowWindow(g_widget, SW_SHOWNOACTIVATE);
    ClampWidgetToMonitor();
    UpdateWidgetFullscreenVisibility();
    UpdateWidgetZOrder();
    RenderWidget(g_widget);
    RECT wr{};
    GetWindowRect(g_widget, &wr);
    LONG ex = GetWindowLongW(g_widget, GWL_EXSTYLE);
    DebugLog(L"widget-created: mode=%s clickThrough=%d transparent=%d lowPower=%d pos=%d,%d",
             app.store.settings.widget.mode.c_str(), app.store.settings.widget.clickThrough ? 1 : 0,
             (ex & WS_EX_TRANSPARENT) ? 1 : 0, app.store.settings.widget.lowPower ? 1 : 0, wr.left, wr.top);
}

static bool g_autoHiddenFullscreen = false;
static bool g_widgetLowered = false;

void UpdateWidgetZOrder() {
    if (!g_widget) return;
    bool mainOpen = g_hwnd && IsWindowVisible(g_hwnd) && !IsIconic(g_hwnd);
    bool wantTop = app.store.settings.widget.alwaysOnTop && !mainOpen;
    if (!wantTop && !g_widgetLowered) {
        SetWindowPos(g_widget, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        g_widgetLowered = true;
        DebugLog(L"widget: lowered while main window open");
    } else if (wantTop && g_widgetLowered) {
        SetWindowPos(g_widget, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        g_widgetLowered = false;
        DebugLog(L"widget: restored topmost (main hidden)");
    }
}

static bool IsOwnProcessWindow(HWND h) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

// 前台是“游戏 / 全屏应用”时返回 true：窗口矩形基本盖满所在显示器
static bool IsFullscreenForegroundApp() {
    HWND fg = GetForegroundWindow();
    if (!fg || IsOwnProcessWindow(fg)) return false;
    wchar_t cls[256] = { 0 };
    GetClassNameW(fg, cls, 256);
    if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") ||
        !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Windows.UI.Core.CoreWindow"))
        return false;
    if (IsZoomed(fg)) return false;   // 最大化窗口不算全屏
    RECT wr{};
    if (!GetWindowRect(fg, &wr)) return false;
    if (wr.right - wr.left < 200 || wr.bottom - wr.top < 200) return false;
    HMONITOR mon = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return false;
    const RECT& mr = mi.rcMonitor;
    int tol = 2;
    return wr.left <= mr.left + tol && wr.top <= mr.top + tol &&
           wr.right >= mr.right - tol && wr.bottom >= mr.bottom - tol;
}

void UpdateWidgetFullscreenVisibility() {
    if (!g_widget) return;
    bool fs = IsFullscreenForegroundApp();
    if (fs) {
        if (IsWindowVisible(g_widget)) {
            ShowWindow(g_widget, SW_HIDE);
            g_autoHiddenFullscreen = true;
            DebugLog(L"widget: auto hidden (fullscreen foreground)");
        }
    } else if (g_autoHiddenFullscreen) {
        g_autoHiddenFullscreen = false;
        if (app.store.settings.widget.enabled) {
            ShowWindow(g_widget, SW_SHOWNOACTIVATE);
            UpdateWidgetZOrder();
            RenderWidget(g_widget);
            DebugLog(L"widget: restored (fullscreen ended)");
        }
    }
}

void WidgetHealthTick() {
    if (!app.store.settings.widget.enabled) return;
    if (g_widget && !IsWindow(g_widget)) g_widget = nullptr;
    if (!g_widget) { CreateWidgetWindow(); return; }
    UpdateWidgetFullscreenVisibility();
    if (!IsWindowVisible(g_widget) && !g_autoHiddenFullscreen && !IsFullscreenForegroundApp()) {
        ShowWindow(g_widget, SW_SHOWNOACTIVATE);
        UpdateWidgetZOrder();
        RenderWidget(g_widget);
        DebugLog(L"widget: re-shown by health tick");
    }
    ClampWidgetToMonitor();
}

void RefreshWidgetNow() {
    if (g_widget && IsWindowVisible(g_widget)) {
        DebugLog(L"widget refresh rev=%d", app.store.revision);
        RenderWidget(g_widget);
    }
}

void DestroyWidgetWindow() {
    if (g_widget) {
        KillTimer(g_widget, 1);
        DestroyWindow(g_widget);
        g_widget = nullptr;
    }
}

void RecreateWidgetWindow() {
    bool enable = app.store.settings.widget.enabled;
    DestroyWidgetWindow();
    if (enable) CreateWidgetWindow();
}

bool WidgetEnabled() { return app.store.settings.widget.enabled; }

void ToggleWidgetWindow() {
    if (g_widget) {
        app.store.settings.widget.enabled = false;
        app.store.Touch();
        DoSaveStore();
        DestroyWidgetWindow();
        ShowToast(L"挂件已隐藏");
    } else {
        app.store.settings.widget.enabled = true;
        app.store.Touch();
        DoSaveStore();
        CreateWidgetWindow();
        ShowToast(L"挂件已显示，可拖动到合适位置");
    }
}
