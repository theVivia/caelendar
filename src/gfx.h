// gfx.h - GDI+ 绘图封装
#pragma once
#include "util.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <map>
#include <vector>

namespace gfx {
using namespace Gdiplus;

extern float dpi;   // 1.0 = 96 DPI
inline float S(float v) { return v * dpi; }

// ---------- 字体分区（设置里可分别自定义字体族与字号） ----------
enum FontPartId {
    FP_TITLEBAR = 0, FP_SIDEBAR, FP_MONTH, FP_CHIP, FP_LIST, FP_EDITOR, FP_DIALOG, FP_WIDGET, FP_COUNT
};
struct FontPartRuntime {
    std::wstring family;      // 空 = 跟随全局（中文用 uiFamily，数字用 numFamily）
    float scale = 1.0f;       // 该区域字号缩放
};
struct FontRuntime {
    std::wstring uiFamily = L"Noto Sans SC";
    std::wstring numFamily = L"Bahnschrift";
    float globalScale = 1.0f;
    FontPartRuntime parts[FP_COUNT];
};
extern FontRuntime fontRuntime;
extern int gFontPart;
void SetFontPart(int part);
inline int CurrentFontPart() { return gFontPart; }
struct FontPartScope {
    int old;
    explicit FontPartScope(int p) : old(gFontPart) { gFontPart = p; }
    ~FontPartScope() { gFontPart = old; }
};
std::vector<std::wstring> ListInstalledFonts();

void Init();
void Shutdown();

// 字体：display=true 用 Bahnschrift（数字/标题），否则 Noto Sans SC
Font* GetFont(float px, bool bold, bool display = false);
void Text(Graphics& g, const std::wstring& s, float px, bool bold, Color c,
          RectF r, int align = 0, bool display = false, bool ellipsis = true, bool wrap = false);
void MeasureText(Graphics& g, const std::wstring& s, float px, bool bold, SizeF& out, bool display = false);
float TextWidth(Graphics& g, const std::wstring& s, float px, bool bold, bool display = false);

// align 位掩码：水平 0=左 1=中 2=右；垂直 0x10=中 0x20=底
enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2,
       ALIGN_TOP = 0, ALIGN_VCENTER = 0x10, ALIGN_BOTTOM = 0x20 };

void FillRoundRect(Graphics& g, RectF r, float radius, Color c);
void FillRoundRectV(Graphics& g, RectF r, float radius, Color top, Color bottom);
void StrokeRoundRect(Graphics& g, RectF r, float radius, Color c, float w = 1.0f);
void FillCircle(Graphics& g, float cx, float cy, float r, Color c);
void StrokeCircle(Graphics& g, float cx, float cy, float r, Color c, float w = 1.0f);
void Line(Graphics& g, float x1, float y1, float x2, float y2, Color c, float w = 1.0f);
void FillVGradient(Graphics& g, RectF r, Color top, Color bottom);
void FillCircleV(Graphics& g, float cx, float cy, float r, Color top, Color bottom);
void Shadow(Graphics& g, RectF r, float radius, int layers = 4, BYTE alpha = 10);
void Glow(Graphics& g, RectF r, float radius, Color c, int layers = 3);

inline float RX(const RectF& r) { return r.X + r.Width; }
inline float BY(const RectF& r) { return r.Y + r.Height; }

Color Hex(const std::wstring& hex, BYTE a = 255);
Color WithAlpha(Color c, BYTE a);

// 图标
enum IconId {
    ICO_PLUS = 0, ICO_MINUS, ICO_CHEV_LEFT, ICO_CHEV_RIGHT, ICO_CHEV_DOWN, ICO_CHEV_UP,
    ICO_SEARCH, ICO_GEAR, ICO_THEME, ICO_CLOSE, ICO_MIN, ICO_MAX, ICO_CHECK,
    ICO_BELL, ICO_DROP, ICO_CLOCK, ICO_CAL, ICO_LIST, ICO_GRID, ICO_WEEK, ICO_DAY,
    ICO_TRASH, ICO_EDIT, ICO_COPY, ICO_MORE, ICO_STAR, ICO_TAG, ICO_FILTER,
    ICO_ARROW_RIGHT, ICO_ARROW_LEFT, ICO_REFRESH, ICO_FOLDER, ICO_CHART, ICO_PIN,
    ICO_DRAG, ICO_PLAY, ICO_PAUSE, ICO_SUN, ICO_MOON, ICO_WARN, ICO_SETTINGS, ICO_HELP,
    ICO_BOOK, ICO_BRIEFCASE, ICO_USER, ICO_USERS, ICO_HOME, ICO_FOOD, ICO_COFFEE, ICO_HEART,
    ICO_MONEY, ICO_PHONE, ICO_MAIL, ICO_MUSIC, ICO_CAMERA, ICO_CODE, ICO_BULB, ICO_FLAG,
    ICO_TARGET, ICO_TROPHY, ICO_GIFT, ICO_PLANE, ICO_FLASK, ICO_PEN, ICO_MIC, ICO_VIDEO,
    ICO_MAP, ICO_CART, ICO_BANK, ICO_LAPTOP, ICO_BUS, ICO_GAME, ICO_UNDO, ICO_REDO,
    ICO_COUNT
};
void Icon(Graphics& g, int id, RectF box, Color c, float weight = 1.5f);
Gdiplus::Bitmap* BrandLogo();   // 内嵌品牌 logo（res\caelendar.png），失败返回 nullptr
const wchar_t* IconName(int id);
int IconFromName(const std::wstring& name);
const std::vector<std::wstring>& TagIconChoices();

// PNG 保存（bmp 需已渲染）
bool SavePng(Bitmap* bmp, const std::wstring& path);
} // namespace gfx
