// yearview.cpp - 年视图：12 个月热力总览
#include "app.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

static std::wstring LowerYearStr(const std::wstring& s) {
    std::wstring o = s;
    for (auto& c : o) c = (wchar_t)towlower(c);
    return o;
}

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

void DrawYearView(Graphics& g, float x, float y, float w, float h) {
    gfx::FontPartScope fontPart(gfx::FP_MONTH);
    const Theme& th = TH();
    ViewHeader(g, x, y, w);
    int year = app.cursor.y;
    DateTime from = MakeDT(year, 1, 1);
    DateTime to = MakeDT(year + 1, 1, 1);
    auto occs = ExpandOccurrences(app.store, from, to);

    std::map<int, int> dayCount, monthCount;
    std::map<int, bool> examDay, deadlineDay;
    for (auto& o : occs) {
        int serial = DaySerial(o.start.y, o.start.mo, o.start.d);
        ++dayCount[serial];
        ++monthCount[o.start.mo];
        for (auto& tid : o.ev.tags) {
            if (app.store.TagName(tid) == L"考试") examDay[serial] = true;
        }
        std::wstring lowerTitle = LowerYearStr(o.ev.title);
        if (Contains(o.ev.title, L"截止") || Contains(lowerTitle, L"ddl")) deadlineDay[serial] = true;
    }
    int total = (int)occs.size();
    int busyMonth = 0, busyCount = 0;
    for (auto& kv : monthCount) if (kv.second > busyCount) { busyCount = kv.second; busyMonth = kv.first; }

    // 年度统计
    std::wstring stat = Fmt(L"全年 %d 项日程", total);
    if (busyCount > 0) stat += Fmt(L" · 最忙 %d 月（%d 项）", busyMonth, busyCount);
    if (!examDay.empty()) stat += Fmt(L" · 考试 %d 天", (int)examDay.size());
    Txt(g, stat, S(10), false, WithAlpha(th.dim2, 220), RectF(x, y + S(54), w - S(220), S(18)));

    const Settings& st = app.store.settings;
    int ws = st.weekStart;
    float top = y + S(78);
    float gap = S(12);
    float cardW = (w - gap * 3) / 4;
    float cardH = (h - S(78) - S(28) - gap * 2) / 3;
    if (cardW < S(120) || cardH < S(120)) return;

    for (int m = 1; m <= 12; ++m) {
        int idx = m - 1;
        int col = idx % 4, row = idx / 4;
        RectF card(x + col * (cardW + gap), top + row * (cardH + gap), cardW, cardH);
        bool hover = IsHovered(CMD_YEAR_MONTH_BASE + idx);
        bool curMonth = (m == app.cursor.mo);
        bool todayMonth = (app.now.y == year && app.now.mo == m);
        FillRoundRect(g, card, S(12), WithAlpha(th.card, 230));
        if (curMonth) StrokeRoundRect(g, card, S(12), WithAlpha(th.accent, 160), S(1.5f));
        else if (hover) StrokeRoundRect(g, card, S(12), WithAlpha(th.accent, 90), S(1));
        else StrokeRoundRect(g, card, S(12), th.borderSoft, S(1));

        Txt(g, Fmt(L"%d月", m), S(13), true, todayMonth ? th.accent : th.text,
            RectF(card.X + S(12), card.Y + S(7), S(70), S(20)));
        Txt(g, Fmt(L"%d", monthCount[m]), S(10), false, WithAlpha(th.dim2, 220),
            RectF(RX(card) - S(46), card.Y + S(8), S(34), S(18)), ALIGN_RIGHT);
        AddHit(card.X, card.Y, card.Width, S(28), CMD_YEAR_MONTH_BASE + idx, [m]() {
            app.cursor = MakeDT(app.cursor.y, m, 1);
            app.view = VIEW_MONTH;
            app.selected.clear();
            InvalidateOccs();
            UIInvalidate();
        });
        float gridX = card.X + S(10), gridW = card.Width - S(20);
        float cellW = gridW / 7;
        float headY = card.Y + S(30);
        for (int i = 0; i < 7; ++i) {
            int wd = (ws + i) % 7;
            Txt(g, WeekdayCN(wd), S(7.5f), false, WithAlpha(th.dim2, 200),
                RectF(gridX + i * cellW, headY, cellW, S(12)), ALIGN_CENTER);
        }
        float gridTop = headY + S(14);
        float rowH = (BY(card) - S(8) - gridTop) / 6;
        DateTime first = MakeDT(year, m, 1);
        int firstW = (DayOfWeek(year, m, 1) - ws + 7) % 7;
        int daysInMonth = DaysInMonth(year, m);
        for (int d = 1; d <= daysInMonth; ++d) {
            int cell = firstW + d - 1;
            int r = cell / 7, c = cell % 7;
            if (r >= 6) break;
            RectF cr(gridX + c * cellW, gridTop + r * rowH, cellW, rowH);
            int serial = DaySerial(year, m, d);
            int cnt = dayCount.count(serial) ? dayCount[serial] : 0;
            DateTime day = MakeDT(year, m, d);
            bool isToday = SameDay(day, app.now);
            bool isWeekend = (DayOfWeek(year, m, d) == 0 || DayOfWeek(year, m, d) == 6);
            if (cnt > 0) {
                int a = 26 + std::min(80, cnt * 14);
                FillRoundRect(g, RectF(cr.X + S(1), cr.Y + S(1), cr.Width - S(2), cr.Height - S(2)),
                              S(3), WithAlpha(th.accent, a));
            }
            if (isToday) {
                FillRoundRect(g, RectF(cr.X + S(1), cr.Y + S(1), cr.Width - S(2), cr.Height - S(2)),
                              S(3), WithAlpha(th.accent, 70));
                StrokeRoundRect(g, RectF(cr.X + S(1), cr.Y + S(1), cr.Width - S(2), cr.Height - S(2)),
                                S(3), th.accent, S(1.4f));
            }
            Color dc = isToday ? th.accent : (isWeekend ? WithAlpha(th.accent2, 230) : WithAlpha(th.text, 220));
            Txt(g, Fmt(L"%d", d), S(8), isToday, dc, RectF(cr.X, cr.Y, cr.Width, cr.Height), ALIGN_CENTER | ALIGN_VCENTER);
            std::wstring fest = LunarFestival(day);
            if (!fest.empty()) {
                SolidBrush b(th.accent2);
                g.FillEllipse(&b, RX(cr) - S(6), cr.Y + S(2), S(3), S(3));
            }
            if (examDay.count(serial)) {
                SolidBrush b(th.danger);
                g.FillEllipse(&b, cr.X + S(3), BY(cr) - S(5), S(3), S(3));
            } else if (deadlineDay.count(serial)) {
                SolidBrush b(th.warn);
                g.FillEllipse(&b, cr.X + S(3), BY(cr) - S(5), S(3), S(3));
            }
           int dayOff = DaySerial(day.y, day.mo, day.d) - DaySerial(year, 1, 1);
            AddHit(cr.X, cr.Y, cr.Width, cr.Height, CMD_YEAR_DAY_BASE + dayOff, [day]() {
                app.cursor = day;
                app.view = VIEW_DAY;
                app.selected.clear();
                InvalidateOccs();
                UIInvalidate();
            });
        }

    }

    // 图例
    Txt(g, L"色块越亮 = 当天日程越多 · 右上圆点 = 节日 · 左下红点 = 考试 · 橙点 = 截止",
        S(8.5f), false, WithAlpha(th.dim2, 190), RectF(x, y + h - S(16), w - S(40), S(14)));
    Txt(g, L"点击月份卡片进入月视图 · 点击具体日期进入日视图", S(8.5f), false, WithAlpha(th.dim2, 160),
        RectF(x, y + h - S(30), w - S(40), S(14)));
}