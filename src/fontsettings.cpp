// fontsettings.cpp - 字体与字号设置（分区域自定义）
#include "app.h"
#include <algorithm>
#include <vector>
#include <string>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

static const wchar_t* kPartNames[FP_COUNT] = {
    L"标题栏", L"侧边栏", L"月视图", L"日程卡片",
    L"周·日·日程列表", L"编辑器", L"弹窗与设置", L"桌面挂件"
};

static void SaveFontsAndApply() {
    ApplyFontSettings();
    app.store.Touch();
    app.needSave = false;
    DoSaveStore();
    UIInvalidate();
}

void OpenFontSettings() {
    app.overlay = OV_FONTS;
    app.fontPickerOpen = false;
    app.fontPickerTarget = -1;
    app.fontPickerScroll = 0;
    if (app.fontList.empty()) app.fontList = ListInstalledFonts();
    UIInvalidate();
}

static std::wstring DefaultFamilyForTarget(int target) {
    if (target == 0) return L"Noto Sans SC";
    if (target == 1) return L"Bahnschrift";
    return L"";
}

static std::wstring CurrentFamilyForTarget(int target) {
    if (target == 0) return app.store.settings.fontUI;
    if (target == 1) return app.store.settings.fontNum;
    if (target >= 2 && target - 2 < FP_COUNT) return app.store.settings.fontParts[target - 2].family;
    return L"";
}

static void SetFamilyForTarget(int target, const std::wstring& family) {
    if (target == 0) app.store.settings.fontUI = family.empty() ? L"Noto Sans SC" : family;
    else if (target == 1) app.store.settings.fontNum = family.empty() ? L"Bahnschrift" : family;
    else if (target >= 2 && target - 2 < FP_COUNT) app.store.settings.fontParts[target - 2].family = family;
}

// ---------------- 字体选择弹层 ----------------
static void DrawFontPicker(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(460), ph = std::min(S(560), H - S(80));
    float x = (W - pw) / 2, y = (H - ph) / 2;
    Shadow(g, RectF(x, y, pw, ph), S(14), 7, 40);
    FillRoundRect(g, RectF(x, y, pw, ph), S(14), th.card);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(14), WithAlpha(th.border, 255), S(1));
    int target = app.fontPickerTarget;
    std::wstring desc = target == 0 ? L"界面中文字体" : target == 1 ? L"数字 / 时间字体"
                     : (target >= 2 && target - 2 < FP_COUNT ? std::wstring(kPartNames[target - 2]) + L" 区域字体" : L"字体");
    Txt(g, L"选择字体 · " + desc, S(14), true, th.text, RectF(x + S(20), y + S(14), pw - S(60), S(24)));
    UI::IconBtn(g, RectF(x + pw - S(40), y + S(12), S(26), S(26)), CMD_FONT_CLOSE, ICO_CLOSE, 12, []() {
        app.fontPickerOpen = false;
        UIInvalidate();
    });
    float listX = x + S(14), listY = y + S(52), listW = pw - S(28), listH = ph - S(100);
    FillRoundRect(g, RectF(listX, listY, listW, listH), S(10), WithAlpha(th.bg, 150));
    StrokeRoundRect(g, RectF(listX, listY, listW, listH), S(10), th.borderSoft, S(1));
    float rowH = S(26);
    int total = (int)app.fontList.size() + 1;      // 第 0 行 = 默认
    int visible = std::max(1, (int)(listH / rowH));
    int maxScroll = std::max(0, total - visible);
    app.fontPickerScroll = std::max(0, std::min(app.fontPickerScroll, maxScroll));
    std::wstring current = CurrentFamilyForTarget(target);
    g.SetClip(RectF(listX, listY, listW, listH));
    for (int i = 0; i < visible; ++i) {
        int idx = app.fontPickerScroll + i;
        if (idx >= total) break;
        RectF row(listX + S(4), listY + S(4) + i * rowH, listW - S(8), rowH - S(2));
        bool isDefault = (idx == 0);
        std::wstring name = isDefault ? L"默认（跟随全局）" : app.fontList[idx - 1];
        if (isDefault && target <= 1) name = L"默认（" + DefaultFamilyForTarget(target) + L"）";
        bool selected = isDefault ? current.empty() : (current == name);
        bool hover = IsHovered(CMD_FONT_PICK_BASE + idx);
        if (hover || selected) FillRoundRect(g, row, S(6), selected ? WithAlpha(th.accent, 26) : WithAlpha(th.cardHover, 200));
        if (selected) FillCircle(g, row.X + S(10), row.Y + row.Height / 2, S(2.6f), th.accent);
        // 字体名用它自己的字体预览
        std::wstring oldFam = fontRuntime.parts[FP_DIALOG].family;
        if (!isDefault) fontRuntime.parts[FP_DIALOG].family = name;
        Txt(g, name, S(11.5f), selected, selected ? th.text : th.dim,
            RectF(row.X + S(20), row.Y, row.Width - S(28), row.Height), ALIGN_VCENTER);
        fontRuntime.parts[FP_DIALOG].family = oldFam;
        int idxCopy = idx;
        AddHit(row.X, row.Y, row.Width, row.Height, CMD_FONT_PICK_BASE + idx, [idxCopy]() {
            int t = app.fontPickerTarget;
            std::wstring fam = (idxCopy == 0) ? std::wstring() : app.fontList[idxCopy - 1];
            SetFamilyForTarget(t, fam);
            app.fontPickerOpen = false;
            SaveFontsAndApply();
        });
    }
    g.ResetClip();
    UI::Scrollbar(g, RectF(listX + listW - S(7), listY + S(4), S(4), listH - S(8)),
                  listH - S(8), total * rowH, app.fontPickerScroll * rowH);
    Txt(g, L"滚轮滚动 · Esc 关闭", S(9), false, th.dim2, RectF(x + S(20), y + ph - S(30), pw - S(40), S(18)));
}

// ---------------- 主面板 ----------------
void DrawFontOverlay(Graphics& g, float W, float H) {
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    const Theme& th = TH();
    Settings& s = app.store.settings;
    float pw = S(900), ph = S(640);
    pw = std::min(pw, W - S(40));
    ph = std::min(ph, H - S(24));
    float x = (W - pw) / 2, y = (H - ph) / 2;
    Shadow(g, RectF(x, y, pw, ph), S(16), 8, 26);
    FillRoundRect(g, RectF(x, y, pw, ph), S(16), th.card);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(16), th.border, S(1));
    Txt(g, L"字体与字号", S(17), true, th.text, RectF(x + S(24), y + S(14), S(200), S(26)));
    Txt(g, L"默认：中文 Noto Sans SC / 数字 Bahnschrift；每个区域可单独指定字体与字号缩放。",
        S(9.5f), false, th.dim2, RectF(x + S(140), y + S(20), pw - S(220), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(46), y + S(12), S(28), S(28)), CMD_FONT_CLOSE, ICO_CLOSE, 13, []() {
        app.fontPickerOpen = false;
        app.overlay = OV_SETTINGS;
        UIInvalidate();
    });
    float pad = S(24), colW = pw - pad * 2;
    float yy = y + S(52);
    // 全局
    UI::SectionTitle(g, L"全局", RectF(x + pad, yy, S(80), S(18)));
    yy += S(22);
    auto fontButton = [&](int target, RectF r) {
        const std::wstring cur = CurrentFamilyForTarget(target);
        std::wstring label = cur.empty() ? L"默认" : cur;
        bool hover = IsHovered(CMD_FONT_PICK_BASE + 900 + target);
        FillRoundRect(g, r, S(7), hover ? th.cardHover : th.panel2);
        StrokeRoundRect(g, r, S(7), hover ? WithAlpha(th.accent, 120) : th.borderSoft, S(1));
        Txt(g, label, S(11), false, hover ? th.text : th.dim, RectF(r.X + S(10), r.Y, r.Width - S(50), r.Height), ALIGN_VCENTER, false, true);
        Txt(g, L"▾", S(10), false, th.dim2, RectF(RX(r) - S(24), r.Y, S(18), r.Height), ALIGN_CENTER | ALIGN_VCENTER);
        AddHit(r.X, r.Y, r.Width, r.Height, CMD_FONT_PICK_BASE + 900 + target, [target]() {
            app.fontPickerOpen = true;
            app.fontPickerTarget = target;
            app.fontPickerScroll = 0;
            UIInvalidate();
        });
    };
    Txt(g, L"界面中文字体", S(10.5f), false, th.dim, RectF(x + pad, yy, S(130), S(30)), ALIGN_VCENTER);
    fontButton(0, RectF(x + pad + S(150), yy, S(300), S(28)));
    Txt(g, L"数字 / 时间字体", S(10.5f), false, th.dim, RectF(x + pad + S(470), yy, S(130), S(28)), ALIGN_VCENTER);
    fontButton(1, RectF(x + pad + S(610), yy, S(240), S(28)));
    yy += S(42);
    Txt(g, L"全局字号", S(10.5f), false, th.dim, RectF(x + pad, yy, S(130), S(28)), ALIGN_VCENTER);
    UI::Stepper(g, RectF(x + pad + S(150), yy, S(190), S(28)), CMD_FONT_GLOBAL_DEC, CMD_FONT_GLOBAL_INC,
                Fmt(L"%d%%", s.fontScale), []() {
                    app.store.settings.fontScale = std::max(80, app.store.settings.fontScale - 5);
                    SaveFontsAndApply();
                }, []() {
                    app.store.settings.fontScale = std::min(150, app.store.settings.fontScale + 5);
                    SaveFontsAndApply();
                }, 10.5f);
    Txt(g, L"所有文字整体缩放（影响全部区域）", S(9), false, th.dim2, RectF(x + pad + S(354), yy, S(380), S(28)), ALIGN_VCENTER);
    yy += S(44);
    // 分区
    UI::SectionTitle(g, L"分区设置", RectF(x + pad, yy, S(200), S(18)));
    Txt(g, L"字体（默认=跟随全局）", S(9), false, th.dim2, RectF(x + pad + S(150), yy, S(260), S(18)), ALIGN_VCENTER);
    Txt(g, L"字号缩放", S(9), false, th.dim2, RectF(x + pad + S(640), yy, S(120), S(18)), ALIGN_VCENTER);
    yy += S(22);
    for (int i = 0; i < FP_COUNT; ++i) {
        RectF row(x + pad - S(4), yy - S(2), colW + S(8), S(34));
        if (i % 2 == 0) FillRoundRect(g, row, S(7), WithAlpha(th.bg, 90));
        Txt(g, kPartNames[i], S(11), false, th.text, RectF(x + pad + S(6), yy, S(160), S(30)), ALIGN_VCENTER);
        fontButton(2 + i, RectF(x + pad + S(170), yy + S(2), S(300), S(26)));
        int pct = s.fontParts[i].size;
        UI::Stepper(g, RectF(x + pad + S(500), yy + S(2), S(150), S(26)), CMD_FONT_SIZE_DEC_BASE + i, CMD_FONT_SIZE_INC_BASE + i,
                    Fmt(L"%d%%", pct), [i]() {
                        app.store.settings.fontParts[i].size = std::max(60, app.store.settings.fontParts[i].size - 5);
                        SaveFontsAndApply();
                    }, [i]() {
                        app.store.settings.fontParts[i].size = std::min(180, app.store.settings.fontParts[i].size + 5);
                        SaveFontsAndApply();
                    }, 10);
        yy += S(38);
    }
    // 底部
    float by = y + ph - S(52);
    Txt(g, L"提示：字号过大可能出现文字重叠；中文建议 Noto Sans SC / 微软雅黑 / 思源黑体，数字建议 Bahnschrift / Consolas。",
        S(9), false, th.dim2, RectF(x + pad, by + S(8), pw - pad * 2 - S(320), S(20)), ALIGN_LEFT, false, true);
    UI::Btn(g, RectF(x + pw - pad - S(110), by, S(110), S(32)), CMD_FONT_RESET, L"恢复默认", 1, ICO_REFRESH, []() {
        Settings& st = app.store.settings;
        st.fontUI = L"Noto Sans SC";
        st.fontNum = L"Bahnschrift";
        st.fontScale = 100;
        for (int i = 0; i < FP_COUNT; ++i) {
            st.fontParts[i].family.clear();
            st.fontParts[i].size = 100;
        }
        SaveFontsAndApply();
        ShowToast(L"字体设置已恢复默认");
    });
    UI::Btn(g, RectF(x + pw - pad - S(110) * 2 - S(10), by, S(110), S(32)), CMD_FONT_CLOSE, L"关闭", 0, ICO_CHECK, []() {
        app.fontPickerOpen = false;
        app.overlay = OV_SETTINGS;
        UIInvalidate();
    });
    if (app.fontPickerOpen) DrawFontPicker(g, W, H);
}
