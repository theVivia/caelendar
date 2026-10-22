// cmdk.cpp - 全局命令面板（Ctrl+K）
#include "app.h"
#include "quick.h"
#include <commctrl.h>
#include <cwctype>
#include <algorithm>

using namespace Gdiplus;
using namespace gfx;

namespace {

enum CmdkKind { CI_COMMAND = 1, CI_EVENT, CI_DATE, CI_CREATE };
struct CmdkItem {
    int kind = CI_COMMAND;
    std::wstring title;
    std::wstring hint;
    std::string eventId;
    DateTime date;
    int cmdIndex = -1;
};
struct CmdDef {
    std::wstring title;
    std::wstring hint;
    std::function<void()> fn;
};

std::vector<CmdkItem> g_items;
static bool g_subclassed = false;
static HWND g_subEdit = nullptr;

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}
bool Contains(const std::wstring& hay, const std::wstring& needle) {
    if (needle.empty()) return true;
    return Lower(hay).find(Lower(needle)) != std::wstring::npos;
}
void Close() {
    app.overlay = OV_NONE;
    app.cmdk.open = false;
    app.cmdk.captureHotkey = false;
    UIInvalidate();
}
void DoCloseAndInvalidate() {
    app.overlay = OV_NONE;
    app.cmdk.open = false;
    InvalidateOccs();
    UIInvalidate();
}

// -------- 命令定义 --------
std::vector<CmdDef> CommonCommands() {
    std::vector<CmdDef> v;
    auto add = [&](const wchar_t* title, const wchar_t* hint, std::function<void()> fn) {
        v.push_back(CmdDef{ title, hint, std::move(fn) });
    };
    add(L"新建日程", L"新建事件（Ctrl+N）", []() { OpenEditorNew(app.cursor, 9, 0); });
    add(L"快速添加", L"自然语言一行添加（Ctrl+Shift+N）", []() { OpenQuickAdd(); });
    add(L"跳转日期", L"跳转到指定日期（Ctrl+G）", []() { OpenDateJump(); });
    add(L"今天", L"回到今天", []() {
        app.cursor = Today();
        InvalidateOccs();
        UIInvalidate();
    });
    add(L"月视图", L"切换为月视图", []() { app.view = VIEW_MONTH; InvalidateOccs(); UIInvalidate(); });
    add(L"周视图", L"切换为周视图", []() { app.view = VIEW_WEEK; InvalidateOccs(); UIInvalidate(); });
    add(L"日视图", L"切换为日视图", []() { app.view = VIEW_DAY; InvalidateOccs(); UIInvalidate(); });
    add(L"年视图", L"切换为年视图", []() { app.view = VIEW_YEAR; InvalidateOccs(); UIInvalidate(); });
    add(L"日程列表", L"打开全部日程", []() { app.view = VIEW_AGENDA; app.agendaBoard = false; InvalidateOccs(); UIInvalidate(); });
    add(L"作业看板", L"打开 DDL 看板", []() { app.view = VIEW_AGENDA; app.agendaBoard = true; InvalidateOccs(); UIInvalidate(); });
    add(L"学习统计", L"周/月/年学习复盘", []() { OpenStats(1, app.cursor); });
    add(L"提醒中心", L"错过提醒集中处理", []() { OpenReminderCenter(); });
    add(L"主题工坊", L"自定义主题 / 导入导出", []() { OpenThemeStudio(); });
    add(L"浅色主题", L"切换到内置浅色主题", []() {
        app.store.settings.theme = L"daylight";
        app.themeIdx = ThemeIndexById(L"daylight");
        ApplyThemeFromSettings();
        app.needSave = true;
        if (app.store.settings.widget.enabled) RecreateWidgetWindow();
        DoSaveStore();
    });
    add(L"设置", L"打开设置", []() { OpenSettings(); });
    add(L"忙闲分享", L"外网忙闲页 / 预约留言", []() { OpenSharePanel(); });
    add(L"挂件外观", L"布局 / 皮肤 / 穿透 / 省电", []() { OpenWidgetSettings(); });
    add(L"快捷键设置", L"自定义视图/今天/搜索/新建快捷键（F10）", []() { OpenHotkeys(); });
    add(L"成绩 / GPA", L"记录课程成绩，学分加权 GPA（F11）", []() { OpenGrades(); });
    add(L"数据体检", L"检查重复/异常事件、损坏标签引用", []() { OpenHealth(); });
    add(L"字体与字号", L"分区字体 / 全局缩放", []() { OpenFontSettings(); });
    add(L"版本历史", L"版本快照与回滚", []() { OpenVersionsOverlay(); });
    add(L"导出加密备份", L"换电脑迁移", []() { OpenBackupExport(); });
    add(L"导入加密备份", L"从 .xlbak 恢复", []() { OpenBackupImport(); });
    add(L"立即备份", L"备份当前数据到 data/backup", []() {
        EnsureDir(PathJoin(GetDataDir(), L"backup"));
        std::wstring dst = PathJoin(GetDataDir(), L"backup/manual-" + ISODate(app.now) + L"-" +
                                    Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
        if (CopyFileSimple(GetCalendarPath(), dst)) ShowToast(L"已备份到 data\\backup");
        else ShowToast(L"备份失败");
    });
    add(L"打开数据目录", L"打开 data 文件夹", []() {
        ShellExecuteW(nullptr, L"open", GetDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    });
    add(L"关于星历", L"版本 / 数据校验 / 版本历史", []() { app.overlay = OV_ABOUT; });
    add(L"选中项改颜色", L"给日程视图中选中的日程换颜色", []() {
        if (app.selected.empty()) { ShowToast(L"先在日程视图里选中日程（Shift+点击）"); return; }
        OpenColorPicker(L"选中项颜色", L"#6EA8FF", [](const std::wstring& hex) {
            app.store.Snapshot();
            std::set<std::string> ids;
            for (auto& o : app.occs) if (app.selected.count(OccKey(o))) ids.insert(o.seriesId);
            for (auto& id : ids) { Event* e = app.store.Find(id); if (e) { e->color = hex; app.store.Touch(); } }
            DoSaveStore();
            InvalidateOccs();
            ShowToast(L"颜色已更新");
        });
    });
    add(L"完成选中", L"把选中日程标记完成", []() {
        if (app.selected.empty()) { ShowToast(L"先选中日程"); return; }
        app.store.Snapshot();
        for (auto& o : app.occs) {
            if (!app.selected.count(OccKey(o))) continue;
            app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, true);
        }
        DoSaveStore();
        app.selected.clear();
        InvalidateOccs();
        ShowToast(L"选中日程已完成");
    });
    add(L"删除选中", L"删除选中的日程", []() {
        if (app.selected.empty()) { ShowToast(L"先选中日程"); return; }
        DeleteSelected();
    });
    add(L"改快捷键", L"自定义命令面板快捷键", []() {
        app.overlay = OV_CMDK;
        app.cmdk.open = true;
        app.cmdk.captureHotkey = true;
        app.cmdk.text.clear();
        app.cmdk.error.clear();
        UIInvalidate();
    });
    add(L"关于快捷键", L"所有快捷键一览", []() { app.overlay = OV_ABOUT; });
    return v;
}

bool LooksLikeCreate(const std::wstring& text) {
    if (text.size() < 2) return false;
    static const wchar_t* keys[] = { L"今天", L"明天", L"后天", L"周一", L"周二", L"周三", L"周四", L"周五", L"周六", L"周日",
                                     L"星期", L"月", L"日", L"点", L":", L"#", L"全天" };
    for (auto* k : keys) if (text.find(k) != std::wstring::npos) return true;
    for (wchar_t c : text) if (c >= L'0' && c <= L'9') return true;
    return false;
}

void BuildItems(const std::wstring& text) {
    g_items.clear();
    app.cmdk.error.clear();
    std::wstring t = text;
    while (!t.empty() && iswspace(t.front())) t.erase(t.begin());
    while (!t.empty() && iswspace(t.back())) t.pop_back();
    if (app.cmdk.captureHotkey) return;
    auto cmds = CommonCommands();
    if (!t.empty() && t[0] == L'/') {
        DateTime d;
        std::wstring body = t.substr(1);
        if (DateInputParse(body, Today(), d)) {
            CmdkItem it;
            it.kind = CI_DATE;
            it.date = d;
            it.title = Fmt(L"跳转到 %d年%d月%d日", d.y, d.mo, d.d);
            it.hint = WeekdayCN(DayOfWeek(d.y, d.mo, d.d));
            g_items.push_back(it);
        } else {
            app.cmdk.error = L"日期没看懂，试试 /2026-10-20 或 /明天";
        }
        return;
    }
    bool onlyCmd = !t.empty() && t[0] == L'>';
    std::wstring q = onlyCmd ? t.substr(1) : t;
    if (!onlyCmd && !q.empty()) {
        if (LooksLikeCreate(q)) {
            QuickResult r = ParseQuick(q, Today(), &app.store);
            if (r.ok) {
                CmdkItem it;
                it.kind = CI_CREATE;
                it.title = L"新建：" + r.ev.title;
                it.hint = r.ev.allDay ? (ISODate(r.ev.start) + L" 全天")
                                      : (DateTimeHuman(r.ev.start) + L" – " + TimeStr(r.ev.end));
                g_items.push_back(it);
            }
        }
        int shown = 0;
        for (auto& e : app.store.events) {
            if (e.status == ST_CANCELLED) continue;
            if (!Contains(e.title, q)) continue;
            CmdkItem it;
            it.kind = CI_EVENT;
            it.eventId = e.id;
            it.date = e.start;
            it.title = e.title;
            it.hint = DateTimeHuman(e.start) + (e.allDay ? L" 全天" : L" · " + TimeStr(e.start));
            g_items.push_back(it);
            if (++shown >= 5) break;
        }
    }
    for (int i = 0; i < (int)cmds.size(); ++i) {
        if (!q.empty() && !Contains(cmds[i].title, q) && !Contains(cmds[i].hint, q)) continue;
        CmdkItem it;
        it.kind = CI_COMMAND;
        it.cmdIndex = i;
        it.title = cmds[i].title;
        it.hint = cmds[i].hint;
        g_items.push_back(it);
    }
}

bool CreateFromText(const std::wstring& text, std::wstring& err) {
    QuickResult r = ParseQuick(text, Today(), &app.store);
    if (!r.ok) { err = r.error.empty() ? L"没看懂这句话" : r.error; return false; }
    app.store.Snapshot();
    for (auto& kv : r.tagNames) app.store.EnsureTag(kv.second);
    app.store.AddEvent(r.ev);
    DoSaveStore();
    InvalidateOccs();
    app.cmdk.text.clear();
    Close();
    ShowToast(Fmt(L"已创建：%s", r.ev.title.c_str()));
    return true;
}

void ExecuteItem(int idx) {
    if (idx < 0 || idx >= (int)g_items.size()) return;
    CmdkItem it = g_items[idx];
    if (it.kind == CI_CREATE) {
        std::wstring err;
        if (!CreateFromText(app.cmdk.text, err)) {
            app.cmdk.error = err;
            UIInvalidate();
        }
        return;
    }
    if (it.kind == CI_DATE) {
        app.cursor = StartOfDay(it.date);
        app.view = VIEW_MONTH;
        Close();
        InvalidateOccs();
        return;
    }
    if (it.kind == CI_EVENT) {
        DateTime when = it.date;
        app.cursor = StartOfDay(when);
        Close();
        InvalidateOccs();
        OpenEditorExisting(it.eventId, when);
        return;
    }
    if (it.cmdIndex >= 0) {
        auto cmds = CommonCommands();
        if (it.cmdIndex >= (int)cmds.size()) return;
        auto fn = cmds[it.cmdIndex].fn;
        Close();
        if (fn) fn();
        UIInvalidate();
    }
}

std::wstring KeyName(WPARAM w) {
    if (w >= 'A' && w <= 'Z') return std::wstring(1, (wchar_t)w);
    if (w >= '0' && w <= '9') return std::wstring(1, (wchar_t)w);
    if (w >= VK_F1 && w <= VK_F12) return Fmt(L"F%d", (int)(w - VK_F1 + 1));
    if (w == VK_SPACE) return L"Space";
    if (w == VK_OEM_3) return L"`";
    return L"";
}

bool SaveHotkeyFromKey(WPARAM w) {
    std::wstring key = KeyName(w);
    if (key.empty()) return false;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (!ctrl && !alt) { app.cmdk.error = L"快捷键至少要有 Ctrl 或 Alt"; return false; }
    std::wstring hk;
    if (ctrl) hk += L"Ctrl+";
    if (alt) hk += L"Alt+";
    if (shift) hk += L"Shift+";
    hk += key;
    app.store.settings.cmdkHotkey = hk;
    app.needSave = true;
    DoSaveStore();
    app.cmdk.captureHotkey = false;
    app.cmdk.error.clear();
    ShowToast(Fmt(L"命令面板快捷键已改为 %s", hk.c_str()));
    UIInvalidate();
    return true;
}

void ToggleCapture() {
    app.cmdk.captureHotkey = !app.cmdk.captureHotkey;
    app.cmdk.error.clear();
    UIInvalidate();
}

LRESULT CALLBACK CmdkEditProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL) {
        int delta = GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA;
        if (delta != 0 && !g_items.empty()) {
            app.cmdk.sel = std::max(0, std::min((int)g_items.size() - 1,
                app.cmdk.sel + (delta > 0 ? -3 : 3)));
            UIInvalidate();
        }
        return 0;
    }
    if (msg == WM_KEYDOWN) {
        if (app.cmdk.captureHotkey) {
            if (w == VK_ESCAPE) {
                app.cmdk.captureHotkey = false;
                app.cmdk.error.clear();
                UIInvalidate();
                return 0;
            }
            if (w == VK_CONTROL || w == VK_SHIFT || w == VK_MENU) return 0;
            if (SaveHotkeyFromKey(w)) return 0;
            return 0;
        }
        if (w == VK_ESCAPE) { Close(); return 0; }
        if (w == VK_RETURN) { ExecuteItem(app.cmdk.sel); return 0; }
        if (w == VK_DOWN) {
            if (!g_items.empty()) app.cmdk.sel = (app.cmdk.sel + 1) % (int)g_items.size();
            UIInvalidate();
            return 0;
        }
        if (w == VK_UP) {
            if (!g_items.empty()) app.cmdk.sel = (app.cmdk.sel - 1 + (int)g_items.size()) % (int)g_items.size();
            UIInvalidate();
            return 0;
        }
    }
    return DefSubclassProc(h, msg, w, l);
}

}  // namespace

bool CmdkHotkeyMatches(WPARAM key, bool ctrl, bool alt, bool shift) {
    std::wstring hk = app.store.settings.cmdkHotkey;
    if (hk.empty()) hk = L"Ctrl+K";
    bool needCtrl = false, needAlt = false, needShift = false;
    std::wstring keyName;
    size_t start = 0;
    while (start <= hk.size()) {
        size_t pos = hk.find(L'+', start);
        std::wstring part = hk.substr(start, pos == std::wstring::npos ? std::wstring::npos : pos - start);
        if (part == L"Ctrl") needCtrl = true;
        else if (part == L"Alt") needAlt = true;
        else if (part == L"Shift") needShift = true;
        else if (!part.empty()) keyName = part;
        if (pos == std::wstring::npos) break;
        start = pos + 1;
    }
    if (needCtrl != ctrl || needAlt != alt || needShift != shift) return false;
    return KeyName(key) == keyName;
}

HWND CmdkEditHandleForProbe() { return g_subEdit; }

void CmdkScroll(int delta) {
    if (!app.cmdk.open || g_items.empty()) return;
    app.cmdk.sel = std::max(0, std::min((int)g_items.size() - 1,
        app.cmdk.sel + (delta > 0 ? -3 : 3)));
    UIInvalidate();
}

void OpenCommandPalette() {
    app.cmdk.open = true;
    app.cmdk.text.clear();
    app.cmdk.lastText = L"@";   // 强制首帧构建命令列表
    app.cmdk.sel = 0;
    app.cmdk.captureHotkey = false;
    app.cmdk.error.clear();
    app.overlay = OV_CMDK;
    UISetEditFocus(9901, false);
    UIInvalidate();
}

void CloseCommandPalette() { Close(); }

bool CmdkProbeRun(const std::wstring& text, std::wstring& err) {
    BuildItems(text);
    if (g_items.empty()) { err = L"没有匹配项"; return false; }
    if (g_items[0].kind != CI_CREATE) { err = L"第一项不是新建"; return false; }
    app.cmdk.text = text;
    return CreateFromText(text, err);
}

void DrawCommandPaletteOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    UI::Dim(g, W, H, 150);
    float pw = S(720), ph = S(640);
    float x = (W - pw) / 2, y = (H - ph) / 2 - S(20);
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 10, 30);
    FillRoundRect(g, panel, S(16), th.card);
    StrokeRoundRect(g, panel, S(16), th.border, S(1));
    FillVGradient(g, RectF(x, y, pw, S(54)), WithAlpha(th.accent, 12), WithAlpha(th.accent, 0));

    if (app.cmdk.lastText != app.cmdk.text) {
        app.cmdk.lastText = app.cmdk.text;
        BuildItems(app.cmdk.text);
        app.cmdk.sel = 0;
    }
    if (app.cmdk.sel >= (int)g_items.size()) app.cmdk.sel = std::max(0, (int)g_items.size() - 1);

    Txt(g, L"命令面板", S(14), true, th.text, RectF(x + S(22), y + S(12), S(200), S(24)));
    Txt(g, Fmt(L"快捷键 %s", app.store.settings.cmdkHotkey.c_str()), S(9), false, th.dim2,
        RectF(x + pw - S(220), y + S(16), S(198), S(18)), ALIGN_RIGHT);

    RectF box(x + S(22), y + S(46), pw - S(44), S(46));
    FillRoundRect(g, box, S(10), WithAlpha(th.panel2, 230));
    StrokeRoundRect(g, box, S(10), WithAlpha(th.accent, 90), S(1));
    Icon(g, ICO_SEARCH, RectF(box.X + S(14), box.Y + S(14), S(16), S(16)), th.dim2, 1.5f);
    if (app.cmdk.captureHotkey) {
        Txt(g, L"按下新的快捷键（至少含 Ctrl 或 Alt；Esc 取消）", S(12), false, th.accent,
            RectF(box.X + S(42), box.Y, box.Width - S(56), box.Height), ALIGN_VCENTER);
    } else {
        HWND eh = UIEdit(9901, RectF(box.X + S(40), box.Y + S(11), box.Width - S(54), S(24)), &app.cmdk.text, false, S(13));
        if (eh && (!g_subclassed || g_subEdit != eh)) {
            g_subEdit = eh;
            SetWindowSubclass(g_subEdit, CmdkEditProc, 1, 0);
            g_subclassed = true;
        }
        if (app.cmdk.text.empty() && GetFocus() != g_subEdit)
            UISetEditFocus(9901, false);
    }

    float ly = box.Y + box.Height + S(12);
    float rowH = S(44);
    if (app.cmdk.captureHotkey) {
        Txt(g, L"当前：" + app.store.settings.cmdkHotkey, S(11), false, th.dim,
            RectF(x + S(24), ly + S(8), pw - S(48), S(24)));
        Txt(g, L"按下组合键后立即保存；建议 Ctrl+K / Ctrl+Shift+K / Alt+Space", S(9), false, th.dim2,
            RectF(x + S(24), ly + S(38), pw - S(48), S(20)));
    } else if (g_items.empty()) {
        Txt(g, L"没有匹配的命令或日程。输入 > 只看命令，输入 / 跳日期。", S(11), false, th.dim2,
            RectF(x + S(24), ly + S(16), pw - S(48), S(24)));
        if (!app.cmdk.error.empty())
            Txt(g, app.cmdk.error, S(10), false, th.danger, RectF(x + S(24), ly + S(48), pw - S(48), S(20)));
    } else {
        int maxRows = 11;
        int total = (int)g_items.size();
        int top = 0;
        if (total > maxRows) {
            top = app.cmdk.sel - maxRows / 2;
            top = std::max(0, std::min(top, total - maxRows));
        }
        for (int i = top; i < total && i < top + maxRows; ++i) {
            auto& it = g_items[i];
            RectF row(x + S(16), ly + (i - top) * rowH, pw - S(32), rowH - S(4));
            bool sel = (i == app.cmdk.sel);
            if (sel) FillRoundRect(g, row, S(8), WithAlpha(th.accent, 26));
            else if ((i - top) % 2 == 1) FillRoundRect(g, row, S(8), WithAlpha(th.panel2, 90));
            int icon = it.kind == CI_CREATE ? ICO_PLUS : (it.kind == CI_EVENT ? ICO_CAL :
                       (it.kind == CI_DATE ? ICO_CAL : ICO_STAR));
            Icon(g, icon, RectF(row.X + S(14), row.Y + S(12), S(16), S(16)),
                 sel ? th.accent : th.dim2, 1.5f);
            Txt(g, it.title, S(11.5f), sel, sel ? th.text : th.dim,
                RectF(row.X + S(42), row.Y + S(4), row.Width - S(56), S(20)), ALIGN_LEFT, false, true);
            Txt(g, it.hint, S(8.5f), false, th.dim2,
                RectF(row.X + S(42), row.Y + S(22), row.Width - S(56), S(16)), ALIGN_LEFT, false, true);
            if (it.kind == CI_COMMAND) {
                Txt(g, L"命令", S(8), false, WithAlpha(th.dim2, 170),
                    RectF(RX(row) - S(52), row.Y, S(42), row.Height), ALIGN_RIGHT | ALIGN_VCENTER);
            }
            AddHit(row.X, row.Y, row.Width, row.Height, 0, [i]() { ExecuteItem(i); });
        }
    }

    // 底部
    Txt(g, L"↑↓/滚轮 选择 · Enter 执行 · Esc 关闭 · 共 " + std::to_wstring((int)g_items.size()) + L" 项", S(8.5f), false, th.dim2,
        RectF(x + S(22), y + ph - S(30), pw - S(190), S(18)), ALIGN_VCENTER);
    Txt(g, L"点击这里改快捷键", S(8.5f), false, WithAlpha(th.accent, 220),
        RectF(x + pw - S(150), y + ph - S(30), S(128), S(18)), ALIGN_RIGHT | ALIGN_VCENTER);
    AddHit(x + pw - S(160), y + ph - S(36), S(146), S(30), 0, []() { ToggleCapture(); });
}
