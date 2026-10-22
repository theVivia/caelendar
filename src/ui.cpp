// ui.cpp - 即时模式控件、文本输入框管理、提示
#include "app.h"
#include <algorithm>
#include <map>
#include <cmath>

using namespace Gdiplus;
using namespace gfx;

std::vector<Hit> g_hits;
int g_hoverCmd = 0;
int g_drawLayer = 0;
int g_inputCmdLayer = 0;
HWND g_hwnd = nullptr;
HDC g_memdc = nullptr;
HBITMAP g_hbmp = nullptr;
void* g_bits = nullptr;
int g_bufW = 0, g_bufH = 0;
bool g_shotMode = false;
std::wstring g_shotPath;
bool g_demoMode = false;
bool g_noSave = false;
bool g_maximized = false;
bool g_running = true;
AppState app;

void AddHit(float x, float y, float w, float h, int cmd, std::function<void()> fn) {
    Hit hh;
    hh.r = RectF(x, y, w, h);
    hh.layer = g_drawLayer;
    hh.cmd = cmd;
    hh.fn = std::move(fn);
    g_hits.push_back(std::move(hh));
}
void ClearHits() { g_hits.clear(); }
bool GetHitPointByCmd(int cmd, float& x, float& y) {
    for (auto& h : g_hits) {
        if (h.cmd == cmd) { x = h.r.X + h.r.Width / 2; y = h.r.Y + h.r.Height / 2; return true; }
    }
    return false;
}
void UIInvalidate() { if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE); }

bool PointInHit(float px, float py, int& outCmd, int& outLayer) {
    int best = -1, bestLayer = -1;
    for (int i = 0; i < (int)g_hits.size(); ++i) {
        auto& h = g_hits[i];
        if (px < h.r.X || px > h.r.X + h.r.Width || py < h.r.Y || py > h.r.Y + h.r.Height) continue;
        if (h.layer > bestLayer || (h.layer == bestLayer && i > best)) { best = i; bestLayer = h.layer; }
    }
    if (best < 0) return false;
    outCmd = g_hits[best].cmd;
    outLayer = g_hits[best].layer;
    return true;
}
bool IsHovered(int cmd) { return cmd != 0 && g_hoverCmd == cmd; }

static int g_kbFocusCmd = 0;
int UIFocusCmd() { return g_kbFocusCmd; }
void UIFocusNext(bool backward) {
    std::vector<int> cmds;
    for (auto& h : g_hits) {
        if (h.cmd == 0) continue;
        bool dup = false;
        for (int c : cmds) if (c == h.cmd) { dup = true; break; }
        if (!dup) cmds.push_back(h.cmd);
    }
    if (cmds.empty()) { g_kbFocusCmd = 0; g_hoverCmd = 0; UIInvalidate(); return; }
    int idx = -1;
    for (int i = 0; i < (int)cmds.size(); ++i) if (cmds[i] == g_kbFocusCmd) { idx = i; break; }
    if (idx < 0) idx = backward ? (int)cmds.size() - 1 : 0;
    else idx = (idx + (backward ? -1 : 1) + (int)cmds.size()) % (int)cmds.size();
    g_kbFocusCmd = cmds[idx];
    g_hoverCmd = g_kbFocusCmd;
    UIInvalidate();
}
bool UIActivateFocus() {
    if (g_kbFocusCmd == 0) return false;
    int cmd = g_kbFocusCmd;
    for (auto& h : g_hits) {
        if (h.cmd == cmd && h.fn) {
            auto fn = h.fn;
            g_kbFocusCmd = 0;
            g_hoverCmd = 0;
            fn();
            UIInvalidate();
            return true;
        }
    }
    return false;
}
void UIClearFocus() {
    if (g_kbFocusCmd == 0 && g_hoverCmd == 0) return;
    g_kbFocusCmd = 0;
    g_hoverCmd = 0;
    UIInvalidate();
}

const Theme& TH() {
    if (!app.store.settings.highContrast) return ThemeAt(app.themeIdx);
    static Theme hc;
    hc = ThemeAt(app.themeIdx);
    hc.text = Color(255, 255, 255, 255);
    hc.dim = Color(255, 212, 220, 234);
    hc.dim2 = Color(255, 178, 188, 204);
    hc.border = Color(255, 236, 240, 248);
    hc.borderSoft = Color(255, 172, 180, 194);
    hc.bg = hc.light ? Color(255, 250, 250, 252) : Color(255, 2, 3, 6);
    hc.bgTop = hc.bg;
    hc.bgBottom = hc.bg;
    hc.card = hc.light ? Color(255, 255, 255, 255) : Color(255, 18, 21, 30);
    hc.panel = hc.light ? Color(255, 255, 255, 255) : Color(255, 24, 28, 38);
    hc.panel2 = hc.light ? Color(255, 244, 246, 250) : Color(255, 34, 40, 52);
    hc.cardHover = hc.light ? Color(255, 235, 238, 244) : Color(255, 46, 52, 66);
    hc.accent = hc.light ? Color(255, 140, 90, 0) : Color(255, 255, 220, 150);
    hc.accent2 = hc.light ? Color(255, 30, 90, 200) : Color(255, 160, 200, 255);
    hc.danger = hc.light ? Color(255, 190, 20, 40) : Color(255, 255, 120, 130);
    hc.success = hc.light ? Color(255, 10, 130, 70) : Color(255, 90, 230, 170);
    hc.warn = hc.light ? Color(255, 170, 100, 0) : Color(255, 255, 200, 90);
    return hc;
}
Color TagColorOf(const std::string& tagId) {
    std::wstring hex = app.store.TagColor(tagId);
    if (hex.empty()) return Color(255, 148, 163, 184);
    return Hex(hex);
}
std::wstring TagHex(const std::string& tagId) { return app.store.TagColor(tagId); }
Color Blend(Color a, Color b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return Color((BYTE)(a.GetA() + (b.GetA() - a.GetA()) * t),
                 (BYTE)(a.GetR() + (b.GetR() - a.GetR()) * t),
                 (BYTE)(a.GetG() + (b.GetG() - a.GetG()) * t),
                 (BYTE)(a.GetB() + (b.GetB() - a.GetB()) * t));
}

void ApplyFontSettings() {
    const Settings& s = app.store.settings;
    gfx::fontRuntime.uiFamily = s.fontUI.empty() ? L"Noto Sans SC" : s.fontUI;
    gfx::fontRuntime.numFamily = s.fontNum.empty() ? L"Bahnschrift" : s.fontNum;
    gfx::fontRuntime.globalScale = std::max(0.5f, std::min(2.0f, s.fontScale / 100.0f));
    for (int i = 0; i < gfx::FP_COUNT && i < kFontPartCount; ++i) {
        gfx::fontRuntime.parts[i].family = s.fontParts[i].family;
        gfx::fontRuntime.parts[i].scale = std::max(0.5f, std::min(2.0f, s.fontParts[i].size / 100.0f));
    }
}

void RequestRepaint() { UIInvalidate(); }

void ShowToast(const std::wstring& msg, int ms) {
    app.toastText = msg;
    app.toastUntil = GetTickCount() + ms;
    UIInvalidate();
}

// ---------------- 字体（GDI，给原生 EDIT 用） ----------------
static std::map<std::wstring, HFONT> g_gdiFonts;
HFONT UIGdiFont(float px, bool bold, bool display) {
    float scale = gfx::fontRuntime.globalScale;
    std::wstring fam;
    int part = gfx::CurrentFontPart();
    if (part >= 0 && part < gfx::FP_COUNT) {
        scale *= gfx::fontRuntime.parts[part].scale;
        fam = gfx::fontRuntime.parts[part].family;
    }
    if (fam.empty()) fam = display ? gfx::fontRuntime.numFamily : gfx::fontRuntime.uiFamily;
    if (scale <= 0.05f) scale = 1.0f;
    int ipx = (int)std::lround(px * scale);
    if (ipx < 1) ipx = 1;
    std::wstring key = Fmt(L"%d|%d|%s", ipx, bold ? 1 : 0, fam.c_str());
    auto it = g_gdiFonts.find(key);
    if (it != g_gdiFonts.end()) return it->second;
    HFONT f = CreateFontW(-ipx, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
                          FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fam.c_str());
    if (!f) f = CreateFontW(-ipx, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
                            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_gdiFonts[key] = f;
    return f;
}
// ---------------- 原生 EDIT 控件池 ----------------
static std::wstring ToEditText(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (wchar_t c : s) {
        if (c == L'\r') continue;
        if (c == L'\n') { out += L"\r\n"; continue; }
        out += c;
    }
    return out;
}
static std::wstring FromEditText(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r' && i + 1 < s.size() && s[i + 1] == L'\n') { out += L'\n'; ++i; }
        else out += s[i];
    }
    return out;
}
struct EditSlot {
    HWND hwnd = nullptr;
    std::wstring* buf = nullptr;
    bool multiline = false;
    bool used = false;
    float px = 13;
    bool bold = false;
};
static std::map<int, EditSlot> g_edits;
static std::map<HWND, int> g_editByHwnd;
static bool g_editsActive = false;
static int g_editSeq = 0;
static HBRUSH g_editBrush = nullptr;
static Color g_editBg;

static void EnsureEditClass() {}
void CloseAllEdits() {
    for (auto& kv : g_edits) if (kv.second.hwnd) DestroyWindow(kv.second.hwnd);
    g_edits.clear();
    g_editByHwnd.clear();
}

static std::map<HWND, WNDPROC> g_editOldProc;

static LRESULT CALLBACK SearchEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto it = g_editOldProc.find(hwnd);
    WNDPROC old = it == g_editOldProc.end() ? nullptr : it->second;
    if (msg == WM_KEYDOWN) {
        if (wp == VK_LEFT || wp == VK_RIGHT) {
            // 用户没有主动编辑搜索时，方向键还给日历；主动点的搜索框仍正常移动文本光标
            if (!app.searchIntent) {
                SetFocus(g_hwnd);
                SendMessageW(g_hwnd, WM_KEYDOWN, wp, lp);
                return 0;
            }
        }
        if (wp == VK_ESCAPE) {
            app.search.clear();
            app.searchFocused = false;
            app.searchIntent = false;
            app.searchEditing = false;
            SetFocus(g_hwnd);
            UIInvalidate();
            return 0;
        }
    }
    return old ? CallWindowProcW(old, hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);

}

HWND UIEdit(int id, RectF r, std::wstring* buf, bool multiline, float px, bool bold) {
    if (!g_hwnd) return nullptr;
    auto it = g_edits.find(id);
    if (it == g_edits.end()) {
        EditSlot s;
        DWORD style = WS_CHILD | ES_AUTOHSCROLL;
        if (multiline) style = WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL;
        s.hwnd = CreateWindowExW(0, L"EDIT", L"", style, 0, 0, 10, 10, g_hwnd, (HMENU)(INT_PTR)(9000 + id),
                                 GetModuleHandleW(nullptr), nullptr);
        if (!s.hwnd) return nullptr;
        s.buf = buf;
        s.multiline = multiline;
        s.px = px;
        s.bold = bold;
        SendMessageW(s.hwnd, WM_SETFONT, (WPARAM)UIGdiFont(px, bold), TRUE);
        SendMessageW(s.hwnd, EM_SETLIMITTEXT, multiline ? 20000 : 512, 0);
        if (buf) SetWindowTextW(s.hwnd, (multiline ? ToEditText(*buf) : *buf).c_str());
        g_edits[id] = s;
        g_editByHwnd[s.hwnd] = id;
        if (id == 9001) {
            g_editOldProc[s.hwnd] = (WNDPROC)SetWindowLongPtrW(s.hwnd, GWLP_WNDPROC, (LONG_PTR)SearchEditProc);
        }
        ShowWindow(s.hwnd, SW_SHOW);
    }
    EditSlot& s = g_edits[id];
    s.used = true;
    s.buf = buf;
    if (!buf) { ShowWindow(s.hwnd, SW_HIDE); return s.hwnd; }
    // 同步外部改动（仅当未聚焦时）
    if (GetFocus() != s.hwnd) {
        wchar_t tmp[1024];
        GetWindowTextW(s.hwnd, tmp, 1024);
        std::wstring cur = FromEditText(tmp);
        if (*buf != cur) SetWindowTextW(s.hwnd, (multiline ? ToEditText(*buf) : *buf).c_str());
    }
    s.px = px; s.bold = bold;
    SendMessageW(s.hwnd, WM_SETFONT, (WPARAM)UIGdiFont(px, bold), TRUE);
    MoveWindow(s.hwnd, (int)std::lround(r.X), (int)std::lround(r.Y), (int)std::lround(r.Width), (int)std::lround(r.Height), TRUE);
    ShowWindow(s.hwnd, SW_SHOW);
    g_editsActive = true;
    return s.hwnd;
}
void UIEditEndFrame() {
    for (auto& kv : g_edits) {
        if (!kv.second.used) ShowWindow(kv.second.hwnd, SW_HIDE);
        kv.second.used = false;
    }
    g_editsActive = false;
}
bool UIHandleCommand(WPARAM wParam, LPARAM lParam) {
    int id = LOWORD(wParam);
    HWND h = (HWND)lParam;
    if (HIWORD(wParam) == EN_CHANGE && h) {
        auto it = g_editByHwnd.find(h);
        if (it != g_editByHwnd.end()) {
            auto& slot = g_edits[it->second];
            if (slot.buf) {
                int len = GetWindowTextLengthW(h);
                std::wstring s(len, 0);
                if (len) GetWindowTextW(h, &s[0], len + 1);
                if (slot.multiline) s = FromEditText(s);
                *slot.buf = s;
                DebugLog(L"edit EN_CHANGE id=%d text=%s", it->second, s.c_str());
            }
            UIInvalidate();
            return true;
        }
    }
    if (HIWORD(wParam) == EN_SETFOCUS) {
        auto it = g_editByHwnd.find(h);
        if (it != g_editByHwnd.end() && it->second == 9001) {
            POINT pt; GetCursorPos(&pt);
            RECT wr; GetWindowRect(h, &wr);
            bool mouseOver = PtInRect(&wr, pt) != 0;
            if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 || mouseOver) app.searchIntent = true;
        }
        UIInvalidate();
        return true;
    }
    if (HIWORD(wParam) == EN_KILLFOCUS) {
        auto it = g_editByHwnd.find(h);
        if (it != g_editByHwnd.end() && it->second == 9001) {
            app.searchIntent = false;
            app.searchEditing = false;
        }
        UIInvalidate();
        return true;
    }
    
    return false;
}
INT_PTR UICtlColorEdit(HWND hwnd, HDC hdc) {
    Color bg = TH().panel2;
    SetTextColor(hdc, RGB(TH().text.GetR(), TH().text.GetG(), TH().text.GetB()));
    SetBkColor(hdc, RGB(bg.GetR(), bg.GetG(), bg.GetB()));
    if (!g_editBrush || g_editBg.GetValue() != bg.GetValue()) {
        if (g_editBrush) DeleteObject(g_editBrush);
        g_editBrush = CreateSolidBrush(RGB(bg.GetR(), bg.GetG(), bg.GetB()));
        g_editBg = bg;
    }
    return (INT_PTR)g_editBrush;
}
bool UIAnyEditVisible() {
    for (auto& kv : g_edits) if (IsWindowVisible(kv.second.hwnd)) return true;
    return false;
}
void UISetEditFocus(int id, bool selectAll) {
    auto it = g_edits.find(id);
    if (it == g_edits.end()) return;
    SetFocus(it->second.hwnd);
    if (selectAll) SendMessageW(it->second.hwnd, EM_SETSEL, 0, -1);
}

// ---------------- 绘制控件 ----------------
namespace UI {

void Panel(Graphics& g, RectF r, float radius, bool raised) {
    const Theme& th = TH();
    if (raised) Shadow(g, r, radius, 5, 14);
    FillRoundRect(g, r, radius, th.card);
    StrokeRoundRect(g, r, radius, th.border, S(1));
}

void Dim(Graphics& g, float W, float H, BYTE alpha) {
    SolidBrush b(Color(alpha, 2, 3, 6));
    g.FillRectangle(&b, RectF(0, 0, W, H));
}

void Btn(Graphics& g, RectF r, int cmd, const std::wstring& label, int style, int icon, std::function<void()> fn) {
    const Theme& th = TH();
    bool hover = IsHovered(cmd);
    bool disabled = !fn;
    if (style == 0) {           // primary
        Color bg = disabled ? WithAlpha(th.dim2, 60) : (hover ? WithAlpha(th.accent, 255) : th.accent);
        FillRoundRect(g, r, S(8), bg);
        if (hover && !disabled) Glow(g, r, S(8), WithAlpha(th.accent, 55), 2);
    } else if (style == 1) {    // secondary / outline
        FillRoundRect(g, r, S(8), hover ? th.cardHover : WithAlpha(th.panel2, 200));
        StrokeRoundRect(g, r, S(8), hover ? WithAlpha(th.accent, 120) : th.border, S(1));
    } else if (style == 2) {    // ghost
        if (hover) FillRoundRect(g, r, S(8), WithAlpha(th.cardHover, 220));
    } else if (style == 3) {    // danger
        FillRoundRect(g, r, S(8), hover ? WithAlpha(th.danger, 40) : WithAlpha(th.danger, 20));
        StrokeRoundRect(g, r, S(8), WithAlpha(th.danger, hover ? 160 : 90), S(1));
    }
    Color fg = (style == 0) ? th.onAccent : (style == 3 ? th.danger : (hover ? th.text : th.dim));
    if (disabled) fg = WithAlpha(th.dim2, 120);
    float iconW = icon >= 0 ? S(15) : 0;
    float gap = (icon >= 0 && !label.empty()) ? S(6) : 0;
    SizeF sz; MeasureText(g, label, S(12.5f), true, sz);
    float total = iconW + gap + sz.Width;
    float x = r.X + (r.Width - total) / 2;
    float cym = r.Y + r.Height / 2;
    if (icon >= 0) {
        Icon(g, icon, RectF(x, cym - S(7.5f), S(15), S(15)), fg, 1.6f);
        x += iconW + gap;
    }
    if (!label.empty()) Text(g, label, S(12.5f), true, fg, RectF(x, r.Y, sz.Width + S(2), r.Height), ALIGN_VCENTER);
    if (!disabled) AddHit(r.X, r.Y, r.Width, r.Height, cmd, std::move(fn));
}

void IconBtn(Graphics& g, RectF r, int cmd, int icon, float iconSize, std::function<void()> fn) {
    const Theme& th = TH();
    bool hover = IsHovered(cmd);
    if (hover) FillRoundRect(g, r, S(8), WithAlpha(th.cardHover, 235));
    Color fg = hover ? th.text : th.dim;
    Icon(g, icon, RectF(r.X + (r.Width - S(iconSize)) / 2, r.Y + (r.Height - S(iconSize)) / 2, S(iconSize), S(iconSize)), fg, 1.6f);
    AddHit(r.X, r.Y, r.Width, r.Height, cmd, std::move(fn));
}

void Chip(Graphics& g, RectF r, int cmd, const std::wstring& label, bool active, Color accent,
          std::function<void()> fn, float px) {
    const Theme& th = TH();
    bool hover = IsHovered(cmd);
    Color bg = active ? WithAlpha(accent, 42) : (hover ? th.cardHover : WithAlpha(th.panel2, 170));
    FillRoundRect(g, r, S(7), bg);
    StrokeRoundRect(g, r, S(7), active ? WithAlpha(accent, 150) : th.borderSoft, S(1));
    Color fg = active ? accent : (hover ? th.text : th.dim);
    Text(g, label, S(px), active, fg, r, ALIGN_CENTER | ALIGN_VCENTER);
    if (fn) AddHit(r.X, r.Y, r.Width, r.Height, cmd, std::move(fn));
}

void TagChip(Graphics& g, RectF r, int cmd, const std::wstring& name, Color color, bool active,
             std::function<void()> fn, int icon) {
    const Theme& th = TH();
    bool hover = IsHovered(cmd);
    Color bg = active ? WithAlpha(color, 46) : (hover ? WithAlpha(color, 26) : WithAlpha(th.panel2, 190));
    FillRoundRect(g, r, S(7), bg);
    StrokeRoundRect(g, r, S(7), active ? WithAlpha(color, 170) : WithAlpha(color, 40), S(1));
    float labelX = r.X + S(19);
    if (icon >= 0) {
        Icon(g, icon, RectF(r.X + S(7), r.Y + (r.Height - S(13)) / 2, S(13), S(13)), color, 1.4f);
        labelX = r.X + S(24);
    } else {
        FillCircle(g, r.X + S(11), r.Y + r.Height / 2, S(3.2f), color);
    }
    Text(g, name, S(10.5f), active, active ? th.text : th.dim,
         RectF(labelX, r.Y, r.Width - (labelX - r.X) - S(4), r.Height), ALIGN_VCENTER);
    if (fn) AddHit(r.X, r.Y, r.Width, r.Height, cmd, std::move(fn));
}

void CheckBox(Graphics& g, RectF r, int cmd, const std::wstring& label, bool checked, std::function<void()> fn) {
    const Theme& th = TH();
    bool hover = IsHovered(cmd);
    float box = S(17);
    float cx = r.X + box / 2, cy = r.Y + r.Height / 2;
    FillRoundRect(g, RectF(r.X, cy - box / 2, box, box), S(5), checked ? th.accent : WithAlpha(th.panel2, 220));
    StrokeRoundRect(g, RectF(r.X, cy - box / 2, box, box), S(5), checked ? th.accent : (hover ? WithAlpha(th.accent, 100) : th.border), S(1.2f));
    if (checked) Icon(g, ICO_CHECK, RectF(r.X + S(3), cy - S(6), S(11), S(11)), th.onAccent, 2.0f);
    Text(g, label, S(12), false, hover ? th.text : th.dim, RectF(r.X + box + S(8), r.Y, r.Width - box - S(8), r.Height), ALIGN_VCENTER);
    AddHit(r.X, r.Y, r.Width, r.Height, cmd, std::move(fn));
}

void SectionTitle(Graphics& g, const std::wstring& label, RectF r) {
    Text(g, label, S(10.5f), true, TH().dim2, r, ALIGN_LEFT | ALIGN_VCENTER);
}

void Divider(Graphics& g, float x, float y, float w) {
    Line(g, x, y, x + w, y, TH().borderSoft, S(1));
}

void Scrollbar(Graphics& g, RectF track, float viewH, float contentH, float scroll) {
    const Theme& th = TH();
    if (contentH <= viewH + 1) return;
    FillRoundRect(g, track, S(3), WithAlpha(th.border, 90));
    float frac = viewH / contentH;
    float thumbH = std::max(S(24), track.Height * frac);
    float maxScroll = contentH - viewH;
    float t = maxScroll > 0 ? std::max(0.0f, std::min(1.0f, scroll / maxScroll)) : 0;
    float y = track.Y + (track.Height - thumbH) * t;
    FillRoundRect(g, RectF(track.X + S(1), y, track.Width - S(2), thumbH), S(3), WithAlpha(th.accent, 90));
}

void Stepper(Graphics& g, RectF r, int cmdDec, int cmdInc, const std::wstring& value,
             std::function<void()> dec, std::function<void()> inc, float px) {
    const Theme& th = TH();
    float bw = S(24);
    RectF left(r.X, r.Y, bw, r.Height), right(r.X + r.Width - bw, r.Y, bw, r.Height);
    FillRoundRect(g, r, S(7), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, r, S(7), th.borderSoft, S(1));
    if (IsHovered(cmdDec)) FillRoundRect(g, left, S(7), th.cardHover);
    if (IsHovered(cmdInc)) FillRoundRect(g, right, S(7), th.cardHover);
    Icon(g, ICO_MINUS, RectF(left.X + S(6), left.Y + left.Height / 2 - S(5), S(12), S(10)), th.dim, 1.5f);
    Icon(g, ICO_PLUS, RectF(right.X + S(6), right.Y + right.Height / 2 - S(5), S(12), S(10)), th.dim, 1.5f);
    Text(g, value, S(px), true, th.text, RectF(left.X + bw, r.Y, r.Width - bw * 2, r.Height), ALIGN_CENTER | ALIGN_VCENTER);
    AddHit(left.X, left.Y, left.Width, left.Height, cmdDec, std::move(dec));
    AddHit(right.X, right.Y, right.Width, right.Height, cmdInc, std::move(inc));
}

void FieldLabel(Graphics& g, const std::wstring& label, float x, float y, float w) {
    Text(g, label, S(10.5f), false, TH().dim2, RectF(x, y, w, S(16)));
}

void EditFrame(Graphics& g, RectF r) {
    FillRoundRect(g, r, S(8), TH().panel2);
    StrokeRoundRect(g, r, S(8), IsHovered(0) ? TH().border : TH().borderSoft, S(1));
}

} // namespace UI

// ---------------- 提示条 ----------------
void DrawToast(Graphics& g, float W, float H) {
    if (app.toastText.empty()) return;
    if ((DWORD)GetTickCount() > app.toastUntil) { app.toastText.clear(); return; }
    const Theme& th = TH();
    float px = S(12);
    const float padL = S(34);      // 图标区
    const float padR = S(16);
    float maxBubbleW = W - S(96);
    float maxTextW = std::max(S(80), maxBubbleW - padL - padR);

    SizeF sz; MeasureText(g, app.toastText, px, false, sz);
    bool twoLine = false;
    std::wstring line1 = app.toastText, line2;
    float textW = sz.Width;

    if (sz.Width > maxTextW) {
        // 超长：找第一行能放下的位置，优先在空格 / 标点处断行
        int best = 0;
        for (int i = 1; i < (int)app.toastText.size(); ++i) {
            SizeF p; MeasureText(g, app.toastText.substr(0, i), px, false, p);
            if (p.Width > maxTextW) break;
            best = i;
        }
        if (best <= 0) best = (int)app.toastText.size();
        int cut = best;
        for (int i = best; i > std::max(1, (int)(best * 0.55f)); --i) {
            wchar_t c = app.toastText[i - 1];
            if (c == L' ' || c == L'·' || c == L'；' || c == L'，' || c == L'（' ||
                c == L'、' || c == L'：' || c == L'—') { cut = i; break; }
        }
        if (cut <= 0) cut = best;
        line1 = app.toastText.substr(0, cut);
        line2 = app.toastText.substr(cut);
        while (!line2.empty() && line2[0] == L' ') line2.erase(0, 1);
        SizeF s1, s2;
        MeasureText(g, line1, px, false, s1);
        MeasureText(g, line2, px, false, s2);
        textW = std::max(s1.Width, s2.Width);
        if (textW > maxTextW) textW = maxTextW;
        twoLine = true;
    }

    float w = std::min(maxBubbleW, textW + padL + padR);
    float h = twoLine ? S(56) : S(38);
    float y = H - S(64) - (twoLine ? S(18) : 0);
    RectF r((W - w) / 2, y, w, h);
    Shadow(g, r, S(10), 5, 20);
    FillRoundRect(g, r, S(10), th.card);
    StrokeRoundRect(g, r, S(10), WithAlpha(th.accent, 90), S(1));
    float textX = r.X + padL;
    float textWAvail = w - padL - padR;
    if (!twoLine) {
        Icon(g, ICO_CHECK, RectF(r.X + S(12), r.Y + S(11), S(15), S(15)), th.accent, 1.6f);
        Text(g, line1, px, false, th.text, RectF(textX, r.Y, textWAvail, r.Height), ALIGN_VCENTER, false, true);
    } else {
        Icon(g, ICO_CHECK, RectF(r.X + S(12), r.Y + S(9), S(15), S(15)), th.accent, 1.6f);
        Text(g, line1, px, false, th.text, RectF(textX, r.Y + S(7), textWAvail, S(20)), ALIGN_LEFT, false, true);
        Text(g, line2, px, false, WithAlpha(th.text, 235), RectF(textX, r.Y + S(29), textWAvail, S(20)), ALIGN_LEFT, false, true);
    }
}