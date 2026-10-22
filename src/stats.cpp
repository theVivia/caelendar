// stats.cpp - 学习统计与复盘（只读）
#include "app.h"
#include <algorithm>
#include <map>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

void Frame(Graphics& g, float W, float H, float pw, float ph, float& px, float& py) {
    const Theme& th = TH();
    pw = std::min(pw, W - S(32));
    ph = std::min(ph, H - S(24));
    px = (W - pw) / 2;
    py = (H - ph) / 2;
    RectF r(px, py, pw, ph);
    Shadow(g, r, S(18), 8, 26);
    FillRoundRect(g, r, S(18), th.card);
    StrokeRoundRect(g, r, S(18), WithAlpha(th.border, 255), S(1));
    FillVGradient(g, RectF(px, py, pw, S(60)), WithAlpha(th.accent, 10), WithAlpha(th.accent, 0));
}

struct TagStat {
    std::wstring name;
    int minutes = 0;
    Color color{ 255, 148, 163, 184 };
};

std::vector<TagStat> TagStats(const DateTime& from, const DateTime& to) {
    std::map<std::string, TagStat> m;
    auto occs = ExpandOccurrences(app.store, from, to);
    for (auto& o : occs) {
        if (!OccMatchesFilter(o) || o.ev.allDay) continue;
        int mins = MinutesBetween(o.start, o.end);
        if (mins <= 0) mins = 30;
        if (mins > 600) continue;
        if (o.ev.tags.empty()) {
            auto& st = m[""];
            st.name = L"未分类";
            st.minutes += mins;
        } else {
            for (auto& tid : o.ev.tags) {
                const Tag* tg = app.store.FindTag(tid);
                auto& st = m[tid];
                st.name = tg ? tg->name : Utf8ToWide(tid);
                if (tg) st.color = Hex(tg->color);
                st.minutes += mins;
            }
        }
    }
    std::vector<TagStat> out;
    for (auto& kv : m) out.push_back(kv.second);
    std::sort(out.begin(), out.end(), [](const TagStat& a, const TagStat& b) { return a.minutes > b.minutes; });
    if (out.size() > 5) out.resize(5);
    return out;
}

bool IsTaskOcc(const Occurrence& o) {
    for (auto& t : o.ev.tags) if (t == "homework" || t == "exam") return true;
    const std::wstring& s = o.ev.title;
    static const wchar_t* keys[] = { L"作业", L"考试", L"截止", L"DDL", L"提交", L"论文", L"报告", L"报名" };
    for (auto* k : keys) if (s.find(k) != std::wstring::npos) return true;
    return false;
}

bool IsExamOcc(const Occurrence& o) {
    for (auto& t : o.ev.tags) if (t == "exam") return true;
    const std::wstring& s = o.ev.title;
    return s.find(L"考试") != std::wstring::npos || s.find(L"期中") != std::wstring::npos ||
           s.find(L"期末") != std::wstring::npos || s.find(L"测验") != std::wstring::npos;
}

std::wstring HoursText(int mins) {
    if (mins <= 0) return L"0h";
    if (mins < 60) return Fmt(L"%dmin", mins);
    return Fmt(L"%dh%02d", mins / 60, mins % 60);
}

int CourseMinutes(const DateTime& from, const DateTime& to) {
    auto occs = ExpandOccurrences(app.store, from, to);
    int total = 0;
    for (auto& o : occs) {
        bool course = false;
        for (auto& t : o.ev.tags) if (t == "course") { course = true; break; }
        if (!course) continue;
        if (o.ev.allDay) continue;
        int m = MinutesBetween(o.start, o.end);
        if (m > 0 && m <= 600) total += m;
    }
    return total;
}

struct TaskStat { int total = 0, done = 0, overdue = 0; int rate() const { return total ? done * 100 / total : 0; } };

TaskStat TaskStatsFor(const DateTime& from, const DateTime& to) {
    TaskStat st;
    auto occs = ExpandOccurrences(app.store, from, to);
    for (auto& o : occs) {
        if (!IsTaskOcc(o)) continue;
        ++st.total;
        if (o.ev.done) ++st.done;
        else if (CompareDT(o.start, app.now) < 0) ++st.overdue;
    }
    return st;
}

struct CourseRow { std::wstring name; int minutes = 0; };

std::vector<CourseRow> CourseRows(const DateTime& from, const DateTime& to) {
    auto occs = ExpandOccurrences(app.store, from, to);
    std::map<std::wstring, int> m;
    for (auto& o : occs) {
        bool course = false;
        for (auto& t : o.ev.tags) if (t == "course") { course = true; break; }
        if (!course || o.ev.allDay) continue;
        int mins = MinutesBetween(o.start, o.end);
        if (mins > 0 && mins <= 600) m[o.ev.title] += mins;
    }
    std::vector<CourseRow> out;
    for (auto& kv : m) out.push_back({ kv.first, kv.second });
    std::sort(out.begin(), out.end(), [](const CourseRow& a, const CourseRow& b) { return a.minutes > b.minutes; });
    if (out.size() > 7) out.resize(7);
    return out;
}

void PeriodRange(int tab, const DateTime& anchor, DateTime& from, DateTime& to, std::wstring& label) {
    DateTime a = anchor;
    if (a.y == 0) a = app.cursor;
    if (tab == 0) {
        from = StartOfWeek(a, app.store.settings.weekStart);
        to = AddDaysDT(from, 7);
        DateTime e = AddDaysDT(from, 6);
        label = Fmt(L"%d-%02d-%02d – %02d-%02d", from.y, from.mo, from.d, e.mo, e.d);
    } else if (tab == 1) {
        from = MakeDT(a.y, a.mo, 1);
        to = AddMonthsDT(from, 1);
        label = Fmt(L"%d 年 %d 月", a.y, a.mo);
    } else {
        from = MakeDT(a.y, 1, 1);
        to = MakeDT(a.y + 1, 1, 1);
        label = Fmt(L"%d 年", a.y);
    }
}

void PrevNextAnchor(int tab, DateTime& a, int dir) {
    if (tab == 0) a = AddDaysDT(a, dir * 7);
    else if (tab == 1) a = AddMonthsDT(a, dir);
    else a = MakeDT(a.y + dir, a.mo, 1);
}

int BusyBuckets(const DateTime& from, const DateTime& to, int out[6]) {
    for (int i = 0; i < 6; ++i) out[i] = 0;
    auto occs = ExpandOccurrences(app.store, from, to);
    int maxV = 0;
    for (auto& o : occs) {
        if (o.ev.allDay) continue;
        int b = std::max(0, std::min(5, o.start.h / 4));
        ++out[b];
        maxV = std::max(maxV, out[b]);
    }
    return maxV;
}

int ExamCount(const DateTime& from, const DateTime& to) {
    auto occs = ExpandOccurrences(app.store, from, to);
    int n = 0;
    for (auto& o : occs) if (IsExamOcc(o)) ++n;
    return n;
}

int DoneStreak() {
    int streak = 0;
    for (int back = 0; back < 90; ++back) {
        DateTime day = AddDaysDT(StartOfDay(app.now), -back);
        auto occs = ExpandOccurrences(app.store, day, AddDaysDT(day, 1));
        bool any = false;
        for (auto& o : occs) if (IsTaskOcc(o) && o.ev.done) { any = true; break; }
        if (any) ++streak;
        else break;
    }
    return streak;
}

void Kpi(Graphics& g, RectF r, const wchar_t* title, const std::wstring& value, Color color) {
    const Theme& th = TH();
    FillRoundRect(g, r, S(10), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, r, S(10), th.borderSoft, S(1));
    Txt(g, title, S(9), false, th.dim2, RectF(r.X + S(12), r.Y + S(8), r.Width - S(24), S(16)));
    Txt(g, value, S(20), true, color, RectF(r.X + S(12), r.Y + S(26), r.Width - S(24), S(30)), ALIGN_LEFT, true);
}

}  // namespace

void OpenStats(int tab, const DateTime& anchor) {
    if (tab < 0) tab = 1;
    app.statsTab = tab;
    app.statsAnchor = (anchor.y != 0) ? anchor : app.cursor;
    app.overlay = OV_STATS;
    UIInvalidate();
}

void DrawStatsOverlay(Graphics& g, float W, float H) {
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    const Theme& th = TH();
    float pw = S(880), ph = S(680);
    float x, y;
    Frame(g, W, H, pw, ph, x, y);
    Txt(g, L"学习统计", S(18), true, th.text, RectF(x + S(24), y + S(16), S(200), S(28)));
    Txt(g, L"只读统计 · 不影响日程数据", S(9), false, th.dim2, RectF(x + S(120), y + S(22), S(220), S(18)));
    Txt(g, Fmt(L"本周专注 %d 分钟 · %d 次", FocusWeekMinutes(), FocusWeekCount()), S(9), false, th.accent2,
        RectF(x + S(350), y + S(22), S(280), S(18)), ALIGN_LEFT, true);
    AddHit(x + S(350), y + S(18), S(260), S(24), 0, []() { OpenFocusPanel(); });
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_STATS_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    const wchar_t* tabs[3] = { L"周", L"月", L"年" };
    for (int i = 0; i < 3; ++i) {
        RectF r(x + S(24) + i * S(58), y + S(54), S(52), S(26));
        UI::Chip(g, r, CMD_STATS_TAB_BASE + i, tabs[i], app.statsTab == i, th.accent, [i]() {
            app.statsTab = i;
            UIInvalidate();
        }, 10);
    }
    DateTime from, to;
    std::wstring label;
    PeriodRange(app.statsTab, app.statsAnchor, from, to, label);
    Txt(g, label, S(12), true, th.text, RectF(x + S(220), y + S(56), S(300), S(22)), ALIGN_LEFT, true);
    UI::IconBtn(g, RectF(x + pw - S(150), y + S(52), S(28), S(28)), CMD_STATS_PREV, ICO_CHEV_LEFT, 12, []() {
        PrevNextAnchor(app.statsTab, app.statsAnchor, -1);
        UIInvalidate();
    });
    UI::IconBtn(g, RectF(x + pw - S(116), y + S(52), S(28), S(28)), CMD_STATS_NEXT, ICO_CHEV_RIGHT, 12, []() {
        PrevNextAnchor(app.statsTab, app.statsAnchor, 1);
        UIInvalidate();
    });

    float contentY = y + S(96);
    if (app.statsTab == 2) {
        // 年视图联动：12 个月卡片
        float gap = S(10);
        float cw = (pw - S(48) - gap * 3) / 4;
        float ch = S(122);
        for (int m = 1; m <= 12; ++m) {
            DateTime mf = MakeDT(app.statsAnchor.y, m, 1);
            DateTime mt = AddMonthsDT(mf, 1);
            int mins = CourseMinutes(mf, mt);
            TaskStat ts = TaskStatsFor(mf, mt);
            int col = (m - 1) % 4, row = (m - 1) / 4;
            RectF card(x + S(24) + col * (cw + gap), contentY + row * (ch + gap), cw, ch);
            bool current = (app.now.y == app.statsAnchor.y && app.now.mo == m);
            FillRoundRect(g, card, S(10), WithAlpha(current ? th.accent : th.card, current ? 20 : 160));
            StrokeRoundRect(g, card, S(10), current ? WithAlpha(th.accent, 140) : th.borderSoft, S(1));
            Txt(g, Fmt(L"%d 月", m), S(13), true, current ? th.accent : th.text, RectF(card.X + S(10), card.Y + S(8), S(80), S(20)));
            Txt(g, Fmt(L"课程 %s", HoursText(mins).c_str()), S(9.5f), false, th.dim, RectF(card.X + S(10), card.Y + S(34), cw - S(20), S(16)));
            Txt(g, Fmt(L"DDL %d/%d", ts.done, ts.total), S(9.5f), false, ts.overdue ? th.danger : th.dim,
                RectF(card.X + S(10), card.Y + S(53), cw - S(20), S(16)));
            float barW = card.Width - S(20);
            FillRoundRect(g, RectF(card.X + S(10), card.Y + S(78), barW, S(6)), S(3), WithAlpha(th.borderSoft, 200));
            if (ts.rate() > 0)
                FillRoundRect(g, RectF(card.X + S(10), card.Y + S(78), barW * ts.rate() / 100, S(6)), S(3), th.success);
            Txt(g, Fmt(L"完成率 %d%%", ts.rate()), S(8.5f), false, th.dim2, RectF(card.X + S(10), card.Y + S(88), cw - S(20), S(14)));
            AddHit(card.X, card.Y, card.Width, card.Height, CMD_STATS_MONTH_BASE + (m - 1), [mf]() {
                app.statsTab = 1;
                app.statsAnchor = mf;
                UIInvalidate();
            });
        }
    } else {
        int courseMin = CourseMinutes(from, to);
        TaskStat tasks = TaskStatsFor(from, to);
        int exams = ExamCount(from, to);
        int streak = DoneStreak();
        float kpiW = (pw - S(48) - S(12) * 3) / 4;
        RectF k1(x + S(24), contentY, kpiW, S(66));
        RectF k2(x + S(24) + (kpiW + S(12)), contentY, kpiW, S(66));
        RectF k3(x + S(24) + (kpiW + S(12)) * 2, contentY, kpiW, S(66));
        RectF k4(x + S(24) + (kpiW + S(12)) * 3, contentY, kpiW, S(66));
        Kpi(g, k1, L"课程投入", HoursText(courseMin), th.accent);
        Kpi(g, k2, L"DDL 完成率", Fmt(L"%d%%（%d/%d）", tasks.rate(), tasks.done, tasks.total), tasks.rate() >= 60 ? th.success : th.warn);
        Kpi(g, k3, L"考试", Fmt(L"%d 场", exams), th.danger);
        Kpi(g, k4, L"连续完成", Fmt(L"%d 天", streak), th.accent2);

        float listY = contentY + S(88);
        // 左：每门课投入
        RectF left(x + S(24), listY, (pw - S(48)) * 0.52f, ph - (listY - y) - S(34));
        FillRoundRect(g, left, S(10), WithAlpha(th.panel2, 150));
        StrokeRoundRect(g, left, S(10), th.borderSoft, S(1));
        Txt(g, L"每门课投入", S(11), true, th.accent, RectF(left.X + S(12), left.Y + S(8), S(140), S(18)));
        auto rows = CourseRows(from, to);
        int maxMin = rows.empty() ? 0 : rows[0].minutes;
        float rowY = left.Y + S(34);
        int shownCourses = std::min((int)rows.size(), 5);
        for (int i = 0; i < shownCourses; ++i) {
            RectF row(left.X + S(12), rowY + i * S(38), left.Width - S(24), S(30));
            Txt(g, rows[i].name, S(9.5f), false, th.text, RectF(row.X, row.Y, row.Width - S(70), S(16)), ALIGN_LEFT, false, true);
            Txt(g, HoursText(rows[i].minutes), S(9), false, th.dim2, RectF(RX(row) - S(70), row.Y, S(66), S(16)), ALIGN_RIGHT);
            float barW = row.Width;
            FillRoundRect(g, RectF(row.X, row.Y + S(20), barW, S(5)), S(2.5f), WithAlpha(th.borderSoft, 180));
            if (maxMin > 0)
                FillRoundRect(g, RectF(row.X, row.Y + S(20), barW * rows[i].minutes / maxMin, S(5)), S(2.5f), th.accent);
        }
        if (rows.empty()) Txt(g, L"这段时间没有课程数据", S(9.5f), false, th.dim2, RectF(left.X + S(12), left.Y + S(40), left.Width - S(24), S(20)));

        // 按标签投入（标签颜色条形图）
        float tagTop = left.Y + S(34) + S(38) * shownCourses + S(10);
        Txt(g, L"按标签投入", S(11), true, th.accent, RectF(left.X + S(12), tagTop, S(160), S(18)));
        auto tags = TagStats(from, to);
        int tagMax = tags.empty() ? 0 : tags[0].minutes;
        for (size_t i = 0; i < tags.size(); ++i) {
            float ty = tagTop + S(26) + i * S(28);
            Color tc = tags[i].color;
            Txt(g, tags[i].name, S(9), false, th.text, RectF(left.X + S(12), ty, left.Width - S(90), S(14)), ALIGN_LEFT, false, true);
            Txt(g, HoursText(tags[i].minutes), S(8.5f), false, th.dim2, RectF(left.X + left.Width - S(78), ty, S(66), S(14)), ALIGN_RIGHT);
            float bw2 = left.Width - S(24);
            FillRoundRect(g, RectF(left.X + S(12), ty + S(16), bw2, S(4)), S(2), WithAlpha(th.borderSoft, 180));
            if (tagMax > 0)
                FillRoundRect(g, RectF(left.X + S(12), ty + S(16), bw2 * tags[i].minutes / tagMax, S(4)), S(2), WithAlpha(tc, 220));
        }
        if (tags.empty()) Txt(g, L"这段时间没有标签数据", S(9), false, th.dim2, RectF(left.X + S(12), tagTop + S(26), left.Width - S(24), S(18)));

        // 右：最忙时段 + DDL 概览
        RectF right(left.X + left.Width + S(14), listY, pw - S(48) - left.Width - S(14), left.Height);
        FillRoundRect(g, right, S(10), WithAlpha(th.panel2, 150));
        StrokeRoundRect(g, right, S(10), th.borderSoft, S(1));
        Txt(g, L"最忙时段", S(11), true, th.accent, RectF(right.X + S(12), right.Y + S(8), S(140), S(18)));
        int buckets[6] = { 0 };
        int maxV = BusyBuckets(from, to, buckets);
        static const wchar_t* bnames[6] = { L"0-4", L"4-8", L"8-12", L"12-16", L"16-20", L"20-24" };
        float bw = (right.Width - S(24) - S(5) * 8) / 6;
        float baseY = right.Y + S(150);
        for (int i = 0; i < 6; ++i) {
            float h = maxV > 0 ? (float)buckets[i] / maxV * S(80) : 0;
            RectF bar(right.X + S(12) + i * (bw + S(8)), baseY - h, bw, h);
            FillRoundRect(g, bar, S(3), WithAlpha(th.accent2, 190));
            Txt(g, Fmt(L"%d", buckets[i]), S(8.5f), false, th.dim2, RectF(bar.X, bar.Y - S(16), bw, S(14)), ALIGN_CENTER);
            Txt(g, bnames[i], S(8), false, th.dim2, RectF(bar.X, baseY + S(4), bw, S(14)), ALIGN_CENTER);
        }
        Txt(g, L"DDL 概览", S(11), true, th.accent, RectF(right.X + S(12), right.Y + S(190), S(140), S(18)));
        Txt(g, Fmt(L"共 %d 项 · 完成 %d · 逾期 %d", tasks.total, tasks.done, tasks.overdue), S(10), false, th.dim,
            RectF(right.X + S(12), right.Y + S(214), right.Width - S(24), S(18)));
        Txt(g, L"连续完成天数从今天往前计算；DDL 按截止日期落在当前周期统计。", S(8.5f), false, WithAlpha(th.dim2, 190),
            RectF(right.X + S(12), right.Y + S(236), right.Width - S(24), S(28)), ALIGN_LEFT, false, true);
    }
}
