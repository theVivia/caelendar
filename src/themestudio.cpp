// themestudio.cpp - 主题工坊：预设 / 自定义 / 实时预览 / JSON 导入导出




#include "app.h"
#include <commdlg.h>

using namespace Gdiplus;
using namespace gfx;

bool ExportThemeToFile(const std::wstring& path);
bool ImportThemeFromFile(const std::wstring& path, std::wstring& err);

namespace {

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

void Frame(Graphics& g, float W, float H, float pw, float ph, float& px, float& py) {
    const Theme& th = TH();
    pw = std::min(pw, W - S(40));
    ph = std::min(ph, H - S(30));
    px = (W - pw) / 2;
    py = (H - ph) / 2;
    RectF r(px, py, pw, ph);
    Shadow(g, r, S(18), 8, 26);
    FillRoundRect(g, r, S(18), th.card);
    StrokeRoundRect(g, r, S(18), WithAlpha(th.border, 255), S(1));
    FillVGradient(g, RectF(px, py, pw, S(60)), WithAlpha(th.accent, 10), WithAlpha(th.accent, 0));
}

void ApplyLive() {
    Settings& s = app.store.settings;
    s.theme = L"custom";
    app.themeIdx = 9;
    ApplyThemeFromSettings();
    app.needSave = true;
    if (app.store.settings.widget.enabled) RecreateWidgetWindow();
    UIInvalidate();
}

bool ValidHexColor(const std::wstring& s) {
    if (s.size() != 7 || s[0] != L'#') return false;
    for (int i = 1; i < 7; ++i) {
        wchar_t c = s[i];
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'))) return false;
    }
    return true;
}

std::wstring ThemeJson() {
    Settings& s = app.store.settings;
    return Fmt(L"{\n  \"format\": \"caelendar-theme\",\n  \"version\": 1,\n  \"name\": \"CAELENDAR 自定义主题\",\n"
               L"  \"bg\": \"%s\",\n  \"accent\": \"%s\",\n  \"radius\": %d,\n  \"density\": %d,\n  \"light\": %s\n}\n",
               s.customBg.c_str(), s.customAccent.c_str(), s.themeRadius, s.themeDensity,
               s.customLight ? L"true" : L"false");
}

bool SaveJsonDialog(std::wstring& path) {
    wchar_t file[1024] = { 0 };
    wcsncpy_s(file, L"CAELENDAR-主题.json", _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"CAELENDAR 主题\0*.json\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    path = file;
    return true;
}

bool OpenJsonDialog(std::wstring& path) {
    wchar_t file[1024] = { 0 };
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"CAELENDAR 主题\0*.json\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;
    path = file;
    return true;
}

void ExportTheme() {
    std::wstring path;
    if (!SaveJsonDialog(path)) return;
    std::string body = WideToUtf8(ThemeJson());
    if (!WriteFileBytesAtomic(path, body)) { ShowToast(L"导出主题失败"); return; }
    DebugLog(L"主题导出: %s", path.c_str());
    ShowToast(L"主题 JSON 已导出");
}

void ImportTheme() {
    std::wstring path;
    if (!OpenJsonDialog(path)) return;
    std::wstring err;
    if (!ImportThemeFromFile(path, err)) { ShowToast(err.empty() ? L"主题导入失败" : err); return; }
    DebugLog(L"主题导入: %s", path.c_str());
    ShowToast(L"主题已导入并应用");
    UIInvalidate();
}

}  // namespace

bool ExportThemeToFile(const std::wstring& path) {
    std::string body = WideToUtf8(ThemeJson());
    return WriteFileBytesAtomic(path, body);
}

bool ImportThemeFromFile(const std::wstring& path, std::wstring& err) {
    bool ok = false;
    std::string raw = ReadFileBytes(path, &ok);
    if (!ok || raw.empty()) { err = L"无法读取主题文件"; return false; }
    std::wstring perr;
    JValue root = JsonParse(raw, &perr);
    if (!root.IsObj()) { err = L"主题 JSON 格式无效"; return false; }
    Settings& s = app.store.settings;
    std::wstring bgS = root.Get(L"bg").AsStr();
    std::wstring acS = root.Get(L"accent").AsStr();
    bool any = false;
    if (ValidHexColor(bgS)) { s.customBg = bgS; any = true; }
    if (ValidHexColor(acS)) { s.customAccent = acS; any = true; }
    if (root.Get(L"radius").type == JValue::NUMBER) {
        s.themeRadius = std::max(0, std::min(20, root.Get(L"radius").AsInt(10)));
        any = true;
    }
    if (root.Get(L"density").type == JValue::NUMBER) {
        s.themeDensity = std::max(85, std::min(115, root.Get(L"density").AsInt(100)));
        any = true;
    }
    if (root.Get(L"light").type == JValue::BOOL) { s.customLight = root.Get(L"light").AsBool(false); any = true; }
    if (!any) { err = L"主题文件里没有可识别的字段"; return false; }
    s.theme = L"custom";
    app.themeIdx = 9;
    ApplyThemeFromSettings();
    DoSaveStore();
    return true;
}

void OpenThemeStudio() {
    app.overlay = OV_THEMESTUDIO;
    UIInvalidate();
}

void DrawThemeStudioOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    Settings& s = app.store.settings;
    float pw = S(760), ph = S(640);
    float x, y;
    Frame(g, W, H, pw, ph, x, y);
    Txt(g, L"主题工坊", S(18), true, th.text, RectF(x + S(24), y + S(16), S(200), S(28)));
    Txt(g, L"改动即时生效并保存；圆角/密度直接作用于界面", S(9), false, th.dim2,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_TS_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        DoSaveStore();
        UIInvalidate();
    });

    float lx = x + S(24), lw = pw - S(48);
    float cy = y + S(78);
    // 预设
    Txt(g, L"预设", S(10), false, th.dim2, RectF(lx, cy, S(80), S(18)));
    cy += S(22);
    int n = ThemeCount();
    float cw = (lw - S(8) * 4) / 5;
    for (int i = 0; i < n; ++i) {
        int idx = i;
        RectF r(lx + (i % 5) * (cw + S(8)), cy + (i / 5) * S(32), cw, S(28));
        bool active = (app.themeIdx == i);
        FillRoundRect(g, r, S(7), active ? WithAlpha(th.accent, 38) : WithAlpha(th.panel2, 180));
        StrokeRoundRect(g, r, S(7), active ? WithAlpha(th.accent, 160) : th.borderSoft, S(1));
        FillCircle(g, r.X + S(10), r.Y + r.Height / 2, S(3), ThemeAt(i).accent);
        Txt(g, ThemeAt(i).name, S(9), active, active ? th.accent : th.dim,
            RectF(r.X + S(19), r.Y, r.Width - S(24), r.Height), ALIGN_VCENTER);
        AddHit(r.X, r.Y, r.Width, r.Height, CMD_TS_THEME_BASE + i, [idx]() {
            app.themeIdx = idx;
            app.store.settings.theme = ThemeIdAt(idx);
            ApplyThemeFromSettings();
            app.needSave = true;
            if (app.store.settings.widget.enabled) RecreateWidgetWindow();
            DoSaveStore();
            UIInvalidate();
        });
    }
    cy += S(74);

    // 自定义
    Txt(g, L"自定义", S(10), false, th.dim2, RectF(lx, cy, S(80), S(18)));
    cy += S(22);
    auto swatch = [&](float sx, const std::wstring& label, const std::wstring& hex) {
        Txt(g, label, S(9), false, th.dim2, RectF(sx, cy, S(60), S(16)));
        RectF box(sx, cy + S(18), S(34), S(34));
        FillRoundRect(g, box, S(8), Hex(hex));
        StrokeRoundRect(g, box, S(8), WithAlpha(th.borderSoft, 255), S(1));
        Txt(g, hex, S(9), false, th.dim, RectF(sx + S(42), cy + S(24), S(90), S(20)), ALIGN_VCENTER);
        AddHit(sx, cy, S(150), S(54), 0, nullptr);
    };
    float col1 = lx;
    float col2 = lx + S(170);
    // 强调色
    Txt(g, L"强调色", S(9), false, th.dim2, RectF(col1, cy, S(60), S(16)));
    RectF accBox(col1, cy + S(18), S(34), S(34));
    FillRoundRect(g, accBox, S(8), Hex(s.customAccent));
    StrokeRoundRect(g, accBox, S(8), WithAlpha(th.borderSoft, 255), S(1));
    Txt(g, s.customAccent, S(9), false, th.dim, RectF(col1 + S(42), cy + S(24), S(100), S(20)), ALIGN_VCENTER);
    AddHit(accBox.X, accBox.Y, accBox.Width, accBox.Height, CMD_TS_ACCENT, []() {
        OpenColorPicker(L"选择强调色", app.store.settings.customAccent, [](const std::wstring& hex) {
            app.store.settings.customAccent = hex;
            ApplyLive();
            DoSaveStore();
        });
        app.colorPick.returnOverlay = OV_THEMESTUDIO;
    });
    // 背景色
    Txt(g, L"背景色", S(9), false, th.dim2, RectF(col2, cy, S(60), S(16)));
    RectF bgBox(col2, cy + S(18), S(34), S(34));
    FillRoundRect(g, bgBox, S(8), Hex(s.customBg));
    StrokeRoundRect(g, bgBox, S(8), WithAlpha(th.borderSoft, 255), S(1));
    Txt(g, s.customBg, S(9), false, th.dim, RectF(col2 + S(42), cy + S(24), S(100), S(20)), ALIGN_VCENTER);
    AddHit(bgBox.X, bgBox.Y, bgBox.Width, bgBox.Height, CMD_TS_BG, []() {
        OpenColorPicker(L"选择背景色", app.store.settings.customBg, [](const std::wstring& hex) {
            app.store.settings.customBg = hex;
            ApplyLive();
            DoSaveStore();
        });
        app.colorPick.returnOverlay = OV_THEMESTUDIO;
    });
    // 浅色 / 圆角 / 密度
    float ctrl = lx + S(360);
    UI::CheckBox(g, RectF(ctrl, cy, S(110), S(24)), CMD_TS_LIGHT, L"浅色模式", s.customLight, []() {
        app.store.settings.customLight = !app.store.settings.customLight;
        ApplyLive();
        DoSaveStore();
    });
    UI::Stepper(g, RectF(ctrl, cy + S(34), S(170), S(28)), CMD_TS_RADIUS_DEC, CMD_TS_RADIUS_INC,
                Fmt(L"圆角 %d", s.themeRadius), []() {
                    app.store.settings.themeRadius = std::max(0, app.store.settings.themeRadius - 1);
                    ApplyLive();
                    DoSaveStore();
                }, []() {
                    app.store.settings.themeRadius = std::min(20, app.store.settings.themeRadius + 1);
                    ApplyLive();
                    DoSaveStore();
                }, 9.5f);
    UI::Stepper(g, RectF(ctrl, cy + S(70), S(170), S(28)), CMD_TS_DENSITY_DEC, CMD_TS_DENSITY_INC,
                Fmt(L"密度 %d%%", s.themeDensity), []() {
                    app.store.settings.themeDensity = std::max(85, app.store.settings.themeDensity - 5);
                    ApplyLive();
                    DoSaveStore();
                }, []() {
                    app.store.settings.themeDensity = std::min(115, app.store.settings.themeDensity + 5);
                    ApplyLive();
                    DoSaveStore();
                }, 9.5f);
    cy += S(112);

    // 实时预览
    Txt(g, L"实时预览", S(10), false, th.dim2, RectF(lx, cy, S(80), S(18)));
    cy += S(22);
    RectF pv(lx, cy, lw, S(150));
    FillRoundRect(g, pv, S(10), th.bg);
    StrokeRoundRect(g, pv, S(10), WithAlpha(th.borderSoft, 255), S(1));
    // 小侧栏 + 卡片 + 按钮，全用当前主题色
    FillRoundRect(g, RectF(pv.X + S(10), pv.Y + S(10), S(120), pv.Height - S(20)), S(8), th.panel);
    Txt(g, L"星历", S(13), true, th.accent, RectF(pv.X + S(20), pv.Y + S(18), S(100), S(20)));
    Txt(g, L"18", S(20), true, th.text, RectF(pv.X + S(20), pv.Y + S(46), S(60), S(28)));
    Txt(g, L"10月 · 周日", S(8.5f), false, th.dim, RectF(pv.X + S(20), pv.Y + S(74), S(100), S(16)));
    FillRoundRect(g, RectF(pv.X + S(20), pv.Y + S(96), S(90), S(5)), S(2), WithAlpha(th.accent, 90));
    RectF pc(pv.X + S(146), pv.Y + S(10), pv.Width - S(156), pv.Height - S(20));
    FillRoundRect(g, pc, S(8), th.panel2);
    Txt(g, L"高等数学", S(11), true, th.text, RectF(pc.X + S(14), pc.Y + S(10), pc.Width - S(28), S(18)));
    Txt(g, L"08:00 – 09:40 · 教三 201", S(8.5f), false, th.dim2, RectF(pc.X + S(14), pc.Y + S(30), pc.Width - S(28), S(16)));
    FillRoundRect(g, RectF(pc.X + S(14), pc.Y + S(50), S(70), S(24)), S(6), th.accent);
    Txt(g, L"完成", S(8.5f), true, th.onAccent, RectF(pc.X + S(14), pc.Y + S(50), S(70), S(24)), ALIGN_CENTER | ALIGN_VCENTER);
    FillRoundRect(g, RectF(pc.X + S(92), pc.Y + S(50), S(70), S(24)), S(6), th.card);
    StrokeRoundRect(g, RectF(pc.X + S(92), pc.Y + S(50), S(70), S(24)), S(6), WithAlpha(th.border, 255), S(1));
    Txt(g, L"稍后", S(8.5f), false, th.dim, RectF(pc.X + S(92), pc.Y + S(50), S(70), S(24)), ALIGN_CENTER | ALIGN_VCENTER);
    Txt(g, L"文字对比度与按钮/卡片圆角会实时变化", S(8.5f), false, th.dim2,
        RectF(pc.X + S(14), BY(pc) - S(26), pc.Width - S(28), S(16)));

    // 底部
    float by = y + ph - S(54);
    UI::Btn(g, RectF(lx, by, S(110), S(34)), CMD_TS_EXPORT, L"导出 JSON", 1, ICO_COPY, []() { ExportTheme(); });
    UI::Btn(g, RectF(lx + S(120), by, S(110), S(34)), CMD_TS_IMPORT, L"导入 JSON", 1, ICO_FOLDER, []() { ImportTheme(); });
    Txt(g, L"主题 JSON 只包含颜色/圆角/密度，不包含日程数据", S(8.5f), false, th.dim2,
        RectF(lx + S(244), by, pw - S(244) - S(24) - S(120), S(34)), ALIGN_VCENTER, false, true);
    UI::Btn(g, RectF(x + pw - S(24) - S(96), by, S(96), S(34)), CMD_TS_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        DoSaveStore();
        UIInvalidate();
    });
}