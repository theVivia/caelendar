// views.cpp - 月 / 周 / 日 / 日程 四个视图
#include "app.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <map>

using namespace Gdiplus;
using namespace gfx;

std::wstring DisplayEventTitle(const Occurrence& o) {
    const std::wstring& title = o.ev.title;
    const Event* base = app.store.Find(o.seriesId);
    if (!base || base->recur.freq != RF_YEARLY) return title;
    bool birthday = Contains(title, L"生日");
    bool anniv = Contains(title, L"纪念") || Contains(title, L"周年");
    if (!birthday && !anniv) return title;
    int y0 = 0, y1 = 0;
    if (base->recur.lunar) { y0 = LunarYearOf(base->start); y1 = LunarYearOf(o.start); }
    if (y0 == 0 || y1 == 0) { y0 = base->start.y; y1 = o.start.y; }
    int n = y1 - y0;
    if (n <= 0) return title;
    if (birthday) return title + Fmt(L" · %d岁", n);
    return title + Fmt(L" · %d周年", n);
}

// 供双击空白定位时间用
static DateTime g_wkDayStart;
static float g_wkGridTop = 0, g_wkPxPerMin = 1, g_wkGridX = 0, g_wkColW = 0;
static bool g_wkValid = false;
static int g_wkScrollMin = 0;
static float g_wkAreaH = 0;
static int g_fitZoom = 0;
static int g_wkTotalMin = 1440;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

static void HudCornerMonth(Graphics& g, RectF r, Color c, float len) {
    float L = S(len), w = S(1.4f);
    Line(g, r.X + r.Width - L, r.Y, r.X + r.Width, r.Y, c, w);
    Line(g, r.X + r.Width, r.Y, r.X + r.Width, r.Y + L, c, w);
    Line(g, r.X, r.Y + r.Height - L, r.X, r.Y + r.Height, c, w);
    Line(g, r.X, r.Y + r.Height, r.X + L, r.Y + r.Height, c, w);
}

std::wstring RelativeDayLabel(const DateTime& d) {
    int diff = DaySerial(d.y, d.mo, d.d) - DaySerial(app.now.y, app.now.mo, app.now.d);
    if (diff == 0) return L"今天";
    if (diff == 1) return L"明天";
    if (diff == 2) return L"后天";
    if (diff == -1) return L"昨天";
    if (diff > 2 && diff < 7) return WeekdayCN(DayOfWeek(d.y, d.mo, d.d));
    return Fmt(L"%d/%d", d.mo, d.d);
}
std::wstring TodayLabel() { return ISODate(Today()); }

std::wstring FormatTimeRange(const Occurrence& o) {
    if (o.ev.allDay) {
        int n = DaySerial(o.end.y, o.end.mo, o.end.d) - DaySerial(o.start.y, o.start.mo, o.start.d);
        if (n <= 1) return L"全天";
        return Fmt(L"全天(+%d)", n - 1);
    }
    if (CompareDT(o.start, o.end) == 0) return TimeStr(o.start);   // 时间点（DDL）
    int sd = DaySerial(o.start.y, o.start.mo, o.start.d);
    int ed = DaySerial(o.end.y, o.end.mo, o.end.d);
    bool endMidnight = (o.end.h == 0 && o.end.mi == 0);
    int n = ed - sd;
    if (endMidnight && n > 0) --n;
    std::wstring s = TimeStr(o.start) + L"–";
    if (endMidnight && ed > sd) s += L"24:00";
    else s += TimeStr(o.end);
    if (n <= 0) return s;
    if (n <= 2) return s + Fmt(L"(+%d)", n);
    return Fmt(L"%d/%d ", o.start.mo, o.start.d) + TimeStr(o.start) + L" – " +
           Fmt(L"%d/%d ", o.end.mo, o.end.d) + TimeStr(o.end);
}

static std::wstring NavTitle() {
    switch (app.view) {
    case VIEW_MONTH: return Fmt(L"%d年%d月", app.cursor.y, app.cursor.mo);
    case VIEW_WEEK: {
        DateTime ws = StartOfWeek(app.cursor, app.store.settings.weekStart);
        DateTime we = AddDaysDT(ws, 6);
        if (ws.y != we.y) return Fmt(L"%d年%d月%d日 – %d年%d月%d日", ws.y, ws.mo, ws.d, we.y, we.mo, we.d);
        if (ws.mo != we.mo) return Fmt(L"%d月%d日 – %d月%d日", ws.mo, ws.d, we.mo, we.d);
        return Fmt(L"%d月%d日 – %d日", ws.mo, ws.d, we.d);
    }
    case VIEW_DAY: {
        return Fmt(L"%d年%d月%d日 %s", app.cursor.y, app.cursor.mo, app.cursor.d,
                   WeekdayCN(DayOfWeek(app.cursor.y, app.cursor.mo, app.cursor.d)));
    }
    case VIEW_YEAR: return Fmt(L"%d 年", app.cursor.y);
    default: return app.agendaBoard ? L"作业看板" : L"全部日程";
    }
}

static std::wstring NavSubtitle() {
    if (app.view == VIEW_AGENDA && app.agendaBoard) {
        return L"DDL / 作业看板 · 逾期优先 · 勾选圆点完成 · Shift+点击卡片多选 · 点卡片编辑";
    }
    if (app.view == VIEW_YEAR) {
        return Fmt(L"%d 年 · 全年概览", app.cursor.y);
    }
    DateTime ref = app.cursor;
    std::wstring s = Fmt(L"第 %d 周", WeekOfYear(ref));
    int sw = SemesterWeek(app.store.settings, ref);
    if (sw > 0) {
        s += Fmt(L" · 学期第 %d 周", sw);
        if (app.store.settings.semester.showParity) s += (sw % 2 == 1) ? L"（单）" : L"（双）";
    }
    if (!app.tagFilter.empty()) s += Fmt(L" · 已筛选 %d 个标签", (int)app.tagFilter.size());
    if (!app.search.empty()) s += L" · 搜索中";
    return s;
}

static RectF g_zoomOutRect, g_zoomInRect, g_zoomFitRect;

bool WeekViewInZoomControl(float px, float py) {
    auto in = [&](const RectF& r) {
        return r.Width > 0 && px >= r.X && px <= r.X + r.Width && py >= r.Y && py <= r.Y + r.Height;
    };
    return in(g_zoomOutRect) || in(g_zoomInRect) || in(g_zoomFitRect);
}

void SetTimeZoom(int delta) {
    int minZ = g_fitZoom > 0 ? g_fitZoom : 25;
    int z = std::max(minZ, std::min(160, app.store.settings.timeZoom + delta));
    if (z == app.store.settings.timeZoom) return;
    app.store.settings.timeZoom = z;
    app.store.Touch();
    DoSaveStore();
    UIInvalidate();
}

void FitTimeZoom() {
    int z = g_fitZoom;
    if (z <= 0) {
        if (g_wkAreaH <= S(40)) return;
        z = (int)std::ceil(g_wkAreaH / (24.0f * S(52)) * 100.0f);
        z = std::max(25, std::min(160, z));
    }
    app.store.settings.timeZoom = z;
    app.scrollMin = 0;
    app.store.Touch();
    DoSaveStore();
    UIInvalidate();
}

void ViewHeader(Graphics& g, float x, float y, float w) {
    const Theme& th = TH();
    Txt(g, NavTitle(), S(22), true, IsHovered(CMD_DATEJUMP) ? th.accent : th.text,
        RectF(x, y, w - S(220), S(34)), ALIGN_LEFT, true);
    AddHit(x, y, w - S(240), S(40), CMD_DATEJUMP, []() { OpenDateJump(); });
    std::wstring sub = NavSubtitle();
    SizeF subSz;
    MeasureText(g, sub, S(10.5f), false, subSz);
    float maxSubW = std::max(S(120), w - S(420));
    float subW = std::min(subSz.Width, maxSubW);
    Txt(g, sub, S(10.5f), false, th.dim2, RectF(x, y + S(34), subW + S(2), S(18)), ALIGN_LEFT, false, true);
    float pfX = x + subW + S(18);
    float pfW = std::max(S(90), std::min(S(260), x + w - pfX - S(170)));
    Txt(g, L"· 档案 " + ActiveProfileName(), S(9), false, WithAlpha(th.accent2, 235),
        RectF(pfX, y + S(34), pfW, S(18)), ALIGN_LEFT, true);
    AddHit(pfX, y + S(30), pfW, S(24), 0, []() { OpenProfilesPanel(); });
    float rx = x + w;
    rx -= S(62);
    UI::Btn(g, RectF(rx, y + S(3), S(62), S(28)), CMD_TODAY, L"今天", 1, -1, []() {
        app.cursor = Today();
        InvalidateOccs();
        UIInvalidate();
    });
    rx -= S(34);
    UI::IconBtn(g, RectF(rx, y + S(3), S(28), S(28)), CMD_NEXT, ICO_CHEV_RIGHT, 13, []() {
        switch (app.view) {
        case VIEW_MONTH: app.cursor = AddMonthsDT(app.cursor, 1); break;
        case VIEW_WEEK: app.cursor = AddDaysDT(app.cursor, 7); break;
        case VIEW_YEAR: app.cursor = MakeDT(app.cursor.y + 1, app.cursor.mo, 1); break;
        case VIEW_DAY: app.cursor = AddDaysDT(app.cursor, 1); break;
        default: app.cursor = AddDaysDT(app.cursor, 7); break;
        }
        InvalidateOccs();
        UIInvalidate();
    });
    rx -= S(32);
    UI::IconBtn(g, RectF(rx, y + S(3), S(28), S(28)), CMD_PREV, ICO_CHEV_LEFT, 13, []() {
        switch (app.view) {
        case VIEW_MONTH: app.cursor = AddMonthsDT(app.cursor, -1); break;
        case VIEW_WEEK: app.cursor = AddDaysDT(app.cursor, -7); break;
        case VIEW_YEAR: app.cursor = MakeDT(app.cursor.y - 1, app.cursor.mo, 1); break;
        case VIEW_DAY: app.cursor = AddDaysDT(app.cursor, -1); break;
        default: app.cursor = AddDaysDT(app.cursor, -7); break;
        }
        InvalidateOccs();
        UIInvalidate();
    });
    if (app.view == VIEW_WEEK || app.view == VIEW_DAY) {
        float zy = y + S(32);
        float zr = x + w;
        g_zoomFitRect = RectF(zr - S(50), zy, S(46), S(22));
        g_zoomInRect = RectF(zr - S(124), zy, S(22), S(22));
        g_zoomOutRect = RectF(zr - S(148), zy, S(22), S(22));
        UI::Btn(g, g_zoomFitRect, CMD_ZOOM_FIT, L"适配", 1, -1, []() { FitTimeZoom(); });
        Txt(g, Fmt(L"%d%%", (int)app.store.settings.timeZoom), S(8.5f), false, th.dim2,
            RectF(zr - S(98), zy, S(40), S(22)), ALIGN_CENTER | ALIGN_VCENTER);
        UI::Btn(g, g_zoomInRect, CMD_ZOOM_IN, L"+", 2, -1, []() { SetTimeZoom(10); });
        UI::Btn(g, g_zoomOutRect, CMD_ZOOM_OUT, L"−", 2, -1, []() { SetTimeZoom(-10); });
    }
}

static void DrawEventChip(Graphics& g, RectF r, const Occurrence& o, bool compact) {
    gfx::FontPartScope fontPart(gfx::FP_CHIP);
    const Theme& th = TH();
    Color tc = Color(255, 148, 163, 184);
    if (!o.ev.tags.empty()) tc = TagColorOf(o.ev.tags[0]);
    if (!o.ev.color.empty()) tc = Hex(o.ev.color);
    bool cancelled = o.ev.status == ST_CANCELLED;
    bool done = o.ev.done;
    bool selected = app.selected.count(OccKey(o)) > 0;
    Color bg = o.ev.allDay ? WithAlpha(tc, 60) : WithAlpha(tc, 32);
    if (cancelled) bg = WithAlpha(th.dim2, 26);
    if (selected) bg = WithAlpha(th.accent, 85);
    FillRoundRect(g, r, S(5), bg);
    if (selected) StrokeRoundRect(g, r, S(5), th.accent, S(2.2f));
    FillRoundRect(g, RectF(r.X, r.Y + S(3), S(2.4f), std::max(S(6), r.Height - S(6))), S(1), cancelled ? th.dim2 : tc);
    float tx = r.X + S(7);
    Color textColor = done ? WithAlpha(th.dim, 235) : (cancelled ? WithAlpha(th.dim, 180) : th.text);
    if (!o.ev.allDay) {
        Txt(g, TimeStr(o.start), S(9), true, done ? th.dim2 : tc, RectF(tx, r.Y, S(33), r.Height), ALIGN_VCENTER);
        tx += S(33);
    }
    float rightPad = S(4);
    if (selected) rightPad = S(17);
    if (o.ev.priority == PRIO_HIGH && !selected) rightPad = S(14);
    Txt(g, (done ? L"✓ " : L"") + DisplayEventTitle(o), S(compact ? 10 : 10.5f), o.ev.priority == PRIO_HIGH && !done,
        textColor, RectF(tx, r.Y, RX(r) - tx - rightPad, r.Height), ALIGN_VCENTER);
    if (o.ev.priority == PRIO_HIGH && !selected) {
        SolidBrush b(th.warn);
        g.FillEllipse(&b, RX(r) - S(11), r.Y + r.Height / 2 - S(2.5f), S(5), S(5));
    }
    if (o.ev.IsRecurring() && !compact && !selected) {
        Pen p(WithAlpha(textColor, 140), S(1));
        g.DrawArc(&p, RX(r) - S(13), r.Y + S(3), S(7), S(7), 40, 250);
    }
    if (selected) {
        float bs = std::min(S(13), r.Height - S(3));
        RectF badge(RX(r) - bs - S(3), r.Y + (r.Height - bs) / 2, bs, bs);
        FillRoundRect(g, badge, S(3), th.accent);
        Icon(g, ICO_CHECK, RectF(badge.X + S(2), badge.Y + S(2), bs - S(4), bs - S(4)), th.onAccent, 2.2f);
    }
}

// ---------------- 月视图 ----------------
static RectF g_monthGrid;
static float g_monthCW = 0, g_monthCH = 0;
static int g_monthStartSerial = 0;
static bool g_monthValid = false;

bool MonthDateAtPoint(float px, float py, DateTime& out) {
    if (!g_monthValid || g_monthCW <= 0 || g_monthCH <= 0) return false;
    if (px < g_monthGrid.X || px >= RX(g_monthGrid) || py < g_monthGrid.Y || py >= BY(g_monthGrid)) return false;
    int col = (int)std::floor((px - g_monthGrid.X) / g_monthCW);
    int row = (int)std::floor((py - g_monthGrid.Y) / g_monthCH);
    if (col < 0 || col > 6 || row < 0 || row > 5) return false;
    Date d = FromSerial(g_monthStartSerial + row * 7 + col);
    out = MakeDT(d.y, d.m, d.d);
    return true;
}

void DrawMonthView(Graphics& g, float x, float y, float w, float h) {
    gfx::FontPartScope fontPart(gfx::FP_MONTH);
    const Theme& th = TH();
    ViewHeader(g, x, y, w);
    float gridTop = y + S(62);
    float headH = S(26);
    const Settings& st = app.store.settings;
    int ws = st.weekStart;
    DateTime first = MakeDT(app.cursor.y, app.cursor.mo, 1);
    DateTime gridStart = StartOfWeek(first, ws);
    // 星期表头
    for (int i = 0; i < 7; ++i) {
        int wd = (ws + i) % 7;
        float cw = w / 7.0f;
        float cx = x + i * cw;
        bool weekend = (wd == 0 || wd == 6);
        Color c = weekend ? WithAlpha(th.accent2, 210) : th.dim;
        Txt(g, WeekdayCN(wd), S(11), false, c, RectF(cx, gridTop, cw, headH), ALIGN_CENTER | ALIGN_VCENTER);
    }
    RectF grid(x, gridTop + headH, w, h - (gridTop + headH - y) - S(22));
    g_monthGrid = grid;
    g_monthCW = grid.Width / 7.0f;
    g_monthCH = grid.Height / 6.0f;
    g_monthStartSerial = DaySerial(gridStart.y, gridStart.mo, gridStart.d);
    g_monthValid = true;
    FillRoundRect(g, grid, S(14), WithAlpha(th.panel2, 205));
    StrokeRoundRect(g, grid, S(14), th.border, S(1));
    float cw = grid.Width / 7.0f, ch = grid.Height / 6.0f;
    int startSerial = DaySerial(gridStart.y, gridStart.mo, gridStart.d);
    g.SetClip(grid);

    // 分桶：单日事件 / 跨日连续条
    std::map<int, std::vector<int>> byDay;
    struct CrossEv { int oi, s0, e0; };
    std::vector<CrossEv> cross;
    for (int i = 0; i < (int)app.occs.size(); ++i) {
        const Occurrence& o = app.occs[i];
        if (!OccMatchesFilter(o)) continue;
        int s0 = DaySerial(o.start.y, o.start.mo, o.start.d);
        int e0 = DaySerial(o.end.y, o.end.mo, o.end.d);
        if (!o.ev.allDay && o.end.h == 0 && o.end.mi == 0 && e0 > s0) --e0;
        if (o.ev.allDay) e0 = std::max(s0, e0 - 1);
        if (e0 > s0) cross.push_back(CrossEv{ i, s0, e0 });
        else byDay[s0].push_back(i);
    }
    // 跨日条：按周行拆分 + 车道
    struct CrossSeg { int oi, row, c0, c1; int days; };
    struct MonthBar { int oi, row, c0, c1, lane; int days; };
    std::vector<CrossSeg> segs;
    for (auto& c : cross) {
        int days = c.e0 - c.s0;
        int cur = c.s0;
        int guard = 0;
        while (cur <= c.e0 && guard++ < 60) {
            int row = (cur - startSerial) / 7;
            if (row >= 6) break;
            if (row < 0) { cur = startSerial; continue; }
            int rowStart = startSerial + row * 7;
            int rowLast = rowStart + 6;
            int segEnd = std::min(c.e0, rowLast);
            segs.push_back(CrossSeg{ c.oi, row, cur - rowStart, segEnd - rowStart, days });
            cur = segEnd + 1;
        }
    }
    std::sort(segs.begin(), segs.end(), [](const CrossSeg& a, const CrossSeg& b) {
        if (a.row != b.row) return a.row < b.row;
        if (a.c0 != b.c0) return a.c0 < b.c0;
        return a.c1 < b.c1;
    });
    std::vector<MonthBar> bars;
    int laneCount[6] = { 0, 0, 0, 0, 0, 0 };
    {
        std::vector<std::vector<int>> laneEnd(6);
        for (auto& s : segs) {
            int lane = -1;
            for (int k = 0; k < (int)laneEnd[s.row].size(); ++k) {
                if (laneEnd[s.row][k] < s.c0) { lane = k; break; }
            }
            if (lane < 0) { lane = (int)laneEnd[s.row].size(); laneEnd[s.row].push_back(-1); }
            laneEnd[s.row][lane] = s.c1;
            bars.push_back(MonthBar{ s.oi, s.row, s.c0, s.c1, lane, s.days });
            laneCount[s.row] = std::max(laneCount[s.row], lane + 1);
        }
    }

    // 第一遍：格子背景、日期、农历
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            int serial = startSerial + row * 7 + col;
            Date d = FromSerial(serial);
            DateTime day = MakeDT(d.y, d.m, d.d);
            RectF cell(grid.X + col * cw, grid.Y + row * ch, cw, ch);
            bool inMonth = (day.mo == app.cursor.mo && day.y == app.cursor.y);
            bool isToday = SameDay(day, app.now);
            bool isCursor = SameDay(day, app.cursor);
            if (!inMonth) FillRoundRect(g, RectF(cell.X + S(1), cell.Y + S(1), cell.Width - S(2), cell.Height - S(2)), S(10), WithAlpha(th.bg, 130));
            if (isToday) {
                FillRoundRect(g, RectF(cell.X + S(2), cell.Y + S(2), cell.Width - S(4), cell.Height - S(4)), S(10), WithAlpha(th.accent, 18));
                StrokeRoundRect(g, RectF(cell.X + S(2), cell.Y + S(2), cell.Width - S(4), cell.Height - S(4)), S(10), WithAlpha(th.accent, 150), S(1.2f));
                HudCornerMonth(g, RectF(cell.X + S(2), cell.Y + S(2), cell.Width - S(4), cell.Height - S(4)), WithAlpha(th.accent, 200), 8);
            } else if (isCursor) {
                FillRoundRect(g, RectF(cell.X + S(2), cell.Y + S(2), cell.Width - S(4), cell.Height - S(4)), S(10), WithAlpha(th.cardHover, 80));
            }
            AddHit(cell.X, cell.Y, cell.Width, cell.Height, CMD_DAY_BASE + serial, [day]() {
                app.cursor = day;
                app.lastClickDay = day;
                app.lastClickMin = 9 * 60;
                UIInvalidate();
            });
            Color numColor = isToday ? th.accent : (inMonth ? (col >= 5 ? WithAlpha(th.accent2, 235) : th.text) : th.dim2);
            Txt(g, Fmt(L"%d", day.d), S(14), isToday, numColor, RectF(cell.X + S(10), cell.Y + S(6), S(46), S(20)), ALIGN_LEFT, true);
            if (isToday) Txt(g, L"今天", S(8.5f), true, th.accent, RectF(cell.X + S(36), cell.Y + S(9), S(34), S(14)));
            else if (day.d == 1) Txt(g, Fmt(L"%d月", day.mo), S(9), false, th.dim2, RectF(cell.X + S(30), cell.Y + S(9), S(36), S(14)));
            if (st.lunar) {
                std::wstring lunar = LunarText(day);
                std::wstring fest = LunarFestival(day);
                std::wstring term = SolarTermNameOn(day);
                float specialW = std::min(cell.Width - S(56), S(132));
                float specialX = RX(cell) - specialW - S(8);
                if (!term.empty()) Txt(g, term, S(8.5f), true, WithAlpha(th.accent2, 230), RectF(specialX, cell.Y + S(7), specialW, S(14)), ALIGN_RIGHT, false, false);
                else if (!fest.empty()) Txt(g, fest, S(8.5f), false, WithAlpha(th.accent, 220), RectF(specialX, cell.Y + S(7), specialW, S(14)), ALIGN_RIGHT, false, false);
                else if (!lunar.empty()) Txt(g, lunar, S(8.5f), false, WithAlpha(th.dim2, 190), RectF(RX(cell) - S(48), cell.Y + S(7), S(40), S(14)), ALIGN_RIGHT, false, false);
            }
            if (app.store.settings.monthCounts) {
                auto itc = byDay.find(serial);
                int cnt = itc != byDay.end() ? (int)itc->second.size() : 0;
                if (cnt > 0) {
                    Txt(g, Fmt(L"%d", cnt), S(8.5f), true, WithAlpha(th.accent, 220),
                        RectF(RX(cell) - S(20), BY(cell) - S(18), S(16), S(12)), ALIGN_RIGHT);
                }
            }
        }
    }

    // 跨日连续条（月视图事件条开关）
    if (app.store.settings.monthChips) {
    float chipH = S(19), gap = S(2.5f);
    auto evColor = [&](const Occurrence& o) -> Color {
        if (!o.ev.color.empty()) return Hex(o.ev.color);
        if (!o.ev.tags.empty()) return TagColorOf(o.ev.tags[0]);
        return Color(255, 148, 163, 184);
    };
    for (auto& b : bars) {
        const Occurrence& o = app.occs[b.oi];
        float rowY = grid.Y + b.row * ch;
        float bx = grid.X + b.c0 * cw + S(3);
        float bw = (b.c1 - b.c0 + 1) * cw - S(6);
        float by = rowY + S(28) + b.lane * (chipH + gap);
        RectF bar(bx, by, bw, chipH);
        Color tc = evColor(o);
        Color bgc = WithAlpha(tc, o.ev.done ? 26 : 58);
        FillRoundRect(g, bar, S(5), bgc);
        if (app.selected.count(OccKey(o)) > 0) {
            FillRoundRect(g, bar, S(5), WithAlpha(th.accent, 80));
            StrokeRoundRect(g, bar, S(5), th.accent, S(2.2f));
        }
        FillRoundRect(g, RectF(bar.X, bar.Y + S(2), S(2.4f), bar.Height - S(4)), S(1), tc);
        Txt(g, (o.ev.done ? L"✓ " : L"") + DisplayEventTitle(o), S(9.5f), true,
            o.ev.done ? WithAlpha(th.dim, 220) : th.text,
            RectF(bar.X + S(7), bar.Y, bar.Width - S(56), bar.Height), ALIGN_LEFT | ALIGN_VCENTER, false, true);
        if (b.days > 0) {
            Txt(g, Fmt(L"(+%d)", b.days), S(8), false, WithAlpha(tc, 235),
                RectF(RX(bar) - S(46), bar.Y, S(40), bar.Height), ALIGN_RIGHT | ALIGN_VCENTER);
        }
        AddHit(bar.X, bar.Y, bar.Width, bar.Height, CMD_EVENT_BASE + b.oi, [oi = b.oi]() {
            if (oi < (int)app.occs.size()) OpenEditorForOccurrence(app.occs[oi]);
        });
    }

    // 第二遍：单日事件 chip（从跨日条占用的车道下方开始）
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            int serial = startSerial + row * 7 + col;
            auto it = byDay.find(serial);
            if (it == byDay.end()) continue;
            auto& idxs = it->second;
            Date d = FromSerial(serial);
            DateTime day = MakeDT(d.y, d.m, d.d);
            RectF cell(grid.X + col * cw, grid.Y + row * ch, cw, ch);
            float cy = cell.Y + S(28) + laneCount[row] * (chipH + gap);
            float bottom = BY(cell) - S(5);
            int cap = (int)std::floor((bottom - cy + gap) / (chipH + gap));
            cap = std::max(0, std::min(cap, 4));
            int showN = std::min((int)idxs.size(), cap);
            int remain = (int)idxs.size() - showN;
            if (remain > 0 && showN > 0) --showN;
            for (int k = 0; k < showN; ++k) {
                int oi = idxs[k];
                RectF chip(cell.X + S(5), cy, cell.Width - S(10), chipH);
                DrawEventChip(g, chip, app.occs[oi], false);
                AddHit(chip.X, chip.Y, chip.Width, chip.Height, CMD_EVENT_BASE + oi, [oi]() {
                    if (oi < (int)app.occs.size()) OpenEditorForOccurrence(app.occs[oi]);
                });
                cy += chipH + gap;
            }
            remain = (int)idxs.size() - showN;
            if (remain > 0 && cy + S(14) < BY(cell)) {
                Txt(g, Fmt(L"+%d", remain), S(9.5f), true, WithAlpha(th.accent, 240), RectF(cell.X + S(10), cy - S(1), cell.Width - S(16), S(14)), ALIGN_LEFT);
            }
        }
    }
    }
    g.ResetClip();
    for (int i = 1; i < 7; ++i) Line(g, grid.X + i * cw, grid.Y, grid.X + i * cw, BY(grid), WithAlpha(th.borderSoft, 160), S(1));
    for (int i = 1; i < 6; ++i) Line(g, grid.X, grid.Y + i * ch, RX(grid), grid.Y + i * ch, WithAlpha(th.borderSoft, 160), S(1));
    Txt(g, L"双击空白新建 · Shift+点击日程多选 · 右键日期看详情 · 滚轮切月", S(9), false, th.dim2,
        RectF(grid.X + S(2), BY(grid) + S(4), grid.Width, S(16)));
}

// ---------------- 周 / 日视图 ----------------
static Color OccColor(const Occurrence& o) {
    if (!o.ev.color.empty()) return Hex(o.ev.color);
    if (!o.ev.tags.empty()) return TagColorOf(o.ev.tags[0]);
    return Color(255, 148, 163, 184);
}

struct TimeBlock { int oi; int startMin, endMin; int col, maxCols; };

static std::vector<TimeBlock> LayoutDayBlocks(const std::vector<int>& idxs, const DateTime& dayStart, int maxMin) {
    std::vector<TimeBlock> blocks;
    for (int oi : idxs) {
        const Occurrence& o = app.occs[oi];
        int sm = MinutesBetween(dayStart, o.start);
        int em = MinutesBetween(dayStart, o.end);
        if (sm < 0) sm = 0;
        if (em > maxMin) em = maxMin;
        if (em <= sm) em = sm + 15;
        blocks.push_back(TimeBlock{ oi, sm, em, 0, 1 });
    }
    std::sort(blocks.begin(), blocks.end(), [](const TimeBlock& a, const TimeBlock& b) {
        if (a.startMin != b.startMin) return a.startMin < b.startMin;
        return a.endMin < b.endMin;
    });
    // 贪心分列
    std::vector<int> colEnd;
    for (auto& b : blocks) {
        int col = -1;
        for (int c = 0; c < (int)colEnd.size(); ++c) {
            if (colEnd[c] <= b.startMin) { col = c; break; }
        }
        if (col < 0) { col = (int)colEnd.size(); colEnd.push_back(0); }
        colEnd[col] = b.endMin;
        b.col = col;
    }
    // 每个块只按「与它时间真正重叠的块」计算占用列数：
    // 不重叠的事件保持全宽，不被同一列别处的重叠对连累缩窄
    for (size_t i = 0; i < blocks.size(); ++i) {
        int groupCols = blocks[i].col + 1;
        for (size_t j = 0; j < blocks.size(); ++j) {
            if (i == j) continue;
            const bool overlap = blocks[i].startMin < blocks[j].endMin &&
                                 blocks[j].startMin < blocks[i].endMin;
            if (overlap) groupCols = (std::max)(groupCols, blocks[j].col + 1);
        }
        blocks[i].maxCols = (std::max)(1, groupCols);
    }
    return blocks;
}

void DrawWeekView(Graphics& g, float x, float y, float w, float h, bool singleDay) {
    gfx::FontPartScope fontPart(gfx::FP_LIST);
    const Theme& th = TH();
    ViewHeader(g, x, y, w);
    int ws = app.store.settings.weekStart;
    DateTime weekStart = singleDay ? StartOfDay(app.cursor) : StartOfWeek(app.cursor, ws);
    int days = singleDay ? 1 : 7;
    float gridTop = y + S(62);
    float headH = S(44);

    // 收集全天事件
    std::vector<int> allDayIdx;
    for (int i = 0; i < (int)app.occs.size(); ++i) {
        const Occurrence& o = app.occs[i];
        if (!OccMatchesFilter(o)) continue;
        if (!o.ev.allDay) continue;
        DateTime s = StartOfDay(o.start), e = StartOfDay(o.end);
        DateTime rangeEnd = AddDaysDT(weekStart, days);
        if (CompareDT(s, rangeEnd) >= 0 || CompareDT(e, weekStart) <= 0) continue;
        allDayIdx.push_back(i);
    }
    float adH = allDayIdx.empty() ? 0 : S(26);
    // 表头
    float colW = (w - S(48)) / days;
    for (int i = 0; i < days; ++i) {
        DateTime day = AddDaysDT(weekStart, i);
        RectF hr(x + S(48) + i * colW, gridTop, colW, headH);
        bool today = SameDay(day, app.now);
        if (today) {
            FillRoundRect(g, RectF(hr.X + S(2), hr.Y + S(2), hr.Width - S(4), hr.Height - S(4)), S(9), WithAlpha(th.accent, 16));
            StrokeRoundRect(g, RectF(hr.X + S(2), hr.Y + S(2), hr.Width - S(4), hr.Height - S(4)), S(9), WithAlpha(th.accent, 110), S(1));
        }
        Txt(g, WeekdayCN(DayOfWeek(day.y, day.mo, day.d)), S(11), today, today ? th.accent : th.dim,
            RectF(hr.X, hr.Y + S(4), hr.Width, S(16)), ALIGN_CENTER);
        Txt(g, Fmt(L"%d", day.d), S(17), true, today ? th.accent : th.text, RectF(hr.X, hr.Y + S(18), hr.Width, S(22)), ALIGN_CENTER, true);
        AddHit(hr.X, hr.Y, hr.Width, hr.Height, CMD_DAY_BASE + DaySerial(day.y, day.mo, day.d), [day]() {
            app.cursor = day;
            UIInvalidate();
        });
    }
    // 全天行
    float adTop = gridTop + headH;
    if (adH > 0) {
        FillRoundRect(g, RectF(x + S(48), adTop + S(2), w - S(48), adH - S(4)), S(6), WithAlpha(th.panel2, 160));
        Txt(g, L"全天", S(9), false, th.dim2, RectF(x, adTop, S(44), adH), ALIGN_RIGHT | ALIGN_VCENTER);
        for (int i = 0; i < days; ++i) {
            DateTime day = AddDaysDT(weekStart, i);
            int serial = DaySerial(day.y, day.mo, day.d);
            int slot = 0;
            for (int oi : allDayIdx) {
                const Occurrence& o = app.occs[oi];
                int s0 = DaySerial(o.start.y, o.start.mo, o.start.d);
                int e0 = DaySerial(o.end.y, o.end.mo, o.end.d);
                if (o.ev.allDay) e0 = std::max(s0, e0 - 1);
                if (serial < s0 || serial > e0) continue;
                RectF chip(x + S(48) + i * colW + S(3), adTop + S(4) + slot * S(21), colW - S(6), S(18));
                DrawEventChip(g, chip, o, true);
                AddHit(chip.X, chip.Y, chip.Width, chip.Height, CMD_EVENT_BASE + oi, [oi]() {
                    if (oi < (int)app.occs.size()) OpenEditorForOccurrence(app.occs[oi]);
                });
                ++slot;
            }
        }
    }
    // 时间网格
    float areaTop = adTop + adH + S(4);
    float areaBottom = y + h - S(22);
    float gutter = S(48);
    int extendMin = 0;
    if (singleDay) {
        DateTime day0 = weekStart;
        DateTime dayEnd = AddDaysDT(day0, 1);
        for (auto& o : app.occs) {
            if (!OccMatchesFilter(o) || o.ev.allDay) continue;
            if (!(CompareDT(o.start, dayEnd) < 0 && CompareDT(o.end, day0) > 0)) continue;
            int endAbs = MinutesBetween(day0, o.end);
            if (endAbs > 1440) extendMin = std::max(extendMin, std::min(endAbs - 1440, 12 * 60));
        }
    }
    int totalMin = 1440 + extendMin;
    g_wkTotalMin = totalMin;
    int fitZoom = (int)std::ceil((areaBottom - areaTop) / ((totalMin / 60.0f) * S(52)) * 100.0f);
    g_fitZoom = std::max(25, std::min(160, fitZoom));
    int savedZoom = std::max(25, std::min(160, app.store.settings.timeZoom));
    if (savedZoom < g_fitZoom) {
        app.store.settings.timeZoom = g_fitZoom;
        app.store.Touch();
        DoSaveStore();
        savedZoom = g_fitZoom;
    }
    int zoomPct = savedZoom;
    float hourH = S(52) * (zoomPct / 100.0f);
    float pxPerMin = hourH / 60.0f;
    int visibleMin = (int)((areaBottom - areaTop) / pxPerMin);
    int maxScroll = std::max(0, totalMin - visibleMin);
    int scrollMin = std::max(0, std::min(maxScroll, app.scrollMin));
    app.scrollMin = scrollMin;
    DateTime dayStart = AddDaysDT(weekStart, 0);
    g_wkDayStart = weekStart;
    g_wkGridTop = areaTop;
    g_wkPxPerMin = pxPerMin;
    g_wkGridX = x + gutter;
    g_wkColW = colW;
    g_wkScrollMin = scrollMin;
    g_wkValid = true;
    g_wkAreaH = areaBottom - areaTop;
    // 时间刻度与横线（跨日时在 24:00 之后继续画 +1 刻度）
    int startHour = scrollMin / 60;
    int endHour = (totalMin + 59) / 60;
    for (int hour = startHour; hour <= endHour; ++hour) {
        float ly = areaTop + (hour * 60 - scrollMin) * pxPerMin;
        if (ly < areaTop - 1) continue;
        if (ly > areaBottom + 1) break;
        Color lc = (hour % 2 == 0) ? WithAlpha(th.borderSoft, 170) : WithAlpha(th.borderSoft, 90);
        if (hour == 24) lc = WithAlpha(th.accent, 90);
        Line(g, x + gutter, ly, x + w, ly, lc, S(1));
        std::wstring label;
        if (hour < 24) label = Fmt(L"%02d:00", hour);
        else if (hour == 24) label = L"24:00";
        else label = Fmt(L"+1 %02d:00", hour - 24);
        Txt(g, label, S(9.5f), false, th.dim2, RectF(x, ly - S(8), gutter - S(6), S(16)), ALIGN_RIGHT);
    }
    // 列分隔
    for (int i = 0; i <= days; ++i) Line(g, x + gutter + i * colW, areaTop, x + gutter + i * colW, areaBottom, WithAlpha(th.borderSoft, 150), S(1));

    // 事件块
    for (int i = 0; i < days; ++i) {
        DateTime day = AddDaysDT(weekStart, i);
        DateTime dayEnd = AddDaysDT(day, 1);
        int daySerial = DaySerial(day.y, day.mo, day.d);
        std::vector<int> idxs;
        for (int k = 0; k < (int)app.occs.size(); ++k) {
            const Occurrence& o = app.occs[k];
            if (!OccMatchesFilter(o) || o.ev.allDay) continue;
            if (singleDay) {
                // 日视图：开始日在当天的、以及前一天延续到当天的跨日事件都要显示
                if (!(CompareDT(o.start, dayEnd) < 0 && CompareDT(o.end, day) > 0)) continue;
            } else {
                if (!(CompareDT(o.start, dayEnd) < 0 && CompareDT(o.end, day) > 0)) continue;
            }
            idxs.push_back(k);
        }
        if (idxs.empty()) continue;
        auto blocks = LayoutDayBlocks(idxs, day, totalMin);
        float colX0 = x + gutter + i * colW;
        for (auto& b : blocks) {
            const Occurrence& o = app.occs[b.oi];
            float top = areaTop + (b.startMin - scrollMin) * pxPerMin;
            float bottom = areaTop + (b.endMin - scrollMin) * pxPerMin;
            if (bottom < areaTop || top > areaBottom) continue;
            float bw = (colW - S(6)) / b.maxCols;
            float bx = colX0 + S(3) + b.col * bw;
            float by = std::max(top, areaTop);
            float bh = std::max(S(18), std::min(bottom, areaBottom) - by);
            // 时间点事件（含时间点 DDL）：块太矮时槽高不够显示时间，给最小高度
            bool pointEvent = (CompareDT(o.start, o.end) == 0);
            if (pointEvent) bh = std::max(bh, S(34));
            Color tc = OccColor(o);
            bool done = o.ev.done;
            bool selected = app.selected.count(OccKey(o)) > 0;
            Color bg = WithAlpha(tc, done ? 26 : 42);
            if (selected) bg = WithAlpha(th.accent, 80);
            RectF br(bx, by, bw - S(2), bh);
            FillRoundRect(g, br, S(6), bg);
            if (selected) StrokeRoundRect(g, br, S(6), th.accent, S(2.2f));
            if (!singleDay && DaySerial(o.start.y, o.start.mo, o.start.d) != daySerial) {
                Pen dp(done ? th.dim2 : tc, S(2.0f));
                dp.SetDashStyle(DashStyleDash);
                g.DrawLine(&dp, br.X + S(1.3f), br.Y + S(2), br.X + S(1.3f), BY(br) - S(2));
            } else {
                FillRoundRect(g, RectF(br.X, br.Y + S(1), S(2.6f), br.Height - S(2)), S(1.3f), done ? th.dim2 : tc);
            }
            RectF txtR(br.X + S(7), br.Y + S(2), br.Width - S(12), br.Height - S(4));
            Txt(g, (done ? L"✓ " : L"") + DisplayEventTitle(o), S(10.5f), true, done ? WithAlpha(th.dim, 220) : th.text,
                RectF(txtR.X, txtR.Y, txtR.Width, S(15)), ALIGN_LEFT);
            if (bh > S(30)) {
                std::wstring timeText = FormatTimeRange(o);
                if (!singleDay) {
                    int sday = DaySerial(o.start.y, o.start.mo, o.start.d);
                    int eday = DaySerial(o.end.y, o.end.mo, o.end.d);
                    if (sday != eday) {
                        if (sday == daySerial) timeText = TimeStr(o.start) + L"–24:00";
                        else if (eday == daySerial) {
                            timeText = L"00:00–" + TimeStr(o.end);
                            int n = eday - sday;
                            if (n > 0) timeText += Fmt(L"(+%d)", n);
                        } else timeText = L"00:00–24:00";
                    }
                }
                float timePx = S(8.5f);
                if (bw <= S(110)) {
                    size_t pp = timeText.find(L"(+");
                    if (pp != std::wstring::npos) timeText = timeText.substr(0, pp);
                    timePx = S(7.5f);
                }
                if (pointEvent && o.ev.ddl) timeText += L" · DDL";
                Txt(g, timeText, timePx, false, done ? th.dim2 : WithAlpha(tc, 240),
                    RectF(txtR.X, txtR.Y + S(15), txtR.Width, S(13)), ALIGN_LEFT);
                if (singleDay && b.endMin >= totalMin && bh > S(30))
                    Txt(g, L"…", S(9), true, WithAlpha(tc, 220), RectF(br.X, BY(br) - S(14), br.Width - S(5), S(12)), ALIGN_RIGHT);
            }
            if (bh > S(44) && !o.ev.location.empty()) {
                Txt(g, o.ev.location, S(8.5f), false, th.dim2, RectF(txtR.X, txtR.Y + S(28), txtR.Width, S(13)), ALIGN_LEFT);
            }
            AddHit(br.X, br.Y, br.Width, br.Height, CMD_EVENT_BASE + b.oi, [oi = b.oi]() {
                if (oi < (int)app.occs.size()) OpenEditorForOccurrence(app.occs[oi]);
            });
        }
    }
    // 当前时间线
    if (singleDay || (CompareDT(weekStart, app.now) <= 0 && CompareDT(AddDaysDT(weekStart, days), app.now) > 0)) {
        DateTime today = StartOfDay(app.now);
        int dayOffset = DaySerial(today.y, today.mo, today.d) - DaySerial(weekStart.y, weekStart.mo, weekStart.d);
        if (dayOffset >= 0 && dayOffset < days) {
            float nowMin = app.now.h * 60 + app.now.mi;
            float ly = areaTop + (nowMin - scrollMin) * pxPerMin;
            if (ly >= areaTop && ly <= areaBottom) {
                float lx0 = x + gutter + dayOffset * colW;
                Line(g, lx0, ly, lx0 + colW, ly, WithAlpha(th.accent, 220), S(1.4f));
                FillCircle(g, lx0 + S(2), ly, S(3.6f), th.accent);
                Txt(g, TimeStr(app.now), S(9), true, th.accent, RectF(x, ly - S(8), gutter - S(6), S(16)), ALIGN_RIGHT);
            }
        }
    }
    // 底部提示
    Txt(g, singleDay ? L"双击空白新建 · Shift+点击日程多选 · 点空白取消" : L"双击空白新建 · Shift+点击日程多选 · 点空白取消 · ←/→ 切换日期",
        S(9), false, th.dim2, RectF(x, y + h - S(16), w, S(14)));
}

// ---------------- 日程视图 ----------------
static std::vector<std::wstring> g_agendaVisibleKeys;

static void AgendaRowClick(const Occurrence& o) {
    std::wstring key = OccKey(o);
    bool shift = ShiftSelectPressed();
    bool ctrl = CtrlSelectPressed();
    int b = -1;
    for (int i = 0; i < (int)g_agendaVisibleKeys.size(); ++i)
        if (g_agendaVisibleKeys[i] == key) { b = i; break; }
    if (shift) {
        int a = -1;
        for (int i = 0; i < (int)g_agendaVisibleKeys.size(); ++i)
            if (g_agendaVisibleKeys[i] == app.agendaAnchorKey) { a = i; break; }
        if (a < 0 || b < 0) {
            app.selected.clear();
            app.selected.insert(key);
        } else {
            for (int i = std::min(a, b); i <= std::max(a, b); ++i) app.selected.insert(g_agendaVisibleKeys[i]);
        }
    } else if (ctrl) {
        ToggleSelected(key);
    } else {
        app.selected.clear();
        app.selected.insert(key);
    }
    app.agendaAnchorKey = key;
    UIInvalidate();
}

void DrawAgendaView(Graphics& g, float x, float y, float w, float h) {
    gfx::FontPartScope fontPart(gfx::FP_LIST);
    const Theme& th = TH();
    ViewHeader(g, x, y, w);
    float top = y + S(62);
    // 列表 / 看板模式切换
    UI::Chip(g, RectF(x, top, S(56), S(28)), CMD_AGENDA_LIST_VIEW, L"列表", !app.agendaBoard, th.accent, []() {
        app.agendaBoard = false;
        app.agendaScroll = 0;
        app.boardSelected.clear();
        UIInvalidate();
    }, 10);
    UI::Chip(g, RectF(x + S(62), top, S(56), S(28)), CMD_AGENDA_BOARD_VIEW, L"看板", app.agendaBoard, th.accent, []() {
        app.agendaBoard = true;
        app.agendaScroll = 0;
        app.boardSelected.clear();
        UIInvalidate();
    }, 10);
    if (app.agendaBoard) {
        DrawAgendaBoard(g, x, top + S(38), w, y + h - S(20) - (top + S(38)));
        return;
    }
    top += S(40);
    // ---- 筛选条 ----
    DateTime filterFrom{}, filterTo{};
    auto calcFilterRange = [&]() {
        DateTime baseFrom = StartOfDay(app.cursor);
        filterFrom = baseFrom;
        filterTo = AddDaysDT(baseFrom, 60);
        int r = app.agendaFilter.range;
        if (r == 1) filterTo = AddDaysDT(baseFrom, 1);
        else if (r == 2) filterTo = AddDaysDT(baseFrom, 7);
        else if (r == 3) {
            auto parseYmd = [](const std::wstring& txt, DateTime& out) {
                int y = 0, m = 0, d = 0;
                if (swscanf_s(txt.c_str(), L"%d-%d-%d", &y, &m, &d) == 3 &&
                    y >= 1901 && y <= 2100 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
                    out = MakeDT(y, m, d);
                    return true;
                }
                return false;
            };
            if (app.agendaFilter.fromText.empty()) app.agendaFilter.fromText = ISODate(baseFrom);
            if (app.agendaFilter.toText.empty()) app.agendaFilter.toText = ISODate(AddDaysDT(baseFrom, 60));
            DateTime cf{}, ct{};
            if (parseYmd(app.agendaFilter.fromText, cf)) filterFrom = StartOfDay(cf);
            if (parseYmd(app.agendaFilter.toText, ct)) filterTo = AddDaysDT(StartOfDay(ct), 1);
            if (CompareDT(filterTo, filterFrom) <= 0) filterTo = AddDaysDT(filterFrom, 60);
        }
    };
    {
        const wchar_t* rangeNames[4] = { L"全部", L"今天", L"7 天", L"自定义" };
        for (int i = 0; i < 4; ++i) {
            UI::Chip(g, RectF(x + i * S(62), top, S(56), S(26)), CMD_AGENDA_RANGE_BASE + i, rangeNames[i],
                     app.agendaFilter.range == i, th.accent, [i]() {
                         app.agendaFilter.range = i;
                         app.agendaScroll = 0;
                         UIInvalidate();
                     }, 9.5f);
        }
        if (app.agendaFilter.range == 3) {
            float cx2 = x + 4 * S(62) + S(10);
            UIEdit(9320, RectF(cx2, top, S(92), S(26)), &app.agendaFilter.fromText, false, S(10));
            Txt(g, L"→", S(11), false, th.dim, RectF(cx2 + S(96), top, S(18), S(26)), ALIGN_CENTER | ALIGN_VCENTER);
            UIEdit(9321, RectF(cx2 + S(116), top, S(92), S(26)), &app.agendaFilter.toText, false, S(10));
            UI::Btn(g, RectF(cx2 + S(214), top, S(52), S(26)), CMD_AGENDA_FILTER_APPLY, L"应用", 1, -1, []() {
                app.agendaScroll = 0;
                UIInvalidate();
            });
        }
        top += S(32);
        const wchar_t* doneNames[3] = { L"全部", L"未完成", L"已完成" };
        for (int i = 0; i < 3; ++i) {
            UI::Chip(g, RectF(x + i * S(62), top, S(56), S(26)), CMD_AGENDA_DONE_BASE + i, doneNames[i],
                     app.agendaFilter.done == i, th.accent, [i]() {
                         app.agendaFilter.done = i;
                         app.agendaScroll = 0;
                         UIInvalidate();
                     }, 9.5f);
        }
    }
    top += S(32);
    calcFilterRange();
    // 当前列表的可见事件（与绘制、多选、批量操作共用同一份顺序）
    auto occs = ExpandOccurrences(app.store, filterFrom, filterTo);
    std::sort(occs.begin(), occs.end(), [](const Occurrence& a, const Occurrence& b) {
        int ca = DaySerial(a.start.y, a.start.mo, a.start.d);
        int cb = DaySerial(b.start.y, b.start.mo, b.start.d);
        if (ca != cb) return ca < cb;
        if (a.ev.allDay != b.ev.allDay) return a.ev.allDay;
        return CompareDT(a.start, b.start) < 0;
    });
    std::vector<Occurrence> visible;
    for (auto& o : occs) {
        if (!OccMatchesFilter(o)) continue;
        if (app.agendaFilter.done == 1 && o.ev.done) continue;
        if (app.agendaFilter.done == 2 && !o.ev.done) continue;
        visible.push_back(o);
    }
    g_agendaVisibleKeys.clear();
    for (auto& o : visible) g_agendaVisibleKeys.push_back(OccKey(o));
    int total = (int)visible.size();
    std::wstring rangeLabel = app.agendaFilter.range == 1 ? L"今天"
                            : app.agendaFilter.range == 2 ? L"未来 7 天"
                            : app.agendaFilter.range == 3 ? (app.agendaFilter.fromText + L" → " + app.agendaFilter.toText)
                            : L"全部";
    Txt(g, Fmt(L"%s · %d 项日程", rangeLabel.c_str(), total), S(10), false, th.dim2,
        RectF(x, top - S(2), S(320), S(16)));
    if (app.selected.empty()) {
Txt(g, L"单击选中 · Ctrl+点击多选 · Shift+点击范围 · 双击编辑 · 点左侧圆点完成 · Ctrl+A 全选", S(9), false, WithAlpha(th.dim2, 210),
            RectF(x, top - S(2), w, S(16)), ALIGN_RIGHT);
    }
    // 批量工具栏
    std::vector<Occurrence*> sel;
    for (auto& o : visible) if (app.selected.count(OccKey(o))) sel.push_back(&o);
    float listTop = top + S(6);
    if (!sel.empty()) {
        // 批量工具栏固定在底部，避免选中/取消时列表整体上下跳动
        RectF bar(x, y + h - S(52), w, S(34));
        FillRoundRect(g, bar, S(9), WithAlpha(th.card, 240));
        StrokeRoundRect(g, bar, S(9), WithAlpha(th.accent, 90), S(1));
        Txt(g, Fmt(L"已选 %d", (int)sel.size()), S(11), true, th.accent, RectF(bar.X + S(12), bar.Y, S(90), bar.Height), ALIGN_VCENTER);
        float bx = bar.X + S(100);
        auto bitem = [&](const wchar_t* label, int cmd, std::function<void()> fn, bool danger = false) {
            SizeF sz; MeasureText(g, label, S(10.5f), false, sz);
            float bw = sz.Width + S(18);
            UI::Btn(g, RectF(bx, bar.Y + S(4), bw, S(26)), cmd, label, danger ? 3 : 1, -1, std::move(fn));
            bx += bw + S(6);
        };
        bitem(L"全选", CMD_AGENDA_SELALL, []() {
            for (auto& k : g_agendaVisibleKeys) app.selected.insert(k);
            UIInvalidate();
        });
        bitem(L"清空", CMD_AGENDA_SELNONE, []() { app.selected.clear(); app.agendaAnchorKey.clear(); UIInvalidate(); });
        bitem(L"反选", CMD_AGENDA_INVERT, []() {
            for (auto& k : g_agendaVisibleKeys) {
                auto it = app.selected.find(k);
                if (it == app.selected.end()) app.selected.insert(k);
                else app.selected.erase(it);
            }
            UIInvalidate();
        });
        bitem(L"完成/取消", CMD_AGENDA_DONE, []() {
            std::vector<Occurrence> sel;
            for (auto& o : app.occs) if (app.selected.count(OccKey(o))) sel.push_back(o);
            if (sel.empty()) return;
            bool allDone = true;
            for (auto& o : sel) if (!o.ev.done) { allDone = false; break; }
            bool target = !allDone;
            app.store.Snapshot();
            for (auto& o : sel) app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, target);
            DoSaveStore();
            InvalidateOccs();
            ShowToast(target ? L"已标记完成" : L"已标记未完成");
        });
        bitem(L"删 除", CMD_AGENDA_DELETE, []() { DeleteSelected(); }, true);
        bitem(L"改标签", CMD_AGENDA_TAGS, []() {
            std::vector<MenuItemDef> items;
            for (auto& tag : app.store.tags) {
                std::string tid = tag.id;
                items.push_back({ tag.name, -1, false, false, [tid]() {
                    app.store.Snapshot();
                    for (auto& o : app.occs) {
                        if (!app.selected.count(OccKey(o))) continue;
                        Event* e = app.store.Find(o.seriesId);
                        if (!e) continue;
                        if (e->HasTag(tid)) e->tags.erase(std::remove(e->tags.begin(), e->tags.end(), tid), e->tags.end());
                        else e->tags.push_back(tid);
                        app.store.Touch();
                    }
                    DoSaveStore();
                    InvalidateOccs();
                } });
            }
            OpenMenu(0, 0, items);
        });
        bitem(L"优先级", CMD_AGENDA_PRIO, []() {
            std::vector<MenuItemDef> items;
            for (int p = 0; p <= 3; ++p) {
                int pp = p;
                items.push_back({ Fmt(L"%s优先级", PriorityName(p)), -1, false, false, [pp]() {
                    app.store.Snapshot();
                    for (auto& o : app.occs) {
                        if (!app.selected.count(OccKey(o))) continue;
                        Event* e = app.store.Find(o.seriesId);
                        if (e) { e->priority = pp; app.store.Touch(); }
                    }
                    DoSaveStore();
                    InvalidateOccs();
                } });
            }
            OpenMenu(0, 0, items);
        });
        bitem(L"颜色", CMD_AGENDA_COLOR, []() {
            const auto& palette = PaletteHex();
            const auto& names = PaletteNames();
            auto applyColor = [](const std::wstring& hex) {
                app.store.Snapshot();
                std::set<std::string> ids;
                for (auto& o : app.occs) if (app.selected.count(OccKey(o))) ids.insert(o.seriesId);
                for (auto& id : ids) {
                    Event* e = app.store.Find(id);
                    if (e) { e->color = hex; app.store.Touch(); }
                }
                DoSaveStore();
                InvalidateOccs();
                UIInvalidate();
            };
            std::vector<MenuItemDef> items;
            items.push_back({ L"自动（跟随标签 / 课程配色）", ICO_THEME, false, false, []() {
                app.store.Snapshot();
                std::set<std::string> ids;
                for (auto& o : app.occs) if (app.selected.count(OccKey(o))) ids.insert(o.seriesId);
                for (auto& id : ids) {
                    Event* e = app.store.Find(id);
                    if (e) { e->color.clear(); app.store.Touch(); }
                }
                DoSaveStore();
                InvalidateOccs();
                UIInvalidate();
            } });
            for (int i = 0; i < (int)palette.size(); ++i) {
                std::wstring hex = palette[i];
                std::wstring nm = i < (int)names.size() ? names[i] : hex;
                items.push_back({ Fmt(L"%s  %s", nm.c_str(), hex.c_str()), -1, false, false,
                                  [applyColor, hex]() { applyColor(hex); }, hex });
            }
            OpenMenu(0, 0, items);
        });
        bitem(L"调休迁移", CMD_AGENDA_RESCHEDULE, []() { OpenRescheduleFromSelection(); });
    }
    float listBottomReserve = S(46);   // 固定预留：选中/清空时行位置不跳动
    RectF list(x, listTop, w, y + h - S(20) - listTop - listBottomReserve);
    g.SetClip(list);
    float ly = list.Y - app.agendaScroll;
    float dens = std::max(0.85f, std::min(1.15f, TH().density / 100.0f));
    int drawn = 0;
    DateTime lastDay{ 0,0,0,0,0 };

    // ---- 跨日区间框：多个跨日事件并排，各自颜色；框内补齐日期头 ----
    struct BoxState { DateTime end{}; Color color; float startY = 0; int slot = 0; };
    std::vector<BoxState> boxes;
    bool slotUsed[6] = { false, false, false, false, false, false };
    const float boxLineX = list.X + S(12);
    const float boxIndent = S(18);
    auto boxX = [&](int slot) { return boxLineX + (float)slot * S(8); };
    auto allocSlot = [&]() -> int {
        for (int i = 0; i < 6; ++i) if (!slotUsed[i]) { slotUsed[i] = true; return i; }
        return 5;
    };
    auto closeEnded = [&](const DateTime& t, float yy) {
        for (int i = (int)boxes.size() - 1; i >= 0; --i) {
            if (CompareDT(boxes[i].end, t) > 0) continue;
            float lineY = yy + (float)boxes[i].slot * S(3);
            if (lineY >= list.Y - S(4) && lineY <= BY(list) + S(4))
                Line(g, boxX(boxes[i].slot), lineY, boxX(boxes[i].slot) + S(14), lineY, WithAlpha(boxes[i].color, 190), S(1.8f));
            slotUsed[boxes[i].slot] = false;
            boxes.erase(boxes.begin() + i);
        }
    };
    auto drawBoxLines = [&](float y1, float y2) {
        for (auto& b : boxes) {
            float y0 = std::max(y1, b.startY);
            if (y2 > y0) Line(g, boxX(b.slot), y0, boxX(b.slot), y2, WithAlpha(b.color, 190), S(1.8f));
        }
    };
    auto drawEventRow = [&](const Occurrence& o, float ind, int vi) {
        RectF row(list.X, ly, list.Width, S(40));
        if (row.Y + row.Height > list.Y - S(20) && row.Y < BY(list)) {
            bool selected = app.selected.count(OccKey(o)) > 0;
            Color tc = OccColor(o);
            if (selected) {
                FillRoundRect(g, RectF(row.X + S(4), row.Y + S(2), row.Width - S(8), row.Height - S(4)), S(8), WithAlpha(th.accent, 62));
                FillRoundRect(g, RectF(row.X + S(4), row.Y + S(6), S(3.2f), row.Height - S(12)), S(1.6f), th.accent);
                StrokeRoundRect(g, RectF(row.X + S(4), row.Y + S(2), row.Width - S(8), row.Height - S(4)), S(8), WithAlpha(th.accent, 150), S(1.2f));
            } else if (IsHovered(CMD_EVENT_BASE + vi)) FillRoundRect(g, RectF(row.X + S(4), row.Y + S(2), row.Width - S(8), row.Height - S(4)), S(8), WithAlpha(th.cardHover, 120));
            if (ind > 0) drawBoxLines(row.Y, BY(row));
            // 单击单选；Ctrl+点击切换；Shift+点击范围；双击编辑
            AddHit(row.X + S(24) + ind, row.Y + S(2), row.Width - S(24) - ind - S(4), row.Height - S(4), CMD_EVENT_BASE + vi, [o]() {
                AgendaRowClick(o);
            });
            bool doneRow = o.ev.done;
            RectF cb(row.X + S(8) + ind, row.Y + S(12), S(16), S(16));
            FillRoundRect(g, cb, S(5), doneRow ? th.success : WithAlpha(th.panel2, 200));
            StrokeRoundRect(g, cb, S(5), doneRow ? th.success : (selected ? th.accent : th.border), selected ? S(1.6f) : S(1));
            if (doneRow) Icon(g, ICO_CHECK, RectF(cb.X + S(3), cb.Y + S(3), S(10), S(10)), th.onAccent, 2.0f);
            AddHit(cb.X, cb.Y - S(2), cb.Width + S(2), cb.Height + S(4), CMD_SELECTED_BASE + vi, [o]() {
                ToggleOccurrenceDone(o, !o.ev.done);
            });
            Txt(g, EventTimeRangeText(o.start, o.end, o.ev.allDay), S(10), true, WithAlpha(tc, 240),
                RectF(row.X + S(32) + ind, row.Y, S(116), row.Height), ALIGN_VCENTER, true);
            bool done = o.ev.done;
            Txt(g, DisplayEventTitle(o), S(12), o.ev.priority == PRIO_HIGH, done ? WithAlpha(th.dim, 220) : th.text,
                RectF(row.X + S(154) + ind, row.Y, row.Width - S(360), row.Height), ALIGN_VCENTER);
            if (done) Txt(g, L"✓", S(10), true, th.success, RectF(row.X + S(118) + ind, row.Y, S(14), row.Height), ALIGN_VCENTER);
            float tx = RX(row) - S(170);
            for (size_t ti = 0; ti < o.ev.tags.size() && ti < 3; ++ti) {
                const Tag* tag = app.store.FindTag(o.ev.tags[ti]);
                if (!tag) continue;
                SizeF sz; MeasureText(g, tag->name, S(9), false, sz);
                RectF chip(tx, row.Y + S(13), sz.Width + S(14), S(16));
                FillRoundRect(g, chip, S(4), WithAlpha(Hex(tag->color), 30));
                FillCircle(g, chip.X + S(6), chip.Y + chip.Height / 2, S(2.4f), Hex(tag->color));
                Txt(g, tag->name, S(9), false, WithAlpha(Hex(tag->color), 250), RectF(chip.X + S(11), chip.Y, chip.Width - S(13), chip.Height), ALIGN_VCENTER);
                tx += chip.Width + S(5);
            }
            if (o.ev.IsRecurring()) {
                Pen p(WithAlpha(th.dim, 150), S(1));
                g.DrawArc(&p, RX(row) - S(28), row.Y + S(16), S(8), S(8), 40, 250);
            }
            if (o.ev.alarms.size()) Icon(g, ICO_BELL, RectF(RX(row) - S(58), row.Y + S(13), S(13), S(13)), WithAlpha(th.dim, 150), 1.4f);
            Line(g, row.X + S(8), BY(row) - S(1), RX(row) - S(8), BY(row) - S(1), WithAlpha(th.borderSoft, 90), S(1));
        }
        ly += S(40);
        ++drawn;
    };
    auto drawDateHeader = [&](const DateTime& day) {
        if (!(ly + S(26) > list.Y)) return;
        RectF hd(list.X, ly, list.Width, S(26));
        std::wstring label = Fmt(L"%d月%d日 %s", day.mo, day.d, WeekdayCN(DayOfWeek(day.y, day.mo, day.d)));
        std::wstring rel = RelativeDayLabel(day);
        if (rel != label && (SameDay(day, app.now))) label += L" · 今天";
        float ind = (float)boxes.size() * boxIndent;
        float hx = hd.X + S(24) + ind;
        drawBoxLines(hd.Y, hd.Y + S(26 * dens));
        Txt(g, label, S(11.5f), true, SameDay(day, app.now) ? th.accent : th.dim, RectF(hx, hd.Y, S(300), hd.Height), ALIGN_VCENTER);
        if (boxes.empty()) Line(g, hd.X + S(6), hd.Y + S(24), RX(hd) - S(6), hd.Y + S(24), WithAlpha(th.borderSoft, 180), S(1));
    };
    auto anyCovered = [&](const DateTime& day) {
        DateTime d0 = StartOfDay(day);
        for (auto& b : boxes) if (CompareDT(b.end, d0) > 0) return true;
        return false;
    };
    auto advanceDate = [&](const DateTime& target) {
        if (lastDay.y == 0) {
            closeEnded(StartOfDay(target), ly);
            drawDateHeader(target);
            ly += S(26 * dens);
            lastDay = target;
            return;
        }
        if (SameDay(target, lastDay)) return;   // 同一天只画一次日期头
        DateTime next = AddDaysDT(lastDay, 1);
        int guard = 0;
        while (CompareDT(next, target) < 0 && guard++ < 90) {
            closeEnded(StartOfDay(next), ly);
            if (anyCovered(next)) { drawDateHeader(next); ly += S(26 * dens); }
            lastDay = next;
            next = AddDaysDT(next, 1);
        }
        closeEnded(StartOfDay(target), ly);
        drawDateHeader(target);
        ly += S(26 * dens);
        lastDay = target;
    };
    for (int vi = 0; vi < (int)visible.size(); ++vi) {
        const Occurrence& o = visible[vi];
        DateTime day = StartOfDay(o.start);
        advanceDate(day);
        closeEnded(o.start, ly);
        int ss = DaySerial(o.start.y, o.start.mo, o.start.d);
        int es = DaySerial(o.end.y, o.end.mo, o.end.d);
        bool endMidnight = (o.end.h == 0 && o.end.mi == 0);
        bool allDayMulti = o.ev.allDay && (es - ss) > 1;
        bool timedCross = !o.ev.allDay && (es > ss) && !(endMidnight && es == ss + 1);
        bool isCross = allDayMulti || timedCross;
        DateTime endAt = o.ev.allDay ? StartOfDay(o.end) : o.end;
        if (isCross) {
            int slot = allocSlot();
            float startY = ly + S(20) + (float)slot * S(3);
            boxes.push_back(BoxState{ endAt, OccColor(o), startY, slot });
            float xText = list.X + S(24) + (float)boxes.size() * boxIndent;
            if (startY >= list.Y - S(4) && startY <= BY(list) + S(4))
                Line(g, boxX(slot), startY, xText, startY, WithAlpha(boxes.back().color, 190), S(1.8f));
        }
        drawEventRow(o, (float)boxes.size() * boxIndent, vi);
    }
    if (!boxes.empty()) {
        DateTime tail{};
        for (auto& b : boxes) {
            DateTime e = StartOfDay(b.end);
            if (b.end.h == 0 && b.end.mi == 0) e = AddDaysDT(e, -1);
            if (tail.y == 0 || CompareDT(e, tail) > 0) tail = e;
        }
        advanceDate(tail);
        closeEnded(AddDaysDT(tail, 1), ly);
        boxes.clear();
        for (auto& u : slotUsed) u = false;
    }
    g.ResetClip();
    if (drawn == 0) {
        Txt(g, L"当前筛选没有日程。按 Ctrl+N 新建，或调整上方筛选条件。", S(12), false, th.dim2,
            RectF(list.X, list.Y + S(20), list.Width, S(24)), ALIGN_CENTER);
    }
}

// 周视图双击定位（供 main 调用；不满足时返回 false）
bool WeekViewHitTime(float px, float py, DateTime& day, int& minute) {
    if (!g_wkValid) return false;
    if (py < g_wkGridTop) return false;
    float colRel = px - g_wkGridX;
    if (colRel < 0) return false;
    int dayOffset = (int)(colRel / std::max(1.0f, g_wkColW));
    int days = (app.view == VIEW_DAY) ? 1 : 7;
    if (dayOffset < 0 || dayOffset >= days) return false;
    day = AddDaysDT(g_wkDayStart, dayOffset);
    int mins = g_wkScrollMin + (int)((py - g_wkGridTop) / g_wkPxPerMin);
    int maxMin = std::max(1440, g_wkTotalMin);
    mins = std::max(0, std::min(maxMin - 15, mins));
    if (mins >= 1440) {
        day = AddDaysDT(day, 1);
        mins -= 1440;
    }
    minute = (mins / 15) * 15;
    return true;
}