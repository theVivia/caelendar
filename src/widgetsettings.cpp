// widgetsettings.cpp - 挂件 / 屏幕右下角提示窗外观设置
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

static const wchar_t* kLayoutNames[5] = { L"卡片", L"细条", L"今日清单", L"极简时钟", L"迷你周视图" };
static const wchar_t* kLayoutKeys[5] = { L"card", L"bar", L"list", L"clock", L"week" };
static const wchar_t* kSkinNames[4] = { L"跟随主题", L"玻璃", L"纯黑", L"霓虹" };
static const wchar_t* kSkinKeys[4] = { L"theme", L"glass", L"solid", L"neon" };

static std::wstring g_scaleBuf, g_opacityBuf;

static void SyncNumBuffers() {
    auto& w = app.store.settings.widget;
    g_scaleBuf = Fmt(L"%d", w.scale);
    g_opacityBuf = Fmt(L"%d", w.opacity);
}

static bool ParseIntStrict(const std::wstring& s, int& out) {
    if (s.empty()) return false;
    wchar_t* end = nullptr;
    long v = wcstol(s.c_str(), &end, 10);
    if (!end || *end != 0) return false;
    out = (int)v;
    return true;
}

// 允许直接输入数值：在有效范围内即时生效
static void ApplyNumericInputs() {
    auto& w = app.store.settings.widget;
    int v = 0;
    bool changed = false;
    if (ParseIntStrict(g_scaleBuf, v) && v >= 80 && v <= 160 && v != w.scale) { w.scale = v; changed = true; }
    if (ParseIntStrict(g_opacityBuf, v) && v >= 30 && v <= 100 && v != w.opacity) { w.opacity = v; changed = true; }
    if (changed) {
        DebugLog(L"widget numeric apply scale=%d opacity=%d", w.scale, w.opacity);
        app.store.Touch();
        DoSaveStore();
        RecreateWidgetWindow();
    }
}

static HWND NumEdit(Graphics& g, int id, RectF r, std::wstring* buf, const wchar_t* suffix) {
    const Theme& th = TH();
    FillRoundRect(g, r, S(7), th.panel2);
    float suffixW = suffix ? S(20) : 0;
    HWND h = UIEdit(id, RectF(r.X + S(8), r.Y + (r.Height - S(18)) / 2, r.Width - S(8) - suffixW, S(18)), buf, false, S(11));
    StrokeRoundRect(g, r, S(7), (h && GetFocus() == h) ? WithAlpha(th.accent, 150) : th.borderSoft, S(1));
    if (suffix) {
        Txt(g, suffix, S(9), false, th.dim2, RectF(r.X + r.Width - suffixW - S(2), r.Y, suffixW, r.Height),
            ALIGN_RIGHT | ALIGN_VCENTER);
    }
    return h;
}

static void SaveWidgetSettings() {
    app.store.Touch();
    app.needSave = false;
    DoSaveStore();
    RecreateWidgetWindow();
    UIInvalidate();
}

void OpenWidgetSettings() {
    app.overlay = OV_WIDGET;
    SyncNumBuffers();
    UIInvalidate();
}

void DrawWidgetSettingsOverlay(Graphics& g, float W, float H) {
    ApplyNumericInputs();
    const Theme& th = TH();
    app.settingsOpen = false;
    UI::Dim(g, W, H, 165);
    float pw = S(700), ph = S(566);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    FillRoundRect(g, RectF(x + S(4), y + S(6), pw, ph), S(18), Color(70, 0, 0, 0));
    FillRoundRect(g, panel, S(18), th.panel);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Txt(g, L"挂件与提示窗", S(18), true, th.text, RectF(x + S(24), y + S(18), S(300), S(28)));
    Txt(g, L"改动即时生效；屏幕右下角的提醒弹窗沿用同一套皮肤 / 大小 / 透明度", S(9.5f), false, th.dim2,
        RectF(x + S(24), y + S(48), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(48), y + S(18), S(28), S(28)), CMD_WS_CLOSE, ICO_CLOSE, S(13), []() {
        CloseOverlay();
    });

    auto& w = app.store.settings.widget;
    float lx = x + S(24), lw = pw - S(48);
    float cy = y + S(84);

    // 布局
    UI::SectionTitle(g, L"布局", RectF(lx, cy, S(80), S(18)));
    cy += S(24);
    float cw = (lw - S(12) * 4) / 5;
    for (int i = 0; i < 5; ++i) {
        UI::Chip(g, RectF(lx + i * (cw + S(12)), cy, cw, S(30)), CMD_WS_LAYOUT_BASE + i, kLayoutNames[i],
                 w.mode == kLayoutKeys[i], th.accent, [i]() {
                     app.store.settings.widget.mode = kLayoutKeys[i];
                     SaveWidgetSettings();
                 }, 10.5f);
    }
    cy += S(46);

    // 皮肤
    UI::SectionTitle(g, L"皮肤", RectF(lx, cy, S(80), S(18)));
    cy += S(24);
    for (int i = 0; i < 4; ++i) {
        UI::Chip(g, RectF(lx + i * (cw + S(12)), cy, cw, S(30)), CMD_WS_SKIN_BASE + i, kSkinNames[i],
                 w.skin == kSkinKeys[i], th.accent, [i]() {
                     app.store.settings.widget.skin = kSkinKeys[i];
                     SaveWidgetSettings();
                 }, 10.5f);
    }
    cy += S(46);

    // 大小 / 透明度
    float halfW = (lw - S(16)) / 2;
    UI::SectionTitle(g, L"大小", RectF(lx, cy, S(80), S(18)));
    UI::SectionTitle(g, L"透明度", RectF(lx + halfW + S(16), cy, S(80), S(18)));
    cy += S(24);
    float minusW = S(28);
    auto numStepper = [&](float rowX, int cmdDec, int cmdInc, int editId, std::wstring* buf,
                          int minV, int maxV, int step,
                          std::function<int()> getter, std::function<void(int)> setter) {
        float editW = halfW - minusW * 2 - S(16);
        auto parser = [buf, getter, minV, maxV, step, setter](int dir) {
            int v = 0;
            if (!ParseIntStrict(*buf, v)) v = getter();
            setter(std::max(minV, std::min(maxV, v + dir * step)));
            SyncNumBuffers();
            SaveWidgetSettings();
        };
        UI::Btn(g, RectF(rowX, cy, minusW, S(30)), cmdDec, L"−", 1, -1, [parser]() { parser(-1); });
        NumEdit(g, editId, RectF(rowX + minusW + S(8), cy, editW, S(30)), buf, L"%");
        UI::Btn(g, RectF(rowX + minusW + S(8) + editW + S(8), cy, minusW, S(30)), cmdInc, L"+", 1, -1,
                [parser]() { parser(1); });
    };
    numStepper(lx, CMD_WS_SCALE_DEC, CMD_WS_SCALE_INC, 9501, &g_scaleBuf, 80, 160, 10,
               []() { return app.store.settings.widget.scale; },
               [](int v) { app.store.settings.widget.scale = v; });
    numStepper(lx + halfW + S(16), CMD_WS_OPACITY_DEC, CMD_WS_OPACITY_INC, 9502, &g_opacityBuf, 30, 100, 5,
               []() { return app.store.settings.widget.opacity; },
               [](int v) { app.store.settings.widget.opacity = v; });
    Txt(g, L"可直接输入数值（80–160 / 30–100）", S(8.5f), false, WithAlpha(th.dim2, 190),
        RectF(lx, cy + S(34), halfW * 2 + S(16), S(16)));
    cy += S(48);

    // 显示内容
    UI::SectionTitle(g, L"显示内容", RectF(lx, cy, S(100), S(18)));
    cy += S(24);
    float cbw = (lw - S(12) * 3) / 4;
    UI::CheckBox(g, RectF(lx, cy, cbw, S(24)), CMD_WS_ENABLE, L"显示挂件", w.enabled, []() {
        ToggleWidgetWindow();
    });
    UI::CheckBox(g, RectF(lx + cbw + S(12), cy, cbw, S(24)), CMD_WS_TOGGLE_DATE, L"显示日期", w.showDate, []() {
        app.store.settings.widget.showDate = !app.store.settings.widget.showDate;
        SaveWidgetSettings();
    });
    UI::CheckBox(g, RectF(lx + (cbw + S(12)) * 2, cy, cbw, S(24)), CMD_WS_TOGGLE_WATER, L"显示喝水", w.showWater, []() {
        app.store.settings.widget.showWater = !app.store.settings.widget.showWater;
        SaveWidgetSettings();
    });
    UI::CheckBox(g, RectF(lx + (cbw + S(12)) * 3, cy, cbw, S(24)), CMD_WS_TOGGLE_TOPMOST, L"窗口置顶", w.alwaysOnTop, []() {
        app.store.settings.widget.alwaysOnTop = !app.store.settings.widget.alwaysOnTop;
        SaveWidgetSettings();
    });
    cy += S(34);
    UI::CheckBox(g, RectF(lx, cy, cbw, S(24)), CMD_WS_TOGGLE_TODAY, L"今日剩余", w.showToday, []() {
        app.store.settings.widget.showToday = !app.store.settings.widget.showToday;
        SaveWidgetSettings();
    });
    UI::CheckBox(g, RectF(lx + cbw + S(12), cy, cbw, S(24)), CMD_WS_TOGGLE_TASKS, L"剩余任务", w.showTasks, []() {
        app.store.settings.widget.showTasks = !app.store.settings.widget.showTasks;
        SaveWidgetSettings();
    });
    UI::CheckBox(g, RectF(lx + (cbw + S(12)) * 2, cy, cbw, S(24)), CMD_WS_TOGGLE_CLICK, L"鼠标穿透", w.clickThrough, []() {
        app.store.settings.widget.clickThrough = !app.store.settings.widget.clickThrough;
        SaveWidgetSettings();
        ShowToast(app.store.settings.widget.clickThrough ? L"已开启穿透，可从托盘菜单关闭" : L"已关闭穿透");
    });
    UI::CheckBox(g, RectF(lx + (cbw + S(12)) * 3, cy, cbw, S(24)), CMD_WS_TOGGLE_LOW, L"省电模式", w.lowPower, []() {
        app.store.settings.widget.lowPower = !app.store.settings.widget.lowPower;
        SaveWidgetSettings();
    });
    cy += S(44);
    cy += S(44);

    // 提示窗 / 位置
    UI::SectionTitle(g, L"提示窗与位置", RectF(lx, cy, S(140), S(18)));
    cy += S(22);
    Txt(g, L"提醒弹窗：沿用挂件皮肤 / 大小 / 透明度，并在日程提醒里多一个「完成」按钮。", S(9.5f), false, th.dim2,
        RectF(lx, cy, lw - S(170), S(18)));
    UI::Btn(g, RectF(lx + lw - S(160), cy - S(6), S(160), S(28)), CMD_WS_RESET_POS, L"位置回到右下角", 1, -1, []() {
        app.store.settings.widget.x = -1;
        app.store.settings.widget.y = -1;
        SaveWidgetSettings();
        ShowToast(L"挂件位置已重置到右下角");
    });

    // 底部按钮
    float by = y + ph - S(58);
    UI::Btn(g, RectF(x + pw - S(24) - S(110), by, S(110), S(34)), CMD_WS_CLOSE, L"关闭", 0, -1, []() {
        CloseOverlay();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(110) - S(10) - S(110), by, S(110), S(34)), CMD_WS_RESET, L"恢复默认", 1, -1, []() {
        auto& sw = app.store.settings.widget;
        sw.enabled = true;
        sw.mode = L"card";
        sw.skin = L"theme";
        sw.scale = 100;
        sw.opacity = 94;
        sw.showWater = true;
        sw.showDate = true;
        sw.showToday = true;
        sw.alwaysOnTop = true;
        sw.clickThrough = false;
        sw.lowPower = true;
        sw.showTasks = true;
        sw.x = -1;
        sw.y = -1;
        SaveWidgetSettings();
        ShowToast(L"挂件外观已恢复默认");
    });
    Txt(g, L"提示：穿透开启后请从托盘菜单关闭；省电模式每 5 秒刷新。", S(9), false,
        WithAlpha(th.dim2, 200), RectF(lx, by, pw - S(300), S(20)), ALIGN_VCENTER);
}