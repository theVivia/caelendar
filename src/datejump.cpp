// datejump.cpp - 输入日期跳转
#include "app.h"
#include <string>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

void OpenDateJump() {
    app.dateJump = ISODate(app.cursor);
    app.overlay = OV_DATEJUMP;
    UIInvalidate();
}

void DrawDateJumpOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(520), ph = S(300);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    UI::Dim(g, W, H, 170);
    FillRoundRect(g, RectF(x + S(4), y + S(6), pw, ph), S(18), Color(80, 0, 0, 0));
    FillRoundRect(g, RectF(x, y, pw, ph), S(18), th.panel);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(18), th.border, S(1));
    Txt(g, L"跳转到日期", S(17), true, th.text, RectF(x + S(24), y + S(18), S(300), S(28)));
    Txt(g, L"支持 2026-12-01 / 明天 / 周三 / 下周一", S(9.5f), false, th.dim2,
        RectF(x + S(24), y + S(50), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(48), y + S(18), S(28), S(28)), CMD_DJ_CANCEL, ICO_CLOSE, S(13), []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    RectF box(x + S(24), y + S(84), pw - S(48), S(40));
    FillRoundRect(g, box, S(9), th.panel2);
    StrokeRoundRect(g, box, S(9), WithAlpha(th.accent, 130), S(1));
    HWND h = UIEdit(9801, RectF(box.X + S(12), box.Y + S(10), box.Width - S(24), S(22)),
                    &app.dateJump, false, S(13));
    Txt(g, Fmt(L"当前：%d年%d月%d日", app.cursor.y, app.cursor.mo, app.cursor.d), S(9.5f), false,
        WithAlpha(th.dim2, 220), RectF(x + S(24), y + S(134), pw - S(48), S(18)));
    Txt(g, L"支持范围 1901–2100 年", S(9), false, WithAlpha(th.dim2, 180),
        RectF(x + S(24), y + S(156), pw - S(48), S(18)));
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(54), S(90), S(34)), CMD_DJ_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) * 2 - S(10), y + ph - S(54), S(96), S(34)), CMD_DJ_OK, L"跳转", 0, ICO_CHECK, []() {
        DateTime dt;
        if (!DateInputParse(app.dateJump, app.cursor, dt)) { ShowToast(L"日期格式不对，例：2026-12-01"); return; }
        if (!IsSupportedDate(dt)) { ShowToast(L"日期范围仅支持 1901–2100 年"); return; }
        app.cursor = dt;
        app.selected.clear();
        InvalidateOccs();
        app.overlay = OV_NONE;
        ShowToast(Fmt(L"已跳转到 %d年%d月%d日", dt.y, dt.mo, dt.d));
        UIInvalidate();
    });
}
