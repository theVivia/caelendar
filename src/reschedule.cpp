// reschedule.cpp - 调休 / 批量迁移日程
#include "app.h"
#include <algorithm>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

void OpenRescheduleDialog(const std::vector<Occurrence>& occs) {
    if (occs.empty()) return;
    app.reschedule = RescheduleState();
    for (auto& o : occs) {
        RescheduleTarget t;
        t.seriesId = o.seriesId;
        t.origStart = o.origStart;
        t.effective = o.ev;
        t.occStart = o.start;
        t.occEnd = o.end;
        app.reschedule.targets.push_back(t);
    }
    DateTime base = AddDaysDT(app.reschedule.targets[0].origStart, 1);
    app.reschedule.date = ISODate(base);
    app.reschedule.open = true;
    app.overlay = OV_RESCHEDULE;
    UIInvalidate();
}

void OpenRescheduleFromSelection() {
    std::vector<Occurrence> occs;
    for (auto& o : app.occs) {
        if (app.selected.count(OccKey(o))) occs.push_back(o);
    }
    if (occs.empty()) {
        ShowToast(L"先选要调休的日程（Shift+点日程可多选）");
        return;
    }
    OpenRescheduleDialog(occs);
}

void DrawRescheduleOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(580), ph = S(410);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    UI::Dim(g, W, H, 170);
    FillRoundRect(g, RectF(x + S(4), y + S(6), pw, ph), S(18), Color(80, 0, 0, 0));
    FillRoundRect(g, RectF(x, y, pw, ph), S(18), th.panel);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(18), th.border, S(1));
    Txt(g, L"调休 / 迁移日程", S(17), true, th.text, RectF(x + S(24), y + S(18), S(320), S(28)));
    Txt(g, Fmt(L"把选中的 %d 项整体平移到目标日期（保留时间和时长）", (int)app.reschedule.targets.size()),
        S(9.5f), false, th.dim2, RectF(x + S(24), y + S(48), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(48), y + S(18), S(28), S(28)), CMD_RS_CANCEL, ICO_CLOSE, S(13), []() {
        app.reschedule.open = false;
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    float cx = x + S(24), cw = pw - S(48);
    Txt(g, L"目标日期", S(10.5f), false, th.dim, RectF(cx, y + S(84), S(100), S(30)), ALIGN_VCENTER);
    RectF box(cx + S(80), y + S(84), S(190), S(34));
    FillRoundRect(g, box, S(8), th.panel2);
    StrokeRoundRect(g, box, S(8), WithAlpha(th.border, 200), S(1));
    Txt(g, app.reschedule.date, S(14), true, th.accent, RectF(box.X, box.Y, box.Width, box.Height), ALIGN_CENTER | ALIGN_VCENTER);
    UI::Btn(g, RectF(box.X - S(36), box.Y + S(3), S(28), S(28)), CMD_RS_DAY_DEC, L"", 2, ICO_CHEV_LEFT, []() {
        bool ok = false;
        DateTime d = ParseISODate(app.reschedule.date, &ok);
        if (ok) { app.reschedule.date = ISODate(AddDaysDT(d, -1)); UIInvalidate(); }
    });
    UI::Btn(g, RectF(box.X + box.Width + S(8), box.Y + S(3), S(28), S(28)), CMD_RS_DAY_INC, L"", 2, ICO_CHEV_RIGHT, []() {
        bool ok = false;
        DateTime d = ParseISODate(app.reschedule.date, &ok);
        if (ok) { app.reschedule.date = ISODate(AddDaysDT(d, 1)); UIInvalidate(); }
    });
    Txt(g, L"按 ◀ ▶ 逐日微调，或点下方快捷日期", S(9), false, WithAlpha(th.dim2, 190),
        RectF(cx, y + S(120), cw, S(16)));

    // 快捷日期：相对第一项原日期
    DateTime base = app.reschedule.targets.empty() ? app.now : app.reschedule.targets[0].origStart;
    struct Quick { const wchar_t* name; int days; };
    Quick qs[] = { { L"次日（调休）", 1 }, { L"再后一天", 2 }, { L"下周一", -1 }, { L"+7 天", 7 } };
    float qx = cx;
    for (auto& q : qs) {
        DateTime d;
        if (q.days == -1) {
            int wd = DayOfWeek(base.y, base.mo, base.d);
            int add = (8 - wd) % 7;
            if (add == 0) add = 7;
            d = AddDaysDT(base, add);
        } else {
            d = AddDaysDT(base, q.days);
        }
        SizeF sz; MeasureText(g, q.name, S(9.5f), false, sz);
        float bw = sz.Width + S(16);
        std::wstring ds = ISODate(d);
        UI::Chip(g, RectF(qx, y + S(144), bw, S(24)), CMD_RS_QUICK_BASE + (int)(&q - qs), q.name, false, th.accent,
                 [ds]() { app.reschedule.date = ds; UIInvalidate(); }, 9.5f);
        qx += bw + S(8);
    }

    // 预览
    RectF list(cx, y + S(172), cw, S(150));
    FillRoundRect(g, list, S(10), WithAlpha(th.bg, 150));
    StrokeRoundRect(g, list, S(10), th.borderSoft, S(1));
    float ly = list.Y + S(8);
    for (size_t i = 0; i < app.reschedule.targets.size() && i < 4; ++i) {
        auto& t = app.reschedule.targets[i];
        DateTime nd = AddDaysDT(t.origStart, 0);
        Txt(g, t.effective.title, S(10), false, th.text, RectF(list.X + S(14), ly, S(250), S(20)), ALIGN_LEFT, false, true);
        Txt(g, Fmt(L"%s → 待迁移", ISODate(t.origStart).c_str()), S(9), false, WithAlpha(th.dim2, 210),
            RectF(list.X + S(280), ly, S(230), S(20)));
        ly += S(28);
        (void)nd;
    }
    if (app.reschedule.targets.size() > 4) {
        Txt(g, Fmt(L"… 等共 %d 项", (int)app.reschedule.targets.size()), S(9), false, WithAlpha(th.dim2, 200),
            RectF(list.X + S(14), ly, S(200), S(18)));
    }
    Txt(g, L"只改日期，不改时间；重复日程只迁移选中的那几次（原系列保留）。", S(8.5f), false, WithAlpha(th.dim2, 190),
        RectF(cx, y + S(334), cw, S(16)));

    float by = y + ph - S(56);
    UI::Btn(g, RectF(x + pw - S(24) - S(96), by, S(96), S(34)), CMD_RS_OK, L"迁移", 0, ICO_CHECK, []() {
        bool ok = false;
        DateTime nd = ParseISODate(app.reschedule.date, &ok);
        if (!ok) { ShowToast(L"日期格式不对，例：2026-10-20"); return; }
        Date newDay{ nd.y, nd.mo, nd.d };
        app.store.Snapshot();
        int moved = 0;
        for (auto& t : app.reschedule.targets) {
            if (app.store.RescheduleOccurrence(t.seriesId, t.origStart, t.effective, t.occStart, t.occEnd, newDay)) ++moved;
        }
        app.selected.clear();
        InvalidateOccs();
        DoSaveStore();
        app.reschedule.open = false;
        app.overlay = OV_NONE;
        ShowToast(Fmt(L"已将 %d 项调休到 %s", moved, ISODate(nd).c_str()));
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(96) * 2 - S(10), by, S(96), S(34)), CMD_RS_CANCEL, L"取消", 1, -1, []() {
        app.reschedule.open = false;
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
