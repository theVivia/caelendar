// theme.cpp
#include "theme.h"
#include <algorithm>
#include <cmath>

using namespace gfx;

static Theme Make(const wchar_t* id, const wchar_t* name,
                  const wchar_t* bg, const wchar_t* bgTop, const wchar_t* bgBottom,
                  const wchar_t* panel, const wchar_t* panel2, const wchar_t* card,
                  const wchar_t* cardHover, const wchar_t* text, const wchar_t* dim,
                  const wchar_t* dim2, const wchar_t* accent, const wchar_t* accent2,
                  const wchar_t* border, const wchar_t* borderSoft,
                  const wchar_t* danger, const wchar_t* success, const wchar_t* warn,
                  const wchar_t* onAccent) {
    Theme t{};
    t.id = id; t.name = name;
    t.bg = Hex(bg); t.bgTop = Hex(bgTop); t.bgBottom = Hex(bgBottom);
    t.panel = Hex(panel); t.panel2 = Hex(panel2); t.card = Hex(card); t.cardHover = Hex(cardHover);
    t.text = Hex(text); t.dim = Hex(dim); t.dim2 = Hex(dim2);
    t.accent = Hex(accent); t.accent2 = Hex(accent2);
    t.border = Hex(border); t.borderSoft = Hex(borderSoft);
    t.danger = Hex(danger); t.success = Hex(success); t.warn = Hex(warn);
    t.onAccent = Hex(onAccent);
    return t;
}

// 8 套深色 + 1 套真正的浅色（浅色单独配可读色，不是把深色反转）
static Theme g_themes[9] = {
    // 星轨（默认）：近黑 + 金 + 青
    Make(L"astral", L"星轨", L"#07080D", L"#0A0D18", L"#05060A",
         L"#0B0E18", L"#0E1220", L"#111528", L"#171D33",
         L"#ECF0FA", L"#8B93AB", L"#5A6178", L"#E3C177", L"#74D8FF",
         L"#232A42", L"#161B2C", L"#FF6B7A", L"#59D6A0", L"#FFB454", L"#15110A"),
    // 熔金：暖琥珀
    Make(L"ember", L"熔金", L"#0B0806", L"#120C07", L"#080504",
         L"#130E09", L"#1A130D", L"#201710", L"#2A1E14",
         L"#F5EDE2", L"#B29A82", L"#7A6754", L"#F0A24B", L"#FF7A59",
         L"#3A2A1B", L"#261B10", L"#FF6B7A", L"#7FD69E", L"#FFC46B", L"#1A0F04"),
    // 极光：青绿
    Make(L"aurora", L"极光", L"#040B0A", L"#07110F", L"#030807",
         L"#081310", L"#0A1815", L"#0D1E1A", L"#122823",
         L"#E6F5EF", L"#7FA79B", L"#547267", L"#5FE3B0", L"#7FD9FF",
         L"#1A3A31", L"#10241E", L"#FF6B7A", L"#5FE3B0", L"#FFD166", L"#03211A"),
    // 绯夜：玫红
    Make(L"crimson", L"绯夜", L"#0A0508", L"#12080E", L"#070305",
         L"#130A10", L"#1A0D15", L"#21101A", L"#2C1622",
         L"#F8E9EF", L"#B18CA0", L"#7A5A6B", L"#FF6E8E", L"#FFC3A0",
         L"#3E1E2C", L"#291521", L"#FF5C7A", L"#6EE7B7", L"#FFB454", L"#260611"),
    // 月白：冰蓝（仍为深色）
    Make(L"moon", L"月白", L"#070910", L"#0A0D16", L"#05070C",
         L"#0C101A", L"#0F1422", L"#131A2B", L"#1A2338",
         L"#EAF0FF", L"#8894B3", L"#59627E", L"#A9C6FF", L"#E8D9A6",
         L"#26304A", L"#171E31", L"#FF6B7A", L"#59D6A0", L"#FFC46B", L"#0C1526"),
    // 樱夜：雾粉
    Make(L"sakura", L"樱夜", L"#08050A", L"#0F0912", L"#060408",
         L"#100A14", L"#160D1C", L"#1D1126", L"#271733",
         L"#F7ECF6", L"#B49AB8", L"#7C657F", L"#FF87B7", L"#B69CFF",
         L"#38203F", L"#25152D", L"#FF6B8A", L"#7FD6A8", L"#FFC46B", L"#210A16"),
    // 深海：电光蓝
    Make(L"abyss", L"深海", L"#03070E", L"#050C18", L"#02050A",
         L"#06101E", L"#081528", L"#0A1B33", L"#0E2442",
         L"#E8F2FF", L"#7F97B8", L"#526D8E", L"#4CC9F0", L"#9D7BFF",
         L"#123152", L"#0B2138", L"#FF6B7A", L"#59D6A0", L"#FFC46B", L"#03101F"),
    // 森夜：青柠
    Make(L"forest", L"森夜", L"#040806", L"#07100B", L"#030604",
         L"#08130D", L"#0A1A12", L"#0D2117", L"#122C1F",
         L"#EAF7EF", L"#83A893", L"#557A64", L"#9BE15D", L"#5FD3A6",
         L"#1B3A27", L"#11271B", L"#FF6B7A", L"#59D6A6", L"#FFD166", L"#0A1F10"),
    // 浅色：暖纸 + 深棕金，所有文字/背景对比度单独校过
    Make(L"daylight", L"浅色", L"#F4F1EA", L"#FBFAF6", L"#EBE6DA",
         L"#FFFFFF", L"#F4F1E8", L"#FFFFFF", L"#F7F2E5",
         L"#1C2230", L"#586174", L"#858EA1", L"#A8741F", L"#0F766E",
         L"#D8D0BE", L"#E8E2D4", L"#C2413F", L"#1F7A55", L"#9A6212", L"#FFFDF7"),
};

static Theme g_custom = [] {
    Theme t = g_themes[0];
    t.id = L"custom";
    t.name = L"自定义";
    t.radius = 10;
    t.density = 100;
    return t;
}();

const Theme& ThemeAt(int i) {
    if (i == 9) return g_custom;
    if (i < 0 || i >= 9) i = 0;
    return g_themes[i];
}
int ThemeCount() { return 10; }
std::wstring ThemeIdAt(int i) {
    if (i == 9) return L"custom";
    if (i < 0 || i >= 9) i = 0;
    return g_themes[i].id;
}
int ThemeIndexById(const std::wstring& id) {
    if (id == L"custom") return 9;
    for (int i = 0; i < 9; ++i) if (id == g_themes[i].id) return i;
    return 0;
}

namespace {
int ClampI(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
Color Mix(Color a, Color b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return Color(255,
                 (BYTE)(a.GetR() + (b.GetR() - a.GetR()) * t),
                 (BYTE)(a.GetG() + (b.GetG() - a.GetG()) * t),
                 (BYTE)(a.GetB() + (b.GetB() - a.GetB()) * t));
}
float Luma(Color c) { return (0.2126f * c.GetR() + 0.7152f * c.GetG() + 0.0722f * c.GetB()) / 255.0f; }
bool ValidHex(const std::wstring& s) {
    if (s.size() != 7 || s[0] != L'#') return false;
    for (int i = 1; i < 7; ++i) {
        wchar_t c = s[i];
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'))) return false;
    }
    return true;
}
}  // namespace

Theme BuildCustomTheme(const std::wstring& bgHex, const std::wstring& accentHex,
                       int radius, int density, bool light) {
    std::wstring bgS = ValidHex(bgHex) ? bgHex : L"#07080D";
    std::wstring acS = ValidHex(accentHex) ? accentHex : L"#E3C177";
    Color bg = Hex(bgS), accent = Hex(acS);
    Theme t{};
    t.id = L"custom";
    t.name = L"自定义";
    t.accent = accent;
    t.bg = bg;
    t.radius = ClampI(radius, 0, 20);
    t.density = ClampI(density, 85, 115);
    t.light = light;
    const Color white(255, 255, 255, 255);
    const Color black(255, 0, 0, 0);
    if (light) {
        t.bgTop = Mix(bg, white, 0.55f);
        t.bgBottom = Mix(bg, black, 0.05f);
        t.panel = Mix(bg, white, 0.82f);
        t.panel2 = Mix(bg, white, 0.70f);
        t.card = Mix(bg, white, 0.92f);
        t.cardHover = Mix(t.card, accent, 0.10f);
        t.text = Mix(bg, black, 0.82f);
        t.dim = Mix(bg, black, 0.62f);
        t.dim2 = Mix(bg, black, 0.45f);
        t.border = Mix(bg, black, 0.16f);
        t.borderSoft = Mix(bg, black, 0.09f);
        t.accent2 = Mix(accent, Hex(L"#0F766E"), 0.55f);
        t.danger = Hex(L"#C2413F");
        t.success = Hex(L"#1F7A55");
        t.warn = Hex(L"#9A6212");
        t.onAccent = Luma(accent) > 0.62f ? Hex(L"#1A1408") : Hex(L"#FFFDF7");
    } else {
        t.bgTop = Mix(bg, white, 0.05f);
        t.bgBottom = Mix(bg, black, 0.40f);
        t.panel = Mix(bg, white, 0.06f);
        t.panel2 = Mix(bg, white, 0.10f);
        t.card = Mix(bg, white, 0.13f);
        t.cardHover = Mix(bg, white, 0.20f);
        t.text = Mix(bg, white, 0.92f);
        t.dim = Mix(bg, white, 0.55f);
        t.dim2 = Mix(bg, white, 0.35f);
        t.border = Mix(bg, white, 0.19f);
        t.borderSoft = Mix(bg, white, 0.11f);
        t.accent2 = Mix(accent, Hex(L"#74D8FF"), 0.55f);
        t.danger = Hex(L"#FF6B7A");
        t.success = Hex(L"#59D6A0");
        t.warn = Hex(L"#FFB454");
        t.onAccent = Luma(accent) > 0.62f ? Hex(L"#1A1408") : Hex(L"#FFFFFF");
    }
    return t;
}

void ApplyCustomTheme(const std::wstring& bgHex, const std::wstring& accentHex,
                      int radius, int density, bool light) {
    g_custom = BuildCustomTheme(bgHex, accentHex, radius, density, light);
}
