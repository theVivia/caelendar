// colorpick.cpp - 颜色编辑器（HEX / RGB / CMYK）
#include "app.h"
#include <algorithm>
#include <string>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

static void ClampRgb(int& r, int& g, int& b) {
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
}

void OpenColorPicker(const std::wstring& title, const std::wstring& initialHex,
                     std::function<void(const std::wstring&)> onOk) {
    app.colorPick.returnOverlay = (int)app.overlay;   // 记住来源界面（编辑器/设置/命令面板），确定后返回
    app.colorPick.open = true;
    app.colorPick.title = title.empty() ? L"自定义颜色" : title;
    app.colorPick.hex = initialHex.empty() ? L"#6EA8FF" : initialHex;
    app.colorPick.onOk = std::move(onOk);
    app.overlay = OV_COLORPICK;
    UIInvalidate();
}

void DrawColorPickOverlay(Graphics& g, float W, float H) {
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    const Theme& th = TH();
    UI::Dim(g, W, H, 170);
    float pw = S(540), ph = S(500);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    FillRoundRect(g, RectF(x + S(4), y + S(6), pw, ph), S(18), Color(80, 0, 0, 0));
    FillRoundRect(g, RectF(x, y, pw, ph), S(18), th.panel);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(18), th.border, S(1));
    Txt(g, app.colorPick.title, S(16), true, th.text, RectF(x + S(24), y + S(18), pw - S(80), S(28)));
    UI::IconBtn(g, RectF(x + pw - S(48), y + S(18), S(28), S(28)), CMD_CP_CANCEL, ICO_CLOSE, S(13), []() {
        app.colorPick.open = false;
        app.colorPick.onOk = nullptr;
        app.overlay = (OverlayKind)app.colorPick.returnOverlay;
        app.colorPick.returnOverlay = 0;
        UIInvalidate();
    });

    int r = 110, gr = 168, b = 255;
    HexToRgb(app.colorPick.hex, r, gr, b);
    ClampRgb(r, gr, b);
    int c = 0, m = 0, yy = 0, k = 0;
    RgbToCmyk(r, gr, b, c, m, yy, k);

    // 预览
    RectF prev(x + S(24), y + S(62), S(78), S(78));
    FillRoundRect(g, prev, S(10), Color(255, (BYTE)r, (BYTE)gr, (BYTE)b));
    StrokeRoundRect(g, prev, S(10), th.border, S(1));
    Txt(g, RgbToHex(r, gr, b), S(18), true, th.text, RectF(x + S(120), y + S(66), S(260), S(28)), ALIGN_LEFT, true);
    Txt(g, L"直接输入 HEX，或调 RGB / CMYK", S(9.5f), false, th.dim2, RectF(x + S(120), y + S(96), S(360), S(18)));

    float cx = x + S(24), cw = pw - S(48);
    float ry = y + S(160);
    Txt(g, L"HEX", S(10.5f), false, th.dim, RectF(cx, ry, S(50), S(30)), ALIGN_VCENTER);
    RectF hexBox(cx + S(56), ry, S(180), S(30));
    FillRoundRect(g, hexBox, S(7), th.panel2);
    HWND h = UIEdit(9601, RectF(hexBox.X + S(8), hexBox.Y + S(6), hexBox.Width - S(16), S(18)),
                    &app.colorPick.hex, false, S(12));
    StrokeRoundRect(g, hexBox, S(7), (h && GetFocus() == h) ? WithAlpha(th.accent, 150) : th.borderSoft, S(1));
    Txt(g, L"例：#6EA8FF", S(9), false, WithAlpha(th.dim2, 190), RectF(cx + S(250), ry, S(180), S(30)), ALIGN_VCENTER);

    // RGB
    float ry2 = y + S(214);
    Txt(g, L"RGB", S(10.5f), false, th.dim, RectF(cx, ry2, S(50), S(30)), ALIGN_VCENTER);
    float sw = (cw - S(60)) / 3;
    auto setRgb = [](int nr, int ng, int nb) {
        ClampRgb(nr, ng, nb);
        app.colorPick.hex = RgbToHex(nr, ng, nb);
        UIInvalidate();
    };
    UI::Stepper(g, RectF(cx + S(56), ry2, sw - S(6), S(30)), CMD_CP_R_DEC, CMD_CP_R_INC,
                Fmt(L"R %d", r), [=]() { setRgb(r - 1, gr, b); }, [=]() { setRgb(r + 1, gr, b); }, 10.0f);
    UI::Stepper(g, RectF(cx + S(56) + sw, ry2, sw - S(6), S(30)), CMD_CP_G_DEC, CMD_CP_G_INC,
                Fmt(L"G %d", gr), [=]() { setRgb(r, gr - 1, b); }, [=]() { setRgb(r, gr + 1, b); }, 10.0f);
    UI::Stepper(g, RectF(cx + S(56) + sw * 2, ry2, sw - S(6), S(30)), CMD_CP_B_DEC, CMD_CP_B_INC,
                Fmt(L"B %d", b), [=]() { setRgb(r, gr, b - 1); }, [=]() { setRgb(r, gr, b + 1); }, 10.0f);

    // CMYK
    float ry3 = y + S(268);
    Txt(g, L"CMYK", S(10.5f), false, th.dim, RectF(cx, ry3, S(56), S(30)), ALIGN_VCENTER);
    float sw4 = (cw - S(56) - S(18)) / 4;
    auto setCmyk = [](int nc, int nm, int ny, int nk) {
        nc = std::max(0, std::min(100, nc));
        nm = std::max(0, std::min(100, nm));
        ny = std::max(0, std::min(100, ny));
        nk = std::max(0, std::min(100, nk));
        int rr, gg, bb;
        CmykToRgb(nc, nm, ny, nk, rr, gg, bb);
        app.colorPick.hex = RgbToHex(rr, gg, bb);
        UIInvalidate();
    };
    UI::Stepper(g, RectF(cx + S(56), ry3, sw4, S(30)), CMD_CP_C_DEC, CMD_CP_C_INC,
                Fmt(L"C %d", c), [=]() { setCmyk(c - 1, m, yy, k); }, [=]() { setCmyk(c + 1, m, yy, k); }, 9.5f);
    UI::Stepper(g, RectF(cx + S(56) + sw4 + S(6), ry3, sw4, S(30)), CMD_CP_M_DEC, CMD_CP_M_INC,
                Fmt(L"M %d", m), [=]() { setCmyk(c, m - 1, yy, k); }, [=]() { setCmyk(c, m + 1, yy, k); }, 9.5f);
    UI::Stepper(g, RectF(cx + S(56) + (sw4 + S(6)) * 2, ry3, sw4, S(30)), CMD_CP_Y_DEC, CMD_CP_Y_INC,
                Fmt(L"Y %d", yy), [=]() { setCmyk(c, m, yy - 1, k); }, [=]() { setCmyk(c, m, yy + 1, k); }, 9.5f);
    UI::Stepper(g, RectF(cx + S(56) + (sw4 + S(6)) * 3, ry3, sw4, S(30)), CMD_CP_K_DEC, CMD_CP_K_INC,
                Fmt(L"K %d", k), [=]() { setCmyk(c, m, yy, k - 1); }, [=]() { setCmyk(c, m, yy, k + 1); }, 9.5f);

    Txt(g, L"提示：CMYK 是屏幕换算值；打印以印刷厂为准。", S(9), false, WithAlpha(th.dim2, 190),
        RectF(cx, y + S(316), cw, S(18)));

    // 底部按钮
    float by = y + ph - S(56);
    UI::Btn(g, RectF(x + pw - S(24) - S(96), by, S(96), S(34)), CMD_CP_OK, L"确定", 0, ICO_CHECK, []() {
        int r, g, b;
        if (!HexToRgb(app.colorPick.hex, r, g, b)) { ShowToast(L"HEX 格式不对，例：#6EA8FF"); return; }
        std::wstring hex = RgbToHex(r, g, b);
        auto cb = app.colorPick.onOk;
        app.colorPick.open = false;
        app.colorPick.onOk = nullptr;
        app.overlay = (OverlayKind)app.colorPick.returnOverlay;
        app.colorPick.returnOverlay = 0;
        if (cb) cb(hex);
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(96) * 2 - S(10), by, S(96), S(34)), CMD_CP_CANCEL, L"取消", 1, -1, []() {
        app.colorPick.open = false;
        app.colorPick.onOk = nullptr;
        app.overlay = (OverlayKind)app.colorPick.returnOverlay;
        app.colorPick.returnOverlay = 0;
        UIInvalidate();
    });
    UI::Btn(g, RectF(cx, by, S(96), S(34)), CMD_CP_RESET, L"恢复默认色", 1, -1, []() {
        app.colorPick.hex = L"#6EA8FF";
        UIInvalidate();
    });
}