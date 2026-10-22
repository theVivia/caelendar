// cleanuppanel.cpp - 批量清理日程面板
#include "app.h"
#include "cleanup.h"
#include <algorithm>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}



static bool BuildFilter(CleanupFilter& f, bool& badFrom, bool& badTo) {
    f = CleanupFilter();
    badFrom = badTo = false;
    DateTime d;
    if (!app.cleanup.from.empty()) {
        if (DateInputParse(app.cleanup.from, Today(), d)) { f.hasFrom = true; f.from = StartOfDay(d); }
        else badFrom = true;
    }
    if (!app.cleanup.to.empty()) {
        if (DateInputParse(app.cleanup.to, Today(), d)) { f.hasTo = true; f.to = StartOfDay(d); }
        else badTo = true;
    }
    f.keyword = app.cleanup.keyword;
    f.tagIds = app.cleanup.tags;
    f.colors = app.cleanup.colors;
    f.typeMode = app.cleanup.typeMode;
    f.statusMode = app.cleanup.statusMode;
    f.onlyAllDay = app.cleanup.onlyAllDay;
    f.onlyWithAlarm = app.cleanup.onlyWithAlarm;
    f.delMode = app.cleanup.delMode;
    return !badFrom && !badTo;
}

void OpenCleanup() {
    app.cleanup = CleanupState();
    app.overlay = OV_CLEANUP;
    UIInvalidate();
}

static void CloseCleanup() {
    app.overlay = OV_NONE;
    UIInvalidate();
}

static HWND EditInBox(Graphics& g, int id, RectF r, std::wstring* buf, float px = 11.0f) {
    const Theme& th = TH();
    FillRoundRect(g, r, S(7), th.panel2);
    HWND h = UIEdit(id, RectF(r.X + S(8), r.Y + (r.Height - S(18)) / 2, r.Width - S(16), S(18)), buf, false, S(px));
    StrokeRoundRect(g, r, S(7), (h && GetFocus() == h) ? WithAlpha(th.accent, 150) : th.borderSoft, S(1));
    return h;
}

void DrawCleanupOverlay(Graphics& g, float W, float H) {
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    const Theme& th = TH();
    UI::Dim(g, W, H, 170);
    float pw = S(940), ph = S(640);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    FillRoundRect(g, RectF(x + S(4), y + S(6), pw, ph), S(18), Color(80, 0, 0, 0));
    FillRoundRect(g, panel, S(18), th.panel);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Txt(g, L"批量清理日程", S(18), true, th.text, RectF(x + S(24), y + S(18), S(260), S(28)));
    Txt(g, L"按条件筛选后删除；删除前可再确认，误删可 Ctrl+Z 撤销", S(9.5f), false, th.dim2,
        RectF(x + S(24), y + S(48), S(520), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(48), y + S(18), S(28), S(28)), CMD_CLEAN_CLOSE, ICO_CLOSE, S(13), []() {
        CloseCleanup();
    });

    float innerW = pw - S(48);
    float gap = S(18);
    float colW = (innerW - gap * 2) / 3;
    float c1 = x + S(24);
    float c2 = c1 + colW + gap;
    float c3 = c2 + colW + gap;
    float y0 = y + S(80);

    CleanupFilter filter;
    bool badFrom = false, badTo = false;
    bool datesOk = BuildFilter(filter, badFrom, badTo);
    CleanupStats stats = CleanupPreview(app.store, filter);
    const auto& palette = PaletteHex();

    // ---------- 第一列：快速操作 / 日期 / 关键词 / 重复处理 ----------
    {
        float cy = y0;
        UI::SectionTitle(g, L"快速操作", RectF(c1, cy, colW, S(18)));
        cy += S(24);
        auto quick = [&](const wchar_t* label, int cmd, std::function<void()> fn) {
            UI::Btn(g, RectF(c1, cy, colW, S(30)), cmd, label, 3, -1, std::move(fn));
            cy += S(38);
        };
        quick(L"清空全部日程", CMD_CLEAN_ALL, []() {
            int n = (int)app.store.events.size();
            if (n == 0) { ShowToast(L"没有日程可清空"); return; }
            OpenConfirm(L"清空全部日程",
                        Fmt(L"将删除全部 %d 条日程（含所有重复系列和例外）。\n此操作可用 Ctrl+Z 撤销。", n),
                        []() {
                            app.store.Snapshot();
                            int n = CleanupDeleteAll(app.store);
                            DoSaveStore();
                            InvalidateOccs();
                            app.selected.clear();
                            ShowToast(Fmt(L"已清空 %d 条日程（Ctrl+Z 可撤销）", n), 4000);
                            CloseCleanup();
                        }, L"全部删除", L"取消");
        });
        quick(L"删除已完成", CMD_CLEAN_DONE, []() {
            int n = 0;
            for (auto& e : app.store.events) if (e.done) ++n;
            if (n == 0) { ShowToast(L"没有已完成的日程"); return; }
            OpenConfirm(L"删除已完成日程", Fmt(L"将删除 %d 条已完成日程。此操作可用 Ctrl+Z 撤销。", n), []() {
                app.store.Snapshot();
                int n = CleanupDeleteDone(app.store);
                DoSaveStore();
                InvalidateOccs();
                app.selected.clear();
                ShowToast(Fmt(L"已删除 %d 条已完成日程", n));
                CloseCleanup();
            }, L"删除", L"取消");
        });
        quick(L"删除导入的课表（ICS）", CMD_CLEAN_IMPORTED, []() {
            int n = 0;
            for (auto& e : app.store.events) if (e.source.find(L"ICS") != std::wstring::npos) ++n;
            if (n == 0) { ShowToast(L"没有从 ICS 导入的日程"); return; }
            OpenConfirm(L"删除导入的课表",
                        Fmt(L"将删除 %d 条从 ICS 导入的日程（不影响手动创建的）。此操作可用 Ctrl+Z 撤销。", n),
                        []() {
                            app.store.Snapshot();
                            int n = CleanupDeleteImported(app.store);
                            DoSaveStore();
                            InvalidateOccs();
                            app.selected.clear();
                            ShowToast(Fmt(L"已删除 %d 条导入日程", n));
                            CloseCleanup();
                        }, L"删除", L"取消");
        });
        cy += S(6);

        UI::SectionTitle(g, L"日期范围", RectF(c1, cy, colW, S(18)));
        cy += S(24);
        Txt(g, L"从", S(10), false, th.dim, RectF(c1, cy, S(28), S(30)), ALIGN_VCENTER);
        EditInBox(g, 9401, RectF(c1 + S(30), cy, colW - S(30), S(30)), &app.cleanup.from);
        cy += S(36);
        Txt(g, L"到", S(10), false, th.dim, RectF(c1, cy, S(28), S(30)), ALIGN_VCENTER);
        EditInBox(g, 9402, RectF(c1 + S(30), cy, colW - S(30), S(30)), &app.cleanup.to);
        cy += S(34);
        Txt(g, L"留空 = 不限；格式 2026-09-01 或“明天”", S(8.5f), false,
            (badFrom || badTo) ? th.danger : WithAlpha(th.dim2, 200),
            RectF(c1, cy, colW, S(16)));
        cy += S(24);

        UI::SectionTitle(g, L"关键词", RectF(c1, cy, colW, S(18)));
        cy += S(24);
        EditInBox(g, 9403, RectF(c1, cy, colW, S(30)), &app.cleanup.keyword);
        cy += S(34);
        Txt(g, L"匹配标题 / 备注 / 地点 / 来源", S(8.5f), false, WithAlpha(th.dim2, 190), RectF(c1, cy, colW, S(16)));
        cy += S(26);

        UI::SectionTitle(g, L"重复日程处理", RectF(c1, cy, colW, S(18)));
        cy += S(24);
        UI::Chip(g, RectF(c1, cy, colW, S(30)), CMD_CLEAN_MODE_BASE + 0, L"删除整个系列",
                 app.cleanup.delMode == 0, th.accent, []() { app.cleanup.delMode = 0; UIInvalidate(); }, 10.0f);
        cy += S(38);
        UI::Chip(g, RectF(c1, cy, colW, S(30)), CMD_CLEAN_MODE_BASE + 1, L"仅删除范围内这一次",
                 app.cleanup.delMode == 1, th.accent, []() { app.cleanup.delMode = 1; UIInvalidate(); }, 10.0f);
        cy += S(34);
        Txt(g, L"「仅删范围内」需要填日期范围", S(8.5f), false, WithAlpha(th.dim2, 190), RectF(c1, cy, colW, S(16)));
    }

    // ---------- 第二列：标签 + 颜色 ----------
    {
        float cy = y0;
        UI::SectionTitle(g, L"按标签筛选（不选 = 不限）", RectF(c2, cy, colW, S(18)));
        cy += S(26);
        float chipW = (colW - S(8)) / 2;
        int shown = std::min((int)app.store.tags.size(), 18);
        for (int i = 0; i < shown; ++i) {
            const Tag& tag = app.store.tags[i];
            int col = i % 2, row = i / 2;
            std::string tid = tag.id;
            bool active = app.cleanup.tags.count(tid) > 0;
            UI::Chip(g, RectF(c2 + col * (chipW + S(8)), cy + row * S(30), chipW, S(26)),
                     CMD_CLEAN_TAG_BASE + i, tag.name, active, Hex(tag.color), [tid]() {
                         if (app.cleanup.tags.count(tid)) app.cleanup.tags.erase(tid);
                         else app.cleanup.tags.insert(tid);
                         UIInvalidate();
                     }, 9.5f);
        }
        cy += ((shown + 1) / 2) * S(30) + S(10);

        UI::SectionTitle(g, L"按颜色筛选（不选 = 不限）", RectF(c2, cy, colW, S(18)));
        cy += S(26);
        float sw = S(24), sgap = S(7);
        for (int i = 0; i < (int)palette.size(); ++i) {
            int col = i % 6, row = i / 6;
            RectF r(c2 + col * (sw + sgap), cy + row * (sw + sgap), sw, sw);
            bool active = app.cleanup.colors.count(palette[i]) > 0;
            FillRoundRect(g, r, S(6), Hex(palette[i]));
            StrokeRoundRect(g, r, S(6), active ? th.text : WithAlpha(th.border, 200), active ? S(2) : S(1));
            std::wstring hex = palette[i];
            AddHit(r.X, r.Y, r.Width, r.Height, CMD_CLEAN_COLOR_BASE + i, [hex]() {
                if (app.cleanup.colors.count(hex)) app.cleanup.colors.erase(hex);
                else app.cleanup.colors.insert(hex);
                UIInvalidate();
            });
        }
        cy += ((int)palette.size() + 5) / 6 * (sw + sgap) + S(14);
        bool noColorActive = app.cleanup.colors.count(L"#NONE") > 0;
        UI::Chip(g, RectF(c2, cy, colW, S(28)), CMD_CLEAN_NOCOLOR, L"无自定义颜色（跟随标签）",
                 noColorActive, th.accent, []() {
                     if (app.cleanup.colors.count(L"#NONE")) app.cleanup.colors.erase(L"#NONE");
                     else app.cleanup.colors.insert(L"#NONE");
                     UIInvalidate();
                 }, 9.5f);
        cy += S(34);
        Txt(g, L"自动配色不算“自定义颜色”", S(8.5f), false, WithAlpha(th.dim2, 190), RectF(c2, cy, colW, S(16)));
    }

    // ---------- 第三列：类型 / 状态 / 更多 / 预览 ----------
    {
        float cy = y0;
        UI::SectionTitle(g, L"日程类型", RectF(c3, cy, colW, S(18)));
        cy += S(26);
        const wchar_t* typeNames[3] = { L"全部", L"单次", L"重复" };
        float tw = (colW - S(12)) / 3;
        for (int i = 0; i < 3; ++i) {
            UI::Chip(g, RectF(c3 + i * (tw + S(6)), cy, tw, S(28)), CMD_CLEAN_TYPE_BASE + i, typeNames[i],
                     app.cleanup.typeMode == i, th.accent, [i]() { app.cleanup.typeMode = i; UIInvalidate(); }, 10.0f);
        }
        cy += S(44);

        UI::SectionTitle(g, L"完成状态", RectF(c3, cy, colW, S(18)));
        cy += S(26);
        const wchar_t* stNames[3] = { L"全部", L"已完成", L"未完成" };
        for (int i = 0; i < 3; ++i) {
            UI::Chip(g, RectF(c3 + i * (tw + S(6)), cy, tw, S(28)), CMD_CLEAN_STATUS_BASE + i, stNames[i],
                     app.cleanup.statusMode == i, th.accent, [i]() { app.cleanup.statusMode = i; UIInvalidate(); }, 10.0f);
        }
        cy += S(44);

        UI::SectionTitle(g, L"更多筛选", RectF(c3, cy, colW, S(18)));
        cy += S(26);
        UI::CheckBox(g, RectF(c3, cy, colW, S(24)), CMD_CLEAN_ALLDAY, L"只含全天日程", app.cleanup.onlyAllDay, []() {
            app.cleanup.onlyAllDay = !app.cleanup.onlyAllDay;
            UIInvalidate();
        });
        cy += S(30);
        UI::CheckBox(g, RectF(c3, cy, colW, S(24)), CMD_CLEAN_ALARM, L"只含带提醒的日程", app.cleanup.onlyWithAlarm, []() {
            app.cleanup.onlyWithAlarm = !app.cleanup.onlyWithAlarm;
            UIInvalidate();
        });
        cy += S(40);

        UI::SectionTitle(g, L"预览", RectF(c3, cy, colW, S(18)));
        cy += S(26);
        Txt(g, Fmt(L"%d 个日程 / %d 个实例", stats.series, stats.occurrences), S(16), true,
            stats.series > 0 ? th.accent : th.dim2, RectF(c3, cy, colW, S(26)));
        cy += S(30);
        std::wstring modeHint = app.cleanup.delMode == 0
            ? L"删除模式：整个系列（重复日程全部实例）"
            : L"删除模式：只删范围内的实例（系列保留）";
        Txt(g, modeHint, S(8.5f), false, WithAlpha(th.dim2, 210), RectF(c3, cy, colW, S(16)));
        cy += S(22);
        if (!datesOk) {
            Txt(g, L"日期格式不对，无法执行删除", S(8.5f), false, th.danger, RectF(c3, cy, colW, S(16)));
        }
    }

    // ---------- 底部 ----------
    float by = y + ph - S(56);
    UI::Btn(g, RectF(x + S(24), by, S(96), S(32)), CMD_CLEAN_RESET, L"重置条件", 1, -1, []() {
        std::wstring from = app.cleanup.from, to = app.cleanup.to, kw = app.cleanup.keyword;
        app.cleanup = CleanupState();
        app.cleanup.from = from;
        app.cleanup.to = to;
        app.cleanup.keyword = kw;
        UIInvalidate();
    });
    Txt(g, datesOk ? L"删除前会再确认一次；Ctrl+Z 可撤销" : L"日期格式不对：请用 2026-09-01 这类格式",
        S(9), false, datesOk ? WithAlpha(th.dim2, 200) : th.danger,
        RectF(x + S(132), by, S(420), S(32)), ALIGN_VCENTER);
    UI::Btn(g, RectF(x + pw - S(24) - S(110) - S(10) - S(120), S(0) + by, S(110), S(32)), CMD_CLEAN_CLOSE, L"取消", 1, -1, []() {
        CloseCleanup();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(120), by, S(120), S(32)), CMD_CLEAN_APPLY, L"删除匹配项", 3, -1, []() {
        CleanupFilter f;
        bool bf = false, bt = false;
        if (!BuildFilter(f, bf, bt) || bf || bt) { ShowToast(L"日期格式不对（例：2026-09-01）"); return; }
        if (app.cleanup.delMode == 1 && !f.hasFrom && !f.hasTo) {
            ShowToast(L"「仅删范围内」需要填日期范围");
            return;
        }
        CleanupStats st = CleanupPreview(app.store, f);
        if (st.series == 0) { ShowToast(L"没有匹配的日程"); return; }
        std::wstring body = app.cleanup.delMode == 0
            ? Fmt(L"将删除 %d 个日程（含全部重复实例）。\n此操作可用 Ctrl+Z 撤销。", st.series)
            : Fmt(L"将删除 %d 个日程在范围内的 %d 个实例（系列本身保留，范围外不受影响）。\n此操作可用 Ctrl+Z 撤销。",
                  st.series, st.occurrences);
        OpenConfirm(L"删除匹配的日程", body, [f, st]() {
            app.store.Snapshot();
            int n = CleanupApply(app.store, f);
            DoSaveStore();
            InvalidateOccs();
            app.selected.clear();
            ShowToast(Fmt(L"已清理 %d 项（Ctrl+Z 可撤销）", n), 4000);
            CloseCleanup();
        }, L"删除", L"取消");
    });
}