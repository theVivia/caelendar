// seriescolor.cpp - 合并后按课程 / 系列逐个配色
#include "app.h"
#include "smart.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

struct CourseGroup {
    std::wstring title;
    std::vector<Event*> evs;
    DateTime first{ 0,0,0,0,0 };
    std::wstring color;
    int count = 0;
    bool recurring = false;
};

static std::vector<CourseGroup> BuildGroups() {
    std::map<std::wstring, CourseGroup> m;
    for (auto& e : app.store.events) {
        bool courseish = e.IsRecurring() || e.HasTag("course") || e.source.find(L"ICS") != std::wstring::npos;
        if (!courseish) continue;
        auto& g = m[e.title];
        g.title = e.title;
        g.evs.push_back(&e);
        g.count++;
        if (g.first.y == 0 || CompareDT(e.start, g.first) < 0) g.first = e.start;
        if (!e.color.empty() && g.color.empty()) g.color = e.color;
        if (e.IsRecurring()) g.recurring = true;
    }
    std::vector<CourseGroup> out;
    out.reserve(m.size());
    for (auto& kv : m) out.push_back(kv.second);
    std::sort(out.begin(), out.end(), [](const CourseGroup& a, const CourseGroup& b) {
        return CompareDT(a.first, b.first) < 0;
    });
    return out;
}

static void ApplyTitleColor(const std::wstring& title, const std::wstring& hex) {
    app.store.Snapshot();
    for (auto& e : app.store.events) {
        if (e.title == title) e.color = hex;
    }
    app.store.Touch();
    DoSaveStore();
    InvalidateOccs();
    UIInvalidate();
}

void OpenSeriesColors() {
    app.seriesColor.open = true;
    app.seriesColor.scroll = 0;
    app.overlay = OV_SERIESCOLOR;
    UIInvalidate();
}

void DrawSeriesColorsOverlay(Graphics& g, float W, float H) {
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    const Theme& th = TH();
    UI::Dim(g, W, H, 170);
    float pw = S(800), ph = S(620);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    FillRoundRect(g, RectF(x + S(4), y + S(6), pw, ph), S(18), Color(80, 0, 0, 0));
    FillRoundRect(g, RectF(x, y, pw, ph), S(18), th.panel);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(18), th.border, S(1));
    Txt(g, L"给课程 / 系列配色", S(17), true, th.text, RectF(x + S(24), y + S(18), S(320), S(28)));
    Txt(g, L"合并之后在这里逐门课决定颜色；同一门课不同时间会拆成多个系列，但颜色按课程名统一。",
        S(9.5f), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(60), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(48), y + S(18), S(28), S(28)), CMD_SERIES_COLOR_CLOSE, ICO_CLOSE, S(13), []() {
        app.seriesColor.open = false;
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    std::vector<CourseGroup> groups = BuildGroups();
    float listTop = y + S(78);
    float listBottom = y + ph - S(74);
    float rowH = S(44);
    int visible = std::max(1, (int)((listBottom - listTop) / rowH));
    int maxScroll = std::max(0, (int)groups.size() - visible);
    app.seriesColor.scroll = std::max(0, std::min(app.seriesColor.scroll, maxScroll));

    g.SetClip(RectF(x + S(12), listTop, pw - S(24), listBottom - listTop));
    const auto& palette = PaletteHex();
    for (int i = 0; i < visible; ++i) {
        int gi = app.seriesColor.scroll + i;
        if (gi >= (int)groups.size()) break;
        CourseGroup& grp = groups[gi];
        RectF row(x + S(16), listTop + i * rowH, pw - S(32), rowH - S(4));
        FillRoundRect(g, row, S(8), (gi % 2 == 0) ? WithAlpha(th.card, 220) : WithAlpha(th.panel2, 160));
        StrokeRoundRect(g, row, S(8), WithAlpha(th.borderSoft, 120), S(1));
        // 课程名 + 信息
        Txt(g, grp.title, S(12), true, th.text, RectF(row.X + S(14), row.Y + S(4), S(250), S(20)), ALIGN_LEFT, false, true);
        std::wstring info = Fmt(L"%d 条%s · %s %s", grp.count, grp.recurring ? L"（含重复系列）" : L"",
                                ISODate(grp.first).c_str(), TimeStr(grp.first).c_str());
        Txt(g, info, S(8.5f), false, WithAlpha(th.dim2, 220), RectF(row.X + S(14), row.Y + S(22), S(260), S(16)), ALIGN_LEFT, false, true);
        // 调色板
        float sx = row.X + S(272);
        float step = S(23);
        for (int ci = 0; ci < (int)palette.size(); ++ci) {
            RectF sw(sx + ci * step, row.Y + S(11), S(19), S(19));
            FillRoundRect(g, sw, S(5), Hex(palette[ci]));
            if (grp.color == palette[ci]) StrokeRoundRect(g, sw, S(5), th.text, S(2));
            std::wstring title = grp.title, hex = palette[ci];
            AddHit(sw.X, sw.Y, sw.Width, sw.Height,
                   CMD_SERIES_COLOR_SWATCH_BASE + gi * 32 + ci, [title, hex]() { ApplyTitleColor(title, hex); });
        }
        // 自定义色
        RectF cu(sx + palette.size() * step + S(2), row.Y + S(11), S(26), S(19));
        FillRoundRect(g, cu, S(5), grp.color.empty() ? WithAlpha(th.panel2, 200) : Hex(grp.color));
        StrokeRoundRect(g, cu, S(5), WithAlpha(th.text, 90), S(1));
        Txt(g, L"…", S(11), true, WithAlpha(th.text, 220), cu, ALIGN_CENTER | ALIGN_VCENTER);
        std::wstring title2 = grp.title, init = grp.color;
        AddHit(cu.X, cu.Y, cu.Width, cu.Height, CMD_SERIES_COLOR_CUSTOM_BASE + gi, [title2, init]() {
            OpenColorPicker(L"自定义课程颜色 · " + title2, init, [title2](const std::wstring& hex) {
                ApplyTitleColor(title2, hex);
            });
        });
    }
    g.ResetClip();
    if (groups.empty()) {
        Txt(g, L"还没有可配色的课程 / 系列。先导入课表或运行「智能整理课表」。", S(11), false, th.dim2,
            RectF(x + S(24), listTop + S(20), pw - S(48), S(24)));
    }
    Txt(g, Fmt(L"共 %d 门 / 个系列 · 滚轮可滚动", (int)groups.size()), S(9), false, WithAlpha(th.dim2, 200),
        RectF(x + S(24), y + ph - S(68), S(300), S(20)), ALIGN_VCENTER);

    // 底部
    float by = y + ph - S(56);
    UI::Btn(g, RectF(x + pw - S(24) - S(96), by, S(96), S(34)), CMD_SERIES_COLOR_CLOSE, L"完成", 0, ICO_CHECK, []() {
        app.seriesColor.open = false;
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(96) * 2 - S(10), by, S(96), S(34)), CMD_SERIES_COLOR_CLEAR, L"清除自定义", 1, -1, []() {
        app.store.Snapshot();
        for (auto& e : app.store.events) {
            if (e.IsRecurring() || e.HasTag("course") || e.source.find(L"ICS") != std::wstring::npos) e.color.clear();
        }
        app.store.Touch();
        DoSaveStore();
        InvalidateOccs();
        ShowToast(L"已清除课程自定义颜色");
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(96) * 3 - S(20), by, S(110), S(34)), CMD_SERIES_COLOR_AUTO, L"自动配色", 1, -1, []() {
        app.store.Snapshot();
        for (auto& e : app.store.events) {
            bool courseish = e.IsRecurring() || e.HasTag("course") || e.source.find(L"ICS") != std::wstring::npos;
            if (courseish) e.color = SmartCourseColor(e.title);
        }
        app.store.Touch();
        DoSaveStore();
        InvalidateOccs();
        ShowToast(L"已按课程名自动配色");
        UIInvalidate();
    });
}
