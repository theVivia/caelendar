// health.cpp - 数据体检报告（只读检查 + 跳转清理）
#include "app.h"
#include <algorithm>
#include <map>
#include <set>

using namespace Gdiplus;
using namespace gfx;

namespace {

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

}  // namespace

HealthReport ScanHealth(const Store& st) {
    HealthReport r;
    r.totalEvents = (int)st.events.size();
    r.totalTags = (int)st.tags.size();
    std::set<std::string> tagIds;
    for (auto& t : st.tags) tagIds.insert(t.id);
    std::map<std::wstring, int> seen;
    for (auto& e : st.events) {
        // 重复：同标题 + 同开始时间出现多次
        std::wstring key = e.title + L"|" + ISODT(e.start);
        if (++seen[key] > 1) ++r.duplicate;
        // 结束早于开始（且不是时间点）
        if (CompareDT(e.end, e.start) < 0) ++r.endBeforeStart;
        // 超长事件（> 7 天）
        if (MinutesBetween(e.start, e.end) > 7 * 24 * 60) ++r.tooLong;
        // 空标题
        if (Trim(e.title).empty()) ++r.emptyTitle;
        // 引用不存在的标签
        for (auto& tid : e.tags) {
            if (!tagIds.count(tid)) { ++r.missingTags; break; }
        }
        // 提醒偏移异常
        for (auto& al : e.alarms) {
            if (al.offsetMin < -30 * 24 * 60 || al.offsetMin > 30 * 24 * 60) { ++r.badAlarms; break; }
        }
    }
    return r;
}

void OpenHealth() {
    app.overlay = OV_HEALTH;
    UIInvalidate();
}

void DrawHealthOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(640), ph = S(520);
    pw = std::min(pw, W - S(32));
    ph = std::min(ph, H - S(24));
    float x = (W - pw) / 2, y = (H - ph) / 2;
    UI::Dim(g, W, H, 170);
    Shadow(g, RectF(x, y, pw, ph), S(18), 8, 26);
    FillRoundRect(g, RectF(x, y, pw, ph), S(18), th.card);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(18), th.border, S(1));
    HealthReport r = ScanHealth(app.store);
    Txt(g, L"数据体检", S(17), true, th.text, RectF(x + S(24), y + S(16), S(240), S(26)));
    Txt(g, Fmt(L"事件 %d 条 · 标签 %d 个 · %s", r.totalEvents, r.totalTags,
               r.issues() == 0 ? L"未发现问题" : Fmt(L"发现 %d 处可疑", r.issues()).c_str()),
        S(10), r.issues() == 0, r.issues() == 0 ? th.success : th.warn,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_HEALTH_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    struct Row { const wchar_t* name; int count; const wchar_t* hint; };
    Row rows[6] = {
        { L"重复事件（同标题同开始时间）", r.duplicate, L"可能是重复导入，可用清理面板删除" },
        { L"结束早于开始", r.endBeforeStart, L"时间点事件允许相等；早于开始属异常" },
        { L"超长事件（> 7 天）", r.tooLong, L"检查是否误填结束日期" },
        { L"空标题", r.emptyTitle, L"补一个标题即可" },
        { L"引用不存在的标签", r.missingTags, L"标签被删过，可在编辑器重新选标签" },
        { L"提醒偏移异常", r.badAlarms, L"提醒偏移超出 ±30 天" },
    };
    float ry = y + S(82);
    for (int i = 0; i < 6; ++i) {
        RectF row(x + S(24), ry + i * S(46), pw - S(48), S(40));
        FillRoundRect(g, row, S(8), WithAlpha(th.panel2, 150));
        bool ok = rows[i].count == 0;
        Txt(g, rows[i].name, S(10), false, th.text, RectF(row.X + S(12), row.Y + S(3), row.Width - S(120), S(16)), ALIGN_VCENTER);
        Txt(g, rows[i].hint, S(8.5f), false, th.dim2, RectF(row.X + S(12), row.Y + S(19), row.Width - S(120), S(14)), ALIGN_VCENTER, false, true);
        Txt(g, ok ? L"正常" : Fmt(L"%d", rows[i].count), S(11), true, ok ? th.success : th.danger,
            RectF(RX(row) - S(76), row.Y, S(64), row.Height), ALIGN_RIGHT | ALIGN_VCENTER);
    }
    UI::Btn(g, RectF(x + S(24), y + ph - S(50), S(150), S(34)), CMD_HEALTH_CLEANUP, L"打开清理面板…", 1, ICO_REFRESH, []() {
        app.overlay = OV_NONE;
        OpenCleanup();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(50), S(90), S(34)), CMD_HEALTH_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    Txt(g, L"体检只读，不改数据；需要删除时用清理面板（删除前自动快照，可撤销）。", S(8.5f), false, th.dim2,
        RectF(x + S(24), y + ph - S(14), pw - S(48), S(14)), ALIGN_LEFT, false, true);
}
