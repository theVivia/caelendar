// gfx.cpp
#include "gfx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gfx {

static Gdiplus::Bitmap* g_brandLogo = nullptr;
Gdiplus::Bitmap* BrandLogo() {
    if (g_brandLogo) return g_brandLogo;
    HRSRC hr = FindResourceW(nullptr, MAKEINTRESOURCEW(200), RT_RCDATA);
    if (!hr) return nullptr;
    HGLOBAL hg = LoadResource(nullptr, hr);
    DWORD sz = SizeofResource(nullptr, hr);
    const void* data = hg ? LockResource(hg) : nullptr;
    if (!data || sz == 0) return nullptr;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if (!mem) return nullptr;
    void* dst = GlobalLock(mem);
    if (!dst) { GlobalFree(mem); return nullptr; }
    memcpy(dst, data, sz);
    GlobalUnlock(mem);
    IStream* stream = nullptr;
    if (SUCCEEDED(CreateStreamOnHGlobal(mem, TRUE, &stream))) {
        g_brandLogo = Gdiplus::Bitmap::FromStream(stream);
        stream->Release();
    }
    if (g_brandLogo && g_brandLogo->GetLastStatus() != Gdiplus::Ok) {
        delete g_brandLogo;
        g_brandLogo = nullptr;
    }
    return g_brandLogo;
}

float dpi = 1.0f;

FontRuntime fontRuntime;
int gFontPart = -1;

struct FontKey {
    int px = 12;
    int bold = 0;
    std::wstring fam;
    bool operator<(const FontKey& o) const {
        if (px != o.px) return px < o.px;
        if (bold != o.bold) return bold < o.bold;
        return fam < o.fam;
    }
};
static std::map<FontKey, Font*> g_fonts;
static std::vector<Font*> g_allFonts;

void Init() { }
void Shutdown() {
    for (auto* f : g_allFonts) delete f;
    g_allFonts.clear();
    g_fonts.clear();
}
void SetFontPart(int p) { gFontPart = p; }

static Font* GetFontEx(float px, bool bold, const std::wstring& famName) {
    int ipx = (int)std::lround(px < 1 ? 1 : px);
    FontKey k{ ipx, bold ? 1 : 0, famName };
    auto it = g_fonts.find(k);
    if (it != g_fonts.end()) return it->second;
    const std::wstring& fam = famName.empty() ? std::wstring(L"Noto Sans SC") : famName;
    FontFamily ffPrimary(fam.c_str());
    FontFamily ffNoto(L"Noto Sans SC");
    FontFamily ffSegoe(L"Segoe UI");
    FontFamily ffArial(L"Arial");
    FontFamily* useFam = ffPrimary.IsAvailable() ? &ffPrimary
                       : (ffNoto.IsAvailable() ? &ffNoto : (ffSegoe.IsAvailable() ? &ffSegoe : &ffArial));
    Gdiplus::Font* f = new Font(useFam, (Gdiplus::REAL)ipx, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    if (f->GetLastStatus() != Ok) {
        delete f;
        f = new Font(&ffArial, (Gdiplus::REAL)ipx, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    }
    g_fonts[k] = f;
    g_allFonts.push_back(f);
    return f;
}

// 统一入口：按当前区域 + 全局设置换算字号与字体
static bool HasCJK(const std::wstring& s) {
    for (wchar_t c : s) {
        if ((c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) ||
            (c >= 0xF900 && c <= 0xFAFF) || (c >= 0x3000 && c <= 0x303F) ||
            (c >= 0xFF00 && c <= 0xFFEF))
            return true;
    }
    return false;
}

Font* GetFont(float px, bool bold, bool display, const std::wstring* text) {
    float scale = fontRuntime.globalScale;
    std::wstring fam;
    if (gFontPart >= 0 && gFontPart < FP_COUNT) {
        scale *= fontRuntime.parts[gFontPart].scale;
        fam = fontRuntime.parts[gFontPart].family;
    }
    bool cjk = text && HasCJK(*text);
    if (cjk) {
        // 中文统一用 UI 字体：避免数字字体缺汉字时回退成宋体
        std::wstring uif = fontRuntime.uiFamily;
        if (uif.empty() || uif == fontRuntime.numFamily) uif = L"Noto Sans SC";
        fam = uif;
    } else if (fam.empty()) {
        fam = display ? fontRuntime.numFamily : fontRuntime.uiFamily;
    }
    if (scale <= 0.05f) scale = 1.0f;
    return GetFontEx(px * scale, bold, fam);
}

static int CALLBACK EnumFontCallback(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lp) {
    auto* out = (std::vector<std::wstring>*)lp;
    std::wstring name = lf->lfFaceName;
    if (!name.empty() && name[0] != L'@') out->push_back(name);
    return 1;
}
std::vector<std::wstring> ListInstalledFonts() {
    std::vector<std::wstring> fonts;
    HDC dc = GetDC(nullptr);
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)EnumFontCallback, (LPARAM)&fonts, 0);
    ReleaseDC(nullptr, dc);
    std::sort(fonts.begin(), fonts.end());
    fonts.erase(std::unique(fonts.begin(), fonts.end()), fonts.end());
    if (fonts.empty()) fonts.push_back(L"Noto Sans SC");
    return fonts;
}
static void MakeFormat(StringFormat& fmt, int align, bool ellipsis, bool wrap) {
    fmt.SetAlignment((StringAlignment)(align & 3));
    fmt.SetLineAlignment((StringAlignment)((align & 0x30) == 0x10 ? StringAlignmentCenter
                        : ((align & 0x30) == 0x20 ? StringAlignmentFar : StringAlignmentNear)));
    if (!wrap) fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    else fmt.SetFormatFlags(StringFormatFlagsLineLimit);
    if (ellipsis) fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    else fmt.SetTrimming(StringTrimmingNone);
}

static bool IsCJKChar(wchar_t c) {
    return (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) ||
           (c >= 0xF900 && c <= 0xFAFF) || (c >= 0x3000 && c <= 0x303F) ||
           (c >= 0xFF00 && c <= 0xFFEF);
}

static std::wstring ResolveLatinFamily(bool display) {
    std::wstring fam;
    if (gFontPart >= 0 && gFontPart < FP_COUNT) fam = fontRuntime.parts[gFontPart].family;
    if (fam.empty()) fam = display ? fontRuntime.numFamily : fontRuntime.uiFamily;
    return fam;
}

static std::wstring ResolveCJKFamily() {
    std::wstring uif = fontRuntime.uiFamily;
    if (uif.empty() || uif == fontRuntime.numFamily) uif = L"Noto Sans SC";
    return uif;
}

static float ResolveFontScale() {
    float scale = fontRuntime.globalScale;
    if (gFontPart >= 0 && gFontPart < FP_COUNT) scale *= fontRuntime.parts[gFontPart].scale;
    if (scale <= 0.05f) scale = 1.0f;
    return scale;
}

struct TextRun {
    std::wstring text;
    bool cjk = false;
    Font* font = nullptr;
    float width = 0;
    float ascent = 0, descent = 0;
};
static float MeasureRun(Graphics& g, const std::wstring& t, Font* f) {
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoFitBlackBox);
    fmt.SetTrimming(StringTrimmingNone);
    RectF layout(0, 0, 100000, 100000), bounds;
    g.MeasureString(t.c_str(), -1, f, layout, &fmt, &bounds);
    float pad = f->GetSize() / 3.0f;

    return std::max(0.0f, bounds.Width - pad);
}

static void FontRunMetrics(Font* f, float& ascent, float& descent) {
    FontFamily fam;
    int em = 0;
    if (f->GetFamily(&fam) == Ok) em = fam.GetEmHeight(f->GetStyle());
    if (em > 0) {
        ascent = f->GetSize() * fam.GetCellAscent(f->GetStyle()) / (float)em;
        descent = f->GetSize() * fam.GetCellDescent(f->GetStyle()) / (float)em;
    } else {
        ascent = f->GetSize() * 0.8f;
        descent = f->GetSize() * 0.2f;
    }
}

static void DrawMixedRuns(Graphics& g, const std::wstring& s, float px, bool bold, Color c,
                          RectF r, int align, bool display, bool ellipsis) {
    float scale = ResolveFontScale();
    Font* latin = GetFontEx(px * scale, bold, ResolveLatinFamily(display));
    Font* cjk = GetFontEx(px * scale, bold, ResolveCJKFamily());
    std::vector<TextRun> runs;
    for (size_t i = 0; i < s.size();) {
        bool isC = IsCJKChar(s[i]);
        size_t j = i;
        while (j < s.size() && IsCJKChar(s[j]) == isC) ++j;
        TextRun run;
        run.text = s.substr(i, j - i);
        run.cjk = isC;
        run.font = isC ? cjk : latin;
        run.width = MeasureRun(g, run.text, run.font);
        FontRunMetrics(run.font, run.ascent, run.descent);
        runs.push_back(run);
        i = j;
    }
    float total = 0, maxAscent = 0, maxDescent = 0;
    for (auto& run : runs) {
        total += run.width;
        maxAscent = std::max(maxAscent, run.ascent);
        maxDescent = std::max(maxDescent, run.descent);
    }
    bool trimmed = ellipsis && total > r.Width;
    float ellW = 0;
    Font* ellFont = cjk;
    float ellAscent = 0, ellDescent = 0;
    if (trimmed) {
        ellW = MeasureRun(g, L"…", ellFont);
        FontRunMetrics(ellFont, ellAscent, ellDescent);
    }
    int ha = align & 3;
    float startX = r.X;
    if (!trimmed) {
        if (ha == ALIGN_CENTER) startX = r.X + (r.Width - total) / 2;
        else if (ha == ALIGN_RIGHT) startX = r.X + r.Width - total;
    }
    // 基线对齐：不同字体按 ascent 计算，避免数字与汉字上下错位
    float baseY;
    int va = align & 0x30;
    float totalH = maxAscent + maxDescent;
    if (va == ALIGN_VCENTER) baseY = r.Y + (r.Height - totalH) / 2 + maxAscent;
    else if (va == ALIGN_BOTTOM) baseY = r.Y + r.Height - maxDescent;
    else baseY = r.Y + maxAscent;
    SolidBrush br(c);
    float x = startX;
    float limit = r.X + r.Width - (trimmed ? ellW + S(2) : 0);
    bool stopped = false;
    auto drawRun = [&](const std::wstring& text, Font* font, float w, float ascent) {
        StringFormat fmt;
        MakeFormat(fmt, ALIGN_LEFT | ALIGN_TOP, false, false);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);
        float drawY = baseY - ascent;
        g.DrawString(text.c_str(), -1, font, RectF(x, drawY, w + font->GetSize() / 3 + S(2), r.Y + r.Height - drawY + S(4)), &fmt, &br);
    };
    if (!trimmed) {
        for (auto& run : runs) {
            drawRun(run.text, run.font, run.width, run.ascent);
            x += run.width;
        }
    } else {
        for (auto& run : runs) {
            std::wstring part;
            float partW = 0;
            for (size_t k = 0; k < run.text.size(); ++k) {
                std::wstring one(1, run.text[k]);
                float cw = MeasureRun(g, one, run.font);
                if (x + partW + cw > limit) { stopped = true; break; }
                part += run.text[k];
                partW += cw;
            }
            if (!part.empty()) {
                drawRun(part, run.font, partW, run.ascent);
                x += partW;
            }
            if (stopped) break;
        }
        drawRun(L"…", ellFont, ellW, ellAscent);
    }
}

void Text(Graphics& g, const std::wstring& s, float px, bool bold, Color c,
          RectF r, int align, bool display, bool ellipsis, bool wrap) {
    if (s.empty() || r.Width <= 0 || r.Height <= 0) return;
    bool hasCJK = false, hasLatin = false;
    for (wchar_t ch : s) {
        if (IsCJKChar(ch)) hasCJK = true;
        else hasLatin = true;
    }
    if (hasCJK && hasLatin && !wrap) {
        DrawMixedRuns(g, s, px, bold, c, r, align, display, ellipsis);
        return;
    }
    Font* f = GetFont(px, bold, display, &s);
    SolidBrush br(c);
    StringFormat fmt;
    MakeFormat(fmt, align, ellipsis, wrap);
    g.DrawString(s.c_str(), -1, f, r, &fmt, &br);
}

void MeasureText(Graphics& g, const std::wstring& s, float px, bool bold, SizeF& out, bool display) {
    bool hasCJK = false, hasLatin = false;
    for (wchar_t ch : s) {
        if (IsCJKChar(ch)) hasCJK = true;
        else hasLatin = true;
    }
    if (hasCJK && hasLatin) {
        float scale = ResolveFontScale();
        Font* latin = GetFontEx(px * scale, bold, ResolveLatinFamily(display));
        Font* cjk = GetFontEx(px * scale, bold, ResolveCJKFamily());
        float w = 0, h = 0;
        for (size_t i = 0; i < s.size();) {
            bool isC = IsCJKChar(s[i]);
            size_t j = i;
            while (j < s.size() && IsCJKChar(s[j]) == isC) ++j;
            std::wstring part = s.substr(i, j - i);
            Font* f = isC ? cjk : latin;
            w += MeasureRun(g, part, f);
            StringFormat fmt;
            fmt.SetFormatFlags(StringFormatFlagsNoWrap);
            RectF layout(0, 0, 100000, 100000), bounds;
            g.MeasureString(part.c_str(), -1, f, layout, &fmt, &bounds);
            h = std::max(h, bounds.Height);
            i = j;
        }
        out = SizeF(w, h);
        return;
    }
    Font* f = GetFont(px, bold, display, &s);
    out = SizeF(0, 0);
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    RectF layout(0, 0, 100000, 100000);
    RectF bounds;
    g.MeasureString(s.c_str(), -1, f, layout, &fmt, &bounds);
    out = SizeF(bounds.Width, bounds.Height);
}
float TextWidth(Graphics& g, const std::wstring& s, float px, bool bold, bool display) {
    if (s.empty()) return 0;
    SizeF sz; MeasureText(g, s, px, bold, sz, display);
    return sz.Width;
}

static float ClampRadius(RectF r, float radius) {
    float m = std::min(r.Width, r.Height) / 2.0f;
    return std::max(0.0f, std::min(radius, m));
}

void FillRoundRect(Graphics& g, RectF r, float radius, Color c) {
    if (r.Width <= 0 || r.Height <= 0) return;
    float rad = ClampRadius(r, radius);
    GraphicsPath p;
    p.AddArc(r.X, r.Y, rad * 2, rad * 2, 180, 90);
    p.AddArc(r.X + r.Width - rad * 2, r.Y, rad * 2, rad * 2, 270, 90);
    p.AddArc(r.X + r.Width - rad * 2, r.Y + r.Height - rad * 2, rad * 2, rad * 2, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - rad * 2, rad * 2, rad * 2, 90, 90);
    p.CloseFigure();
    SolidBrush br(c);
    g.FillPath(&br, &p);
}

void FillRoundRectV(Graphics& g, RectF r, float radius, Color top, Color bottom) {
    if (r.Width <= 0 || r.Height <= 0) return;
    float rad = ClampRadius(r, radius);
    GraphicsPath p;
    p.AddArc(r.X, r.Y, rad * 2, rad * 2, 180, 90);
    p.AddArc(r.X + r.Width - rad * 2, r.Y, rad * 2, rad * 2, 270, 90);
    p.AddArc(r.X + r.Width - rad * 2, r.Y + r.Height - rad * 2, rad * 2, rad * 2, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - rad * 2, rad * 2, rad * 2, 90, 90);
    p.CloseFigure();
    LinearGradientBrush br(r, top, bottom, LinearGradientModeVertical);
    g.FillPath(&br, &p);
}

void StrokeRoundRect(Graphics& g, RectF r, float radius, Color c, float w) {
    if (r.Width <= 0 || r.Height <= 0) return;
    float rad = ClampRadius(r, radius);
    GraphicsPath p;
    p.AddArc(r.X, r.Y, rad * 2, rad * 2, 180, 90);
    p.AddArc(r.X + r.Width - rad * 2, r.Y, rad * 2, rad * 2, 270, 90);
    p.AddArc(r.X + r.Width - rad * 2, r.Y + r.Height - rad * 2, rad * 2, rad * 2, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - rad * 2, rad * 2, rad * 2, 90, 90);
    p.CloseFigure();
    Pen pen(c, w);
    g.DrawPath(&pen, &p);
}

void FillCircle(Graphics& g, float cx, float cy, float r, Color c) {
    if (r <= 0) return;
    SolidBrush br(c);
    g.FillEllipse(&br, cx - r, cy - r, r * 2, r * 2);
}
void StrokeCircle(Graphics& g, float cx, float cy, float r, Color c, float w) {
    if (r <= 0) return;
    Pen pen(c, w);
    g.DrawEllipse(&pen, cx - r, cy - r, r * 2, r * 2);
}
void Line(Graphics& g, float x1, float y1, float x2, float y2, Color c, float w) {
    Pen pen(c, w);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawLine(&pen, x1, y1, x2, y2);
}
void FillVGradient(Graphics& g, RectF r, Color top, Color bottom) {
    if (r.Width <= 0 || r.Height <= 0) return;
    LinearGradientBrush br(r, top, bottom, LinearGradientModeVertical);
    g.FillRectangle(&br, r);
}
void FillCircleV(Graphics& g, float cx, float cy, float r, Color top, Color bottom) {
    if (r <= 0) return;
    RectF rc(cx - r, cy - r, r * 2, r * 2);
    LinearGradientBrush br(rc, top, bottom, LinearGradientModeVertical);
    g.FillEllipse(&br, rc);
}
void Shadow(Graphics& g, RectF r, float radius, int layers, BYTE alpha) {
    for (int i = layers; i >= 1; --i) {
        float off = (float)i * S(2.0f);
        RectF rr(r.X - off, r.Y - off + S(2.0f), r.Width + off * 2, r.Height + off * 2);
        FillRoundRect(g, rr, radius + off, Color(alpha / (layers + 1), 0, 0, 0));
    }
}
void Glow(Graphics& g, RectF r, float radius, Color c, int layers) {
    BYTE a0 = c.GetA();
    for (int i = layers; i >= 1; --i) {
        float off = (float)i * S(2.5f);
        RectF rr(r.X - off, r.Y - off, r.Width + off * 2, r.Height + off * 2);
        Color cc(a0 / (i + 1), c.GetR(), c.GetG(), c.GetB());
        FillRoundRect(g, rr, radius + off, cc);
    }
}

Color Hex(const std::wstring& hex, BYTE a) {
    std::wstring h = hex;
    if (!h.empty() && h[0] == L'#') h = h.substr(1);
    unsigned long v = 0;
    if (h.size() >= 6) v = wcstoul(h.substr(0, 6).c_str(), nullptr, 16);
    BYTE r = (BYTE)((v >> 16) & 0xFF), gg = (BYTE)((v >> 8) & 0xFF), b = (BYTE)(v & 0xFF);
    return Color(a, r, gg, b);
}
Color WithAlpha(Color c, BYTE a) { return Color(a, c.GetR(), c.GetG(), c.GetB()); }

// ---------------- 图标 ----------------
static void Poly(Graphics& g, Pen& pen, const PointF* pts, int n, bool close) {
    for (int i = 0; i + 1 < n; ++i) g.DrawLine(&pen, pts[i], pts[i + 1]);
    if (close && n > 2) g.DrawLine(&pen, pts[n - 1], pts[0]);
}

void Icon(Graphics& g, int id, RectF box, Color c, float weight) {
    float x = box.X, y = box.Y, w = box.Width, h = box.Height;
    float cx = x + w / 2, cy = y + h / 2;
    float s = std::min(w, h);
    Pen pen(c, std::max(1.0f, S(weight)));
    pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
    switch (id) {
    case ICO_PLUS:
        g.DrawLine(&pen, cx - s * 0.30f, cy, cx + s * 0.30f, cy);
        g.DrawLine(&pen, cx, cy - s * 0.30f, cx, cy + s * 0.30f);
        break;
    case ICO_MINUS:
        g.DrawLine(&pen, cx - s * 0.30f, cy, cx + s * 0.30f, cy);
        break;
    case ICO_CHEV_LEFT: {
        PointF p[3] = { {cx + s*0.12f, cy - s*0.22f}, {cx - s*0.12f, cy}, {cx + s*0.12f, cy + s*0.22f} };
        Poly(g, pen, p, 3, false); break; }
    case ICO_CHEV_RIGHT: {
        PointF p[3] = { {cx - s*0.12f, cy - s*0.22f}, {cx + s*0.12f, cy}, {cx - s*0.12f, cy + s*0.22f} };
        Poly(g, pen, p, 3, false); break; }
    case ICO_CHEV_DOWN: {
        PointF p[3] = { {cx - s*0.20f, cy - s*0.10f}, {cx, cy + s*0.10f}, {cx + s*0.20f, cy - s*0.10f} };
        Poly(g, pen, p, 3, false); break; }
    case ICO_CHEV_UP: {
        PointF p[3] = { {cx - s*0.20f, cy + s*0.10f}, {cx, cy - s*0.10f}, {cx + s*0.20f, cy + s*0.10f} };
        Poly(g, pen, p, 3, false); break; }
    case ICO_SEARCH:
        g.DrawEllipse(&pen, cx - s * 0.28f, cy - s * 0.30f, s * 0.44f, s * 0.44f);
        g.DrawLine(&pen, cx + s * 0.06f, cy + s * 0.08f, cx + s * 0.28f, cy + s * 0.28f);
        break;
    case ICO_GEAR:
    case ICO_SETTINGS: {
        g.DrawEllipse(&pen, cx - s * 0.16f, cy - s * 0.16f, s * 0.32f, s * 0.32f);
        for (int i = 0; i < 8; ++i) {
            float ang = (float)i * 3.1415926f / 4.0f;
            float r1 = s * 0.25f, r2 = s * 0.36f;
            g.DrawLine(&pen, cx + cosf(ang) * r1, cy + sinf(ang) * r1, cx + cosf(ang) * r2, cy + sinf(ang) * r2);
        }
        break; }
    case ICO_THEME: {
        // 半明半暗的圆
        g.DrawEllipse(&pen, cx - s * 0.30f, cy - s * 0.30f, s * 0.60f, s * 0.60f);
        SolidBrush br(c);
        GraphicsPath p;
        p.AddArc(cx - s * 0.30f, cy - s * 0.30f, s * 0.60f, s * 0.60f, 90, 180);
        p.CloseFigure();
        g.FillPath(&br, &p);
        break; }
    case ICO_CLOSE:
        g.DrawLine(&pen, cx - s * 0.20f, cy - s * 0.20f, cx + s * 0.20f, cy + s * 0.20f);
        g.DrawLine(&pen, cx + s * 0.20f, cy - s * 0.20f, cx - s * 0.20f, cy + s * 0.20f);
        break;
    case ICO_MIN:
        g.DrawLine(&pen, cx - s * 0.22f, cy, cx + s * 0.22f, cy);
        break;
    case ICO_MAX:
        g.DrawRectangle(&pen, cx - s * 0.20f, cy - s * 0.20f, s * 0.40f, s * 0.40f);
        break;
    case ICO_CHECK: {
        PointF p[3] = { {cx - s*0.24f, cy + s*0.02f}, {cx - s*0.06f, cy + s*0.20f}, {cx + s*0.25f, cy - s*0.20f} };
        Poly(g, pen, p, 3, false); break; }
    case ICO_BELL: {
        GraphicsPath p;
        p.AddArc(cx - s * 0.22f, cy - s * 0.30f, s * 0.44f, s * 0.44f, 180, 180);
        p.AddLine(cx + s * 0.22f, cy - s * 0.08f, cx + s * 0.28f, cy + s * 0.12f);
        p.AddLine(cx + s * 0.28f, cy + s * 0.12f, cx - s * 0.28f, cy + s * 0.12f);
        p.AddLine(cx - s * 0.28f, cy + s * 0.12f, cx - s * 0.22f, cy - s * 0.08f);
        g.DrawPath(&pen, &p);
        g.DrawArc(&pen, cx - s * 0.07f, cy + s * 0.13f, s * 0.14f, s * 0.12f, 0, 180);
        break; }
    case ICO_DROP: {
        GraphicsPath p;
        p.AddBezier(cx, cy - s * 0.36f, cx + s * 0.24f, cy - s * 0.02f, cx + s * 0.24f, cy + s * 0.10f,
                    cx + s * 0.24f, cy + s * 0.20f);
        p.AddArc(cx - s * 0.24f, cy + s * 0.02f, s * 0.48f, s * 0.40f, 0, 180);
        p.AddBezier(cx - s * 0.24f, cy + s * 0.10f, cx - s * 0.24f, cy - s * 0.02f, cx, cy - s * 0.36f, cx, cy - s * 0.36f);
        g.DrawPath(&pen, &p);
        break; }
    case ICO_CLOCK:
        g.DrawEllipse(&pen, cx - s * 0.34f, cy - s * 0.34f, s * 0.68f, s * 0.68f);
        g.DrawLine(&pen, cx, cy - s * 0.20f, cx, cy);
        g.DrawLine(&pen, cx, cy, cx + s * 0.16f, cy + s * 0.10f);
        break;
    case ICO_CAL: {
        g.DrawRectangle(&pen, cx - s * 0.30f, cy - s * 0.26f, s * 0.60f, s * 0.54f);
        g.DrawLine(&pen, cx - s * 0.30f, cy - s * 0.10f, cx + s * 0.30f, cy - s * 0.10f);
        g.DrawLine(&pen, cx - s * 0.16f, cy - s * 0.34f, cx - s * 0.16f, cy - s * 0.22f);
        g.DrawLine(&pen, cx + s * 0.16f, cy - s * 0.34f, cx + s * 0.16f, cy - s * 0.22f);
        SolidBrush br(c);
        g.FillRectangle(&br, cx - s * 0.18f, cy + s * 0.00f, s * 0.10f, s * 0.10f);
        g.FillRectangle(&br, cx + s * 0.04f, cy + s * 0.00f, s * 0.10f, s * 0.10f);
        break; }
    case ICO_LIST: {
        float dotx = cx - s * 0.28f;
        for (int i = 0; i < 3; ++i) {
            float yy = cy - s * 0.20f + i * s * 0.20f;
            g.DrawLine(&pen, dotx, yy, dotx + s * 0.02f, yy);
            g.DrawLine(&pen, cx - s * 0.12f, yy, cx + s * 0.26f, yy);
        }
        break; }
    case ICO_GRID:
        g.DrawRectangle(&pen, cx - s * 0.28f, cy - s * 0.28f, s * 0.24f, s * 0.24f);
        g.DrawRectangle(&pen, cx + s * 0.04f, cy - s * 0.28f, s * 0.24f, s * 0.24f);
        g.DrawRectangle(&pen, cx - s * 0.28f, cy + s * 0.04f, s * 0.24f, s * 0.24f);
        g.DrawRectangle(&pen, cx + s * 0.04f, cy + s * 0.04f, s * 0.24f, s * 0.24f);
        break;
    case ICO_WEEK:
        g.DrawRectangle(&pen, cx - s * 0.30f, cy - s * 0.26f, s * 0.60f, s * 0.54f);
        g.DrawLine(&pen, cx - s * 0.30f, cy - s * 0.10f, cx + s * 0.30f, cy - s * 0.10f);
        g.DrawLine(&pen, cx - s * 0.10f, cy - s * 0.26f, cx - s * 0.10f, cy + s * 0.28f);
        g.DrawLine(&pen, cx + s * 0.10f, cy - s * 0.26f, cx + s * 0.10f, cy + s * 0.28f);
        break;
    case ICO_DAY:
        g.DrawRectangle(&pen, cx - s * 0.30f, cy - s * 0.26f, s * 0.60f, s * 0.54f);
        g.DrawLine(&pen, cx - s * 0.30f, cy - s * 0.10f, cx + s * 0.30f, cy - s * 0.10f);
        {
            SolidBrush br(WithAlpha(c, 90));
            g.FillRectangle(&br, cx - s * 0.18f, cy + s * 0.00f, s * 0.36f, s * 0.20f);
        }
        break;
    case ICO_TRASH:
        g.DrawRectangle(&pen, cx - s * 0.20f, cy - s * 0.16f, s * 0.40f, s * 0.42f);
        g.DrawLine(&pen, cx - s * 0.28f, cy - s * 0.24f, cx + s * 0.28f, cy - s * 0.24f);
        g.DrawLine(&pen, cx - s * 0.08f, cy - s * 0.30f, cx + s * 0.08f, cy - s * 0.30f);
        g.DrawLine(&pen, cx - s * 0.06f, cy - s * 0.08f, cx - s * 0.06f, cy + s * 0.16f);
        g.DrawLine(&pen, cx + s * 0.06f, cy - s * 0.08f, cx + s * 0.06f, cy + s * 0.16f);
        break;
    case ICO_EDIT:
        g.DrawLine(&pen, cx - s * 0.24f, cy + s * 0.24f, cx + s * 0.20f, cy - s * 0.20f);
        g.DrawLine(&pen, cx - s * 0.24f, cy + s * 0.24f, cx - s * 0.24f, cy + s * 0.10f);
        g.DrawLine(&pen, cx - s * 0.24f, cy + s * 0.24f, cx - s * 0.10f, cy + s * 0.24f);
        break;
    case ICO_COPY:
        g.DrawRectangle(&pen, cx - s * 0.26f, cy - s * 0.26f, s * 0.32f, s * 0.32f);
        g.DrawRectangle(&pen, cx - s * 0.06f, cy - s * 0.06f, s * 0.32f, s * 0.32f);
        break;
    case ICO_MORE: {
        SolidBrush br(c);
        for (int i = 0; i < 3; ++i) g.FillEllipse(&br, cx - s * 0.22f + i * s * 0.18f, cy - s * 0.04f, s * 0.08f, s * 0.08f);
        break; }
    case ICO_STAR: {
        // 四角星（品牌标记）
        GraphicsPath p;
        float R = s * 0.40f, r = s * 0.14f;
        for (int i = 0; i < 8; ++i) {
            float ang = -3.1415926f / 2 + i * 3.1415926f / 4;
            float rr = (i % 2 == 0) ? R : r;
            float px = cx + cosf(ang) * rr, py = cy + sinf(ang) * rr;
            if (i == 0) p.StartFigure(); // 占位，随后 AddLine
            (void)px; (void)py;
        }
        PointF pts[8];
        for (int i = 0; i < 8; ++i) {
            float ang = -3.1415926f / 2 + i * 3.1415926f / 4;
            float rr = (i % 2 == 0) ? R : r;
            pts[i] = PointF(cx + cosf(ang) * rr, cy + sinf(ang) * rr);
        }
        p.Reset();
        p.AddPolygon(pts, 8);
        SolidBrush br(c);
        g.FillPath(&br, &p);
        break; }
    case ICO_TAG: {
        GraphicsPath p;
        p.AddLine(cx - s * 0.30f, cy - s * 0.10f, cx - s * 0.10f, cy - s * 0.30f);
        p.AddLine(cx - s * 0.10f, cy - s * 0.30f, cx + s * 0.28f, cy - s * 0.30f);
        p.AddLine(cx + s * 0.28f, cy - s * 0.30f, cx + s * 0.28f, cy + s * 0.08f);
        p.AddLine(cx + s * 0.28f, cy + s * 0.08f, cx + s * 0.04f, cy + s * 0.32f);
        p.AddLine(cx + s * 0.04f, cy + s * 0.32f, cx - s * 0.30f, cy - s * 0.02f);
        p.CloseFigure();
        g.DrawPath(&pen, &p);
        g.DrawEllipse(&pen, cx + s * 0.08f, cy - s * 0.18f, s * 0.08f, s * 0.08f);
        break; }
    case ICO_FILTER: {
        PointF p[4] = { {cx - s*0.30f, cy - s*0.24f}, {cx + s*0.30f, cy - s*0.24f}, {cx + s*0.06f, cy + s*0.02f}, {cx + s*0.06f, cy + s*0.28f} };
        g.DrawLine(&pen, p[0], p[1]);
        g.DrawLine(&pen, p[1], p[2]);
        g.DrawLine(&pen, p[2], p[3]);
        g.DrawLine(&pen, p[3], PointF(cx - s*0.06f, cy + s*0.28f));
        g.DrawLine(&pen, PointF(cx - s*0.06f, cy + s*0.28f), p[0]);
        break; }
    case ICO_ARROW_RIGHT:
        g.DrawLine(&pen, cx - s * 0.24f, cy, cx + s * 0.24f, cy);
        g.DrawLine(&pen, cx + s * 0.08f, cy - s * 0.16f, cx + s * 0.24f, cy);
        g.DrawLine(&pen, cx + s * 0.08f, cy + s * 0.16f, cx + s * 0.24f, cy);
        break;
    case ICO_ARROW_LEFT:
        g.DrawLine(&pen, cx + s * 0.24f, cy, cx - s * 0.24f, cy);
        g.DrawLine(&pen, cx - s * 0.08f, cy - s * 0.16f, cx - s * 0.24f, cy);
        g.DrawLine(&pen, cx - s * 0.08f, cy + s * 0.16f, cx - s * 0.24f, cy);
        break;
    case ICO_REFRESH: {
        g.DrawArc(&pen, cx - s * 0.26f, cy - s * 0.26f, s * 0.52f, s * 0.52f, 40, 250);
        PointF a(cx + s * 0.24f, cy - s * 0.28f);
        PointF b(cx + s * 0.06f, cy - s * 0.30f);
        PointF d(cx + s * 0.28f, cy - s * 0.10f);
        g.DrawLine(&pen, a, b); g.DrawLine(&pen, a, d);
        break; }
    case ICO_UNDO: {
        g.DrawLine(&pen, cx - s * 0.30f, cy, cx + s * 0.28f, cy);
        g.DrawLine(&pen, cx - s * 0.30f, cy, cx - s * 0.10f, cy - s * 0.20f);
        g.DrawLine(&pen, cx - s * 0.30f, cy, cx - s * 0.10f, cy + s * 0.20f);
        break; }
    case ICO_REDO: {
        g.DrawLine(&pen, cx - s * 0.28f, cy, cx + s * 0.30f, cy);
        g.DrawLine(&pen, cx + s * 0.30f, cy, cx + s * 0.10f, cy - s * 0.20f);
        g.DrawLine(&pen, cx + s * 0.30f, cy, cx + s * 0.10f, cy + s * 0.20f);
        break; }
    case ICO_FOLDER: {
        GraphicsPath p;
        p.AddLine(cx - s * 0.30f, cy + s * 0.24f, cx - s * 0.30f, cy - s * 0.18f);
        p.AddLine(cx - s * 0.30f, cy - s * 0.18f, cx - s * 0.08f, cy - s * 0.18f);
        p.AddLine(cx - s * 0.08f, cy - s * 0.18f, cx - s * 0.02f, cy - s * 0.10f);
        p.AddLine(cx - s * 0.02f, cy - s * 0.10f, cx + s * 0.30f, cy - s * 0.10f);
        p.AddLine(cx + s * 0.30f, cy - s * 0.10f, cx + s * 0.30f, cy + s * 0.24f);
        p.CloseFigure();
        g.DrawPath(&pen, &p);
        break; }
    case ICO_CHART: {
        g.DrawLine(&pen, cx - s * 0.28f, cy + s * 0.26f, cx + s * 0.28f, cy + s * 0.26f);
        g.DrawLine(&pen, cx - s * 0.18f, cy + s * 0.26f, cx - s * 0.18f, cy + s * 0.04f);
        g.DrawLine(&pen, cx - s * 0.02f, cy + s * 0.26f, cx - s * 0.02f, cy - s * 0.14f);
        g.DrawLine(&pen, cx + s * 0.14f, cy + s * 0.26f, cx + s * 0.14f, cy - s * 0.04f);
        break; }
    case ICO_PIN: {
        g.DrawEllipse(&pen, cx - s * 0.20f, cy - s * 0.30f, s * 0.40f, s * 0.36f);
        g.DrawLine(&pen, cx, cy + s * 0.02f, cx, cy + s * 0.30f);
        g.DrawLine(&pen, cx - s * 0.14f, cy + s * 0.12f, cx + s * 0.14f, cy + s * 0.12f);
        break; }
    case ICO_DRAG: {
        SolidBrush br(WithAlpha(c, 170));
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 2; ++j)
            g.FillEllipse(&br, cx - s * 0.10f + j * s * 0.14f, cy - s * 0.16f + i * s * 0.14f, s * 0.06f, s * 0.06f);
        break; }
    case ICO_PLAY: {
        PointF p[3] = { {cx - s*0.16f, cy - s*0.24f}, {cx + s*0.24f, cy}, {cx - s*0.16f, cy + s*0.24f} };
        SolidBrush br(c);
        GraphicsPath path; path.AddPolygon(p, 3);
        g.FillPath(&br, &path);
        break; }
    case ICO_PAUSE: {
        SolidBrush br(c);
        g.FillRectangle(&br, cx - s * 0.20f, cy - s * 0.22f, s * 0.12f, s * 0.44f);
        g.FillRectangle(&br, cx + s * 0.08f, cy - s * 0.22f, s * 0.12f, s * 0.44f);
        break; }
    case ICO_SUN: {
        g.DrawEllipse(&pen, cx - s * 0.16f, cy - s * 0.16f, s * 0.32f, s * 0.32f);
        for (int i = 0; i < 8; ++i) {
            float ang = (float)i * 3.1415926f / 4;
            g.DrawLine(&pen, cx + cosf(ang) * s * 0.26f, cy + sinf(ang) * s * 0.26f,
                             cx + cosf(ang) * s * 0.36f, cy + sinf(ang) * s * 0.36f);
        }
        break; }
    case ICO_MOON: {
        GraphicsPath p;
        p.AddArc(cx - s * 0.28f, cy - s * 0.30f, s * 0.58f, s * 0.58f, 60, 240);
        p.AddArc(cx - s * 0.06f, cy - s * 0.30f, s * 0.52f, s * 0.58f, 240, -180);
        p.CloseFigure();
        g.DrawPath(&pen, &p);
        break; }
    case ICO_WARN: {
        PointF p[3] = { {cx, cy - s*0.28f}, {cx + s*0.30f, cy + s*0.24f}, {cx - s*0.30f, cy + s*0.24f} };
        Poly(g, pen, p, 3, true);
        g.DrawLine(&pen, cx, cy - s * 0.06f, cx, cy + s * 0.06f);
        SolidBrush br(c);
        g.FillEllipse(&br, cx - s * 0.025f, cy + s * 0.14f, s * 0.05f, s * 0.05f);
        break; }
    case ICO_HELP: {
        g.DrawEllipse(&pen, cx - s * 0.32f, cy - s * 0.32f, s * 0.64f, s * 0.64f);
        g.DrawArc(&pen, cx - s * 0.12f, cy - s * 0.20f, s * 0.24f, s * 0.22f, 180, 220);
        g.DrawLine(&pen, cx + s * 0.01f, cy - s * 0.02f, cx + s * 0.01f, cy + s * 0.08f);
        SolidBrush br(c);
        g.FillEllipse(&br, cx - s * 0.03f, cy + s * 0.16f, s * 0.07f, s * 0.07f);
        break; }
    case ICO_BOOK: {
        g.DrawRectangle(&pen, cx - s*0.24f, cy - s*0.28f, s*0.44f, s*0.56f);
        g.DrawLine(&pen, cx - s*0.24f, cy - s*0.28f, cx - s*0.24f, cy + s*0.28f);
        g.DrawLine(&pen, cx - s*0.10f, cy - s*0.10f, cx + s*0.12f, cy - s*0.10f);
        g.DrawLine(&pen, cx - s*0.10f, cy + s*0.02f, cx + s*0.12f, cy + s*0.02f);
        break; }
    case ICO_BRIEFCASE: {
        g.DrawRectangle(&pen, cx - s*0.30f, cy - s*0.12f, s*0.60f, s*0.34f);
        g.DrawRectangle(&pen, cx - s*0.12f, cy - s*0.24f, s*0.24f, s*0.12f);
        g.DrawLine(&pen, cx - s*0.30f, cy + s*0.02f, cx + s*0.30f, cy + s*0.02f);
        break; }
    case ICO_USER: {
        g.DrawEllipse(&pen, cx - s*0.14f, cy - s*0.28f, s*0.28f, s*0.28f);
        g.DrawArc(&pen, cx - s*0.26f, cy + s*0.02f, s*0.52f, s*0.34f, 180, 180);
        break; }
    case ICO_USERS: {
        g.DrawEllipse(&pen, cx - s*0.26f, cy - s*0.24f, s*0.20f, s*0.20f);
        g.DrawEllipse(&pen, cx + s*0.04f, cy - s*0.24f, s*0.20f, s*0.20f);
        g.DrawArc(&pen, cx - s*0.34f, cy + s*0.04f, s*0.38f, s*0.28f, 180, 180);
        g.DrawArc(&pen, cx - s*0.04f, cy + s*0.04f, s*0.38f, s*0.28f, 180, 180);
        break; }
    case ICO_HOME: {
        PointF roof[3] = {{cx - s*0.30f, cy}, {cx, cy - s*0.28f}, {cx + s*0.30f, cy}};
        Poly(g, pen, roof, 3, false);
        g.DrawRectangle(&pen, cx - s*0.20f, cy, s*0.40f, s*0.28f);
        break; }
    case ICO_FOOD: {
        g.DrawLine(&pen, cx - s*0.14f, cy - s*0.28f, cx - s*0.14f, cy + s*0.28f);
        g.DrawLine(&pen, cx - s*0.22f, cy - s*0.28f, cx - s*0.22f, cy - s*0.04f);
        g.DrawLine(&pen, cx - s*0.06f, cy - s*0.28f, cx - s*0.06f, cy - s*0.04f);
        g.DrawLine(&pen, cx - s*0.22f, cy - s*0.04f, cx - s*0.06f, cy - s*0.04f);
        g.DrawLine(&pen, cx + s*0.18f, cy - s*0.28f, cx + s*0.18f, cy + s*0.28f);
        break; }
    case ICO_COFFEE: {
        g.DrawRectangle(&pen, cx - s*0.24f, cy - s*0.12f, s*0.38f, s*0.34f);
        g.DrawArc(&pen, cx + s*0.10f, cy - s*0.04f, s*0.18f, s*0.18f, -90, 180);
        g.DrawLine(&pen, cx - s*0.20f, cy + s*0.30f, cx + s*0.18f, cy + s*0.30f);
        g.DrawLine(&pen, cx - s*0.14f, cy - s*0.22f, cx - s*0.14f, cy - s*0.30f);
        g.DrawLine(&pen, cx + s*0.00f, cy - s*0.22f, cx + s*0.00f, cy - s*0.30f);
        break; }
    case ICO_HEART: {
        g.DrawArc(&pen, cx - s*0.30f, cy - s*0.26f, s*0.32f, s*0.32f, 160, 200);
        g.DrawArc(&pen, cx - s*0.02f, cy - s*0.26f, s*0.32f, s*0.32f, 180, 200);
        g.DrawLine(&pen, cx - s*0.26f, cy - s*0.08f, cx, cy + s*0.28f);
        g.DrawLine(&pen, cx + s*0.26f, cy - s*0.08f, cx, cy + s*0.28f);
        break; }
    case ICO_MONEY: {
        g.DrawRectangle(&pen, cx - s*0.30f, cy - s*0.20f, s*0.60f, s*0.40f);
        g.DrawEllipse(&pen, cx - s*0.09f, cy - s*0.09f, s*0.18f, s*0.18f);
        g.DrawLine(&pen, cx, cy - s*0.14f, cx, cy + s*0.14f);
        break; }
    case ICO_PHONE: {
        g.DrawRectangle(&pen, cx - s*0.16f, cy - s*0.30f, s*0.32f, s*0.60f);
        g.DrawLine(&pen, cx - s*0.06f, cy + s*0.21f, cx + s*0.06f, cy + s*0.21f);
        break; }
    case ICO_MAIL: {
        g.DrawRectangle(&pen, cx - s*0.30f, cy - s*0.20f, s*0.60f, s*0.40f);
        g.DrawLine(&pen, cx - s*0.30f, cy - s*0.20f, cx, cy + s*0.02f);
        g.DrawLine(&pen, cx + s*0.30f, cy - s*0.20f, cx, cy + s*0.02f);
        break; }
    case ICO_MUSIC: {
        g.DrawLine(&pen, cx + s*0.04f, cy - s*0.26f, cx + s*0.04f, cy + s*0.16f);
        g.DrawLine(&pen, cx + s*0.04f, cy - s*0.26f, cx + s*0.26f, cy - s*0.18f);
        g.DrawLine(&pen, cx + s*0.26f, cy - s*0.18f, cx + s*0.26f, cy + s*0.10f);
        g.DrawEllipse(&pen, cx - s*0.14f, cy + s*0.10f, s*0.18f, s*0.14f);
        g.DrawEllipse(&pen, cx + s*0.08f, cy + s*0.04f, s*0.18f, s*0.14f);
        break; }
    case ICO_CAMERA: {
        g.DrawRectangle(&pen, cx - s*0.30f, cy - s*0.16f, s*0.60f, s*0.38f);
        g.DrawRectangle(&pen, cx - s*0.10f, cy - s*0.26f, s*0.20f, s*0.10f);
        g.DrawEllipse(&pen, cx - s*0.11f, cy - s*0.05f, s*0.22f, s*0.22f);
        break; }
    case ICO_CODE: {
        PointF a[3] = {{cx - s*0.06f, cy - s*0.22f}, {cx - s*0.26f, cy}, {cx - s*0.06f, cy + s*0.22f}};
        PointF b[3] = {{cx + s*0.06f, cy - s*0.22f}, {cx + s*0.26f, cy}, {cx + s*0.06f, cy + s*0.22f}};
        Poly(g, pen, a, 3, false); Poly(g, pen, b, 3, false);
        break; }
    case ICO_BULB: {
        g.DrawEllipse(&pen, cx - s*0.18f, cy - s*0.30f, s*0.36f, s*0.36f);
        g.DrawLine(&pen, cx - s*0.10f, cy + s*0.10f, cx + s*0.10f, cy + s*0.10f);
        g.DrawLine(&pen, cx - s*0.08f, cy + s*0.18f, cx + s*0.08f, cy + s*0.18f);
        g.DrawLine(&pen, cx - s*0.06f, cy + s*0.26f, cx + s*0.06f, cy + s*0.26f);
        break; }
    case ICO_FLAG: {
        g.DrawLine(&pen, cx - s*0.20f, cy - s*0.28f, cx - s*0.20f, cy + s*0.30f);
        PointF f[3] = {{cx - s*0.20f, cy - s*0.26f}, {cx + s*0.26f, cy - s*0.14f}, {cx - s*0.20f, cy - s*0.02f}};
        Poly(g, pen, f, 3, true);
        break; }
    case ICO_TARGET: {
        g.DrawEllipse(&pen, cx - s*0.30f, cy - s*0.30f, s*0.60f, s*0.60f);
        g.DrawEllipse(&pen, cx - s*0.16f, cy - s*0.16f, s*0.32f, s*0.32f);
        SolidBrush dot(c); g.FillEllipse(&dot, cx - s*0.05f, cy - s*0.05f, s*0.10f, s*0.10f);
        break; }
    case ICO_TROPHY: {
        g.DrawRectangle(&pen, cx - s*0.16f, cy - s*0.22f, s*0.32f, s*0.26f);
        g.DrawArc(&pen, cx - s*0.28f, cy - s*0.22f, s*0.16f, s*0.22f, 90, 180);
        g.DrawArc(&pen, cx + s*0.12f, cy - s*0.22f, s*0.16f, s*0.22f, 270, 180);
        g.DrawLine(&pen, cx, cy + s*0.04f, cx, cy + s*0.20f);
        g.DrawLine(&pen, cx - s*0.12f, cy + s*0.28f, cx + s*0.12f, cy + s*0.28f);
        break; }
    case ICO_GIFT: {
        g.DrawRectangle(&pen, cx - s*0.28f, cy - s*0.06f, s*0.56f, s*0.34f);
        g.DrawRectangle(&pen, cx - s*0.34f, cy - s*0.20f, s*0.68f, s*0.14f);
        g.DrawLine(&pen, cx, cy - s*0.20f, cx, cy + s*0.28f);
        g.DrawArc(&pen, cx - s*0.18f, cy - s*0.34f, s*0.18f, s*0.16f, 0, 220);
        g.DrawArc(&pen, cx, cy - s*0.34f, s*0.18f, s*0.16f, 320, 220);
        break; }
    case ICO_PLANE: {
        PointF p[4] = {{cx - s*0.30f, cy + s*0.04f}, {cx + s*0.30f, cy - s*0.24f}, {cx + s*0.06f, cy + s*0.26f}, {cx - s*0.02f, cy + s*0.06f}};
        Poly(g, pen, p, 4, false);
        g.DrawLine(&pen, cx - s*0.02f, cy + s*0.06f, cx + s*0.30f, cy - s*0.24f);
        break; }
    case ICO_FLASK: {
        g.DrawLine(&pen, cx - s*0.10f, cy - s*0.26f, cx + s*0.10f, cy - s*0.26f);
        g.DrawLine(&pen, cx - s*0.10f, cy - s*0.26f, cx - s*0.24f, cy + s*0.24f);
        g.DrawLine(&pen, cx + s*0.10f, cy - s*0.26f, cx + s*0.24f, cy + s*0.24f);
        g.DrawLine(&pen, cx - s*0.24f, cy + s*0.24f, cx + s*0.24f, cy + s*0.24f);
        g.DrawLine(&pen, cx - s*0.18f, cy + s*0.08f, cx + s*0.18f, cy + s*0.08f);
        break; }
    case ICO_PEN: {
        PointF p[4] = {{cx - s*0.26f, cy + s*0.22f}, {cx - s*0.18f, cy - s*0.06f}, {cx + s*0.16f, cy - s*0.28f}, {cx + s*0.26f, cy - s*0.14f}};
        Poly(g, pen, p, 4, true);
        g.DrawLine(&pen, cx - s*0.18f, cy - s*0.06f, cx + s*0.16f, cy - s*0.28f);
        break; }
    case ICO_MIC: {
        g.DrawRectangle(&pen, cx - s*0.11f, cy - s*0.30f, s*0.22f, s*0.36f);
        g.DrawArc(&pen, cx - s*0.20f, cy - s*0.14f, s*0.40f, s*0.34f, 0, 180);
        g.DrawLine(&pen, cx, cy + s*0.18f, cx, cy + s*0.30f);
        g.DrawLine(&pen, cx - s*0.12f, cy + s*0.30f, cx + s*0.12f, cy + s*0.30f);
        break; }
    case ICO_VIDEO: {
        g.DrawRectangle(&pen, cx - s*0.30f, cy - s*0.18f, s*0.38f, s*0.36f);
        PointF p[3] = {{cx + s*0.10f, cy - s*0.02f}, {cx + s*0.30f, cy - s*0.14f}, {cx + s*0.30f, cy + s*0.14f}};
        Poly(g, pen, p, 3, true);
        break; }
    case ICO_MAP: {
        PointF p[5] = {{cx - s*0.30f, cy - s*0.22f}, {cx - s*0.10f, cy - s*0.28f}, {cx + s*0.10f, cy - s*0.18f}, {cx + s*0.30f, cy - s*0.24f}, {cx + s*0.30f, cy + s*0.24f}};
        Poly(g, pen, p, 5, false);
        g.DrawLine(&pen, cx - s*0.30f, cy - s*0.22f, cx - s*0.30f, cy + s*0.24f);
        g.DrawLine(&pen, cx - s*0.10f, cy - s*0.28f, cx - s*0.10f, cy + s*0.20f);
        g.DrawLine(&pen, cx + s*0.10f, cy - s*0.18f, cx + s*0.10f, cy + s*0.28f);
        g.DrawLine(&pen, cx - s*0.30f, cy + s*0.24f, cx + s*0.10f, cy + s*0.28f);
        break; }
    case ICO_CART: {
        g.DrawLine(&pen, cx - s*0.30f, cy - s*0.22f, cx - s*0.20f, cy - s*0.22f);
        g.DrawLine(&pen, cx - s*0.18f, cy - s*0.14f, cx - s*0.08f, cy + s*0.10f);
        g.DrawLine(&pen, cx - s*0.18f, cy - s*0.14f, cx + s*0.24f, cy - s*0.14f);
        g.DrawLine(&pen, cx + s*0.24f, cy - s*0.14f, cx + s*0.16f, cy + s*0.10f);
        g.DrawLine(&pen, cx - s*0.08f, cy + s*0.10f, cx + s*0.16f, cy + s*0.10f);
        g.DrawEllipse(&pen, cx - s*0.10f, cy + s*0.16f, s*0.12f, s*0.12f);
        g.DrawEllipse(&pen, cx + s*0.12f, cy + s*0.16f, s*0.12f, s*0.12f);
        break; }
    case ICO_BANK: {
        PointF roof[3] = {{cx - s*0.30f, cy - s*0.10f}, {cx, cy - s*0.28f}, {cx + s*0.30f, cy - s*0.10f}};
        Poly(g, pen, roof, 3, false);
        g.DrawLine(&pen, cx - s*0.24f, cy - s*0.02f, cx + s*0.24f, cy - s*0.02f);
        g.DrawLine(&pen, cx - s*0.24f, cy + s*0.24f, cx + s*0.24f, cy + s*0.24f);
        for (int i = -2; i <= 2; ++i) g.DrawLine(&pen, cx + i * s*0.10f, cy + s*0.02f, cx + i * s*0.10f, cy + s*0.20f);
        break; }
    case ICO_LAPTOP: {
        g.DrawRectangle(&pen, cx - s*0.24f, cy - s*0.26f, s*0.48f, s*0.32f);
        g.DrawLine(&pen, cx - s*0.34f, cy + s*0.16f, cx + s*0.34f, cy + s*0.16f);
        g.DrawLine(&pen, cx - s*0.28f, cy + s*0.24f, cx + s*0.28f, cy + s*0.24f);
        break; }
    case ICO_BUS: {
        g.DrawRectangle(&pen, cx - s*0.26f, cy - s*0.28f, s*0.52f, s*0.40f);
        g.DrawRectangle(&pen, cx - s*0.20f, cy - s*0.20f, s*0.16f, s*0.14f);
        g.DrawRectangle(&pen, cx + s*0.04f, cy - s*0.20f, s*0.16f, s*0.14f);
        g.DrawEllipse(&pen, cx - s*0.20f, cy + s*0.14f, s*0.12f, s*0.12f);
        g.DrawEllipse(&pen, cx + s*0.08f, cy + s*0.14f, s*0.12f, s*0.12f);
        break; }
    case ICO_GAME: {
        g.DrawRectangle(&pen, cx - s*0.30f, cy - s*0.14f, s*0.60f, s*0.30f);
        g.DrawLine(&pen, cx - s*0.20f, cy, cx - s*0.10f, cy);
        g.DrawLine(&pen, cx - s*0.15f, cy - s*0.05f, cx - s*0.15f, cy + s*0.05f);
        g.DrawEllipse(&pen, cx + s*0.10f, cy - s*0.06f, s*0.08f, s*0.08f);
        g.DrawEllipse(&pen, cx + s*0.16f, cy + s*0.02f, s*0.06f, s*0.06f);
        break; }
    default:
        g.DrawEllipse(&pen, cx - s * 0.28f, cy - s * 0.28f, s * 0.56f, s * 0.56f);
        break;
    }
}

int IconFromName(const std::wstring& name) {
    static const std::pair<const wchar_t*, int> kMap[] = {
        {L"tag", ICO_TAG}, {L"star", ICO_STAR}, {L"cal", ICO_CAL}, {L"bell", ICO_BELL},
        {L"clock", ICO_CLOCK}, {L"drop", ICO_DROP}, {L"chart", ICO_CHART}, {L"folder", ICO_FOLDER},
        {L"pin", ICO_PIN}, {L"edit", ICO_EDIT}, {L"list", ICO_LIST}, {L"check", ICO_CHECK},
        {L"sun", ICO_SUN}, {L"moon", ICO_MOON}, {L"warn", ICO_WARN}, {L"help", ICO_HELP},
        {L"gear", ICO_GEAR}, {L"filter", ICO_FILTER}, {L"copy", ICO_COPY}, {L"trash", ICO_TRASH},
        {L"refresh", ICO_REFRESH}, {L"play", ICO_PLAY}, {L"pause", ICO_PAUSE}, {L"settings", ICO_SETTINGS},
        {L"grid", ICO_GRID}, {L"week", ICO_WEEK}, {L"day", ICO_DAY}, {L"bell", ICO_BELL},
        {L"book", ICO_BOOK}, {L"briefcase", ICO_BRIEFCASE}, {L"user", ICO_USER}, {L"users", ICO_USERS},
        {L"home", ICO_HOME}, {L"food", ICO_FOOD}, {L"coffee", ICO_COFFEE}, {L"heart", ICO_HEART},
        {L"money", ICO_MONEY}, {L"phone", ICO_PHONE}, {L"mail", ICO_MAIL}, {L"music", ICO_MUSIC},
        {L"camera", ICO_CAMERA}, {L"code", ICO_CODE}, {L"bulb", ICO_BULB}, {L"flag", ICO_FLAG},
        {L"target", ICO_TARGET}, {L"trophy", ICO_TROPHY}, {L"gift", ICO_GIFT}, {L"plane", ICO_PLANE},
        {L"flask", ICO_FLASK}, {L"pen", ICO_PEN}, {L"mic", ICO_MIC}, {L"video", ICO_VIDEO},
        {L"map", ICO_MAP}, {L"cart", ICO_CART}, {L"bank", ICO_BANK}, {L"laptop", ICO_LAPTOP},
        {L"bus", ICO_BUS}, {L"game", ICO_GAME},
    };
    for (auto& kv : kMap) if (name == kv.first) return kv.second;
    return ICO_TAG;
}

const std::vector<std::wstring>& TagIconChoices() {
    static const std::vector<std::wstring> v = {
        L"book", L"briefcase", L"user", L"users", L"home", L"food", L"coffee", L"heart",
        L"money", L"phone", L"mail", L"music", L"camera", L"code", L"bulb", L"flag",
        L"target", L"trophy", L"gift", L"plane", L"flask", L"pen", L"mic", L"video",
        L"map", L"cart", L"bank", L"laptop", L"bus", L"game",
        L"tag", L"star", L"cal", L"bell", L"clock", L"drop", L"chart", L"folder", L"pin", L"edit",
    };
    return v;
}

const wchar_t* IconName(int id) {
    switch (id) {
    case ICO_PLUS: return L"plus"; case ICO_SEARCH: return L"search"; case ICO_GEAR: return L"gear";
    case ICO_THEME: return L"theme"; case ICO_CLOSE: return L"close"; case ICO_BELL: return L"bell";
    case ICO_DROP: return L"drop"; case ICO_CLOCK: return L"clock"; case ICO_CAL: return L"cal";
    default: return L"icon";
    }
}

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pInfo = (ImageCodecInfo*)malloc(size);
    if (!pInfo) return -1;
    GetImageEncoders(num, size, pInfo);
    int found = -1;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(pInfo[i].MimeType, format) == 0) { *pClsid = pInfo[i].Clsid; found = (int)i; break; }
    }
    free(pInfo);
    return found;
}

bool SavePng(Bitmap* bmp, const std::wstring& path) {
    CLSID clsid;
    if (GetEncoderClsid(L"image/png", &clsid) < 0) return false;
    return bmp->Save(path.c_str(), &clsid, nullptr) == Ok;
}

} // namespace gfx