// daydetail.cpp - 某一天详情（右键日期打开）
#include "app.h"
#include <algorithm>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

static void Frame(Graphics& g, float W, float H, float pw, float ph, float& px, float& py) {
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

void OpenDayDetail(const DateTime& day) {
    app.dayDetail = StartOfDay(day);
    app.dayDetailOccs = ExpandOccurrences(app.store, app.dayDetail, AddDaysDT(app.dayDetail, 1));
    std::sort(app.dayDetailOccs.begin(), app.dayDetailOccs.end(), [](const Occurrence& a, const Occurrence& b) {
        return CompareDT(a.start, b.start) < 0;
    });
    app.overlay = OV_DAYDETAIL;
    UIInvalidate();
}

static void RefreshDayDetail() {
    app.dayDetailOccs = ExpandOccurrences(app.store, app.dayDetail, AddDaysDT(app.dayDetail, 1));
    std::sort(app.dayDetailOccs.begin(), app.dayDetailOccs.end(), [](const Occurrence& a, const Occurrence& b) {
        return CompareDT(a.start, b.start) < 0;
    });
}

void DrawDayDetailOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(660), ph = S(620);
    float x, y;
    Frame(g, W, H, pw, ph, x, y);
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(16), S(28), S(28)), CMD_DAYDETAIL_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    const DateTime& d = app.dayDetail;
    bool isToday = SameDay(d, Today());
    Txt(g, Fmt(L"%d 年 %d 月 %d 日", d.y, d.mo, d.d), S(22), true, th.text,
        RectF(x + S(26), y + S(16), pw - S(190), S(34)), ALIGN_LEFT, true);
    Txt(g, WeekdayCN(DayOfWeek(d.y, d.mo, d.d)), S(13), false, th.dim,
        RectF(x + S(26), y + S(50), S(80), S(22)));
    if (isToday) {
        RectF badge(x + pw - S(120), y + S(22), S(48), S(20));
        FillRoundRect(g, badge, S(10), th.accent);
        Txt(g, L"今天", S(9), true, th.onAccent, badge, ALIGN_CENTER);
    }

    // 农历 / 节气 / 节日
    std::wstring lunar = LunarFullText(d);
    std::wstring term = SolarTermNameOn(d);
    auto holidays = HolidaysOn(d);
    std::wstring info = lunar.empty() ? L"" : lunar;
    if (!term.empty()) info += (info.empty() ? L"" : L" · ") + term;
    for (auto& h : holidays) info += (info.empty() ? L"" : L" · ") + h.name + L"（" + h.category + L"）";
    if (!info.empty())
        Txt(g, info, S(11), false, th.accent, RectF(x + S(26), y + S(78), pw - S(52), S(20)), ALIGN_LEFT, false, true);

    float listY = y + S(136);
    int done = 0;
    for (auto& o : app.dayDetailOccs) if (o.ev.done) ++done;
    Txt(g, Fmt(L"当天 %d 项日程 · 已完成 %d 项 · 左键点开编辑，勾选圆点可完成", (int)app.dayDetailOccs.size(), done),
        S(9.5f), false, th.dim2, RectF(x + S(26), listY - S(24), pw - S(52), S(18)));
    Line(g, x + S(24), listY - S(2), x + pw - S(24), listY - S(2), th.borderSoft, S(1));

    float rowH = S(44);
    int maxRows = (int)((ph - S(260)) / rowH);
    maxRows = std::max(1, maxRows);
    int shown = 0;
    for (int i = 0; i < (int)app.dayDetailOccs.size() && shown < maxRows; ++i) {
        const Occurrence& o = app.dayDetailOccs[i];
        float ry = listY + shown * rowH;
        bool hover = false;
        RectF row(x + S(24), ry, pw - S(48), rowH - S(4));
        FillRoundRect(g, row, S(8), hover ? th.cardHover : WithAlpha(th.card, 150));
        // 完成圆点
        RectF dot(row.X + S(12), row.Y + S(11), S(18), S(18));
        FillCircle(g, dot.X + S(9), dot.Y + S(9), S(9), o.ev.done ? th.success : WithAlpha(th.dim2, 80));
        if (o.ev.done) Icon(g, ICO_CHECK, RectF(dot.X + S(3), dot.Y + S(3), S(12), S(12)), th.onAccent, 1.5f);
        std::wstring time = EventTimeRangeText(o.start, o.end, o.ev.allDay);
        Txt(g, time, S(10), false, th.dim2, RectF(row.X + S(38), row.Y, S(104), rowH - S(4)), ALIGN_VCENTER);
        Txt(g, o.ev.title, S(11.5f), o.ev.priority == PRIO_HIGH, o.ev.done ? WithAlpha(th.dim, 220) : th.text,
            RectF(row.X + S(148), row.Y, row.Width - S(170), rowH - S(4)), ALIGN_VCENTER, false, true);
        if (!o.ev.location.empty())
            Txt(g, o.ev.location, S(9), false, th.dim2, RectF(row.X + row.Width - S(150), row.Y, S(130), rowH - S(4)), ALIGN_RIGHT | ALIGN_VCENTER);
        AddHit(row.X, row.Y, row.Width, row.Height, CMD_DAYDETAIL_CLOSE + 100 + i, [i]() {
            if (i < (int)app.dayDetailOccs.size()) OpenEditorForOccurrence(app.dayDetailOccs[i]);
        });
        AddHit(dot.X, dot.Y, S(24), S(24), CMD_DAYDETAIL_CLOSE + 300 + i, [i]() {
            if (i >= (int)app.dayDetailOccs.size()) return;
            const Occurrence& oc = app.dayDetailOccs[i];
            app.store.Snapshot();
            app.store.SetOccurrenceDone(oc.seriesId, oc.origStart, oc.start, oc.end, !oc.ev.done);
            DoSaveStore();
            InvalidateOccs();
            RefreshDayDetail();
            UIInvalidate();
        });
        ++shown;
    }
    if (app.dayDetailOccs.empty())
        Txt(g, L"这天还没有日程，可以直接新建。", S(11), false, th.dim2,
            RectF(x + S(26), listY + S(30), pw - S(52), S(24)), ALIGN_CENTER);
    if ((int)app.dayDetailOccs.size() > shown)
        Txt(g, Fmt(L"还有 %d 项未显示", (int)app.dayDetailOccs.size() - shown), S(9), false, th.dim2,
            RectF(x + S(26), listY + maxRows * rowH + S(2), pw - S(52), S(16)));

    float by = y + ph - S(58);
    UI::Btn(g, RectF(x + S(24), by, S(104), S(34)), CMD_DD_NEW, L"新建日程", 1, ICO_PLUS, [d]() {
        app.overlay = OV_NONE;
        OpenEditorNew(d, 9, 0);
    });
    UI::Btn(g, RectF(x + S(136), by, S(104), S(34)), CMD_DD_QUICK, L"快速添加", 1, ICO_EDIT, []() {
        app.overlay = OV_NONE;
        OpenQuickAdd();
    });
    UI::Btn(g, RectF(x + S(248), by, S(104), S(34)), CMD_DD_DAYVIEW, L"日视图", 1, ICO_CAL, [d]() {
        app.cursor = d;
        app.view = VIEW_DAY;
        app.overlay = OV_NONE;
        InvalidateOccs();
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90), by, S(90), S(34)), CMD_DAYDETAIL_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
