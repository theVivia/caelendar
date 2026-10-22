// board.cpp - 日程视图里的 DDL / 作业看板
#include "app.h"
#include <algorithm>
#include <map>

using namespace Gdiplus;
using namespace gfx;

namespace {

const Theme& th() { return TH(); }

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

bool IsTask(const Occurrence& o) {
    if (o.ev.ddl) return true;
    for (auto& t : o.ev.tags) {
        const Tag* tag = app.store.FindTag(t);
        if (tag && tag->task) return true;
    }
    const std::wstring& s = o.ev.title;
    static const wchar_t* keys[] = { L"作业", L"考试", L"截止", L"DDL", L"提交", L"论文", L"报告", L"报名" };
    for (auto* k : keys) if (s.find(k) != std::wstring::npos) return true;
    return false;
}

bool IsHomework(const Occurrence& o) {
    for (auto& t : o.ev.tags) if (t == "homework") return true;
    const std::wstring& s = o.ev.title;
    return s.find(L"作业") != std::wstring::npos || s.find(L"提交") != std::wstring::npos ||
           s.find(L"论文") != std::wstring::npos || s.find(L"报告") != std::wstring::npos ||
           s.find(L"截止") != std::wstring::npos || s.find(L"DDL") != std::wstring::npos;
}

bool IsExam(const Occurrence& o) {
    for (auto& t : o.ev.tags) if (t == "exam") return true;
    const std::wstring& s = o.ev.title;
    return s.find(L"考试") != std::wstring::npos || s.find(L"期中") != std::wstring::npos ||
           s.find(L"期末") != std::wstring::npos || s.find(L"测验") != std::wstring::npos;
}

std::vector<std::wstring> CourseNames() {
    std::vector<std::wstring> out;
    for (auto& e : app.store.events) {
        bool isCourse = false;
        for (auto& t : e.tags) if (t == "course") { isCourse = true; break; }
        if (!isCourse) continue;
        if (std::find(out.begin(), out.end(), e.title) == out.end()) out.push_back(e.title);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// 从 DDL 标题猜课程：常见简称映射 + 包含匹配
std::wstring CourseOf(const std::wstring& title, const std::vector<std::wstring>& courses) {
    for (auto& c : courses) if (title.find(c) != std::wstring::npos) return c;
    static const std::pair<const wchar_t*, const wchar_t*> aliases[] = {
        { L"高数", L"高等数学" }, { L"大英", L"大学英语" }, { L"英语", L"大学英语" },
        { L"物理", L"大学物理" }, { L"电路", L"电路实验" }, { L"程序", L"程序设计" },
        { L"思政", L"思想道德与法治" }, { L"史纲", L"中国近现代史纲要" },
        { L"心理", L"大学生心理健康" }, { L"体育", L"体育" },
    };
    for (auto& a : aliases) {
        if (title.find(a.first) == std::wstring::npos) continue;
        for (auto& c : courses) if (c.find(a.second) != std::wstring::npos) return c;
    }
    return L"";
}

struct TaskItem {
    Occurrence occ;
    std::wstring course;
    int bucket = 4;      // 0 逾期 1 今天 2 本周 3 以后 4 已完成
};

std::wstring DueText(const Occurrence& o) {
    if (o.ev.allDay) return Fmt(L"%d-%02d-%02d", o.start.y, o.start.mo, o.start.d);
    return Fmt(L"%d-%02d-%02d %02d:%02d", o.start.y, o.start.mo, o.start.d, o.start.h, o.start.mi);
}

void ToggleDone(const Occurrence& o, bool done) {
    app.store.Snapshot();
    app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, done);
    DoSaveStore();
    InvalidateOccs();
    ShowToast(done ? L"已标记完成" : L"已标记未完成");
    UIInvalidate();
}

void ApplyBulk(bool done) {
    if (app.boardSelected.empty()) return;
    DateTime from = AddDaysDT(StartOfDay(app.now), -365);
    DateTime to = AddDaysDT(StartOfDay(app.now), 365);
    auto all = ExpandOccurrences(app.store, from, to);
    std::vector<Occurrence> hit;
    for (auto& o : all) {
        if (!app.boardAllEvents && !IsTask(o)) continue;
        if (app.boardSelected.count(OccKey(o))) hit.push_back(o);
    }
    if (hit.empty()) return;
    app.store.Snapshot();
    for (auto& o : hit) app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, done);
    DoSaveStore();
    InvalidateOccs();
    app.boardSelected.clear();
    ShowToast(Fmt(L"已更新 %d 项", (int)hit.size()));
    UIInvalidate();
}

void FilterChip(Graphics& g, RectF r, int cmd, const wchar_t* label, bool active) {
    const Theme& t = th();
    FillRoundRect(g, r, S(7), active ? WithAlpha(t.accent, 30) : WithAlpha(t.card, 180));
    StrokeRoundRect(g, r, S(7), active ? WithAlpha(t.accent, 150) : t.borderSoft, S(1));
    Txt(g, label, S(9.5f), active, active ? t.accent : t.dim, r, ALIGN_CENTER | ALIGN_VCENTER);
    AddHit(r.X, r.Y, r.Width, r.Height, cmd, [cmd]() {
        if (cmd >= CMD_BOARD_FILTER_BASE && cmd < CMD_BOARD_FILTER_BASE + 4) app.boardFilter = cmd - CMD_BOARD_FILTER_BASE;
        UIInvalidate();
    });
}

}  // namespace

void DrawAgendaBoard(Graphics& g, float x, float y, float w, float h) {
    gfx::FontPartScope fontPart(gfx::FP_LIST);
    const Theme& t = th();
    auto courses = CourseNames();

    // ---- 收集任务 ----
    DateTime from = AddDaysDT(StartOfDay(app.now), -365);
    DateTime to = AddDaysDT(StartOfDay(app.now), 365);
    auto all = ExpandOccurrences(app.store, from, to);
    std::vector<TaskItem> items;
    for (auto& o : all) {
        if (!app.boardAllEvents && !IsTask(o)) continue;
        if (app.boardFilter == 1 && !IsHomework(o)) continue;
        if (app.boardFilter == 2 && !IsExam(o)) continue;
        if (!app.boardCourseFilter.empty()) {
            std::wstring c = CourseOf(o.ev.title, courses);
            if (c != Utf8ToWide(app.boardCourseFilter)) continue;
        }
        if (!app.tagFilter.empty()) {
            bool hit = false;
            for (auto& tg : o.ev.tags) if (app.tagFilter.count(tg)) { hit = true; break; }
            if (!hit) continue;
        }
        TaskItem it;
        it.occ = o;
        it.course = CourseOf(o.ev.title, courses);
        int today = DaySerial(app.now.y, app.now.mo, app.now.d);
        int due = DaySerial(o.start.y, o.start.mo, o.start.d);
        int weekStart = DaySerial(app.now.y, app.now.mo, app.now.d) - ((DayOfWeek(app.now.y, app.now.mo, app.now.d) - app.store.settings.weekStart + 7) % 7);
        if (o.ev.done) it.bucket = 4;
        else if (due < today) it.bucket = 0;
        else if (due == today) it.bucket = 1;
        else if (due < weekStart + 7) it.bucket = 2;
        else it.bucket = 3;
        items.push_back(it);
    }
    std::sort(items.begin(), items.end(), [](const TaskItem& a, const TaskItem& b) {
        if (a.bucket != b.bucket) return a.bucket < b.bucket;
        return CompareDT(a.occ.start, b.occ.start) < 0;
    });

    // ---- 过滤 + 统计 ----
    int overdue = 0, thisWeek = 0, doneCount = 0;
    DateTime weekEnd = AddDaysDT(StartOfDay(app.now), 7);
    for (auto& it : items) {
        if (it.bucket == 0) ++overdue;
        if ((it.bucket == 1 || it.bucket == 2)) ++thisWeek;
        if (it.bucket == 4) ++doneCount;
    }
    int total = (int)items.size();
    int rate = total > 0 ? doneCount * 100 / total : 0;

    // 模式切换在 DrawAgendaView 里；这里画过滤条
    float cy = y;
    FilterChip(g, RectF(x, cy, S(56), S(28)), CMD_BOARD_FILTER_BASE + 0, L"全部", app.boardFilter == 0);
    FilterChip(g, RectF(x + S(62), cy, S(56), S(28)), CMD_BOARD_FILTER_BASE + 1, L"作业", app.boardFilter == 1);
    FilterChip(g, RectF(x + S(124), cy, S(56), S(28)), CMD_BOARD_FILTER_BASE + 2, L"考试", app.boardFilter == 2);
    {
        bool on = app.boardAllEvents;
        RectF r(x + S(386), cy, S(96), S(28));
        FillRoundRect(g, r, S(7), on ? WithAlpha(t.accent, 30) : WithAlpha(t.card, 180));
        StrokeRoundRect(g, r, S(7), on ? WithAlpha(t.accent, 150) : t.borderSoft, S(1));
        Txt(g, on ? L"全部日程" : L"仅 DDL", S(9.5f), on, on ? t.accent : t.dim, r, ALIGN_CENTER | ALIGN_VCENTER);
        AddHit(r.X, r.Y, r.Width, r.Height, CMD_BOARD_ALL_EVENTS, []() {
            app.boardAllEvents = !app.boardAllEvents;
            app.boardSelected.clear();
            UIInvalidate();
        });
    }
    {
        std::wstring label = app.boardCourseFilter.empty() ? L"课程：全部" : L"课程：" + Utf8ToWide(app.boardCourseFilter);
        RectF r(x + S(190), cy, S(190), S(28));
        FillRoundRect(g, r, S(7), WithAlpha(t.card, 180));
        StrokeRoundRect(g, r, S(7), t.borderSoft, S(1));
        Txt(g, label, S(9.5f), false, t.dim, RectF(r.X + S(10), r.Y, r.Width - S(28), r.Height), ALIGN_VCENTER, false, true);
        Icon(g, ICO_CHEV_DOWN, RectF(RX(r) - S(20), r.Y + S(9), S(10), S(10)), t.dim2, 1.3f);
        AddHit(r.X, r.Y, r.Width, r.Height, CMD_BOARD_COURSE_FILTER, [courses]() {
            std::vector<MenuItemDef> menu;
            menu.push_back({ L"全部课程", ICO_CAL, false, false, []() { app.boardCourseFilter.clear(); UIInvalidate(); } });
            for (auto& c : courses) {
                std::string u8 = WideToUtf8(c);
                menu.push_back({ c, ICO_CAL, false, false, [u8]() { app.boardCourseFilter = u8; UIInvalidate(); } });
            }
            OpenMenu(0, 0, menu);
        });
    }

    float statsY = cy + S(36);
    Txt(g, Fmt(L"%s · 共 %d 项 · 逾期 %d · 本周截止 %d · 完成率 %d%%",
               app.boardAllEvents ? L"全部日程" : L"任务", total, overdue, thisWeek, rate),
        S(10), false, total ? t.dim : t.dim2, RectF(x, statsY, w, S(18)));
    if (!app.boardSelected.empty()) {
        Txt(g, Fmt(L"已选 %d 项（Shift+点击卡片多选）", (int)app.boardSelected.size()), S(9.5f), false, t.accent,
            RectF(x + w - S(330), statsY, S(330), S(18)), ALIGN_RIGHT);
    }

    // ---- 分栏 ----
    float colTop = statsY + S(28);
    float bulkH = app.boardSelected.empty() ? 0 : S(40);
    float colBottom = y + h - bulkH - S(6);
    float colH = std::max(S(120), colBottom - colTop);
    static const wchar_t* titles[5] = { L"逾期", L"今天", L"本周", L"以后", L"已完成" };
    static const Color colColors[5] = {
        Color(255, 255, 107, 122), Color(255, 255, 180, 84), Color(255, 227, 193, 119),
        Color(255, 139, 147, 171), Color(255, 89, 214, 160)
    };
    float gap = S(8);
    float colW = (w - gap * 4) / 5;
    int maxHeight = 0;
    for (int c = 0; c < 5; ++c) {
        int count = 0;
        for (auto& it : items) if (it.bucket == c) ++count;
        int need = (int)(S(30) + count * S(56));
        if (need > maxHeight) maxHeight = need;
    }
    int maxScroll = std::max(0, maxHeight - (int)colH);
    app.agendaScroll = std::max(0, std::min(app.agendaScroll, maxScroll));
    float scroll = (float)app.agendaScroll;

    RectF content(x, colTop, w, colH);
    g.SetClip(content);
    for (int c = 0; c < 5; ++c) {
        float cx = x + c * (colW + gap);
        RectF head(cx, colTop, colW, S(24));
        FillRoundRect(g, head, S(6), WithAlpha(colColors[c], 22));
        Txt(g, titles[c], S(9.5f), true, colColors[c], RectF(head.X + S(8), head.Y, S(66), head.Height), ALIGN_VCENTER);
        int count = 0;
        for (auto& it : items) if (it.bucket == c) ++count;
        Txt(g, Fmt(L"%d", count), S(9), false, t.dim2, RectF(RX(head) - S(28), head.Y, S(20), head.Height), ALIGN_RIGHT | ALIGN_VCENTER);

        float ry = colTop + S(30) - scroll;
        for (auto& it : items) {
            if (it.bucket != c) continue;
            const Occurrence& o = it.occ;
            RectF card(cx, ry, colW, S(52));
            if (BY(card) < colTop || card.Y > colBottom) { ry += S(56); continue; }
            bool sel = app.boardSelected.count(OccKey(o)) > 0;
            FillRoundRect(g, card, S(8), sel ? WithAlpha(t.accent, 26) : WithAlpha(t.card, 210));
            StrokeRoundRect(g, card, S(8), sel ? WithAlpha(t.accent, 150) : t.borderSoft, S(1));
            if (c == 0) FillRoundRect(g, RectF(card.X, card.Y + S(6), S(3), card.Height - S(12)), S(2), t.danger);
            // 完成圆点
            RectF dot(card.X + S(9), card.Y + S(16), S(18), S(18));
            FillCircle(g, dot.X + S(9), dot.Y + S(9), S(8.5f), o.ev.done ? t.success : WithAlpha(t.dim2, 70));
            if (o.ev.done) Icon(g, ICO_CHECK, RectF(dot.X + S(3), dot.Y + S(3), S(12), S(12)), t.onAccent, 1.4f);
            Txt(g, o.ev.title, S(10), o.ev.priority == PRIO_HIGH, o.ev.done ? WithAlpha(t.dim, 210) : t.text,
                RectF(card.X + S(30), card.Y + S(5), card.Width - S(38), S(18)), ALIGN_LEFT, false, true);
            std::wstring sub = DueText(o);
            if (!it.course.empty()) sub = it.course + L" · " + sub;
            Txt(g, sub, S(8.5f), false, c == 0 ? WithAlpha(t.danger, 220) : t.dim2,
                RectF(card.X + S(30), card.Y + S(25), card.Width - S(38), S(15)), ALIGN_LEFT, false, true);
            AddHit(card.X, card.Y, card.Width, card.Height, CMD_BOARD_DONE + 1000 + (&it - &items[0]), [o]() {
                if (GetKeyState(VK_SHIFT) & 0x8000) {
                    std::wstring k = OccKey(o);
                    if (app.boardSelected.count(k)) app.boardSelected.erase(k);
                    else app.boardSelected.insert(k);
                    UIInvalidate();
                } else {
                    OpenEditorForOccurrence(o);
                }
            });
            AddHit(dot.X, dot.Y, S(22), S(22), CMD_BOARD_DONE + 2000 + (&it - &items[0]), [o]() {
                ToggleDone(o, !o.ev.done);
            });
            ry += S(56);
        }
        if (count == 0) {
            Txt(g, L"暂无", S(9), false, WithAlpha(t.dim2, 150), RectF(cx, colTop + S(36), colW, S(18)), ALIGN_CENTER);
        }
    }
    g.ResetClip();

    // ---- 批量操作栏 ----
    if (bulkH > 0) {
        RectF bar(x, y + h - bulkH, w, S(34));
        FillRoundRect(g, bar, S(8), WithAlpha(t.card, 245));
        StrokeRoundRect(g, bar, S(8), WithAlpha(t.accent, 90), S(1));
        Txt(g, Fmt(L"已选 %d 项", (int)app.boardSelected.size()), S(10), true, t.accent,
            RectF(bar.X + S(12), bar.Y, S(110), bar.Height), ALIGN_VCENTER);
        UI::Btn(g, RectF(bar.X + S(120), bar.Y + S(4), S(96), S(26)), CMD_BOARD_DONE, L"标记完成", 1, ICO_CHECK, []() { ApplyBulk(true); });
        UI::Btn(g, RectF(bar.X + S(222), bar.Y + S(4), S(96), S(26)), CMD_BOARD_UNDONE, L"标记未完成", 1, -1, []() { ApplyBulk(false); });
        UI::Btn(g, RectF(bar.X + S(324), bar.Y + S(4), S(80), S(26)), CMD_BOARD_CLEAR, L"清除", 1, -1, []() { app.boardSelected.clear(); UIInvalidate(); });
    }
}
