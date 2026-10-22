// theme.h - 主题调色板
#pragma once
#include "gfx.h"
#include <string>

struct Theme {
    const wchar_t* id;
    const wchar_t* name;
    Gdiplus::Color bg;        // 窗口底色
    Gdiplus::Color bgTop;     // 背景渐变上
    Gdiplus::Color bgBottom;  // 背景渐变下
    Gdiplus::Color panel;     // 侧栏/面板
    Gdiplus::Color panel2;    // 面板内层
    Gdiplus::Color card;      // 卡片
    Gdiplus::Color cardHover; // 卡片悬停
    Gdiplus::Color text;      // 主文字
    Gdiplus::Color dim;       // 次文字
    Gdiplus::Color dim2;      // 弱文字
    Gdiplus::Color accent;    // 主强调（金）
    Gdiplus::Color accent2;   // 副强调
    Gdiplus::Color border;    // 边框
    Gdiplus::Color borderSoft;// 弱边框
    Gdiplus::Color danger;
    Gdiplus::Color success;
    Gdiplus::Color warn;
    Gdiplus::Color onAccent;  // 强调色上的文字
    int radius = 10;          // 主要控件圆角（0–20）
    int density = 100;        // 信息密度（85–115）
    bool light = false;       // 浅色主题
};

const Theme& ThemeAt(int i);
int ThemeCount();
int ThemeIndexById(const std::wstring& id);
std::wstring ThemeIdAt(int i);

// 自定义主题：由强调色 + 背景色生成完整可读调色板
Theme BuildCustomTheme(const std::wstring& bgHex, const std::wstring& accentHex,
                       int radius, int density, bool light);
void ApplyCustomTheme(const std::wstring& bgHex, const std::wstring& accentHex,
                      int radius, int density, bool light);
