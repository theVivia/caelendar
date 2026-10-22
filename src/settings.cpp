// settings.cpp - 设置面板
#include "app.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <algorithm>
#include <vector>
#include <string>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}
static HWND EditInBox(Graphics& g, int id, RectF r, std::wstring* buf, float px = 11.5f) {
    const Theme& th = TH();
    FillRoundRect(g, r, S(7), th.panel2);
    HWND h = UIEdit(id, RectF(r.X + S(8), r.Y + (r.Height - S(19)) / 2, r.Width - S(16), S(19)), buf, false, S(px));
    StrokeRoundRect(g, r, S(7), (h && GetFocus() == h) ? WithAlpha(th.accent, 150) : th.borderSoft, S(1));
    return h;
}
static void SectionBox(Graphics& g, float x, float y, float w, float h, const wchar_t* title) {
    const Theme& th = TH();
    FillRoundRect(g, RectF(x, y, w, h), S(12), WithAlpha(th.bg, 130));
    StrokeRoundRect(g, RectF(x, y, w, h), S(12), th.borderSoft, S(1));
    Txt(g, title, S(11), true, th.accent, RectF(x + S(14), y + S(9), w - S(28), S(18)));
}

// 最近一份可用备份（按修改时间最新）；解析出错则返回 false
static bool LatestBackup(std::wstring& pathOut, std::wstring& labelOut, int& eventsOut) {
    std::wstring dir = PathJoin(GetDataDir(), L"backup");
    auto files = ListFiles(dir, L".json");
    ULONGLONG best = 0;
    for (auto& f : files) {
        WIN32_FILE_ATTRIBUTE_DATA ad{};
        if (!GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &ad)) continue;
        ULONGLONG t = ((ULONGLONG)ad.ftLastWriteTime.dwHighDateTime << 32) | ad.ftLastWriteTime.dwLowDateTime;
        if (t < best) continue;
        bool ok = false;
        std::string data = ReadFileBytes(f, &ok);
        if (!ok || data.empty()) continue;
        Store tmp;
        std::wstring err;
        if (!tmp.FromJsonStr(data, &err)) continue;
        bool welcomeCover = tmp.settings.seededSample && tmp.events.size() == 1 &&
            Contains(tmp.events[0].title, L"欢迎使用 CAELENDAR");
        if (tmp.events.empty() || welcomeCover) continue;
        best = t;
        pathOut = f;
        eventsOut = (int)tmp.events.size();
        size_t p = f.find_last_of(L"\\/");
        labelOut = (p == std::wstring::npos) ? f : f.substr(p + 1);
    }
    return !pathOut.empty();
}

static void ApplyRestoredStore(Store&& tmp) {
    app.store = std::move(tmp);
    DoSaveStore();
    InvalidateOccs();
    app.selected.clear();
    ApplyThemeFromSettings();
    ApplyFontSettings();
    app.overlay = OV_NONE;
    app.settingsOpen = false;
    UIInvalidate();
}

static void CloseSettings() {
    Settings& s = app.store.settings;
    DateTime d;
    if (DateInputParse(app.setSemStart, s.semester.start, d)) s.semester.start = d;
    int h = 0, mi = 0;
    if (TimeInputParse(app.setWaterFrom, h, mi)) s.water.fromMin = h * 60 + mi;
    if (TimeInputParse(app.setWaterTo, h, mi)) s.water.toMin = h * 60 + mi;
    app.store.settings.theme = ThemeIdAt(app.themeIdx);
    app.store.Touch();
    DoSaveStore();
    app.overlay = OV_NONE;
    app.settingsOpen = false;
    if (app.store.settings.widget.enabled) {
        DestroyWidgetWindow();
        CreateWidgetWindow();
    } else {
        DestroyWidgetWindow();
    }
    InvalidateOccs();
    UIInvalidate();
}

static std::wstring PickAutoBackupFolder() {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = L"选择自动备份文件夹（每天保存一份，保留最近 30 份）";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    std::wstring out;
    if (pidl) {
        wchar_t buf[MAX_PATH] = { 0 };
        if (SHGetPathFromIDListW(pidl, buf)) out = buf;
        CoTaskMemFree(pidl);
    }
    return out;
}

void DrawSettingsOverlay(Graphics& g, float W, float H) {
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    const Theme& th = TH();
    Settings& s = app.store.settings;
    float pw = S(800), ph = S(760);
    pw = std::min(pw, W - S(40));
    ph = std::min(ph, H - S(24));
    float x = (W - pw) / 2, y = (H - ph) / 2;
    Shadow(g, RectF(x, y, pw, ph), S(16), 8, 26);
    FillRoundRect(g, RectF(x, y, pw, ph), S(16), th.card);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(16), th.border, S(1));
    float pad = S(22);
    Txt(g, L"设置", S(17), true, th.text, RectF(x + pad, y + S(14), S(120), S(26)));
    Txt(g, L"改动即时生效并保存", S(9.5f), false, th.dim2, RectF(x + pad + S(60), y + S(20), S(220), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(26), S(26)), CMD_SET_CLOSE, ICO_CLOSE, 12, []() { CloseSettings(); });

    float colW = (pw - pad * 2 - S(16)) / 2;
    float lx = x + pad, rx = lx + colW + S(16);
    // 内容可能高于可视区域：裁剪 + 滚轮滚动
    float viewTop = y + S(46);
    float viewH = ph - S(56);
    float contentH = S(980);
    float maxScroll = std::max(0.0f, contentH - viewH);
    app.settingsScroll = std::max(0.0f, std::min(maxScroll, app.settingsScroll));
    float scroll = app.settingsScroll;
    g.SetClip(RectF(x + S(4), viewTop, pw - S(8), viewH));
    float yy = y + S(50) - scroll;

    // ---- 左列 ----
    // 外观
    SectionBox(g, lx, yy, colW, S(212), L"外观");
    Txt(g, L"主题", S(10), false, th.dim2, RectF(lx + S(14), yy + S(34), S(60), S(18)));
    for (int i = 0; i < ThemeCount(); ++i) {
        RectF r(lx + S(62) + (i % 5) * S(52), yy + S(30) + (i / 5) * S(28), S(48), S(24));
        int idx = i;
        UI::Chip(g, r, CMD_SET_THEME_BASE + i, ThemeAt(i).name, app.themeIdx == i, th.accent, [idx]() {
            app.themeIdx = idx;
            app.store.settings.theme = ThemeIdAt(idx);
            app.needSave = true;
            UIInvalidate();
        }, 9.5f);
    }
    Txt(g, L"每周从", S(10), false, th.dim2, RectF(lx + S(14), yy + S(98), S(60), S(18)));
    UI::Chip(g, RectF(lx + S(62), yy + S(95), S(74), S(24)), CMD_SET_WEEKSTART_BASE + 1, L"周一开始",
             s.weekStart == 1, th.accent, []() { app.store.settings.weekStart = 1; app.needSave = true; InvalidateOccs(); UIInvalidate(); }, 9.5f);
    UI::Chip(g, RectF(lx + S(142), yy + S(95), S(74), S(24)), CMD_SET_WEEKSTART_BASE + 0, L"周日开始",
             s.weekStart == 0, th.accent, []() { app.store.settings.weekStart = 0; app.needSave = true; InvalidateOccs(); UIInvalidate(); }, 9.5f);
    // 修正第二个按钮文字
    Txt(g, L"", S(1), false, th.dim2, RectF(0, 0, 1, 1));
    UI::CheckBox(g, RectF(lx + S(14), yy + S(126), S(90), S(24)), CMD_SET_LUNAR, L"显示节日", s.lunar, []() {
        app.store.settings.lunar = !app.store.settings.lunar;
        app.needSave = true;
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(lx + S(112), yy + S(126), S(96), S(24)), CMD_SET_WEEKNUM, L"显示周数", s.weekNumbers, []() {
        app.store.settings.weekNumbers = !app.store.settings.weekNumbers;
        app.needSave = true;
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(lx + S(214), yy + S(126), S(110), S(24)), CMD_SET_REDUCEMOTION, L"减少动画", s.reduceMotion, []() {
        app.store.settings.reduceMotion = !app.store.settings.reduceMotion;
        app.needSave = true;
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(lx + S(150), yy + S(152), S(120), S(24)), CMD_SET_MONTHCHIPS, L"月视图事件条", s.monthChips, []() {
        app.store.settings.monthChips = !app.store.settings.monthChips;
        app.needSave = true;
        DoSaveStore();
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(lx + S(276), yy + S(152), S(92), S(24)), CMD_SET_MONTHCOUNTS, L"事件数", s.monthCounts, []() {
        app.store.settings.monthCounts = !app.store.settings.monthCounts;
        app.needSave = true;
        DoSaveStore();
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(lx + S(14), yy + S(152), S(130), S(24)), CMD_SET_HIGHCONTRAST, L"高对比度", s.highContrast, []() {
        app.store.settings.highContrast = !app.store.settings.highContrast;
        app.needSave = true;
        DoSaveStore();
        ShowToast(app.store.settings.highContrast ? L"高对比度已开启" : L"高对比度已关闭");
        UIInvalidate();
    });
    Txt(g, L"提示：周一开始 / 周日开始影响排列", S(9), false, th.dim2,
        RectF(lx + S(14), yy + S(182), colW - S(130), S(16)));
    UI::Btn(g, RectF(lx + colW - S(114), yy + S(176), S(108), S(26)), CMD_THEMESTUDIO_OPEN, L"主题工坊…", 1, ICO_STAR, []() {
        OpenThemeStudio();
    });
    yy += S(222);

    // 学期
    SectionBox(g, lx, yy, colW, S(118), L"学期周");
    UI::CheckBox(g, RectF(lx + S(14), yy + S(32), S(100), S(24)), CMD_SET_SEM_ENABLE, L"启用学期周", s.semester.enabled, []() {
        app.store.settings.semester.enabled = !app.store.settings.semester.enabled;
        app.needSave = true;
        InvalidateOccs();
        UIInvalidate();
    });
    Txt(g, L"开学日期", S(9.5f), false, th.dim2, RectF(lx + S(14), yy + S(62), S(58), S(18)));
    EditInBox(g, 9401, RectF(lx + S(74), yy + S(60), S(110), S(28)), &app.setSemStart);
    UI::Stepper(g, RectF(lx + S(196), yy + S(60), S(120), S(28)), CMD_SET_SEM_DEC, CMD_SET_SEM_INC,
                Fmt(L"%d 周", s.semester.weeks), []() {
                    app.store.settings.semester.weeks = std::max(1, app.store.settings.semester.weeks - 1);
                    app.needSave = true;
                    UIInvalidate();
                }, []() {
                    app.store.settings.semester.weeks = std::min(30, app.store.settings.semester.weeks + 1);
                    app.needSave = true;
                    UIInvalidate();
                }, 10);
    UI::CheckBox(g, RectF(lx + S(14), yy + S(90), S(140), S(24)), CMD_SET_SEM_PARITY, L"显示单双周", s.semester.showParity, []() {
        app.store.settings.semester.showParity = !app.store.settings.semester.showParity;
        app.needSave = true;
        UIInvalidate();
    });
    yy += S(128);

    // 喝水
    SectionBox(g, lx, yy, colW, S(150), L"喝水提醒");
    UI::CheckBox(g, RectF(lx + S(14), yy + S(32), S(90), S(24)), CMD_SET_WATER_ENABLE, L"启用", s.water.enabled, []() {
        app.store.settings.water.enabled = !app.store.settings.water.enabled;
        app.needSave = true;
        UIInvalidate();
    });
    UI::Stepper(g, RectF(lx + S(110), yy + S(30), S(130), S(28)), CMD_SET_WATER_INT_DEC, CMD_SET_WATER_INT_INC,
                Fmt(L"每 %d 分", s.water.intervalMin), []() {
                    app.store.settings.water.intervalMin = std::max(10, app.store.settings.water.intervalMin - 5);
                    app.needSave = true;
                    UIInvalidate();
                }, []() {
                    app.store.settings.water.intervalMin = std::min(240, app.store.settings.water.intervalMin + 5);
                    app.needSave = true;
                    UIInvalidate();
                }, 10);
    Txt(g, L"活跃时段", S(9.5f), false, th.dim2, RectF(lx + S(14), yy + S(66), S(58), S(18)));
    EditInBox(g, 9402, RectF(lx + S(74), yy + S(64), S(64), S(26)), &app.setWaterFrom, 10.5f);
    Txt(g, L"–", S(11), false, th.dim2, RectF(lx + S(140), yy + S(64), S(14), S(26)), ALIGN_CENTER | ALIGN_VCENTER);
    EditInBox(g, 9403, RectF(lx + S(156), yy + S(64), S(64), S(26)), &app.setWaterTo, 10.5f);
    UI::Stepper(g, RectF(lx + S(14), yy + S(98), S(160), S(28)), CMD_SET_WATER_TARGET_DEC, CMD_SET_WATER_TARGET_INC,
                Fmt(L"目标 %d 杯", s.water.targetCups), []() {
                    app.store.settings.water.targetCups = std::max(1, app.store.settings.water.targetCups - 1);
                    app.needSave = true;
                    UIInvalidate();
                }, []() {
                    app.store.settings.water.targetCups = std::min(20, app.store.settings.water.targetCups + 1);
                    app.needSave = true;
                    UIInvalidate();
                }, 10);
    UI::CheckBox(g, RectF(lx + S(190), yy + S(100), S(100), S(24)), CMD_SET_WATER_SOUND, L"提示音", s.water.sound, []() {
        app.store.settings.water.sound = !app.store.settings.water.sound;
        app.needSave = true;
        UIInvalidate();
    });
    Txt(g, L"点侧边栏喝水卡片可 +1 杯", S(9), false, th.dim2, RectF(lx + S(14), yy + S(128), colW - S(28), S(16)));
    // 字体与后台
    float fy = yy + S(160);
    SectionBox(g, lx, fy, colW, S(122), L"字体与后台");
    UI::Btn(g, RectF(lx + S(14), fy + S(32), S(128), S(28)), CMD_SET_FONTS, L"字体与字号…", 1, ICO_EDIT, []() {
        OpenFontSettings();
    });
    Txt(g, Fmt(L"%s / %s · 全局 %d%%", s.fontUI.c_str(), s.fontNum.c_str(), s.fontScale),
        S(8.5f), false, th.dim2, RectF(lx + S(150), fy + S(36), colW - S(164), S(20)), ALIGN_LEFT, false, true);
    UI::CheckBox(g, RectF(lx + S(14), fy + S(68), colW - S(28), S(22)), CMD_SET_CLOSE2TRAY,
                 L"关闭/最小化时隐藏到托盘，后台继续提醒", s.closeToTray, []() {
        app.store.settings.closeToTray = !app.store.settings.closeToTray;
        app.store.Touch();
        app.needSave = true;
        DoSaveStore();
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(lx + S(14), fy + S(94), colW - S(28), S(22)), CMD_SET_AUTOSTART,
                 L"开机自启动（登录后在后台运行）", s.autoStart, []() {
        app.store.settings.autoStart = !app.store.settings.autoStart;
        app.store.Touch();
        app.needSave = true;
        DoSaveStore();
        SyncAutoStart();
        ShowToast(app.store.settings.autoStart ? L"已设置开机自启动" : L"已取消开机自启动");
        UIInvalidate();
    });


    // ---- 右列 ----
    float ry = y + S(50) - scroll;
    // 提醒
    SectionBox(g, rx, ry, colW, S(150), L"日程提醒");
    UI::CheckBox(g, RectF(rx + S(14), ry + S(32), S(90), S(24)), CMD_SET_NOTIFY_ENABLE, L"启用", s.notify.enabled, []() {
        app.store.settings.notify.enabled = !app.store.settings.notify.enabled;
        app.needSave = true;
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(rx + S(108), ry + S(32), S(100), S(24)), CMD_SET_NOTIFY_SOUND, L"提示音", s.notify.sound, []() {
        app.store.settings.notify.sound = !app.store.settings.notify.sound;
        app.needSave = true;
        UIInvalidate();
    });
    UI::CheckBox(g, RectF(rx + S(214), ry + S(32), S(104), S(24)), CMD_SET_NOTIFY_FOCUS, L"专注模式", s.notify.focusMode, []() {
        app.store.settings.notify.focusMode = !app.store.settings.notify.focusMode;
        app.needSave = true;
        UIInvalidate();
        ShowToast(app.store.settings.notify.focusMode ? L"专注模式：只放行考试/DDL 提醒" : L"专注模式已关闭");
    });
    UI::Btn(g, RectF(rx + S(214), ry + S(66), S(104), S(28)), CMD_REMINDERS_OPEN, L"提醒中心…", 1, ICO_BELL, []() {
        OpenReminderCenter();
    });
    UI::Stepper(g, RectF(rx + S(14), ry + S(66), S(190), S(28)), CMD_SET_NOTIFY_LEAD_DEC, CMD_SET_NOTIFY_LEAD_INC,
                Fmt(L"新日程默认提前 %d 分钟", s.notify.defaultLead), []() {
                    app.store.settings.notify.defaultLead = std::max(0, app.store.settings.notify.defaultLead - 5);
                    app.needSave = true;
                    UIInvalidate();
                }, []() {
                    app.store.settings.notify.defaultLead = std::min(1440, app.store.settings.notify.defaultLead + 5);
                    app.needSave = true;
                    UIInvalidate();
                }, 9.5f);
    // 自定义铃声
    {
        float ry2 = ry + S(102);
        UI::Btn(g, RectF(rx + S(14), ry2, S(96), S(28)), CMD_SET_NOTIFY_SOUND_FILE, L"铃声…", 1, ICO_MUSIC, []() {
            wchar_t file[MAX_PATH] = { 0 };
            OPENFILENAMEW ofn = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_hwnd;
            ofn.lpstrFilter = L"WAV 音频 (*.wav)\0*.wav\0所有文件 (*.*)\0*.*\0\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (!GetOpenFileNameW(&ofn)) return;
            app.store.settings.notify.soundFile = file;
            app.needSave = true;
            DoSaveStore();
            ShowToast(L"铃声已设置");
            UIInvalidate();
        });
        std::wstring sf = app.store.settings.notify.soundFile;
        if (!sf.empty()) {
            size_t slash = sf.find_last_of(L"\\/");
            std::wstring name = (slash == std::wstring::npos) ? sf : sf.substr(slash + 1);
            Txt(g, L"当前：" + name, S(9), false, th.dim, RectF(rx + S(118), ry2, colW - S(190), S(28)), ALIGN_VCENTER, false, true);
            UI::Btn(g, RectF(rx + colW - S(66), ry2, S(58), S(28)), CMD_SET_NOTIFY_SOUND_CLEAR, L"清除", 1, -1, []() {
                app.store.settings.notify.soundFile.clear();
                app.needSave = true;
                DoSaveStore();
                ShowToast(L"已恢复系统提示音");
                UIInvalidate();
            });
        } else {
            Txt(g, L"当前：系统提示音", S(9), false, WithAlpha(th.dim2, 210), RectF(rx + S(118), ry2, colW - S(132), S(28)), ALIGN_VCENTER);
        }
    }
    ry += S(160);

    // 挂件
    SectionBox(g, rx, ry, colW, S(112), L"桌面挂件");
    UI::CheckBox(g, RectF(rx + S(14), ry + S(32), S(120), S(24)), CMD_SET_WIDGET_ENABLE, L"启动时显示挂件", s.widget.enabled, []() {
        app.store.settings.widget.enabled = !app.store.settings.widget.enabled;
        app.needSave = true;
        ToggleWidgetWindow();
        UIInvalidate();
    });
    UI::Chip(g, RectF(rx + S(14), ry + S(62), S(80), S(26)), CMD_SET_WIDGET_MODE_BASE + 0, L"卡片模式",
             s.widget.mode == L"card", th.accent, []() {
                 app.store.settings.widget.mode = L"card";
                 app.needSave = true;
                 UIInvalidate();
             }, 9.5f);
    UI::Chip(g, RectF(rx + S(100), ry + S(62), S(80), S(26)), CMD_SET_WIDGET_MODE_BASE + 1, L"细条模式",
             s.widget.mode == L"bar", th.accent, []() {
                 app.store.settings.widget.mode = L"bar";
                 app.needSave = true;
                 UIInvalidate();
             }, 9.5f);
    UI::Btn(g, RectF(rx + S(190), ry + S(62), S(150), S(26)), CMD_SET_WIDGET_OPACITY_DEC, L"挂件外观…", 1, -1, []() {
        OpenWidgetSettings();
    });
    ry += S(122);

    // 标签管理
    float secH = S(232);
    {
        int nTags = (int)app.store.tags.size();
        int cols = 3;
        int tagRows = (nTags + cols - 1) / cols;
        secH = std::max(S(232), S(32) + tagRows * S(20) + S(58));
        SectionBox(g, rx, ry, colW, secH, L"标签管理");
        float ty2 = ry + S(32);
        float tw = (colW - S(28) - S(16)) / cols;
        int n = nTags;
        int rows = tagRows;
        for (int i = 0; i < n; ++i) {
            const Tag& tag = app.store.tags[i];
            int col = i / rows;
            int row = i % rows;
            float bx = rx + S(14) + col * (tw + S(8));
            float by = ty2 + row * S(20);
            Color tc = Hex(tag.color);
            int ic = IconFromName(tag.icon.empty() ? std::wstring(DefaultTagIcon(tag.id)) : tag.icon);
            Icon(g, ic, RectF(bx, by + S(2), S(13), S(13)), tc, 1.3f);
            Txt(g, tag.name, S(9.5f), false, th.dim, RectF(bx + S(17), by - S(3), tw - S(44), S(20)), ALIGN_LEFT, true);
            std::string tid = tag.id;
            AddHit(bx + tw - S(22), by - S(2), S(20), S(20), CMD_SET_TAG_DEL_BASE + i, [tid]() {
                app.store.Snapshot();
                app.store.DeleteTag(tid);
                DoSaveStore();
                UIInvalidate();
            });
            Txt(g, L"×", S(11), false, WithAlpha(th.dim2, 180), RectF(bx + tw - S(22), by - S(2), S(20), S(20)), ALIGN_CENTER | ALIGN_VCENTER);
            AddHit(bx, by - S(2), tw - S(24), S(20), CMD_SET_TAG_EDIT_BASE + i, [i, tid]() {
                app.tagEdit.open = true;
                app.tagEdit.isNew = false;
                app.tagEdit.editingId = tid;
                app.tagEdit.name = app.store.tags[i].name;
                app.tagEdit.color = app.store.tags[i].color;
                app.tagEdit.icon = app.store.tags[i].icon.empty() ? std::wstring(DefaultTagIcon(tid)) : app.store.tags[i].icon;
                app.tagEdit.error.clear();
                app.overlay = OV_TAGEDIT;
                UIInvalidate();
            });
        }
        UI::Btn(g, RectF(rx + S(14), ry + S(32) + tagRows * S(20) + S(10), S(110), S(26)), CMD_SET_TAG_ADD, L"新建标签", 1, ICO_PLUS, []() {
            app.tagEdit.open = true;
            app.tagEdit.isNew = true;
            app.tagEdit.editingId.clear();
            app.tagEdit.task = false;
            app.tagEdit.customIcon.clear();
            app.tagEdit.name.clear();
            app.tagEdit.color = L"#6EA8FF";
            app.tagEdit.icon = L"tag";
            app.tagEdit.error.clear();
            app.overlay = OV_TAGEDIT;
            UIInvalidate();
        });
    }
    ry += secH + S(10);

    // 数据
    SectionBox(g, rx, ry, colW, S(224), L"数据与备份");
    UI::Btn(g, RectF(rx + S(14), ry + S(34), S(118), S(28)), CMD_SET_OPEN_DATA_DIR, L"打开数据目录", 1, ICO_FOLDER, []() {
        ShellExecuteW(nullptr, L"open", GetDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    });
    UI::Btn(g, RectF(rx + S(140), ry + S(34), S(108), S(28)), CMD_SET_BACKUP, L"立即备份", 1, ICO_COPY, []() {
        EnsureDir(PathJoin(GetDataDir(), L"backup"));
        std::wstring dst = PathJoin(GetDataDir(), L"backup/manual-" + ISODate(app.now) + L"-" +
                                    Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
        if (CopyFileSimple(GetCalendarPath(), dst)) ShowToast(L"已备份到 data\\backup");
        else ShowToast(L"备份失败");
    });
    UI::Btn(g, RectF(rx + S(14), ry + S(70), S(150), S(28)), CMD_SET_RESTORE_BACKUP, L"恢复最近备份", 1, ICO_REFRESH, []() {
        std::wstring bak, name;
        int events = 0;
        if (!LatestBackup(bak, name, events)) { ShowToast(L"没有可用备份"); return; }
        OpenConfirm(L"从备份恢复",
                    Fmt(L"将用备份「%s」（%d 条日程）覆盖当前数据。\n恢复前会自动把当前数据另存为 backup\\manual-*.json。", name.c_str(), events),
                    [bak]() {
                        EnsureDir(PathJoin(GetDataDir(), L"backup"));
                        std::wstring pre = PathJoin(GetDataDir(), L"backup/manual-before-restore-" + ISODate(app.now) + L"-" +
                                                    Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
                        CopyFileSimple(GetCalendarPath(), pre);
                        bool ok = false;
                        std::string data = ReadFileBytes(bak, &ok);
                        Store tmp;
                        std::wstring err;
                        if (!ok || !tmp.FromJsonStr(data, &err)) { ShowToast(L"备份文件损坏，未恢复"); return; }
                        int n = (int)tmp.events.size();
                        ApplyRestoredStore(std::move(tmp));
                        ShowToast(Fmt(L"已从备份恢复 %d 条日程", n), 4000);
                    }, L"恢复", L"取消");
    });
    UI::Btn(g, RectF(rx + S(14), ry + S(106), S(150), S(28)), CMD_BACKUP_OPEN_EXPORT, L"导出加密备份", 1, ICO_COPY, []() {
        OpenBackupExport();
    });
    UI::Btn(g, RectF(rx + S(172), ry + S(106), S(150), S(28)), CMD_BACKUP_OPEN_IMPORT, L"导入加密备份", 1, ICO_FOLDER, []() {
        OpenBackupImport();
    });
    UI::Btn(g, RectF(rx + S(172), ry + S(70), S(150), S(28)), CMD_SET_ACCOUNT_SYNC, L"账号同步…", 1, ICO_USER, []() {
        OpenAccountSync();
    });
    UI::Btn(g, RectF(rx + S(14), ry + S(140), S(150), S(26)), CMD_SET_AUTOBACKUP_DIR, L"自动备份到…", 1, ICO_FOLDER, []() {
        std::wstring dir = PickAutoBackupFolder();
        if (dir.empty()) return;
        app.store.settings.autoBackupDir = dir;
        DoSaveStore();
        AutoBackupTick();
        ShowToast(L"自动备份文件夹已设置");
        UIInvalidate();
    });
    if (!app.store.settings.autoBackupDir.empty()) {
        Txt(g, app.store.settings.autoBackupDir, S(8.5f), false, WithAlpha(th.dim2, 220),
            RectF(rx + S(172), ry + S(140), colW - S(200), S(26)), ALIGN_LEFT, false, true);
        AddHit(rx + colW - S(24), ry + S(140), S(20), S(26), CMD_SET_AUTOBACKUP_CLEAR, []() {
            app.store.settings.autoBackupDir.clear();
            DoSaveStore();
            ShowToast(L"已关闭自动备份");
            UIInvalidate();
        });
        Txt(g, L"×", S(11), false, WithAlpha(th.dim2, 180), RectF(rx + colW - S(24), ry + S(140), S(20), S(26)), ALIGN_CENTER | ALIGN_VCENTER);
    }
    Txt(g, L"所有数据在 data\\calendar.json；自动备份每天一份、保留最近 30 份。", S(9), false, th.dim2,
        RectF(rx + S(14), ry + S(172), colW - S(28), S(16)));
    Txt(g, L"换机迁移 / 同步基础…", S(9), false, th.accent,
        RectF(rx + S(14), ry + S(194), colW - S(28), S(16)));
    AddHit(rx + S(14), ry + S(192), S(160), S(20), 0, []() { OpenSyncCenter(); });
    g.ResetClip();
    if (maxScroll > 0) {
        float trackX = x + pw - S(10);
        float trackY = viewTop + S(4);
        float trackH = viewH - S(8);
        FillRoundRect(g, RectF(trackX, trackY, S(4), trackH), S(2), WithAlpha(th.border, 100));
        float frac = viewH / contentH;
        float thumbH = std::max(S(28), trackH * frac);
        float t = scroll / maxScroll;
        float thumbY = trackY + (trackH - thumbH) * t;
        FillRoundRect(g, RectF(trackX, thumbY, S(4), thumbH), S(2), WithAlpha(th.accent, 160));
    }
}