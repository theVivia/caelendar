// slash.cpp - 斜杠指令面板（/ 打开）
// 设计：输入 / 后进入指令面板；命令表带语法、参数说明、示例与分类；
// 前端命令直接复用现有功能，后端命令提供数据/诊断/运维入口。
#include "app.h"
#include "quick.h"
#include "theme.h"
#include <algorithm>
#include <cwctype>
#include <commctrl.h>
#include <fstream>
#include <sstream>

using namespace Gdiplus;
using namespace gfx;

namespace {

struct SlashCmd {
    std::wstring name;
    std::vector<std::wstring> aliases;
    std::wstring category;
    std::wstring usage;
    std::wstring summary;
    std::wstring detail;
    std::vector<std::wstring> examples;
    int minArgs = 0;
    int maxArgs = 0;
    std::function<int(const std::vector<std::wstring>& args, std::wstring& err)> run;
};

static bool g_slashSubclassed = false;
static HWND g_slashEdit = nullptr;

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}
std::wstring TrimW(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}
bool ContainsText(const std::wstring& hay, const std::wstring& needle) {
    if (needle.empty()) return true;
    return Lower(hay).find(Lower(needle)) != std::wstring::npos;
}
std::wstring JoinArgs(const std::vector<std::wstring>& args, size_t from) {
    std::wstring s;
    for (size_t i = from; i < args.size(); ++i) {
        if (!s.empty()) s += L" ";
        s += args[i];
    }
    return s;
}

// shell 风格分词：支持 "..." / '...' 保留空格
std::vector<std::wstring> Tokenize(const std::wstring& line) {
    std::vector<std::wstring> out;
    std::wstring cur;
    wchar_t quote = 0;
    for (wchar_t c : line) {
        if (quote) {
            if (c == quote) quote = 0;
            else cur += c;
            continue;
        }
        if (c == L'"' || c == L'\'') { quote = c; continue; }
        if (iswspace(c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            continue;
        }
        cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    // 首 token 允许带前导 /
    while (!out.empty() && !out[0].empty() && out[0][0] == L'/') out[0] = out[0].substr(1);
    return out;
}

bool ParseDateArg(const std::wstring& raw, DateTime& out) {
    std::wstring s = TrimW(raw);
    if (s.empty()) return false;
    DateTime base = Today();
    if (s == L"今天" || Lower(s) == L"today") { out = base; return true; }
    if (s == L"明天" || Lower(s) == L"tomorrow") { out = AddDaysDT(base, 1); return true; }
    if (s == L"后天") { out = AddDaysDT(base, 2); return true; }
    if (s == L"昨天" || Lower(s) == L"yesterday") { out = AddDaysDT(base, -1); return true; }
    if (s[0] == L'+' || s[0] == L'-') {
        wchar_t* end = nullptr;
        long n = wcstol(s.c_str() + 1, &end, 10);
        if (end && *end == 0) {
            out = AddDaysDT(base, s[0] == L'+' ? (int)n : -(int)n);
            return true;
        }
    }
    static const wchar_t* names[] = { L"周日", L"周一", L"周二", L"周三", L"周四", L"周五", L"周六" };
    for (int i = 0; i < 7; ++i) {
        if (s == names[i]) {
            int baseWd = ((DaySerial(base.y, base.mo, base.d) + 4) % 7 + 7) % 7;
            int delta = (i - baseWd + 7) % 7;   // 本周对应日
            out = AddDaysDT(base, delta);
            return true;
        }
    }
    bool ok = false;
    DateTime dt = ParseISODate(s, &ok);
    if (!ok) return false;
    out = MakeDT(dt.y, dt.mo, dt.d);
    return true;
}

int SetViewCmd(int v) {
    app.view = v;
    if (v == VIEW_AGENDA) app.agendaBoard = false;
    app.selected.clear();
    InvalidateOccs();
    UIInvalidate();
    return 1;
}

int ShiftPage(int dir) {
    if (app.view == VIEW_YEAR) app.cursor = MakeDT(app.cursor.y + dir, app.cursor.mo, 1);
    else if (app.view == VIEW_MONTH) app.cursor = AddMonthsDT(app.cursor, dir);
    else if (app.view == VIEW_WEEK) app.cursor = AddDaysDT(app.cursor, dir * 7);
    else if (app.view == VIEW_DAY) app.cursor = AddDaysDT(app.cursor, dir);
    else app.cursor = AddDaysDT(app.cursor, dir * 7);
    InvalidateOccs();
    UIInvalidate();
    return 1;
}

int RunDump(const std::vector<std::wstring>& args, std::wstring& err) {
    std::wstring name = args.empty() ? L"slash-dump.json" : args[0];
    for (auto& c : name) if (c == L'/' || c == L'\\') { err = L"文件名不能包含路径分隔符"; return 0; }
    std::wstring path = PathJoin(GetDataDir(), name);
    if (!WriteFileBytes(path, app.store.ToJsonStr())) { err = L"写文件失败：" + path; return 0; }
    ShowToast(L"已导出：" + path, 4000);
    return 1;
}

static std::wstring WithSlash(const std::wstring& raw) {
    std::wstring t = raw;
    size_t i = 0;
    while (i < t.size() && (t[i] == L'/' || iswspace(t[i]))) ++i;
    t = t.substr(i);
    if (t.empty()) return L"/";
    return L"/" + t;
}

static void SetSlashText(const std::wstring& t) {
    std::wstring v = WithSlash(t);
    app.slash.text = v;
    app.slash.lastText = v;
    app.slash.sel = 0;
    app.slash.error.clear();
    if (g_slashEdit) {
        SetWindowTextW(g_slashEdit, v.c_str());
        SendMessageW(g_slashEdit, EM_SETSEL, (WPARAM)v.size(), (LPARAM)v.size());
    }
    UIInvalidate();
}

std::vector<SlashCmd>& Commands() {
    static std::vector<SlashCmd> v;
    if (!v.empty()) return v;
    auto add = [&](const wchar_t* name, std::vector<std::wstring> aliases, const wchar_t* cat,
                   const wchar_t* usage, const wchar_t* summary, const wchar_t* detail,
                   std::vector<std::wstring> examples, int minArgs, int maxArgs,
                   std::function<int(const std::vector<std::wstring>&, std::wstring&)> fn) {
        SlashCmd c;
        c.name = name; c.aliases = std::move(aliases); c.category = cat; c.usage = usage;
        c.summary = summary; c.detail = detail; c.examples = std::move(examples);
        c.minArgs = minArgs; c.maxArgs = maxArgs; c.run = std::move(fn);
        v.push_back(std::move(c));
    };

    // 导航
    add(L"today", {L"t"}, L"导航", L"/today", L"回到今天",
        L"把日期光标移到今天，保持当前视图。", {L"/today"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { app.cursor = Today(); InvalidateOccs(); UIInvalidate(); return 1; });
    add(L"goto", {L"g", L"jump"}, L"导航", L"/goto <日期>", L"跳到指定日期",
        L"<日期>：2026-10-18、2026/10/18、今天、明天、后天、昨天、+3、-2、周一…周日。",
        {L"/goto 2026-10-18", L"/goto 明天", L"/goto +7"}, 1, 1,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            DateTime d;
            if (!ParseDateArg(a[0], d)) { err = L"日期格式无法识别：" + a[0]; return 0; }
            app.cursor = d; InvalidateOccs(); UIInvalidate(); return 1;
        });
    add(L"view", {L"v"}, L"导航", L"/view <视图>", L"切换视图",
        L"<视图>：year / month / week / day / agenda / board。", {L"/view week", L"/view board"}, 1, 1,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            std::wstring k = Lower(a[0]);
            if (k == L"year" || k == L"年") return SetViewCmd(VIEW_YEAR);
            if (k == L"month" || k == L"月") return SetViewCmd(VIEW_MONTH);
            if (k == L"week" || k == L"周") return SetViewCmd(VIEW_WEEK);
            if (k == L"day" || k == L"日") return SetViewCmd(VIEW_DAY);
            if (k == L"agenda" || k == L"日程") return SetViewCmd(VIEW_AGENDA);
            if (k == L"board" || k == L"看板") { app.view = VIEW_AGENDA; app.agendaBoard = true; InvalidateOccs(); UIInvalidate(); return 1; }
            err = L"未知视图：" + a[0] + L"（year/month/week/day/agenda/board）"; return 0;
        });
    add(L"next", {L"n"}, L"导航", L"/next [页数]", L"向后翻页",
        L"月/周/日/年按当前视图翻页，默认 1 页。", {L"/next", L"/next 3"}, 0, 1,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            int n = 1;
            if (!a.empty()) { try { n = std::stoi(a[0]); } catch (...) { err = L"页数必须是整数"; return 0; } }
            for (int i = 0; i < std::max(1, n); ++i) ShiftPage(1);
            return 1;
        });
    add(L"prev", {L"p"}, L"导航", L"/prev [页数]", L"向前翻页",
        L"与 /next 相反，默认 1 页。", {L"/prev"}, 0, 1,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            int n = 1;
            if (!a.empty()) { try { n = std::stoi(a[0]); } catch (...) { err = L"页数必须是整数"; return 0; } }
            for (int i = 0; i < std::max(1, n); ++i) ShiftPage(-1);
            return 1;
        });

    add(L"month", {L"m"}, L"导航", L"/month", L"月视图", L"等价于 /view month。", {L"/month"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { return SetViewCmd(VIEW_MONTH); });
    add(L"week", {L"w"}, L"导航", L"/week", L"周视图", L"等价于 /view week。", {L"/week"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { return SetViewCmd(VIEW_WEEK); });
    add(L"day", {L"d"}, L"导航", L"/day", L"日视图", L"等价于 /view day。", {L"/day"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { return SetViewCmd(VIEW_DAY); });
    add(L"year", {L"y"}, L"导航", L"/year", L"年视图", L"等价于 /view year。", {L"/year"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { return SetViewCmd(VIEW_YEAR); });
    add(L"agenda", {L"a"}, L"导航", L"/agenda", L"日程列表", L"等价于 /view agenda。", {L"/agenda"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { return SetViewCmd(VIEW_AGENDA); });
    add(L"board", {L"b"}, L"导航", L"/board", L"作业看板", L"等价于 /view board。", {L"/board"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            app.view = VIEW_AGENDA; app.agendaBoard = true; InvalidateOccs(); UIInvalidate(); return 1;
        });

    // 日程
    add(L"new", {L"add"}, L"日程", L"/new [标题]", L"新建日程",
        L"打开新建窗口，可选标题会预填；默认 09:00，日期为当前光标日。",
        {L"/new", L"/new 高数作业"}, 0, 99,
        [](const std::vector<std::wstring>& a, std::wstring&) { OpenEditorNew(app.cursor, 9, 0); app.editor.title = JoinArgs(a, 0); UIInvalidate(); return 1; });
    add(L"quick", {L"q"}, L"日程", L"/quick <自然语言>", L"快速添加",
        L"例子：明天 9:00 高数、每周三 16:00 健身。", {L"/quick 明天 9:00 高数"}, 1, 99,
        [](const std::vector<std::wstring>& a, std::wstring&) { OpenQuickAdd(); app.quick.text = JoinArgs(a, 0); UIInvalidate(); return 1; });
    add(L"find", {L"search"}, L"日程", L"/find <关键词>", L"搜索日程/标签",
        L"把关键词放进搜索过滤，不打开搜索框。", {L"/find 高数"}, 1, 99,
        [](const std::vector<std::wstring>& a, std::wstring&) {
            app.search = JoinArgs(a, 0); app.searchEditing = false; app.searchIntent = false; app.searchFocused = false;
            AppEnableIme(false); SetFocus(g_hwnd); InvalidateOccs(); UIInvalidate(); return 1;
        });
    add(L"tag", {L"tl"}, L"日程", L"/tag <标签名|id>", L"切换标签筛选",
        L"标签名或 id，例如 面试 / interview；再次执行取消筛选。", {L"/tag 面试"}, 1, 99,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            std::wstring key = JoinArgs(a, 0);
            std::string id;
            for (auto& t : app.store.tags)
                if (t.name == key || Utf8ToWide(t.id) == key || Lower(Utf8ToWide(t.id)) == Lower(key)) { id = t.id; break; }
            if (id.empty()) { err = L"找不到标签：" + key; return 0; }
            if (app.tagFilter.count(id)) app.tagFilter.erase(id); else app.tagFilter.insert(id);
            InvalidateOccs(); UIInvalidate(); return 1;
        });
    add(L"filter", {L"filters"}, L"日程", L"/filter clear", L"清除筛选",
        L"清空标签筛选和搜索词。", {L"/filter clear"}, 1, 1,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            if (Lower(a[0]) != L"clear") { err = L"只支持 /filter clear"; return 0; }
            app.tagFilter.clear(); app.search.clear(); InvalidateOccs(); UIInvalidate(); return 1;
        });
    add(L"stats", {L"st"}, L"日程", L"/stats [week|month|year]", L"学习统计",
        L"打开统计面板，可选默认范围。", {L"/stats month"}, 0, 1,
        [](const std::vector<std::wstring>& a, std::wstring&) {
            int tab = 1;
            if (!a.empty()) { std::wstring k = Lower(a[0]); if (k == L"week") tab = 0; else if (k == L"year") tab = 2; }
            OpenStats(tab, app.cursor); return 1;
        });
    add(L"reminders", {L"remind"}, L"日程", L"/reminders", L"提醒中心", L"集中处理错过/今天提醒。", {L"/reminders"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenReminderCenter(); return 1; });
    add(L"exam", {L"exams"}, L"日程", L"/exam", L"考试面板", L"考试倒计时与复习计划。", {L"/exam"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenExamPanel(); return 1; });
    add(L"focus", {L"pomodoro"}, L"日程", L"/focus", L"专注计时", L"番茄钟/专注记录。", {L"/focus"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenFocusPanel(); return 1; });
    add(L"habits", {L"habit"}, L"日程", L"/habits", L"习惯打卡", L"习惯与喝水打卡。", {L"/habits"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenHabitsPanel(); return 1; });
    add(L"print", {L"pdf"}, L"日程", L"/print", L"打印/导出", L"打开打印面板。", {L"/print"}, 0, 1,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenPrintPanel(); return 1; });
    add(L"profiles", {}, L"日程", L"/profiles", L"课表档案", L"多套课表档案切换。", {L"/profiles"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenProfilesPanel(); return 1; });

    // 数据 / 界面
    add(L"save", {L"s"}, L"数据", L"/save", L"立即保存", L"强制写盘。", {L"/save"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { DoSaveStore(); ShowToast(L"已保存"); return 1; });
    add(L"undo", {}, L"数据", L"/undo", L"撤销", L"撤销上一次修改。", {L"/undo"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { UndoAction(); return 1; });
    add(L"redo", {}, L"数据", L"/redo", L"重做", L"重做上一次撤销。", {L"/redo"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { RedoAction(); return 1; });
    add(L"backup", {}, L"数据", L"/backup", L"立即备份", L"复制数据到 data/backup。", {L"/backup"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            EnsureDir(PathJoin(GetDataDir(), L"backup"));
            DateTime n = NowDT();
            std::wstring dst = PathJoin(GetDataDir(), L"backup/manual-" + ISODate(n) + L"-" + Fmt(L"%02d%02d", n.h, n.mi) + L".json");
            if (!CopyFileSimple(GetCalendarPath(), dst)) { ShowToast(L"备份失败"); return 1; }
            ShowToast(L"已备份：" + dst, 3500); return 1;
        });
    add(L"import", {}, L"数据", L"/import", L"导入数据", L"打开导入窗口。", {L"/import"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenBackupImport(); return 1; });
    add(L"settings", {L"config"}, L"界面", L"/settings", L"设置", L"打开设置面板。", {L"/settings"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenSettings(); return 1; });
    add(L"themes", {}, L"界面", L"/themes", L"主题列表", L"列出内置主题 id，配合 /theme。", {L"/themes"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            std::wstring list;
            for (int i = 0; i < ThemeCount(); ++i) { if (!list.empty()) list += L", "; list += ThemeIdAt(i); }
            ShowToast(L"主题：" + list, 6000); return 1;
        });
    add(L"theme", {L"th"}, L"界面", L"/theme <主题id>", L"切换主题",
        L"先用 /themes 查 id，例如 aurora / daylight。", {L"/theme aurora"}, 1, 1,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            int idx = ThemeIndexById(Lower(a[0]));
            if (idx < 0) { err = L"未知主题：" + a[0]; return 0; }
            app.store.settings.theme = ThemeIdAt(idx); app.themeIdx = idx;
            ApplyThemeFromSettings(); app.needSave = true;
            if (app.store.settings.widget.enabled) RecreateWidgetWindow();
            DoSaveStore(); UIInvalidate(); ShowToast(L"主题：" + ThemeIdAt(idx)); return 1;
        });
    add(L"widget", {}, L"界面", L"/widget", L"挂件外观", L"布局/皮肤/穿透/省电。", {L"/widget"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenWidgetSettings(); return 1; });
    add(L"font", {}, L"界面", L"/font", L"字体与字号", L"分区字体/全局缩放。", {L"/font"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenFontSettings(); return 1; });
    add(L"versions", {L"history"}, L"数据", L"/versions", L"版本历史", L"数据快照/回滚。", {L"/versions"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenVersionsOverlay(); return 1; });
    add(L"cleanup", {}, L"数据", L"/cleanup", L"批量清理", L"按日期/标签/颜色清理。", {L"/cleanup"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenCleanup(); return 1; });
    add(L"db", {L"sqlite"}, L"数据", L"/db", L"数据库管理", L"SQLite 管理面板。", {L"/db"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenDbManager(); return 1; });
    add(L"sync", {}, L"数据", L"/sync", L"同步中心", L"换机迁移/同步设置。", {L"/sync"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) { OpenSyncCenter(); return 1; });

    // 后端
    add(L"check", {L"verify"}, L"后端", L"/check", L"数据校验", L"只读校验 Store，返回问题摘要。", {L"/check"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            std::wstring r = ValidateStoreData(app.store); ShowToast(r, 4000); DebugLog(L"slash check: %s", r.c_str()); return 1;
        });
    add(L"dump", {L"raw"}, L"后端", L"/dump [文件名]", L"导出完整 JSON",
        L"写到 data/<文件名>，默认 slash-dump.json。", {L"/dump"}, 0, 1, RunDump);
    add(L"json", {L"info"}, L"后端", L"/json", L"数据文件信息", L"路径/大小/事件数/标签数。", {L"/json"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            bool ok = false; std::string bytes = ReadFileBytes(GetCalendarPath(), &ok);
            ShowToast(Fmt(L"calendar.json：%lld 字节 · %d 事件 · %d 标签 · %s", ok ? (long long)bytes.size() : 0LL,
                          (int)app.store.events.size(), (int)app.store.tags.size(), ValidateStoreData(app.store).c_str()), 5000);
            return 1;
        });
    add(L"repair", {}, L"后端", L"/repair", L"生成修复副本",
        L"重新解析生成 data/repair-copy-时间.json，不改原数据。", {L"/repair"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring& err) {
            Store tmp; std::wstring e;
            if (!tmp.FromJsonStr(app.store.ToJsonStr(), &e)) { err = L"解析失败：" + e; return 0; }
            DateTime n = NowDT();
            std::wstring path = PathJoin(GetDataDir(), L"repair-copy-" + ISODate(n) + L"-" + Fmt(L"%02d%02d", n.h, n.mi) + L".json");
            if (!WriteFileBytes(path, tmp.ToJsonStr())) { err = L"写修复副本失败"; return 0; }
            ShowToast(L"修复副本：" + path, 5000); return 1;
        });
    add(L"bench", {}, L"后端", L"/bench", L"展开性能", L"重复展开当前月份 20 次并计时。", {L"/bench"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            DateTime from = MakeDT(app.cursor.y, app.cursor.mo, 1), to = AddMonthsDT(from, 1);
            DWORD t0 = GetTickCount(); size_t n = 0;
            for (int i = 0; i < 20; ++i) n += ExpandOccurrences(app.store, from, to).size();
            ShowToast(Fmt(L"展开 20 次 / %d 实例：%lu ms", (int)n, (unsigned long)(GetTickCount() - t0)), 4000); return 1;
        });
    add(L"log", {L"logs"}, L"后端", L"/log", L"打开调试日志", L"打开 data/debug.log。", {L"/log"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            std::wstring log = PathJoin(GetDataDir(), L"debug.log");
            if (FileExists(log)) ShellExecuteW(nullptr, L"open", log.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            else ShellExecuteW(nullptr, L"open", GetDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 1;
        });
    add(L"dir", {L"data"}, L"后端", L"/dir", L"数据目录", L"在资源管理器打开 data。", {L"/dir"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            ShellExecuteW(nullptr, L"open", GetDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); return 1;
        });
    add(L"version", {L"ver"}, L"后端", L"/version", L"版本信息", L"版本/构建日期/数据格式。", {L"/version"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            ShowToast(Fmt(L"星历 %s · %s · 数据格式 v%d", kAppVersion, kAppBuildDate, kDataFormatVersion), 4000); return 1;
        });
    add(L"probe", {L"probes"}, L"后端", L"/probe", L"探针列表", L"列出回归用命令行探针。", {L"/probe"}, 0, 0,
        [](const std::vector<std::wstring>&, std::wstring&) {
            ShowToast(L"探针：--probe-rawkeys, --probe-slash, --probe-slashui, --probe-tags, --probe-alarm, --probe-help, --probe-focusstart", 6000);
            return 1;
        });

    // 帮助
    add(L"help", {L"h", L"?"}, L"帮助", L"/help [命令]", L"帮助总览 / 单条帮助",
        L"无参数打开 F1 帮助；/help <命令> 在面板内定位该命令详情。", {L"/help", L"/help goto"}, 0, 1,
        [](const std::vector<std::wstring>& a, std::wstring& err) {
            if (a.empty()) { CloseSlashPalette(); OpenHelp(); return 1; }
            std::wstring key = Lower(a[0]);
            auto& all = Commands();
            for (size_t i = 0; i < all.size(); ++i) {
                bool hit = Lower(all[i].name) == key;
                for (auto& al : all[i].aliases) if (Lower(al) == key) hit = true;
                if (hit) {
                    SetSlashText(L"");
                    app.slash.sel = (int)i;
                    AppEnableIme(true);
                    if (g_slashEdit) SetFocus(g_slashEdit);
                    UIInvalidate();
                    return 2;
                }
            }
            err = L"没有这个命令：" + a[0];
            return 0;
        });

    return v;
}


const SlashCmd* FindCmd(const std::wstring& first, size_t* outIndex = nullptr) {
    std::wstring key = Lower(first);
    auto& all = Commands();
    for (size_t i = 0; i < all.size(); ++i) {
        if (Lower(all[i].name) == key) { if (outIndex) *outIndex = i; return &all[i]; }
        for (auto& a : all[i].aliases) if (Lower(a) == key) { if (outIndex) *outIndex = i; return &all[i]; }
    }
    return nullptr;
}

std::vector<const SlashCmd*> Matches() {
    std::vector<const SlashCmd*> out;
    std::wstring first = app.slash.text;
    size_t sp = first.find(L' ');
    if (sp != std::wstring::npos) first = first.substr(0, sp);
    while (!first.empty() && (first[0] == L'/' || iswspace(first[0]))) first = first.substr(1);
    for (auto& c : Commands()) {
        if (first.empty() || ContainsText(c.name, first) || ContainsText(c.summary, first)) { out.push_back(&c); continue; }
        for (auto& a : c.aliases) if (ContainsText(a, first)) { out.push_back(&c); break; }
    }
    return out;
}

bool RunLine(const std::wstring& line, bool probe, std::wstring& err, int& rc) {
    std::vector<std::wstring> tokens = Tokenize(line);
    rc = 0;
    if (tokens.empty()) { err = L"请输入命令，例如 /goto 明天"; return false; }
    const SlashCmd* c = FindCmd(tokens[0]);
    if (!c) { err = L"未知命令：" + tokens[0] + L"（输入 /help 查看全部命令）"; return false; }
    std::vector<std::wstring> args(tokens.begin() + 1, tokens.end());
    if ((int)args.size() < c->minArgs) { err = L"参数不足：" + c->usage; return false; }
    if (c->maxArgs > 0 && (int)args.size() > c->maxArgs) { err = L"参数过多：" + c->usage; return false; }
    int r = c->run(args, err);
    if (r == 0) {
        if (err.empty()) err = L"命令执行失败：" + c->usage;
        return false;
    }
    rc = r;
    if (!probe && r == 1) {
        if (app.overlay == OV_SLASH) {
            app.overlay = OV_NONE;
            app.slash.open = false;
            AppEnableIme(false);
            SetFocus(g_hwnd);
            UIInvalidate();
        }
    }
    return true;
}

static std::vector<std::wstring> ArgCandidates(const std::wstring& cmd) {
    std::wstring n = Lower(cmd);
    if (n == L"goto" || n == L"g" || n == L"jump")
        return { L"今天", L"明天", L"后天", L"昨天", L"+1", L"+3", L"+7", L"周一", L"周二", L"周三", L"周四", L"周五", L"周六", L"周日" };
    if (n == L"view" || n == L"v")
        return { L"year", L"month", L"week", L"day", L"agenda", L"board" };
    if (n == L"next" || n == L"n" || n == L"prev" || n == L"p")
        return { L"1", L"2", L"3" };
    if (n == L"stats" || n == L"st")
        return { L"week", L"month", L"year" };
    if (n == L"theme" || n == L"th") {
        std::vector<std::wstring> v;
        for (int i = 0; i < ThemeCount(); ++i) v.push_back(ThemeIdAt(i));
        return v;
    }
    if (n == L"tag" || n == L"tl") {
        std::vector<std::wstring> v;
        for (auto& t : app.store.tags) v.push_back(t.name);
        return v;
    }
    if (n == L"help" || n == L"h" || n == L"?") {
        std::vector<std::wstring> v;
        for (auto& c : Commands()) v.push_back(c.name);
        return v;
    }
    if (n == L"print" || n == L"pdf") return { L"week", L"month" };
    return {};
}

static void SlashComplete() {
    auto tokens = Tokenize(app.slash.text);
    bool trailingSpace = !app.slash.text.empty() && iswspace(app.slash.text.back());
    if (tokens.empty()) {
        auto m = Matches();
        if (!m.empty()) SetSlashText(m[0]->name + L" ");
        return;
    }
    const SlashCmd* c = FindCmd(tokens[0]);
    if (!c) {
        auto m = Matches();
        if (!m.empty()) {
            int idx = std::max(0, std::min(app.slash.sel, (int)m.size() - 1));
            SetSlashText(m[idx]->name + L" ");
        }
        return;
    }
    if (tokens.size() == 1) {
        if (!trailingSpace) { SetSlashText(c->name + L" "); return; }
        auto cands = ArgCandidates(c->name);
        if (!cands.empty()) SetSlashText(c->name + L" " + cands[0] + L" ");
        return;
    }
    auto cands = ArgCandidates(c->name);
    if (cands.empty()) return;
    const std::wstring& last = tokens.back();
    int found = -1;
    for (int i = 0; i < (int)cands.size(); ++i) if (cands[i] == last) { found = i; break; }
    int idx = (found >= 0) ? (found + 1) % (int)cands.size() : 0;
    std::wstring base = app.slash.text;
    size_t p = base.rfind(last);
    if (p != std::wstring::npos) base = base.substr(0, p);
    SetSlashText(base + cands[idx] + L" ");
}

void Close() {
    app.slash.open = false;
    app.slash.error.clear();
    if (app.overlay == OV_SLASH) app.overlay = OV_NONE;
    AppEnableIme(false);
    SetFocus(g_hwnd);
    UIInvalidate();
}

LRESULT CALLBACK SlashEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_CHAR && (wchar_t)wp == L'/') return 0;   // 前缀已画 /，不再往编辑框里补斜杠
    if (msg == WM_MOUSEWHEEL) {                            // 滚轮先到焦点 EDIT，需要在这里转给列表
        SlashScroll(GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA);
        return 0;
    }
    if (msg == WM_KEYDOWN) {
        if (wp == VK_ESCAPE) { Close(); return 0; }
        if (wp == VK_RETURN) {
            std::wstring err;
            int rc = 0;
            if (!RunLine(app.slash.text, false, err, rc)) {
                app.slash.error = err;
                UIInvalidate();
            }
            return 0;
        }
        if (wp == VK_UP || wp == VK_DOWN) {
            auto m = Matches();
            if (!m.empty()) {
                int delta = (wp == VK_DOWN) ? 1 : -1;
                app.slash.sel = (app.slash.sel + delta + (int)m.size()) % (int)m.size();
                UIInvalidate();
            }
            return 0;
        }
        if (wp == VK_TAB) {
            SlashComplete();
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

} // namespace

void OpenSlashPalette(const std::wstring& prefill) {
    app.slash.open = true;
    app.slash.text = WithSlash(prefill);
    app.slash.lastText = app.slash.text;
    app.slash.error.clear();
    app.slash.sel = 0;
    app.overlay = OV_SLASH;
    AppEnableIme(true);
    UIInvalidate();
}

void CloseSlashPalette() { Close(); }

void SlashScroll(int delta) {
    if (!app.slash.open) return;
    auto m = Matches();
    if (m.empty() || delta == 0) return;
    int step = delta < 0 ? 3 : -3;   // 滚轮向下 = 往后翻
    app.slash.sel = (app.slash.sel + step + (int)m.size()) % (int)m.size();
    UIInvalidate();
}

void SlashExecuteCurrent() {
    std::wstring err;
    int rc = 0;
    if (!RunLine(app.slash.text, false, err, rc)) {
        app.slash.error = err;
        UIInvalidate();
    }
}

bool SlashProbeRun(const std::wstring& line, std::wstring& err) {
    int rc = 0;
    return RunLine(line, true, err, rc);
}

void DrawSlashOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    UI::Dim(g, W, H, 160);
    float pw = S(840), ph = S(600);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 12, 34);
    FillRoundRect(g, panel, S(16), th.card);
    StrokeRoundRect(g, panel, S(16), th.border, S(1));
    FillVGradient(g, RectF(x, y, pw, S(58)), WithAlpha(th.accent, 14), WithAlpha(th.accent, 0));

    Txt(g, L"斜杠指令", S(15), true, th.text, RectF(x + S(22), y + S(14), S(220), S(26)));
    Txt(g, L"输入 / 后跟命令和参数", S(9), false, th.dim2,
        RectF(x + pw - S(260), y + S(18), S(238), S(18)), ALIGN_RIGHT);

    // 输入框
    RectF box(x + S(22), y + S(50), pw - S(44), S(46));
    FillRoundRect(g, box, S(10), WithAlpha(th.panel2, 235));
    StrokeRoundRect(g, box, S(10), WithAlpha(th.accent, 110), S(1));
    HWND eh = UIEdit(9701, RectF(box.X + S(16), box.Y + S(11), box.Width - S(28), S(24)), &app.slash.text, false, S(14));
    if (eh && (!g_slashSubclassed || g_slashEdit != eh)) {
        g_slashEdit = eh;
        SetWindowSubclass(eh, SlashEditProc, 1, 0);
        g_slashSubclassed = true;
    }
    if (eh && GetFocus() != eh) {
        UISetEditFocus(9701, true);
        SendMessageW(eh, EM_SETSEL, (WPARAM)app.slash.text.size(), (LPARAM)app.slash.text.size());
    }

    if (app.slash.lastText != app.slash.text) {
        app.slash.lastText = app.slash.text;
        app.slash.sel = 0;
        app.slash.error.clear();
    }
    auto matches = Matches();
    if (app.slash.sel >= (int)matches.size()) app.slash.sel = std::max(0, (int)matches.size() - 1);

    float bodyY = box.Y + box.Height + S(14);
    float listW = S(320);
    float listH = ph - (bodyY - y) - S(58);
    RectF listR(x + S(18), bodyY, listW, listH);
    FillRoundRect(g, listR, S(10), WithAlpha(th.panel2, 150));
    StrokeRoundRect(g, listR, S(10), th.borderSoft, S(1));
    RectF detailR(x + S(18) + listW + S(12), bodyY, pw - listW - S(60), listH);
    FillRoundRect(g, detailR, S(10), WithAlpha(th.panel2, 110));
    StrokeRoundRect(g, detailR, S(10), th.borderSoft, S(1));

    // 左侧命令列表
    int rows = std::max(3, (int)(listH / S(34)));
    int maxRows = rows - 1;
    int top = std::max(0, std::min(app.slash.sel - maxRows / 2, std::max(0, (int)matches.size() - maxRows)));
    Txt(g, matches.empty() ? L"没有匹配的命令" : Fmt(L"命令 %d / %d", (int)matches.size(), (int)Commands().size()),
        S(9), false, th.dim2, RectF(listR.X + S(12), listR.Y + S(8), listR.Width - S(24), S(16)), ALIGN_LEFT);
    float ry = listR.Y + S(28);
    for (int i = top; i < (int)matches.size() && i < top + maxRows; ++i) {
        const SlashCmd* c = matches[i];
        RectF row(listR.X + S(8), ry, listR.Width - S(16), S(32));
        bool sel = (i == app.slash.sel);
        if (sel) FillRoundRect(g, row, S(7), WithAlpha(th.accent, 30));
        Txt(g, c->name, S(11), sel, sel ? th.text : th.dim, RectF(row.X + S(10), row.Y + S(2), S(120), S(16)));
        Txt(g, c->category, S(8), false, th.dim2, RectF(row.X + row.Width - S(54), row.Y + S(4), S(46), S(14)), ALIGN_RIGHT);
        Txt(g, c->summary, S(8.5f), false, sel ? th.dim : WithAlpha(th.dim2, 210),
            RectF(row.X + S(10), row.Y + S(16), row.Width - S(70), S(14)), ALIGN_LEFT, false, true);
        AddHit(row.X, row.Y, row.Width, row.Height, 0, [i]() {
            app.slash.sel = i;
            UIInvalidate();
        });
        ry += S(34);
    }
    // 列表滚动提示
    if ((int)matches.size() > maxRows) {
        Txt(g, app.slash.sel > 0 ? L"↑" : L"", S(9), false, th.dim2,
            RectF(listR.X + listR.Width - S(20), listR.Y + S(6), S(14), S(12)), ALIGN_CENTER);
    }

    // 右侧详情
    float dx = detailR.X + S(16), dy = detailR.Y + S(14);
    if (matches.empty()) {
        Txt(g, L"没有匹配的命令。", S(11), false, th.dim, RectF(dx, dy, detailR.Width - S(32), S(20)));
        Txt(g, L"输入 /help 打开帮助；或输入命令名用 ↑↓/滚轮浏览，Tab 补全。", S(9.5f), false, th.dim2,
            RectF(dx, dy + S(24), detailR.Width - S(32), S(18)));
    } else {
        const SlashCmd* c = matches[std::max(0, std::min(app.slash.sel, (int)matches.size() - 1))];
        Txt(g, c->usage, S(13), true, th.text, RectF(dx, dy, detailR.Width - S(32), S(22)));
        dy += S(26);
        Txt(g, c->summary, S(10.5f), false, th.accent, RectF(dx, dy, detailR.Width - S(32), S(18)));
        dy += S(22);
        Txt(g, c->detail, S(9.5f), false, th.dim, RectF(dx, dy, detailR.Width - S(32), S(94)), ALIGN_LEFT, true);
        dy += S(100);
        if (!c->examples.empty()) {
            Txt(g, L"示例", S(9), true, th.dim2, RectF(dx, dy, S(60), S(16)));
            dy += S(18);
            for (auto& e : c->examples) {
                Txt(g, e, S(9.5f), false, WithAlpha(th.text, 220), RectF(dx + S(6), dy, detailR.Width - S(38), S(16)), ALIGN_LEFT, false, false);
                dy += S(17);
            }
        }
        // 语法速查（始终显示）
        float sy = detailR.Y + detailR.Height - S(128);
        Txt(g, L"语法速查", S(9), true, th.dim2, RectF(dx, sy, S(80), S(16)));
        sy += S(18);
        Txt(g, L"<日期> 2026-10-18 / 今天 / 明天 / 后天 / +3 / 周一…周日", S(8.5f), false, th.dim2, RectF(dx, sy, detailR.Width - S(32), S(14)), ALIGN_LEFT, false, true); sy += S(15);
        Txt(g, L"<视图> year / month / week / day / agenda / board", S(8.5f), false, th.dim2, RectF(dx, sy, detailR.Width - S(32), S(14)), ALIGN_LEFT, false, true); sy += S(15);
        Txt(g, L"<标签> 名称或 id，如 面试 / interview；<文本> 含空格时用引号", S(8.5f), false, th.dim2, RectF(dx, sy, detailR.Width - S(32), S(14)), ALIGN_LEFT, false, true); sy += S(15);
        Txt(g, L"首 token 可带 /；命令不区分大小写；别名也能用", S(8.5f), false, th.dim2, RectF(dx, sy, detailR.Width - S(32), S(14)), ALIGN_LEFT, false, true);
    }

    // 错误/提示
    if (!app.slash.error.empty()) {
        Txt(g, app.slash.error, S(9.5f), false, th.danger, RectF(x + S(22), y + ph - S(46), pw - S(44), S(18)));
    } else {
        Txt(g, L"↑↓ / 滚轮 选择 · Tab 补全命令与参数 · Enter 执行 · Esc 关闭 · /help 打开帮助 · 后端：check/dump/repair/bench/log",
            S(8.5f), false, th.dim2, RectF(x + S(22), y + ph - S(46), pw - S(44), S(18)));
    }
    // 底部命令名提示
    Txt(g, L"命令由 / 开头；输入 /help 列出全部命令", S(8.5f), false, WithAlpha(th.dim2, 190),
        RectF(x + S(22), y + ph - S(26), pw - S(44), S(16)));
}
