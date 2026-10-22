// exampanel.cpp - 考试倒计时面板（只读）
#include "app.h"
#include <algorithm>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

bool IsExamLike(const Occurrence& o) {
    for (auto& t : o.ev.tags) if (t == "exam") return true;
    const std::wstring& s = o.ev.title;
    return s.find(L"考试") != std::wstring::npos || s.find(L"期中") != std::wstring::npos ||
           s.find(L"期末") != std::wstring::npos || s.find(L"测验") != std::wstring::npos ||
           s.find(L"补考") != std::wstring::npos || s.find(L"quiz") != std::wstring::npos;
}

struct ExamItem {
    Occurrence occ;
    int days = 0;
};

std::vector<ExamItem> CollectExams() {
    std::vector<ExamItem> out;
    DateTime today = StartOfDay(Today());
    auto occs = ExpandOccurrences(app.store, today, AddDaysDT(today, 400));
    int todaySerial = DaySerial(today.y, today.mo, today.d);
    for (auto& o : occs) {
        if (o.ev.done || o.ev.status == ST_CANCELLED) continue;
        if (!IsExamLike(o)) continue;
        ExamItem it;
        it.occ = o;
        it.days = DaySerial(o.start.y, o.start.mo, o.start.d) - todaySerial;
        out.push_back(it);
    }
    std::sort(out.begin(), out.end(), [](const ExamItem& a, const ExamItem& b) {
        return CompareDT(a.occ.start, b.occ.start) < 0;
    });
    if (out.size() > 50) out.resize(50);
    return out;
}

std::wstring CountdownText(int days) {
    if (days < 0) return L"已过";
    if (days == 0) return L"今天";
    if (days == 1) return L"明天";
    return Fmt(L"%d 天", days);
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

}  // namespace

int ExamPanelProbe(std::wstring& firstTitle, int& days) {
    auto v = CollectExams();
    if (!v.empty()) { firstTitle = v[0].occ.ev.title; days = v[0].days; }
    return (int)v.size();
}

void OpenExamPanel() {
    app.overlay = OV_EXAMS;
    UIInvalidate();
}

void DrawExamPanelOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(620);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    FillVGradient(g, RectF(x, y, pw, S(64)), WithAlpha(th.danger, 10), WithAlpha(th.danger, 0));
    auto exams = CollectExams();

    Txt(g, L"考试倒计时", S(17), true, th.text, RectF(x + S(24), y + S(16), S(300), S(26)));
    Txt(g, exams.empty() ? L"未来一年没有找到考试类日程（标题含考试/期中/期末/测验，或带「考试」标签）"
                         : Fmt(L"未来一年 %d 场考试 · 红色为最近一场", (int)exams.size()),
        S(9), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    float cy = y + S(78);
    if (!exams.empty()) {
        auto& nx = exams[0];
        RectF hero(x + S(24), cy, pw - S(48), S(150));
        FillRoundRect(g, hero, S(12), WithAlpha(th.danger, 14));
        StrokeRoundRect(g, hero, S(12), WithAlpha(th.danger, 90), S(1));
        FillRoundRect(g, RectF(hero.X, hero.Y + S(14), S(4), hero.Height - S(28)), S(2), th.danger);
        Txt(g, nx.occ.ev.title, S(18), true, th.text, RectF(hero.X + S(20), hero.Y + S(16), hero.Width - S(190), S(28)), ALIGN_LEFT, false, true);
        Txt(g, Fmt(L"%s · %s", DateTimeHuman(nx.occ.start).c_str(),
                   nx.occ.ev.allDay ? L"全天" : Fmt(L"%s – %s", TimeStr(nx.occ.start).c_str(), TimeStr(nx.occ.end).c_str()).c_str()),
            S(11), false, th.accent, RectF(hero.X + S(20), hero.Y + S(50), hero.Width - S(190), S(20)));
        std::wstring loc = nx.occ.ev.location.empty() ? L"地点未填写" : nx.occ.ev.location;
        Txt(g, L"地点：" + loc, S(10), false, th.dim, RectF(hero.X + S(20), hero.Y + S(76), hero.Width - S(190), S(18)));
        int studyDays = 0;
        for (auto& e : app.store.events) if (e.tags.end() != std::find(e.tags.begin(), e.tags.end(), std::string("homework"))) ++studyDays;
        Txt(g, Fmt(L"还有 %d 个作业/DDL 系列未完成 · 建议先滚动复习", studyDays), S(9), false, th.dim2,
            RectF(hero.X + S(20), hero.Y + S(102), hero.Width - S(190), S(18)), ALIGN_LEFT, false, true);
        Txt(g, CountdownText(nx.days), S(34), true, th.danger,
            RectF(hero.X + S(200), hero.Y + S(22), hero.Width - S(220), S(46)), ALIGN_RIGHT, true);
        Txt(g, Fmt(L"%02d:%02d 开始", nx.occ.start.h, nx.occ.start.mi), S(10), false, th.dim,
            RectF(hero.X + S(200), hero.Y + S(78), hero.Width - S(220), S(18)), ALIGN_RIGHT);
        AddHit(hero.X, hero.Y, hero.Width, hero.Height, 0, [nx]() {
            app.cursor = StartOfDay(nx.occ.start);
            app.overlay = OV_NONE;
            InvalidateOccs();
            OpenEditorExisting(nx.occ.seriesId, nx.occ.origStart);
        });
    } else {
        RectF empty(x + S(24), cy, pw - S(48), S(150));
        FillRoundRect(g, empty, S(12), WithAlpha(th.panel2, 150));
        StrokeRoundRect(g, empty, S(12), th.borderSoft, S(1));
        Txt(g, L"还没有考试日程", S(15), true, th.dim, RectF(empty.X + S(20), empty.Y + S(28), empty.Width - S(40), S(24)));
        Txt(g, L"新建日程时把标签选为「考试」，或标题写「期中/期末/考试」，就会出现在这里。",
            S(9.5f), false, th.dim2, RectF(empty.X + S(20), empty.Y + S(62), empty.Width - S(40), S(36)), ALIGN_LEFT, false, true);
        UI::Btn(g, RectF(empty.X + S(20), empty.Y + S(104), S(120), S(30)), CMD_IMPORT_OK, L"新建考试日程", 0, ICO_PLUS, []() {
            app.overlay = OV_NONE;
            OpenEditorNew(Today(), 9, 0);
        });
    }

    // 列表
    float listY = cy + S(170);
    Txt(g, exams.size() > 1 ? L"接下来" : L"没有更多考试", S(10.5f), false, th.dim2, RectF(x + S(24), listY, S(200), S(18)));
    listY += S(24);
    int shown = 0;
    for (size_t i = 1; i < exams.size() && shown < 6; ++i, ++shown) {
        auto& e = exams[i];
        RectF row(x + S(24), listY + shown * S(46), pw - S(48), S(40));
        FillRoundRect(g, row, S(8), WithAlpha(th.panel2, 150));
        Txt(g, Fmt(L"%02d/%02d", e.occ.start.mo, e.occ.start.d), S(11), true, th.text,
            RectF(row.X + S(12), row.Y, S(50), row.Height), ALIGN_VCENTER);
        Txt(g, WeekdayCN(DayOfWeek(e.occ.start.y, e.occ.start.mo, e.occ.start.d)), S(8.5f), false, th.dim2,
            RectF(row.X + S(62), row.Y, S(44), row.Height), ALIGN_VCENTER);
        Txt(g, e.occ.ev.title, S(10.5f), true, th.text, RectF(row.X + S(108), row.Y, row.Width - S(280), S(24)), ALIGN_VCENTER, false, true);
        std::wstring loc = e.occ.ev.location.empty() ? L"" : L" · " + e.occ.ev.location;
        Txt(g, TimeStr(e.occ.start) + loc, S(8.5f), false, th.dim2, RectF(row.X + S(108), row.Y + S(21), row.Width - S(280), S(16)), ALIGN_LEFT, false, true);
        Txt(g, CountdownText(e.days), S(10), true, e.days <= 7 ? th.danger : th.dim,
            RectF(RX(row) - S(100), row.Y, S(88), row.Height), ALIGN_RIGHT | ALIGN_VCENTER);
        AddHit(row.X, row.Y, row.Width, row.Height, 0, [e]() {
            app.cursor = StartOfDay(e.occ.start);
            app.overlay = OV_NONE;
            InvalidateOccs();
            OpenEditorExisting(e.occ.seriesId, e.occ.origStart);
        });
    }
    if (exams.size() <= 1) {
        Txt(g, L"更远的考试可以到月/年视图查看；这里只读，不修改数据。", S(9), false, th.dim2,
            RectF(x + S(24), listY + S(30), pw - S(48), S(18)));
    }
    Txt(g, L"点击任意考试可打开编辑器；数据与日历完全同步。", S(8.5f), false, th.dim2,
        RectF(x + S(24), y + ph - S(88), pw - S(48), S(18)));
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}