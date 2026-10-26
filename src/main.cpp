// main.cpp - 星历 XingLi：窗口、标题栏、侧边栏、消息循环







#include "app.h"







#include "ics.h"







#include "quick.h"
#include <oleacc.h>
#include <oleauto.h>







#include "smart.h"







#include <windowsx.h>



#include <imm.h>







#include <shellapi.h>







#include <dwmapi.h>







#include <algorithm>







#include <cmath>







#include <vector>







#include <string>















using namespace Gdiplus;







static HIMC g_mainImc = nullptr;







static bool g_rawKeyboard = false;



static int g_rawNavCount = 0;



static bool g_rawProbeActive = false;







void RegisterRawKeyboard() {



    RAWINPUTDEVICE rid{};



    rid.usUsagePage = 0x01;



    rid.usUsage = 0x06;



    rid.dwFlags = RIDEV_INPUTSINK;



    rid.hwndTarget = g_hwnd;



    g_rawKeyboard = RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;



    DebugLog(L"rawkeyboard registered=%d", g_rawKeyboard ? 1 : 0);



}







void AppEnableIme(bool on) {



    if (!g_hwnd) return;



    // 不销毁 IME 上下文，只切换“中文/英文”输入状态：



    // 主窗口是自绘 UI，方向键/M/W/D 等必须先到日历；点搜索框时再打开输入法。



    HIMC ctx = ImmGetContext(g_hwnd);



    if (!ctx) return;



    ImmSetOpenStatus(ctx, on ? TRUE : FALSE);



    ImmReleaseContext(g_hwnd, ctx);



}











using namespace gfx;















int RunSelfTests();







static std::wstring g_startOverlay;







static std::wstring g_themeArg;



static bool g_boardDemo = false;







static bool g_smartCli = false;







static bool g_probeNext = false;







static bool g_probeAgenda = false;







static bool g_agendaSelectAll = false;







static bool g_probeCleanup = false;







static int g_probeOpacity = 0;







static std::wstring g_toastTextArg;







static bool g_probeSelectAllDelete = false;







static bool g_probeBulkDedupe = false;







static bool g_probeImportSeries = false;







static bool g_probeWidgetRefresh = false;







static bool g_probeReschedule = false;







static bool g_probeShiftSelect = false;







static bool g_probeDoneEdit = false;







static int g_selectN = 0;







static std::wstring g_lateShotPath;







static bool g_lateReschedule = false;







static bool g_keepOpen = false;







static bool g_probeYear = false;







static bool g_probeRestart = false;







static bool g_probeSearch = false;







static bool g_probeWheel = false;







static bool g_probeEditorInput = false;







static bool g_probeDateJump = false;







static bool g_probeData = false;







static bool g_probeRestore = false;







static bool g_probeSolar = false;







static bool g_probeLunarEditor = false;







static bool g_probeHelp = false;



static bool g_probeRawKeys = false;



static bool g_probeAlarm = false;

static bool g_probeTags = false;

static bool g_probeSlash = false;
static bool g_probeSlashShot = false;
static bool g_probeSlashUi = false;
static bool g_probeShare = false;
static bool g_probeSharePanel = false;
static bool g_probeCmdkShot = false;
static bool g_probeHelpShot = false;
static bool g_probeCmdkWheel = false;
static int g_sharePort = 17899;

static bool g_probeTagUi = false;



static bool g_probeFocusStart = false;



static bool g_probeInputFix = false;



static bool g_probeDayDetail = false;
static bool g_probeClip = false;
static bool g_probeDone = false;
static DWORD g_suppressClickUntil = 0;
static DWORD g_suppressDblclkUntil = 0;
static bool g_swallowNextUp = false;
static bool g_shiftAtPress = false;
static bool g_ctrlAtPress = false;
static DWORD g_swallowTick = 0;
static bool g_probeMisclick = false;
static bool g_probeBulkDay = false;
static bool g_probeQuick = false;
static bool g_probeAgendaFilter = false;
static bool g_probeClipHistory = false;
static bool g_probeConflict = false;
static bool g_probeMulti2 = false;
static bool g_probeTemplates = false;
static bool g_probeShiftOrder = false;
static bool g_probeAccountSync = false;
static bool g_probeTagsUi = false;
static bool g_probeAccountUi = false;
static bool g_probeAutoBackup = false;
static bool g_probeRecurPreview = false;
static bool g_probeMultiDdl = false;
static bool g_probeBot = false;
static bool g_probeDdlClick = false;
static bool g_probeDdlPoint = false;
static bool g_probeBoardAll = false;
static bool g_probePrintPng = false;
static bool g_probeMulti3 = false;
static bool g_probeDdlView = false;
static bool g_probeRingtone = false;
static bool g_probeMulti4 = false;
static bool g_probeStatsTags = false;
static bool g_probeToolbar = false;
static bool g_probeHotkeys = false;
static bool g_probeMultiReal = false;
static bool g_probeMultiView = false;
static bool g_probeGrades = false;
static bool g_probeMerge = false;
static bool g_probeTombstone = false;
static bool g_probeTagBtn = false;
static bool g_probeHC = false;
static bool g_probeBot2 = false;
static bool g_probeKbNav = false;
static bool g_probeHealth = false;
static bool g_probeMonthDetail = false;
static bool g_probeA11y = false;
static bool g_probeGradesSync = false;
static bool g_probeMulti = false;



static bool g_probeBoard = false;



static bool g_probeVersion = false;



static bool g_rollbackTest = false;



static bool g_probeBackup = false;



static bool g_probeWidget = false;



static bool g_probeReminder = false;



static bool g_probeTheme = false;



static bool g_probeThemeIo = false;



static bool g_probeCmdk = false;



static bool g_probeCmdkUi = false;



static bool g_probeImport2 = false;



static bool g_probeImport2Ui = false;



static bool g_probeOcr = false;



static std::wstring g_subUrlArg;



static bool g_probeSubscription = false;



static bool g_probeSqliteEnable = false;



static bool g_probeSqliteRead = false;



static bool g_probeSqliteRollback = false;



static bool g_probeSync = false;



static bool g_probeSyncStatus = false;



static bool g_probeCorruptJson = false;



static bool g_probeSqliteFallback = false;



static bool g_probeExams = false;



static bool g_probeFocus = false;



static bool g_probeFocusTimer = false;



static bool g_probeHabits = false;



static bool g_probePrint = false;



static bool g_probeProfiles = false;



static bool g_probeOverrideDup = false;



static int g_focusProbeBefore = 0;



static std::wstring g_rollbackExe, g_rollbackData;







static std::wstring g_cleanupKeywordArg;







static std::wstring g_widgetModeArg, g_widgetSkinArg;







static int g_widgetScaleArg = 0, g_widgetOpacityArg = 0;







static int g_widgetDateArg = -1, g_widgetWaterArg = -1, g_widgetTodayArg = -1;















static UINT_PTR g_gdiToken = 0;







static int g_forceW = 0, g_forceH = 0;







static DWORD g_lastSaveTick = 0;







// ---------- 托盘 / 后台运行 ----------







static const UINT WM_TRAYICON_MSG = WM_APP + 100;







static NOTIFYICONDATAW g_nid{};







static bool g_trayAdded = false;







bool g_quitting = false;







HANDLE g_singleMutex = nullptr;







static bool g_startHidden = false;
// 托盘图标重试 + explorer(TaskbarCreated) 重建
static UINT g_taskbarCreatedMsg = 0;
static const UINT kTrayRetryTimerId = 7801;
static int  g_trayRetryCount = 0;

bool g_offscreen = false;
static bool g_testMode = false;
static bool g_probeWidgetTest = false;







static bool g_trayWarned = false;







static std::wstring g_widgetShotPath;







static std::wstring g_reminderShotPath;



static std::wstring g_cmdkTextArg;















// ---------------- 绘制小工具 ----------------







static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,







                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {







    Text(g, s, px, bold, c, r, align, display, ellipsis);







}







static void HudCorner(Graphics& g, RectF r, Color c, float len = 9) {







    float L = S(len), w = S(1.4f);







    Line(g, r.X + r.Width - L, r.Y, r.X + r.Width, r.Y, c, w);







    Line(g, r.X + r.Width, r.Y, r.X + r.Width, r.Y + L, c, w);







    Line(g, r.X, r.Y + r.Height - L, r.X, r.Y + r.Height, c, w);







    Line(g, r.X, r.Y + r.Height, r.X + L, r.Y + r.Height, c, w);







}















// ---------------- 数据刷新 ----------------







static void ViewRange(DateTime& from, DateTime& to) {







    const Settings& st = app.store.settings;







    int ws = st.weekStart;







    switch (app.view) {







    case VIEW_MONTH: {







        DateTime first = MakeDT(app.cursor.y, app.cursor.mo, 1);







        from = StartOfWeek(first, ws);







        to = AddDaysDT(from, 42);







        break;







    }







    case VIEW_WEEK:







        from = StartOfWeek(app.cursor, ws);







        to = AddDaysDT(from, 7);







        break;







    case VIEW_DAY:







        from = StartOfDay(app.cursor);







        to = AddDaysDT(from, 1);







        break;







    case VIEW_YEAR:







        from = MakeDT(app.cursor.y, 1, 1);







        to = MakeDT(app.cursor.y + 1, 1, 1);







        break;







    default:







        from = AddDaysDT(StartOfDay(app.cursor), -1);







        to = AddDaysDT(from, 62);







        break;







    }







}















void RefreshNow() { app.now = NowDT(); }















static std::wstring LowerStr(const std::wstring& s) {







    std::wstring o = s;







    for (auto& c : o) c = (wchar_t)towlower(c);







    return o;







}















bool OccMatchesFilter(const Occurrence& o) {







    if (!app.tagFilter.empty()) {







        bool hit = false;







        for (auto& t : o.ev.tags) if (app.tagFilter.count(t)) { hit = true; break; }







        if (!hit) return false;







    }







    if (!app.search.empty()) {







        std::wstring q = LowerStr(app.search);







        if (!Contains(LowerStr(o.ev.title), q) && !Contains(LowerStr(o.ev.desc), q) &&







            !Contains(LowerStr(o.ev.location), q)) {







            bool tagHit = false;







            for (auto& t : o.ev.tags) if (Contains(LowerStr(app.store.TagName(t)), q)) { tagHit = true; break; }







            if (!tagHit) return false;







        }







    }







    return true;







}















static void RefreshOccs() {







    app.cursor = ClampDateToSupported(app.cursor);







    DateTime from, to;







    ViewRange(from, to);







    app.occs = ExpandOccurrences(app.store, from, to);







    app.occFrom = from; app.occTo = to; app.occValid = true;







}







void InvalidateOccs() { app.occValid = false; }















std::vector<Occurrence> TodayOccurrences() {







    DateTime from = StartOfDay(app.now), to = AddDaysDT(from, 1);







    return ExpandOccurrences(app.store, from, to);







}















// ---------------- 标题栏 ----------------







void DrawTitleBar(Graphics& g, float W) {







    const Theme& th = TH();







    float H = S(TITLE_H);







    FillVGradient(g, RectF(0, 0, W, H), WithAlpha(th.panel, 255), WithAlpha(th.panel, 238));







    Line(g, 0, H - S(1), W, H - S(1), th.borderSoft, S(1));







    FillVGradient(g, RectF(0, 0, W, S(110)), WithAlpha(th.accent, 7), WithAlpha(th.accent, 0));















    // 品牌：四角星在前，logo 在后；不再重复写 CAELENDAR 文字







    float x = S(18), starS = S(17);







    Icon(g, ICO_STAR, RectF(x, (H - starS) / 2, starS, starS), WithAlpha(th.accent, 240), 1.5f);







    float logoH = S(32), logoW = logoH * (739.0f / 326.0f);







    Bitmap* brand = BrandLogo();







    if (brand) {







        g.DrawImage(brand, RectF(x + starS + S(10), (H - logoH) / 2, logoW, logoH));







    }















    struct TabDef { const wchar_t* label; int view; };







    static const TabDef tabs[5] = { { L"年", VIEW_YEAR }, { L"月", VIEW_MONTH }, { L"周", VIEW_WEEK }, { L"日", VIEW_DAY }, { L"日程", VIEW_AGENDA } };







    float tabW = S(54), tabH = S(32), gap = S(3), tx = S(150);







    RectF seg(tx - S(4), (H - tabH) / 2 - S(1), tabW * 5 + gap * 4 + S(8), tabH + S(2));







    FillRoundRect(g, seg, S(10), WithAlpha(th.panel2, 170));







    StrokeRoundRect(g, seg, S(10), th.borderSoft, S(1));







    for (int i = 0; i < 5; ++i) {







        float rx = tx + i * (tabW + gap);







        RectF r(rx, (H - tabH) / 2, tabW, tabH);







        bool active = app.view == tabs[i].view;







        int cmd = CMD_VIEW_BASE + tabs[i].view;







        bool hover = IsHovered(cmd);







        if (active) {







            FillRoundRect(g, r, S(8), WithAlpha(th.accent, 28));







            StrokeRoundRect(g, r, S(8), WithAlpha(th.accent, 80), S(1));







        } else if (hover) FillRoundRect(g, r, S(8), WithAlpha(th.cardHover, 180));







        Color fg = active ? th.accent : (hover ? th.text : th.dim);







        Txt(g, tabs[i].label, S(12.5f), active, fg, r, ALIGN_CENTER | ALIGN_VCENTER);







            AddHit(r.X, r.Y, r.Width, r.Height, cmd, [v = tabs[i].view]() { app.view = v; app.selected.clear(); InvalidateOccs(); UIInvalidate(); });







    }















    float rx = W - S(16);







    auto rbtn = [&](int cmd, int icon, std::function<void()> fn, float sz = 14) {







        rx -= S(34);







        UI::IconBtn(g, RectF(rx, (H - S(34)) / 2, S(34), S(34)), cmd, icon, sz, std::move(fn));







        rx -= S(2);







    };







    rbtn(CMD_CLOSE, ICO_CLOSE, []() { PostMessageW(g_hwnd, WM_CLOSE, 0, 0); });







    rbtn(CMD_MAX, ICO_MAX, []() { ShowWindow(g_hwnd, IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE); });







    rbtn(CMD_MIN, ICO_MIN, []() { HideMainWindowToTray(); });







    rx -= S(4);







    rbtn(CMD_SETTINGS, ICO_GEAR, []() { OpenSettings(); });







    rbtn(CMD_MORE, ICO_MORE, []() {







        std::vector<MenuItemDef> items;







        items.push_back({ L"快速添加（自然语言）", ICO_EDIT, false, false, []() { OpenQuickAdd(); } });







        items.push_back({ L"批量添加", ICO_LIST, false, false, []() { OpenBulkAdd(); } });







        items.push_back({ L"智能导入…", ICO_FOLDER, false, false, []() { OpenSmartImportFromFile(); } });



        items.push_back({ L"导出为 ICS…", ICO_FOLDER, false, false, []() {







            wchar_t file[MAX_PATH] = L"CAELENDAR导出.ics";







            OPENFILENAMEW ofn{};







            ofn.lStructSize = sizeof(ofn);







            ofn.hwndOwner = g_hwnd;







            ofn.lpstrFilter = L"日历文件 (*.ics)\0*.ics\0";







            ofn.lpstrFile = file;







            ofn.nMaxFile = MAX_PATH;







            ofn.lpstrDefExt = L"ics";







            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;







            if (!GetSaveFileNameW(&ofn)) return;







            std::string data = ExportIcs(app.store);







            if (WriteFileBytesAtomic(file, data)) ShowToast(L"已导出 ICS");







            else ShowToast(L"导出失败");







        } });







        items.push_back({ L"批量管理日程（多选 / 删除）", ICO_LIST, false, false, []() {







            app.view = VIEW_AGENDA;







            InvalidateOccs();







            UIInvalidate();







        } });







        items.push_back({ L"批量清理日程…（日期 / 标签 / 颜色 / 类型）", ICO_TRASH, false, false, []() {







            OpenCleanup();







        } });







        items.push_back({ L"课程 / 系列配色…", ICO_TAG, false, false, []() {







            OpenSeriesColors();







        } });







        items.push_back({ L"智能整理课表（识别课程 / 合并重复）", ICO_STAR, false, false, []() {







            OpenConfirm(L"智能整理课表",







                        L"将扫描没有标签的日程：自动识别课程并打「课程」标签、按课程自动配色、把有规律的每周重复日程合并成可编辑的重复系列。\n\n合并后可以整组编辑（仅此次 / 此后所有 / 整个系列）。此操作可用 Ctrl+Z 撤销。",







                        []() {







                            app.store.Snapshot();







                            SmartOptions so;







                            SmartResult sr = SmartOrganize(app.store, so);







                            DoSaveStore();







                            InvalidateOccs();







                            ShowToast(sr.summary, 6000);







                            UIInvalidate();







                        },







                        L"开始整理");







        } });







        items.push_back({ L"撤销  Ctrl+Z", ICO_UNDO, false, !app.store.CanUndo(), []() { UndoAction(); } });







        items.push_back({ L"重做  Ctrl+Y", ICO_REDO, false, !app.store.CanRedo(), []() { RedoAction(); } });

        items.push_back({ L"从模板新建…", ICO_EDIT, false, false, []() { OpenTemplateNewMenu(); } });







        items.push_back({ L"桌面挂件", ICO_PIN, false, false, []() { ToggleWidgetWindow(); } });







        items.push_back({ L"账号同步…", ICO_USER, false, false, []() { OpenAccountSync(); } });

        items.push_back({ L"机器人（Telegram）…", ICO_PHONE, false, false, []() { OpenBotPanel(); } });

        items.push_back({ L"设置", ICO_SETTINGS, false, false, []() { OpenSettings(); } });







        items.push_back({ L"功能地图 / 快捷键（F1）", ICO_HELP, false, false, []() { OpenHelp(); } });







        items.push_back({ L"重启星历", ICO_REFRESH, false, false, []() { RestartApp(); } });







        items.push_back({ L"退出 CAELENDAR", ICO_CLOSE, true, false, []() { g_quitting = true; PostMessageW(g_hwnd, WM_CLOSE, 0, 0); } });







        OpenMenu(0, 0, items);







    });







    rx -= S(6);







    rbtn(CMD_THEME, ICO_THEME, []() {







        std::vector<MenuItemDef> items;







        for (int i = 0; i < ThemeCount(); ++i) {







            int idx = i;







            items.push_back({ ThemeAt(i).name, app.themeIdx == i ? ICO_CHECK : -1, false, false, [idx]() {







                app.themeIdx = idx;







                app.store.settings.theme = ThemeIdAt(idx);







                app.needSave = true;







                UIInvalidate();







            } });







        }







        OpenMenu(0, 0, items);







    });







    rx -= S(10);







    rx -= S(88);







    UI::Btn(g, RectF(rx, (H - S(34)) / 2, S(88), S(34)), CMD_NEW, L"新建", 0, ICO_PLUS, []() {







        OpenEditorNew(app.cursor, 9, 0);







    });







    rx -= S(12);







    rx -= S(208);







    RectF searchR(rx, (H - S(34)) / 2, S(208), S(34));







    FillRoundRect(g, searchR, S(9), WithAlpha(th.panel2, 200));







    StrokeRoundRect(g, searchR, S(9), app.searchEditing ? WithAlpha(th.accent, 140) : th.borderSoft, S(1));







    Icon(g, ICO_SEARCH, RectF(searchR.X + S(11), searchR.Y + S(10), S(14), S(14)), th.dim2, 1.5f);







    bool searchEditable = app.searchEditing && app.overlay == OV_NONE;







    if (searchEditable) {







        HWND se = UIEdit(9001, RectF(searchR.X + S(31), searchR.Y + S(6), searchR.Width - S(44), S(22)), &app.search, false, S(12));







        if (app.searchFocusPending && se) {



            app.searchFocusPending = false;



            UISetEditFocus(9001, true);



        }







    } else {







        // 非编辑态不创建原生 EDIT：从根源上避免搜索框抢键盘焦点



        if (app.search.empty()) {



            Txt(g, L"搜索日程、标签…", S(11.5f), false, th.dim2, RectF(searchR.X + S(33), searchR.Y, searchR.Width - S(76), searchR.Height), ALIGN_VCENTER);



        } else {



            Txt(g, app.search, S(11.5f), false, th.text, RectF(searchR.X + S(33), searchR.Y, searchR.Width - S(76), searchR.Height), ALIGN_VCENTER, false, true);



        }



        AddHit(searchR.X, searchR.Y, searchR.Width, searchR.Height, CMD_SEARCH_EDIT, []() {



            app.searchEditing = true;



            app.searchIntent = true;



            app.searchFocusPending = true;



            UIInvalidate();



        });







    }




























}















// ---------------- 侧边栏 ----------------







static float g_tagRegionTop = 0, g_tagRegionBottom = 0;

static int g_tagVisibleRows = 1;



void DrawSidebar(Graphics& g, float W, float H) {







    const Theme& th = TH();







    float top = S(TITLE_H), sideW = S(SIDEBAR_W);







    FillRoundRect(g, RectF(0, top, sideW, H - top), 0, th.panel);







    Line(g, sideW - S(1), top, sideW - S(1), H, th.borderSoft, S(1));







    float pad = S(16), innerW = sideW - pad * 2;







    float y = top + pad;















    RectF today(pad, y, innerW, S(140));







    FillRoundRect(g, today, S(14), th.card);







    FillRoundRectV(g, RectF(today.X, today.Y, today.Width, today.Height * 0.55f), S(14), WithAlpha(th.accent, 12), WithAlpha(th.accent, 0));







    StrokeRoundRect(g, today, S(14), th.border, S(1));







    HudCorner(g, today, WithAlpha(th.accent, 200), 9);







    Txt(g, Fmt(L"%d", app.now.d), S(42), true, th.accent, RectF(today.X + S(14), today.Y + S(8), S(80), S(56)), ALIGN_LEFT, true);







    Txt(g, Fmt(L"%d月 · %s", app.now.mo, WeekdayCN(DayOfWeek(app.now.y, app.now.mo, app.now.d))),







        S(12), false, th.dim, RectF(today.X + S(90), today.Y + S(15), today.Width - S(104), S(20)));







    int sw = SemesterWeek(app.store.settings, app.now);







    if (sw > 0) {







        std::wstring sem = Fmt(L"第 %d 周", sw);







        if (app.store.settings.semester.showParity) sem += (sw % 2 == 1) ? L" · 单周" : L" · 双周";







        Txt(g, sem, S(10.5f), false, th.dim2, RectF(today.X + S(90), today.Y + S(37), today.Width - S(104), S(18)));







    } else {







        Txt(g, L"假期 / 学期外", S(10.5f), false, th.dim2, RectF(today.X + S(90), today.Y + S(37), today.Width - S(104), S(18)));







    }







    Txt(g, Fmt(L"%02d:%02d", app.now.h, app.now.mi), S(21), true, th.text, RectF(today.X + S(90), today.Y + S(56), S(100), S(28)), ALIGN_LEFT, true);







    int todayCnt = 0, todayDone = 0;







    {







        auto tOccs = TodayOccurrences();







        for (auto& o : tOccs) { if (!OccMatchesFilter(o)) continue; ++todayCnt; if (o.ev.done) ++todayDone; }







    }







    {







        std::wstring lunar = LunarFullText(app.now);







        if (!lunar.empty())







            Txt(g, lunar, S(8.5f), false, WithAlpha(th.accent, 210),







                RectF(today.X + S(14), today.Y + S(78), innerW - S(28), S(14)));







    }







    Txt(g, Fmt(L"今日 %d 项日程", todayCnt), S(10.5f), false, th.dim2, RectF(today.X + S(14), today.Y + S(94), S(120), S(16)));







    Txt(g, Fmt(L"%d 项已完成", todayDone), S(9), false, WithAlpha(th.dim2, 200), RectF(today.X + S(14), today.Y + S(110), S(120), S(14)));







    {







        float frac = todayCnt > 0 ? (float)todayDone / todayCnt : 0;







        RectF bar(today.X + S(14), today.Y + S(126), innerW - S(28), S(4));







        FillRoundRect(g, bar, S(2), WithAlpha(th.border, 255));







        FillRoundRect(g, RectF(bar.X, bar.Y, std::max(S(4), bar.Width * frac), bar.Height), S(2), th.accent);







    }







    y += today.Height + S(14);















    RectF water(pad, y, innerW, S(84));







    FillRoundRect(g, water, S(14), th.card);







    StrokeRoundRect(g, water, S(14), IsHovered(CMD_WATER) ? WithAlpha(th.accent2, 110) : th.border, S(1));







    int cups = app.store.WaterToday();







    int target = std::min(12, std::max(1, app.store.settings.water.targetCups));







    // 整卡点击 = 快速 +1（先注册，水滴命中会覆盖它）







    AddHit(water.X, water.Y, water.Width, water.Height, CMD_WATER, []() {







        app.store.Snapshot();







        app.store.AddWaterCup();







        DoSaveStore();







        ShowToast(L"已记一杯水 · 右键可减/清零");







        UIInvalidate();







    });







    Icon(g, ICO_DROP, RectF(water.X + S(13), water.Y + S(12), S(15), S(15)), th.accent2, 1.5f);







    Txt(g, L"喝水", S(11.5f), true, th.dim, RectF(water.X + S(35), water.Y + S(10), S(60), S(18)));







    Txt(g, Fmt(L"%d / %d 杯", cups, app.store.settings.water.targetCups), S(14.5f), true, th.accent2,







        RectF(water.X + water.Width - S(104), water.Y + S(8), S(90), S(22)), ALIGN_RIGHT, true);







    for (int i = 0; i < target; ++i) {







        float dz = S(14);







        float dx = water.X + S(14) + i * (dz + S(4));







        bool on = i < cups;







        bool hover = IsHovered(CMD_WATER_SET_BASE + i);







        if (hover) FillCircle(g, dx + dz / 2, water.Y + S(36) + dz / 2, S(9), WithAlpha(th.accent2, 40));







        Icon(g, ICO_DROP, RectF(dx, water.Y + S(36), dz, dz), on ? th.accent2 : WithAlpha(th.dim2, 80), 1.3f);







        // 点第 N 滴水滴 = 直接把今天杯数改成 N（可增可减）







        AddHit(dx - S(2), water.Y + S(33), dz + S(4), S(20), CMD_WATER_SET_BASE + i, [i]() {







            app.store.Snapshot();







            app.store.SetWater(ISODate(Today()), i + 1);







            DoSaveStore();







            ShowToast(Fmt(L"今日喝水已设为 %d 杯", app.store.WaterToday()));







            UIInvalidate();







        });







    }







    {







        int iv = app.store.settings.water.intervalMin;







        int from = app.store.settings.water.fromMin, to = app.store.settings.water.toMin;







        Txt(g, Fmt(L"每 %d 分钟 · %02d:%02d–%02d:%02d · 点水滴改杯数 · 右键更多", iv, from / 60, from % 60, to / 60, to % 60),







            S(8.5f), false, th.dim2, RectF(water.X + S(14), water.Y + S(60), water.Width - S(28), S(14)));







    }







    y += water.Height + S(14);















    Txt(g, L"标签", S(11), true, th.dim, RectF(pad, y, S(80), S(18)));







    Txt(g, L"点击筛选 · 滚轮滚动", S(8.5f), false, th.dim2, RectF(pad, y, innerW, S(18)), ALIGN_RIGHT);







    y += S(22);







    float bottomLimit = H - S(16) - S(96);







    float rowH = S(22);







    int totalTags = (int)app.store.tags.size();

    int visibleRows = std::max(1, (int)((bottomLimit - y) / rowH));

    app.tagScroll = std::max(0, std::min(app.tagScroll, std::max(0, totalTags - visibleRows)));

    g_tagRegionTop = y;

    g_tagRegionBottom = bottomLimit;

    g_tagVisibleRows = visibleRows;

    for (int i = app.tagScroll; i < totalTags && y + rowH <= bottomLimit; ++i) {

        Tag& tag = app.store.tags[i];

        RectF row(pad - S(4), y, innerW + S(8), rowH);

        bool active = app.tagFilter.count(tag.id) > 0;

        bool hover = IsHovered(CMD_TAG_BASE + i);

        if (hover || active) FillRoundRect(g, row, S(7), WithAlpha(th.cardHover, active ? 220 : 150));

        Color tc = Hex(tag.color);

        DrawTagIcon(g, tag, RectF(row.X + S(6), row.Y + (rowH - S(14)) / 2, S(14), S(14)), tc, 1.3f);

        Txt(g, tag.name, S(11.5f), active, active ? th.text : th.dim, RectF(row.X + S(26), row.Y, S(120), rowH), ALIGN_VCENTER);

        int cnt = 0;

        for (auto& o : app.occs) if (o.ev.HasTag(tag.id)) ++cnt;

        Txt(g, Fmt(L"%d", cnt), S(9.5f), false, th.dim2, RectF(row.X + row.Width - S(44), row.Y, S(28), rowH),

            ALIGN_RIGHT | ALIGN_VCENTER);

        std::string tid = tag.id;

        AddHit(row.X, row.Y, row.Width, row.Height, CMD_TAG_BASE + i, [tid]() {

            if (app.tagFilter.count(tid)) app.tagFilter.erase(tid); else app.tagFilter.insert(tid);

            UIInvalidate();

        });

        y += rowH;

    }























    RectF up(pad, H - S(16) - S(92), innerW, S(92));







    FillRoundRect(g, up, S(14), th.card);







    StrokeRoundRect(g, up, S(14), th.border, S(1));







    Txt(g, L"近期", S(10.5f), true, th.dim2, RectF(up.X + S(14), up.Y + S(9), S(80), S(16)));







    auto future = ExpandOccurrences(app.store, StartOfDay(app.now), AddDaysDT(StartOfDay(app.now), 30));







    int shown = 0;







    float ly = up.Y + S(28);







    for (auto& o : future) {







        if (CompareDT(o.start, app.now) <= 0) continue;







        if (!OccMatchesFilter(o)) continue;







        if (shown >= 3) break;







        Color tc = Color(255, 148, 163, 184);







        if (!o.ev.tags.empty()) tc = TagColorOf(o.ev.tags[0]);
        if (!o.ev.color.empty()) tc = Hex(o.ev.color);







        FillCircle(g, up.X + S(17), ly + S(6), S(2.6f), tc);







        Txt(g, DisplayEventTitle(o), S(10.5f), false, th.text, RectF(up.X + S(26), ly - S(4), up.Width - S(80), S(18)), ALIGN_LEFT);







        Txt(g, RelativeDayLabel(o.start), S(9), false, th.dim2, RectF(up.X + up.Width - S(58), ly - S(4), S(48), S(18)), ALIGN_RIGHT);







        ly += S(19);







        ++shown;







    }







    if (shown == 0) Txt(g, L"近 30 天没有更多日程", S(9.5f), false, th.dim2, RectF(up.X + S(14), up.Y + S(36), up.Width - S(28), S(16)));







}















// ---------------- 主帧 ----------------







static void EnsureBuffer(int w, int h, HWND hwnd) {







    if (g_memdc && g_bufW == w && g_bufH == h) return;







    if (g_memdc) { DeleteDC(g_memdc); g_memdc = nullptr; }







    if (g_hbmp) { DeleteObject(g_hbmp); g_hbmp = nullptr; }







    HDC screen = GetDC(hwnd);







    g_memdc = CreateCompatibleDC(screen);







    BITMAPINFO bi{};







    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);







    bi.bmiHeader.biWidth = w;







    bi.bmiHeader.biHeight = -h;







    bi.bmiHeader.biPlanes = 1;







    bi.bmiHeader.biBitCount = 32;







    bi.bmiHeader.biCompression = BI_RGB;







    void* bits = nullptr;







    g_hbmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);







    ReleaseDC(hwnd, screen);







    g_bits = bits;







    SelectObject(g_memdc, g_hbmp);







    g_bufW = w; g_bufH = h;







}















void DrawSelectionBar(Graphics& g, float W, float H) {







    gfx::FontPartScope fontPart(gfx::FP_LIST);







    const Theme& th = TH();







    std::vector<Occurrence> sel;







    for (auto& o : app.occs) if (app.selected.count(OccKey(o))) sel.push_back(o);







    if (sel.empty()) return;







    float pw = S(580), ph = S(46);







    float x = (W - pw) / 2, y = H - ph - S(58);







    FillRoundRect(g, RectF(x + S(3), y + S(4), pw, ph), S(13), Color(70, 0, 0, 0));







    FillRoundRect(g, RectF(x, y, pw, ph), S(13), WithAlpha(th.card, 248));







    StrokeRoundRect(g, RectF(x, y, pw, ph), S(13), WithAlpha(th.accent, 120), S(1));







    Txt(g, Fmt(L"已选 %d", (int)sel.size()), S(11), true, th.accent,







        RectF(x + S(14), y, S(74), ph), ALIGN_VCENTER);







    float bx = x + S(92);







    auto btn = [&](const wchar_t* label, int cmd, float w, std::function<void()> fn, bool danger = false) {







        UI::Btn(g, RectF(bx, y + S(8), w, S(30)), cmd, label, danger ? 3 : 1, -1, std::move(fn));







        bx += w + S(6);







    };







    btn(L"完成/取消", CMD_BATCH_DONE, S(82), []() {







        std::vector<Occurrence> sel;







        for (auto& o : app.occs) if (app.selected.count(OccKey(o))) sel.push_back(o);







        if (sel.empty()) return;







        bool allDone = true;







        for (auto& o : sel) if (!o.ev.done) { allDone = false; break; }







        bool target = !allDone;







        app.store.Snapshot();







        for (auto& o : sel) app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, target);







        DoSaveStore();







        InvalidateOccs();







        ShowToast(target ? L"已标记完成" : L"已标记未完成");







    });







    btn(L"调休迁移", CMD_BATCH_RESCHEDULE, S(82), []() { OpenRescheduleFromSelection(); });







    btn(L"删除", CMD_BATCH_DELETE, S(62), []() { DeleteSelected(); }, true);







    btn(L"反选", CMD_BATCH_INVERT, S(62), []() {







        for (auto& o : app.occs) {







            std::wstring k = OccKey(o);







            auto it = app.selected.find(k);







            if (it == app.selected.end()) app.selected.insert(k);







            else app.selected.erase(it);







        }







        UIInvalidate();







    });







    btn(L"清空", CMD_BATCH_CLEAR, S(62), []() { app.selected.clear(); UIInvalidate(); });







}















static void PaintAll(Graphics& g, int W, int H) {







    const Theme& th = TH();







    ClearHits();







    g_drawLayer = 0;







    FillVGradient(g, RectF(0, 0, (float)W, (float)H), th.bgTop, th.bgBottom);







    for (int i = 5; i >= 1; --i) {







        float ex = i * S(26);







        FillRoundRect(g, RectF((float)W * 0.52f - ex, -S(240) - ex, (float)W * 0.55f + ex * 2, S(620) + ex * 2),







                      S(280), Color(2, th.accent2.GetR(), th.accent2.GetG(), th.accent2.GetB()));







    }







    RefreshOccs();







    gfx::SetFontPart(gfx::FP_TITLEBAR);







    DrawTitleBar(g, (float)W);







    gfx::SetFontPart(gfx::FP_SIDEBAR);







    DrawSidebar(g, (float)W, (float)H);







    gfx::SetFontPart(gfx::FP_MONTH);







    float cx = S(SIDEBAR_W) + S(22), cy = S(TITLE_H) + S(14);







    float cw = (float)W - cx - S(22), chh = (float)H - cy - S(22);







    switch (app.view) {







    case VIEW_MONTH: DrawMonthView(g, cx, cy, cw, chh); break;







    case VIEW_WEEK: DrawWeekView(g, cx, cy, cw, chh, false); break;







    case VIEW_DAY: DrawWeekView(g, cx, cy, cw, chh, true); break;







    case VIEW_YEAR: DrawYearView(g, cx, cy, cw, chh); break;







    default: DrawAgendaView(g, cx, cy, cw, chh); break;







    }







    if (app.overlay == OV_NONE && app.view != VIEW_AGENDA && app.view != VIEW_YEAR && !app.selected.empty())







        DrawSelectionBar(g, (float)W, (float)H);







    gfx::SetFontPart(gfx::FP_DIALOG);







    DrawToast(g, (float)W, (float)H);







    if (app.overlay != OV_NONE) {







        UI::Dim(g, (float)W, (float)H, 168);







        g_drawLayer = 1;







        DrawOverlays(g, (float)W, (float)H);







        g_drawLayer = 0;







    }







    UIEditEndFrame();







}















void RenderFrame() {







    if (!g_hwnd) return;







    RECT rc; GetClientRect(g_hwnd, &rc);







    int W = rc.right, H = rc.bottom;







    if (W <= 0 || H <= 0) return;







    EnsureBuffer(W, H, g_hwnd);







    Graphics g(g_memdc);







    g.SetSmoothingMode(SmoothingModeAntiAlias);







    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);







    g.SetPixelOffsetMode(PixelOffsetModeHalf);







    PaintAll(g, W, H);







    HDC dc = GetDC(g_hwnd);







    BitBlt(dc, 0, 0, W, H, g_memdc, 0, 0, SRCCOPY);







    ReleaseDC(g_hwnd, dc);







}















static bool SaveScreenshot(const std::wstring& path) {







    RECT rc; GetClientRect(g_hwnd, &rc);







    int W = rc.right, H = rc.bottom;







    if (W <= 0 || H <= 0) return false;







    HDC wdc = GetDC(g_hwnd);







    HDC mem = CreateCompatibleDC(wdc);







    HBITMAP bmp = CreateCompatibleBitmap(wdc, W, H);







    HGDIOBJ old = SelectObject(mem, bmp);







    BitBlt(mem, 0, 0, W, H, wdc, 0, 0, SRCCOPY | CAPTUREBLT);







    SelectObject(mem, old);







    Gdiplus::Bitmap gb(bmp, nullptr);







    bool ok = SavePng(&gb, path);







    DeleteObject(bmp);







    DeleteDC(mem);







    ReleaseDC(g_hwnd, wdc);







    DebugLog(L"shot ok=%d path=%s size=%dx%d", ok ? 1 : 0, path.c_str(), W, H);







    return ok;







}







static bool CaptureWindowPng(HWND hw, const std::wstring& path) {







    if (!hw) return false;







    RECT rc; GetWindowRect(hw, &rc);







    int W = rc.right - rc.left, H = rc.bottom - rc.top;







    if (W <= 0 || H <= 0) return false;







    HDC wdc = GetWindowDC(hw);







    HDC mem = CreateCompatibleDC(wdc);







    HBITMAP bmp = CreateCompatibleBitmap(wdc, W, H);







    HGDIOBJ old = SelectObject(mem, bmp);







    BOOL ok = PrintWindow(hw, mem, 2 /*PW_RENDERFULLCONTENT*/);







    if (!ok) BitBlt(mem, 0, 0, W, H, wdc, 0, 0, SRCCOPY | CAPTUREBLT);







    SelectObject(mem, old);







    Gdiplus::Bitmap gb(bmp, nullptr);







    bool saved = SavePng(&gb, path);







    DeleteObject(bmp);







    DeleteDC(mem);







    ReleaseDC(hw, wdc);







    DebugLog(L"capture hwnd=%p ok=%d path=%s", hw, saved ? 1 : 0, path.c_str());







    return saved;







}















// ---------------- 数据辅助 ----------------







void AutoBackupTick() {
    const std::wstring& dir = app.store.settings.autoBackupDir;
    if (dir.empty()) return;
    EnsureDir(dir);
    std::wstring today = ISODate(app.now);
    std::wstring file = PathJoin(dir, L"星历备份-" + today + L".json");
    if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) {
        CopyFileSimple(GetCalendarPath(), file);
    }
    WIN32_FIND_DATAW fd;
    std::wstring pattern = PathJoin(dir, L"星历备份-*.json");
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        std::vector<std::wstring> files;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        std::sort(files.begin(), files.end());
        for (size_t i = 0; i + 30 < files.size(); ++i) DeleteFileW(PathJoin(dir, files[i]).c_str());
    }
}

void DoSaveStore() {







    if (g_noSave) { app.needSave = false; return; }







    if (app.store.Save()) {







        g_lastSaveTick = GetTickCount();







        app.needSave = false;
        AutoBackupTick();







        RefreshWidgetNow();







    } else {







        ShowToast(L"保存失败，请检查磁盘权限");







    }







}















void ApplyThemeFromSettings() {



    Settings& s = app.store.settings;



    app.themeIdx = ThemeIndexById(s.theme);



    ApplyCustomTheme(s.customBg, s.customAccent, s.themeRadius, s.themeDensity, s.customLight);



}















void RestartApp() {







    DoSaveStore();







    if (g_singleMutex) {







        ReleaseMutex(g_singleMutex);







        CloseHandle(g_singleMutex);







        g_singleMutex = nullptr;







    }







    wchar_t exe[1024] = { 0 };







    GetModuleFileNameW(nullptr, exe, 1024);







    std::wstring cmd = std::wstring(L"\"") + exe + L"\" --restarted --wait-pid " + std::to_wstring(GetCurrentProcessId());







    STARTUPINFOW si{};







    si.cb = sizeof(si);







    PROCESS_INFORMATION pi{};







    BOOL ok = CreateProcessW(exe, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);







    DebugLog(L"restart: ok=%d cmd=%s", ok ? 1 : 0, cmd.c_str());







    if (ok) {







        CloseHandle(pi.hProcess);







        CloseHandle(pi.hThread);







    }







    g_quitting = true;







    if (g_hwnd) DestroyWindow(g_hwnd);







}















std::wstring OccKey(const Occurrence& o) {







    return Utf8ToWide(o.seriesId) + L"|" + ISODT(o.origStart) + L"|" + ISODT(o.start);







}















void UndoAction() {







    if (app.store.Undo()) {







        InvalidateOccs();







        DoSaveStore();







        ShowToast(L"已撤销");







    } else {







        ShowToast(L"没有可撤销的操作");







    }







    UIInvalidate();







}







void RedoAction() {







    if (app.store.Redo()) {







        InvalidateOccs();







        DoSaveStore();







        ShowToast(L"已重做");







    }







    UIInvalidate();







}















void ToggleSelected(const std::wstring& key) {







    if (app.selected.count(key)) app.selected.erase(key); else app.selected.insert(key);







    UIInvalidate();







}

static std::wstring KeyNameVk(WPARAM w) {
    if (w >= 'A' && w <= 'Z') return std::wstring(1, (wchar_t)w);
    if (w >= '0' && w <= '9') return std::wstring(1, (wchar_t)w);
    if (w >= VK_F1 && w <= VK_F12) return Fmt(L"F%d", (int)(w - VK_F1 + 1));
    if (w == VK_SPACE) return L"Space";
    if (w == VK_OEM_3) return L"`";
    return L"";
}

bool HotkeyMatches(const std::wstring& spec, WPARAM key, bool ctrl, bool alt, bool shift) {
    bool needCtrl = false, needAlt = false, needShift = false;
    std::wstring keyName;
    size_t start = 0;
    while (start <= spec.size()) {
        size_t pos = spec.find(L'+', start);
        std::wstring part = spec.substr(start, pos == std::wstring::npos ? std::wstring::npos : pos - start);
        if (part == L"Ctrl") needCtrl = true;
        else if (part == L"Alt") needAlt = true;
        else if (part == L"Shift") needShift = true;
        else if (!part.empty()) keyName = part;
        if (pos == std::wstring::npos) break;
        start = pos + 1;
    }
    if (keyName.empty()) return false;
    if (needCtrl != ctrl || needAlt != alt || needShift != shift) return false;
    return KeyNameVk(key) == keyName;
}

static void SetHotkeyByIndex(int idx, const std::wstring& spec) {
    Settings& st = app.store.settings;
    switch (idx) {
    case 0: st.hotkeyMonth = spec; break;
    case 1: st.hotkeyWeek = spec; break;
    case 2: st.hotkeyDay = spec; break;
    case 3: st.hotkeyYear = spec; break;
    case 4: st.hotkeyAgenda = spec; break;
    case 5: st.hotkeyToday = spec; break;
    case 6: st.hotkeySearch = spec; break;
    case 7: st.hotkeyNew = spec; break;
    default: break;
    }
}

static void HandleHotkeyCapture(WPARAM key, bool ctrl, bool alt, bool shift) {
    if (key == VK_ESCAPE) {
        app.hotkeysCapturing = false;
        app.hotkeyCaptureIdx = -1;
        app.hotkeyError.clear();
        UIInvalidate();
        return;
    }
    if (key == VK_CONTROL || key == VK_SHIFT || key == VK_MENU) return;
    std::wstring kn = KeyNameVk(key);
    if (kn.empty()) return;
    if (!ctrl && !alt) {
        app.hotkeyError = L"快捷键至少要有 Ctrl 或 Alt";
        UIInvalidate();
        return;
    }
    std::wstring spec;
    if (ctrl) spec += L"Ctrl+";
    if (alt) spec += L"Alt+";
    if (shift) spec += L"Shift+";
    spec += kn;
    int idx = app.hotkeyCaptureIdx;
    SetHotkeyByIndex(idx, spec);
    app.hotkeysCapturing = false;
    app.hotkeyCaptureIdx = -1;
    app.hotkeyError.clear();
    app.needSave = true;
    DoSaveStore();
    ShowToast(Fmt(L"快捷键已改为 %s", spec.c_str()));
    UIInvalidate();
}

static bool HandleHotkeys(WPARAM key, bool ctrl, bool alt, bool shift) {
    Settings& st = app.store.settings;
    auto setView = [&](int v) {
        app.view = v;
        if (v == VIEW_YEAR) app.selected.clear();
        InvalidateOccs();
        UIInvalidate();
    };
    if (HotkeyMatches(st.hotkeyMonth, key, ctrl, alt, shift)) { setView(VIEW_MONTH); return true; }
    if (HotkeyMatches(st.hotkeyWeek, key, ctrl, alt, shift)) { setView(VIEW_WEEK); return true; }
    if (HotkeyMatches(st.hotkeyDay, key, ctrl, alt, shift)) { setView(VIEW_DAY); return true; }
    if (HotkeyMatches(st.hotkeyYear, key, ctrl, alt, shift)) { setView(VIEW_YEAR); return true; }
    if (HotkeyMatches(st.hotkeyAgenda, key, ctrl, alt, shift)) { setView(VIEW_AGENDA); return true; }
    if (HotkeyMatches(st.hotkeyToday, key, ctrl, alt, shift)) {
        app.cursor = Today();
        InvalidateOccs();
        UIInvalidate();
        return true;
    }
    if (HotkeyMatches(st.hotkeySearch, key, ctrl, alt, shift)) {
        AppEnableIme(true);
        app.searchFocused = true;
        app.searchIntent = true;
        app.searchEditing = true;
        app.searchFocusPending = true;
        UIInvalidate();
        return true;
    }
    if (HotkeyMatches(st.hotkeyNew, key, ctrl, alt, shift)) {
        OpenEditorNew(app.cursor, 9, 0);
        return true;
    }
    return false;
}

bool ShiftSelectPressed() {
    if (g_shiftAtPress) return true;
    return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
}
bool CtrlSelectPressed() {
    if (g_ctrlAtPress) return true;
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}
bool MultiSelectPressed() {
    if (g_shiftAtPress || g_ctrlAtPress) return true;
    return ((GetKeyState(VK_SHIFT) | GetKeyState(VK_CONTROL)) & 0x8000) != 0;
}















void DeleteSelected() {







    if (app.selected.empty()) return;







    // 不依赖当前视图的 occs：“全选”会把所有日期的实例 key 都放进来，







    // 这里按 key 前缀反查系列，保证一次删除干净（包括不在当前 60 天窗口里的旧日程）







    std::vector<std::string> seriesIds;







    for (auto& e : app.store.events) {







        std::wstring prefix = Utf8ToWide(e.id) + L"|";







        for (auto& k : app.selected) {







            if (k.rfind(prefix, 0) == 0) {







                seriesIds.push_back(e.id);







                break;







            }







        }







    }







    if (seriesIds.empty()) {







        app.selected.clear();







        UIInvalidate();







        ShowToast(L"没有可删除的日程");







        return;







    }







    OpenConfirm(L"删除所选日程", Fmt(L"将删除 %d 个系列（含其重复实例）。此操作可撤销。", (int)seriesIds.size()), [seriesIds]() {







        app.store.Snapshot();







        for (auto& id : seriesIds) app.store.DeleteEvent(id);







        app.selected.clear();







        InvalidateOccs();







        DoSaveStore();







        ShowToast(Fmt(L"已删除 %d 个系列", (int)seriesIds.size()));







    }, L"删除", L"取消");







}















// ---------------- 上下文菜单 ----------------







static void SetWaterTo(int cups) {







    app.store.Snapshot();







    app.store.SetWater(ISODate(Today()), cups);







    DoSaveStore();







    ShowToast(Fmt(L"今日喝水：%d / %d 杯", app.store.WaterToday(), app.store.settings.water.targetCups));







    UIInvalidate();







}















static void ShowWaterMenu() {







    std::vector<MenuItemDef> items;







    items.push_back({ L"加一杯", ICO_PLUS, false, false, []() { SetWaterTo(app.store.WaterToday() + 1); } });







    items.push_back({ L"减一杯", ICO_MINUS, false, false, []() { SetWaterTo(app.store.WaterToday() - 1); } });







    items.push_back({ L"今天清零", ICO_TRASH, false, false, []() { SetWaterTo(0); } });







    items.push_back({ L"撤销上一步", ICO_REFRESH, false, false, []() { UndoAction(); } });







    items.push_back({ L"设置目标杯数…", ICO_SETTINGS, false, false, []() { OpenSettings(); } });







    OpenMenu(0, 0, items);







}















void ToggleOccurrenceDone(const Occurrence& o, bool done) {







    app.store.Snapshot();







    if (app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, done)) {







        InvalidateOccs();







        DoSaveStore();







        ShowToast(done ? L"已标记完成" : L"已标记未完成");







    }







}















void DrawTagIcon(Graphics& g, const Tag& tag, RectF box, Color fallback, float weight) {
    static std::map<std::wstring, Bitmap*> cache;
    std::wstring ci = tag.customIcon;
    if (!ci.empty()) {
        std::wstring path = ci;
        if (!(path.size() > 1 && path[1] == L':') && path[0] != L'\\') path = PathJoin(GetDataDir(), path);
        auto it = cache.find(path);
        Bitmap* bmp = (it != cache.end()) ? it->second : nullptr;
        if (!bmp) {
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                Bitmap* nb = Bitmap::FromFile(path.c_str());
                if (nb && nb->GetLastStatus() == Ok) {
                    cache[path] = nb;
                    bmp = nb;
                } else {
                    delete nb;
                }
            }
        }
        if (bmp) {
            g.DrawImage(bmp, box);
            return;
        }
    }
    Icon(g, IconFromName(tag.icon.empty() ? std::wstring(DefaultTagIcon(tag.id)) : tag.icon), box, fallback, weight);
}
std::vector<std::wstring> FindEventConflicts(const Event& ev, const std::string& ignoreSeriesId) {
    std::vector<std::wstring> out;
    if (ev.allDay) return out;   // 全天事件先不做冲突提示
    DateTime from = AddDaysDT(StartOfDay(ev.start), -1);
    DateTime to = AddDaysDT(StartOfDay(ev.end), 2);
    auto occs = ExpandOccurrences(app.store, from, to);
    for (auto& o : occs) {
        if (o.ev.allDay) continue;
        if (!ignoreSeriesId.empty() && o.seriesId == ignoreSeriesId) continue;
        if (CompareDT(o.start, ev.end) < 0 && CompareDT(ev.start, o.end) < 0) {
            if (std::find(out.begin(), out.end(), o.ev.title) == out.end()) out.push_back(o.ev.title);
            if ((int)out.size() >= 3) break;
        }
    }
    return out;
}
static void CopyEventToClip(const Occurrence& o, int mode) {
    const Event* base = app.store.Find(o.seriesId);
    if (!base) return;
    app.eventClip.valid = true;
    app.eventClip.mode = mode;
    if (mode == 0) {
        app.eventClip.ev = o.ev;
        app.eventClip.ev.recur = Recur();
        app.eventClip.ev.overrides.clear();
        app.eventClip.sourceStart = o.start;
    } else if (mode == 1) {
        app.eventClip.ev = *base;
        app.eventClip.ev.overrides.clear();
        app.eventClip.sourceStart = base->start;
    } else {
        app.eventClip.ev = *base;
        app.eventClip.ev.overrides.clear();
        app.eventClip.ev.start = o.start;
        app.eventClip.ev.end = o.end;
        app.eventClip.sourceStart = o.start;
    }
    app.eventClip.label = DisplayEventTitle(o);
    // 保存到最近 5 条历史（去重）
    auto clipKey = [](const EventClipState& c) {
        return c.label + L"|" + ISODT(c.ev.start) + L"|" + Fmt(L"%d", c.mode);
    };
    std::wstring key = clipKey(app.eventClip);
    for (auto it = app.eventClipHistory.begin(); it != app.eventClipHistory.end();) {
        if (clipKey(*it) == key) it = app.eventClipHistory.erase(it);
        else ++it;
    }
    app.eventClipHistory.insert(app.eventClipHistory.begin(), app.eventClip);
    if ((int)app.eventClipHistory.size() > 5) app.eventClipHistory.resize(5);
    ShowToast(mode == 0 ? L"已复制这一次日程，右键目标日期粘贴"
                       : (mode == 1 ? L"已复制整个系列，右键目标日期粘贴" : L"已复制此后所有，右键目标日期粘贴"));
}
static void PasteClipAt(const EventClipState& clip, const DateTime& targetDay) {
    if (!clip.valid) { ShowToast(L"剪贴板里没有日程"); return; }
    Event ev = clip.ev;
    int delta = DaySerial(targetDay.y, targetDay.mo, targetDay.d) -
                DaySerial(clip.sourceStart.y, clip.sourceStart.mo, clip.sourceStart.d);
    ev.start = AddDaysDT(ev.start, delta);
    ev.end = AddDaysDT(ev.end, delta);
    if (ev.recur.freq != RF_NONE) {
        for (auto& ex : ev.recur.exDates) ex = AddDaysDT(ex, delta);
        if (ev.recur.hasUntil) ev.recur.until = AddDaysDT(ev.recur.until, delta);
    }
    ev.id.clear();
    ev.uid.clear();
    ev.overrides.clear();
    ev.done = false;
    ev.created = NowDT();
    ev.updated = NowDT();
    auto doPaste = [ev, clip, targetDay]() {
        app.store.Snapshot();
        app.store.AddEvent(ev);
        InvalidateOccs();
        DoSaveStore();
        ShowToast(Fmt(L"已粘贴「%s」到 %d/%d", clip.label.c_str(), targetDay.mo, targetDay.d));
    };
    auto conflicts = FindEventConflicts(ev);
    if (!conflicts.empty()) {
        std::wstring desc = L"与「" + conflicts[0] + L"」";
        if (conflicts.size() > 1) desc += Fmt(L" 等 %d 条日程", (int)conflicts.size());
        desc += L"时间重叠，仍要粘贴吗？";
        OpenConfirm(L"时间冲突", desc, doPaste, L"仍然粘贴", L"取消");
        return;
    }
    doPaste();
}
static void PasteEventClip(const DateTime& targetDay) {
    if (!app.eventClip.valid) { ShowToast(L"剪贴板里没有日程"); return; }
    PasteClipAt(app.eventClip, targetDay);
}
static void ShowPasteHistoryMenu(const DateTime& targetDay) {
    if (app.eventClipHistory.empty()) { ShowToast(L"剪贴板历史为空"); return; }
    std::vector<MenuItemDef> items;
    for (int i = 0; i < (int)app.eventClipHistory.size(); ++i) {
        EventClipState clip = app.eventClipHistory[i];
        std::wstring label = clip.label + L" · " + ISODate(clip.ev.start);
        items.push_back({ label, ICO_COPY, false, false, [clip, targetDay]() { PasteClipAt(clip, targetDay); } });
    }
    OpenMenu(0, 0, items);
}

static void ShowEventMenu(int occIndex) {







    if (occIndex < 0 || occIndex >= (int)app.occs.size()) return;







    Occurrence o = app.occs[occIndex];







    std::vector<MenuItemDef> items;







    items.push_back({ L"查看当天详情", ICO_CAL, false, false, [o]() { OpenDayDetail(StartOfDay(o.start)); } });



    items.push_back({ L"编辑", ICO_EDIT, false, false, [o]() { OpenEditorForOccurrence(o); } });
    items.push_back({ L"复制日程（这一次）", ICO_COPY, false, false, [o]() { CopyEventToClip(o, 0); } });
    {
        const Event* base = app.store.Find(o.seriesId);
        if (base && base->IsRecurring()) {
            items.push_back({ L"复制整个系列", ICO_COPY, false, false, [o]() { CopyEventToClip(o, 1); } });
            items.push_back({ L"复制此后所有", ICO_COPY, false, false, [o]() { CopyEventToClip(o, 2); } });
        }
    }
    items.push_back({ L"粘贴日程到这一天", ICO_COPY, false, !app.eventClip.valid, [o]() { PasteEventClip(StartOfDay(o.start)); } });
    items.push_back({ L"粘贴历史…", ICO_COPY, false, app.eventClipHistory.empty(), [o]() { ShowPasteHistoryMenu(StartOfDay(o.start)); } });














    items.push_back({ L"调休 / 移到其他日期…", ICO_CAL, false, false, [o]() {







        OpenRescheduleDialog({ o });







    } });







    items.push_back({ o.ev.done ? L"标记为未完成" : L"标记为完成", ICO_CHECK, false, false, [o]() {







        ToggleOccurrenceDone(o, !o.ev.done);







    } });







    items.push_back({ L"删除此日程", ICO_TRASH, true, false, [o]() {







        if (o.ev.IsRecurring()) {







            OpenConfirm(L"删除重复日程", L"这是一个重复日程。此按钮将只删除这一次（可撤销）。",







                        [o]() {







                            app.store.Snapshot();







                            app.store.DeleteOccurrence(o.seriesId, o.origStart, 0);







                            InvalidateOccs();







                            DoSaveStore();







                            ShowToast(L"已删除这一次");







                        }, L"仅删除这一次");







        } else {







            OpenConfirm(L"删除日程", o.ev.title + L" 将被删除（可撤销）", [o]() {







                app.store.Snapshot();







                app.store.DeleteEvent(o.seriesId);







                InvalidateOccs();







                DoSaveStore();







                ShowToast(L"已删除");







            }, L"删除");







        }







    } });







    OpenMenu(0, 0, items);







}















static void ShowDayMenu(int serial) {
    Date d = FromSerial(serial);
    DateTime day = MakeDT(d.y, d.m, d.d);
    std::vector<MenuItemDef> items;
    items.push_back({ L"查看当天详情", ICO_CAL, false, false, [day]() { OpenDayDetail(day); } });
    items.push_back({ L"新建日程…", ICO_PLUS, false, false, [day]() { OpenEditorNew(day, 9, 0); } });
    items.push_back({ L"粘贴日程到这里", ICO_COPY, false, !app.eventClip.valid, [day]() { PasteEventClip(day); } });
    items.push_back({ L"粘贴历史…", ICO_COPY, false, app.eventClipHistory.empty(), [day]() { ShowPasteHistoryMenu(day); } });
    OpenMenu(0, 0, items);
}















// ---------------- 窗口过程 ----------------







// ---------- 托盘 / 后台辅助函数 ----------







static HICON LoadAppIcon(int size) {







    HICON h = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(100), IMAGE_ICON,







                                size, size, LR_DEFAULTCOLOR);







    if (!h) h = (HICON)LoadIconW(nullptr, IDI_APPLICATION);







    return h;







}







static void ReleaseBackbuffer() {







    if (g_memdc) { DeleteDC(g_memdc); g_memdc = nullptr; }







    if (g_hbmp) { DeleteObject(g_hbmp); g_hbmp = nullptr; }







    g_bits = nullptr;







    g_bufW = g_bufH = 0;







}







static void AddTrayIcon() {







    if (g_trayAdded || !g_hwnd) return;







    memset(&g_nid, 0, sizeof(g_nid));







    g_nid.cbSize = sizeof(g_nid);







    g_nid.hWnd = g_hwnd;







    g_nid.uID = 1;







    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;







    g_nid.uCallbackMessage = WM_TRAYICON_MSG;







    g_nid.hIcon = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));
    if (!g_nid.hIcon) DebugLog(L"tray: LoadAppIcon NULL (size=%d)", GetSystemMetrics(SM_CXSMICON));







    wcscpy_s(g_nid.szTip, L"CAELENDAR · 日程与提醒");







    if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {







        g_trayAdded = true;







        DebugLog(L"tray icon added");







    } else {







        DWORD err = GetLastError();
        DebugLog(L"tray icon add FAILED err=%lu cbSize=%u hIcon=%p uFlags=0x%x hwnd=%p",
                 err, (unsigned)g_nid.cbSize, (void*)g_nid.hIcon, (unsigned)g_nid.uFlags, (void*)g_hwnd);
        if (g_hwnd && g_trayRetryCount < 30) {
            ++g_trayRetryCount;
            SetTimer(g_hwnd, kTrayRetryTimerId, 2000, nullptr);
        }







    }







}







static void RemoveTrayIcon() {







    if (!g_trayAdded) return;







    Shell_NotifyIconW(NIM_DELETE, &g_nid);







    g_trayAdded = false;







}







static void ShowMainWindowNow() {







    if (!g_hwnd) return;







    ShowWindow(g_hwnd, SW_SHOW);







    ShowWindow(g_hwnd, SW_RESTORE);







    SetForegroundWindow(g_hwnd);







    InvalidateRect(g_hwnd, nullptr, FALSE);







}







void HideMainWindowToTray() {







    if (!g_hwnd) return;
    // 托盘图标挂不上时降级：最小化到任务栏（窗口仍可恢复），不隐藏
    if (!g_trayAdded) {
        AddTrayIcon();
        if (!g_trayAdded) {
            ShowToast(L"系统托盘不可用，已最小化到任务栏（可点任务栏恢复）");
            DebugLog(L"hide-to-tray: tray unavailable, fallback to taskbar minimize");
            ShowWindow(g_hwnd, SW_MINIMIZE);
            return;
        }
    }







    ShowWindow(g_hwnd, SW_HIDE);







    ReleaseBackbuffer();          // 隐藏后释放绘制缓冲，降低内存占用







    if (g_trayAdded && !g_trayWarned) {







        g_trayWarned = true;







        NOTIFYICONDATAW nid = g_nid;







        nid.uFlags = NIF_INFO;







        wcscpy_s(nid.szInfoTitle, L"CAELENDAR 仍在后台运行");







        wcscpy_s(nid.szInfo, L"提醒和桌面挂件继续工作，点托盘图标可重新打开；右键托盘可退出。");







        nid.dwInfoFlags = NIIF_INFO;







        Shell_NotifyIconW(NIM_MODIFY, &nid);







    }







}







void SyncAutoStart() {







    HKEY key = nullptr;







    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",







                      0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;







    if (app.store.settings.autoStart) {







        wchar_t exe[MAX_PATH * 2] = L"";







        GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);







        std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --autostart";







        RegSetValueExW(key, L"XingLi", 0, REG_SZ, (const BYTE*)cmd.c_str(),







                       (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));







    } else {







        RegDeleteValueW(key, L"XingLi");







    }







    RegCloseKey(key);







}







static void ShowTrayMenu() {







    POINT pt;







    GetCursorPos(&pt);







    HMENU menu = CreatePopupMenu();







    AppendMenuW(menu, MF_STRING, 1, L"打开 CAELENDAR");







    AppendMenuW(menu, MF_STRING, 2, L"显示 / 隐藏桌面挂件");



    AppendMenuW(menu, MF_STRING, 8, MissedReminderCount() > 0 ? Fmt(L"提醒中心（%d）", MissedReminderCount()).c_str() : L"提醒中心");



    AppendMenuW(menu, MF_STRING | (app.store.settings.widget.clickThrough ? MF_CHECKED : 0), 7, L"挂件鼠标穿透");







    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);







    AppendMenuW(menu, MF_STRING | (app.store.settings.autoStart ? MF_CHECKED : 0), 3, L"开机自启动");







    AppendMenuW(menu, MF_STRING | (app.store.settings.closeToTray ? MF_CHECKED : 0), 4, L"关闭时隐藏到托盘");







    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);







    AppendMenuW(menu, MF_STRING, 6, L"重启 CAELENDAR");







    AppendMenuW(menu, MF_STRING, 5, L"退出 CAELENDAR");







    int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,







                                  pt.x, pt.y, 0, g_hwnd, nullptr);







    DestroyMenu(menu);







    switch (cmd) {







    case 1: ShowMainWindowNow(); break;







    case 2: ToggleWidgetWindow(); break;



    case 8:



        ShowMainWindowNow();



        OpenReminderCenter();



        break;



    case 7:



        app.store.settings.widget.clickThrough = !app.store.settings.widget.clickThrough;



        app.store.Touch();



        DoSaveStore();



        RecreateWidgetWindow();



        ShowToast(app.store.settings.widget.clickThrough ? L"挂件已开启鼠标穿透" : L"挂件已关闭鼠标穿透");



        break;







    case 3:







        app.store.settings.autoStart = !app.store.settings.autoStart;







        app.store.Touch();







        DoSaveStore();







        SyncAutoStart();







        ShowToast(app.store.settings.autoStart ? L"已设置开机自启动" : L"已取消开机自启动");







        break;







    case 4:







        app.store.settings.closeToTray = !app.store.settings.closeToTray;







        app.store.Touch();







        DoSaveStore();







        break;







    case 5:







        g_quitting = true;







        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);







        break;







    case 6:







        RestartApp();







        break;







    default: break;







    }







    PostMessageW(g_hwnd, WM_NULL, 0, 0);







}















static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // explorer 重启（任务栏重建）：重新挂托盘图标
    if (g_taskbarCreatedMsg != 0 && msg == g_taskbarCreatedMsg) {
        g_trayAdded = false;
        g_trayRetryCount = 0;
        AddTrayIcon();
        return 0;
    }







    switch (msg) {







    case WM_GETOBJECT: {
        LRESULT lr = A11yHandleGetObject(hwnd, wParam, lParam);
        if (lr != -1) return lr;
        break;
    }

    case WM_CREATE: {







        BOOL dark = TRUE;







        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));







        int corner = 2;







        DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));







        HICON iconBig = LoadAppIcon(GetSystemMetrics(SM_CXICON));







        HICON iconSmall = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));







        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)iconBig);







        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)iconSmall);







        SetTimer(hwnd, 1, 1000, nullptr);







        break;







    }







    case WM_NCCALCSIZE:







        if (wParam == TRUE) {







            NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lParam;







            if (IsZoomed(hwnd)) {







                MONITORINFO mi{ sizeof(mi) };







                GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);







                p->rgrc[0] = mi.rcWork;







            }







            return 0;







        }







        break;







    case WM_NCACTIVATE:







        return TRUE;







    case WM_GETMINMAXINFO: {







        MINMAXINFO* mm = (MINMAXINFO*)lParam;







        mm->ptMinTrackSize.x = (LONG)S(1060);







        mm->ptMinTrackSize.y = (LONG)S(680);







        return 0;







    }







    case WM_NCHITTEST: {







        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };







        RECT wr; GetWindowRect(hwnd, &wr);







        int border = (int)S(6);







        if (!IsZoomed(hwnd)) {







            bool left = pt.x < wr.left + border, right = pt.x >= wr.right - border;







            bool top = pt.y < wr.top + border, bottom = pt.y >= wr.bottom - border;







            if (top && left) return HTTOPLEFT;







            if (top && right) return HTTOPRIGHT;







            if (bottom && left) return HTBOTTOMLEFT;







            if (bottom && right) return HTBOTTOMRIGHT;







            if (top) return HTTOP;







            if (bottom) return HTBOTTOM;







            if (left) return HTLEFT;







            if (right) return HTRIGHT;







        }







        POINT cp = pt; ScreenToClient(hwnd, &cp);







        if (cp.y < (int)S(TITLE_H)) {







            HWND child = ChildWindowFromPointEx(hwnd, cp, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT);







            if (child && child != hwnd) return HTCLIENT;







            int cmd = 0, layer = 0;







            if (PointInHit((float)cp.x, (float)cp.y, cmd, layer)) return HTCLIENT;







            return HTCAPTION;







        }







        return HTCLIENT;







    }







    case WM_SIZE:







        g_maximized = IsZoomed(hwnd) != 0;







        InvalidateRect(hwnd, nullptr, FALSE);







        break;







    case WM_ERASEBKGND:







        return 1;







    case WM_PAINT: {







        PAINTSTRUCT ps;







        BeginPaint(hwnd, &ps);







        RenderFrame();







        EndPaint(hwnd, &ps);







        return 0;







    }







    case WM_TIMER:
        if (wParam == kTrayRetryTimerId) {
            KillTimer(hwnd, kTrayRetryTimerId);
            AddTrayIcon();
            if (!g_trayAdded && g_trayRetryCount < 30) {
                ++g_trayRetryCount;
                SetTimer(hwnd, kTrayRetryTimerId, 2000, nullptr);
                DebugLog(L"tray retry %d", g_trayRetryCount);
            }
            return 0;
        }







        if (wParam == 1) {
            ShareTick();
            if (!g_testMode) WidgetHealthTick();











            if (app.overlay == OV_NONE && !g_widgetMenuOpen) {



                HWND f = GetFocus();



                if (f && f != g_hwnd) {



                    bool isSearch = (GetDlgCtrlID(f) == 18001);



                    if (!isSearch || !app.searchIntent || !app.searchEditing) {



                        app.searchEditing = false;



                        app.searchIntent = false;



                        SetFocus(g_hwnd);







        AppEnableIme(false);



                    }



                }



            }







            DateTime before = app.now;







            RefreshNow();







            bool minuteChanged = (before.h != app.now.h || before.mi != app.now.mi || before.d != app.now.d);







            bool toastWas = !app.toastText.empty();







            if (!app.toastText.empty() && (DWORD)GetTickCount() > app.toastUntil) app.toastText.clear();







            OnTimerTick();







            if (app.needSave && GetTickCount() - g_lastSaveTick > 900) DoSaveStore();







            if (minuteChanged || toastWas || app.needSave) InvalidateRect(hwnd, nullptr, FALSE);







        }







        return 0;







    case WM_COMMAND:







        UIHandleCommand(wParam, lParam);







        return 0;







    case WM_CTLCOLOREDIT:







        return UICtlColorEdit((HWND)lParam, (HDC)wParam);







    case WM_MOUSEMOVE: {







        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };







        TrackMouseEvent(&tme);







        int cmd = 0, layer = 0;







        bool hit = PointInHit((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), cmd, layer);







        if (hit && app.overlay != OV_NONE && layer == 0) hit = false;   // 弹层打开时不悬停背后的日历







        int newHover = hit ? cmd : 0;







        if (newHover != g_hoverCmd) { g_hoverCmd = newHover; InvalidateRect(hwnd, nullptr, FALSE); }







        return 0;







    }







    case WM_MOUSELEAVE:







        if (g_hoverCmd) { g_hoverCmd = 0; InvalidateRect(hwnd, nullptr, FALSE); }







        return 0;







    case WM_SETCURSOR:







        if (LOWORD(lParam) == HTCLIENT) {







            SetCursor(g_hoverCmd ? LoadCursorW(nullptr, IDC_HAND) : LoadCursorW(nullptr, IDC_ARROW));







            return TRUE;







        }







        break;







    case WM_LBUTTONDOWN: {
        if (app.overlay == OV_NONE && g_suppressClickUntil && GetTickCount() < g_suppressClickUntil) {
            g_shiftAtPress = false;
            g_ctrlAtPress = false;
            return 0;
        }
        g_shiftAtPress = ((GetAsyncKeyState(VK_SHIFT) | GetKeyState(VK_SHIFT)) & 0x8000) != 0;
        g_ctrlAtPress = ((GetAsyncKeyState(VK_CONTROL) | GetKeyState(VK_CONTROL)) & 0x8000) != 0;







        SetFocus(hwnd);







        break;







    }







    case WM_LBUTTONUP: {
        if (g_swallowNextUp) {
            g_swallowNextUp = false;
            if (GetTickCount() - g_swallowTick < 500) return 0;
        }
        if (app.overlay == OV_NONE && g_suppressClickUntil && GetTickCount() < g_suppressClickUntil) return 0;
        OverlayKind ovBeforeClick = app.overlay;







        if (HWND se = GetDlgItem(hwnd, 18001)) { if (GetFocus() == se) SetFocus(hwnd); }







        app.searchFocused = false;







        float x = (float)GET_X_LPARAM(lParam), y = (float)GET_Y_LPARAM(lParam);







        int cmd = 0, layer = 0;







        if (PointInHit(x, y, cmd, layer)) {







            if (app.overlay != OV_NONE && layer == 0) return 0;   // 关键修复：弹层打开时点击不穿透到日历














            Hit h;







            for (auto& cand : g_hits) {







                if (cand.cmd == cmd && cand.layer == layer &&







                    x >= cand.r.X && x <= cand.r.X + cand.r.Width && y >= cand.r.Y && y <= cand.r.Y + cand.r.Height) {







                    h = cand; break;







                }







            }







            // 非列表视图：Ctrl/Shift+点击日程块 = 多选（恢复旧行为）
            if (app.overlay == OV_NONE && app.view != VIEW_AGENDA && MultiSelectPressed() &&
                cmd >= CMD_EVENT_BASE && cmd < CMD_EVENT_BASE + 5000) {
                int oi = cmd - CMD_EVENT_BASE;
                if (oi >= 0 && oi < (int)app.occs.size()) {
                    ToggleSelected(OccKey(app.occs[oi]));
                    return 0;
                }
            }
            if (h.fn) h.fn();
        // 批量工具栏按钮：吞掉紧随其后的双击第二下，避免工具栏消失后命中下面列表行打开编辑器
        if (cmd >= CMD_AGENDA_SELALL && cmd <= CMD_AGENDA_COLOR + 8)
            g_suppressDblclkUntil = GetTickCount() + 550;







            UIInvalidate();







        } else if (app.overlay == OV_SLASH) {
            CloseSlashPalette();
        } else if (app.overlay == OV_MENU) {







            app.overlay = OV_NONE;







            app.menu.open = false;







            UIInvalidate();







        } else if (!app.selected.empty()) {







            app.selected.clear();







            UIInvalidate();







        }







        if (ovBeforeClick != OV_NONE && app.overlay == OV_NONE) g_suppressClickUntil = GetTickCount() + 280;
        g_shiftAtPress = false;
        g_ctrlAtPress = false;
        return 0;







    }







    case WM_LBUTTONDBLCLK: {
        g_swallowNextUp = true;
        g_swallowTick = GetTickCount();
        g_shiftAtPress = false;
        g_ctrlAtPress = false;
        if (app.overlay == OV_NONE && g_suppressClickUntil && GetTickCount() < g_suppressClickUntil) return 0;
        if (app.overlay != OV_NONE) return 0;
        float x = (float)GET_X_LPARAM(lParam), y = (float)GET_Y_LPARAM(lParam);
        // 缩放控件上的双击（连续点 +/- 的第二次）不触发新建
        if ((app.view == VIEW_WEEK || app.view == VIEW_DAY) && WeekViewInZoomControl(x, y)) return 0;

        // 统一优先级：日程块 → 编辑；控件 → 忽略；日期格/空白 → 新建
        int cmd = 0, layer = 0;
        if (PointInHit(x, y, cmd, layer)) {
            if (cmd >= CMD_SELECTED_BASE && cmd < CMD_SELECTED_BASE + 5000) {
                g_swallowNextUp = false;   // 双击完成方框：允许第二下切换
                return 0;
            }
            if (cmd >= CMD_EVENT_BASE && cmd < CMD_EVENT_BASE + 5000) {
                int oi = cmd - CMD_EVENT_BASE;
                if (oi >= 0 && oi < (int)app.occs.size()) {
                    OpenEditorForOccurrence(app.occs[oi]);
                    return 0;
                }
            }
            if (cmd < CMD_DAY_BASE || cmd >= CMD_DAY_BASE + 150000) return 0;  // 按钮/方框等控件
            Date d = FromSerial(cmd - CMD_DAY_BASE);
            OpenEditorNew(MakeDT(d.y, d.m, d.d), 9, 0);
            return 0;
        }
        if (app.view == VIEW_WEEK || app.view == VIEW_DAY) {
            DateTime day;
            int minute = 0;
            if (WeekViewHitTime(x, y, day, minute)) {
                app.lastClickMin = minute;
                app.lastClickDay = day;
                OpenEditorNew(day, minute / 60, minute % 60);
                return 0;
            }
        }
        if (app.view == VIEW_MONTH) {
            DateTime day;
            if (MonthDateAtPoint(x, y, day)) OpenEditorNew(day, 9, 0);
        }
        return 0;
    }







    case WM_RBUTTONUP: {







        if (app.overlay != OV_NONE) return 0;







        float x = (float)GET_X_LPARAM(lParam), y = (float)GET_Y_LPARAM(lParam);







        int cmd = 0, layer = 0;







        bool handled = false;







        bool gotHit = PointInHit(x, y, cmd, layer);







        if (gotHit) {







            if (cmd >= CMD_EVENT_BASE && cmd < CMD_EVENT_BASE + 5000) {



                int ei = cmd - CMD_EVENT_BASE;



                if (ei >= 0 && ei < (int)app.occs.size()) { ShowEventMenu(ei); handled = true; }



            }







            else if (cmd >= CMD_DAY_BASE && cmd < CMD_DAY_BASE + 150000) { ShowDayMenu(cmd - CMD_DAY_BASE); handled = true; }







            else if (cmd == CMD_WATER || (cmd >= CMD_WATER_SET_BASE && cmd < CMD_WATER_SET_BASE + 40)) { ShowWaterMenu(); handled = true; }







            else if (cmd >= CMD_YEAR_DAY_BASE && cmd < CMD_YEAR_DAY_BASE + 400) {







                DateTime day = AddDaysDT(MakeDT(app.cursor.y, 1, 1), cmd - CMD_YEAR_DAY_BASE);







                OpenDayDetail(day);







                handled = true;







            }







        }







        // 兜底：月视图任意日期格（包括没有日程的空白格或格线边缘）右键都打开当天详情



        // 兜底：月视图任意日期格（含空白格/格线边缘）右键打开当天详情；



        // 周/日视图空白时间段右键也打开当天详情。



        if (!handled) {



            if (app.view == VIEW_MONTH) {



                DateTime day;



                if (MonthDateAtPoint(x, y, day)) ShowDayMenu(DaySerial(day.y, day.mo, day.d));



            } else if (app.view == VIEW_WEEK || app.view == VIEW_DAY) {



                DateTime wd; int wmin = 0;



                if (WeekViewHitTime(x, y, wd, wmin)) ShowDayMenu(DaySerial(wd.y, wd.mo, wd.d));



            }



        }







        return 0;







    }







    case WM_MOUSEWHEEL: {
            if (app.overlay == OV_SETTINGS) {
                int n = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                app.settingsScroll = std::max(0.0f, app.settingsScroll - n * S(70));
                UIInvalidate();
                return 0;
            }
            if (app.overlay == OV_SLASH) { SlashScroll(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA); return 0; }
                if (app.overlay == OV_CMDK) { CmdkScroll(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA); return 0; }


        {

            POINT cpt{}; GetCursorPos(&cpt); ScreenToClient(hwnd, &cpt);

            if (app.overlay == OV_NONE && cpt.x < S(SIDEBAR_W) && cpt.y >= g_tagRegionTop && cpt.y <= g_tagRegionBottom) {

                int n = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;

                int total = (int)app.store.tags.size();

                app.tagScroll = std::max(0, std::min(app.tagScroll - n, std::max(0, total - g_tagVisibleRows)));

                UIInvalidate();

                return 0;

            }

        }

    







        if (app.overlay == OV_FONTS && app.fontPickerOpen) {







            int n = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;







            app.fontPickerScroll = std::max(0, app.fontPickerScroll - n * 3);







            InvalidateRect(hwnd, nullptr, FALSE);







            return 0;







        }







        if (app.overlay == OV_SERIESCOLOR) {







            int n = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;







            app.seriesColor.scroll = std::max(0, app.seriesColor.scroll - n * 3);







            InvalidateRect(hwnd, nullptr, FALSE);







            return 0;







        }







        if (app.overlay != OV_NONE) return 0;







        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };







        ScreenToClient(hwnd, &pt);







        int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;







        if (pt.x < (int)S(SIDEBAR_W)) break;

        if ((app.view == VIEW_WEEK || app.view == VIEW_DAY) && (LOWORD(wParam) & MK_CONTROL)) {
            SetTimeZoom(notches > 0 ? 10 : -10);
            break;
        }







        if (app.view == VIEW_MONTH) {







            app.cursor = AddMonthsDT(app.cursor, -notches);







            InvalidateOccs();







        } else if (app.view == VIEW_YEAR) {







            app.cursor = MakeDT(app.cursor.y - notches, app.cursor.mo, 1);







        } else if (app.view == VIEW_WEEK || app.view == VIEW_DAY) {







            app.scrollMin -= notches * 60;







            app.scrollMin = std::max(0, std::min(24 * 60, app.scrollMin));







        } else {







            app.agendaScroll -= notches * 120;







            app.agendaScroll = std::max(0, app.agendaScroll);







        }







        InvalidateRect(hwnd, nullptr, FALSE);







        return 0;







    }







    case WM_INPUT: {



        if (!g_rawKeyboard) break;



        UINT size = 0;



        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));



        if (size == 0 || size > 1024) break;



        BYTE buf[1024];



        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size) break;



        RAWINPUT* ri = (RAWINPUT*)buf;



        if (ri->header.dwType != RIM_TYPEKEYBOARD) break;



        USHORT vk = ri->data.keyboard.VKey;



        bool up = (ri->data.keyboard.Flags & RI_KEY_BREAK) != 0;



        // 焦点兜底：主视图下若焦点残留在已隐藏的输入框，把焦点还给主窗口，避免方向键失灵
        if (!up && app.overlay == OV_NONE && !app.searchEditing &&
            GetForegroundWindow() == g_hwnd && GetFocus() != g_hwnd) {
            HWND pf = GetFocus();
            if (!pf || !IsWindowVisible(pf)) SetFocus(g_hwnd);
        }
        if (!up && app.overlay == OV_NONE && !app.searchEditing &&



            GetForegroundWindow() == g_hwnd && GetFocus() == g_hwnd) {



            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;



            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;



            if (vk == VK_LEFT || vk == VK_RIGHT) {



                if (app.view == VIEW_YEAR) app.cursor = MakeDT(app.cursor.y + (vk == VK_RIGHT ? 1 : -1), app.cursor.mo, 1);



                else if (app.view == VIEW_MONTH) app.cursor = AddMonthsDT(app.cursor, vk == VK_RIGHT ? 1 : -1);



                else if (app.view == VIEW_WEEK) app.cursor = AddDaysDT(app.cursor, vk == VK_RIGHT ? 7 : -7);



                else if (app.view == VIEW_DAY) app.cursor = AddDaysDT(app.cursor, vk == VK_RIGHT ? 1 : -1);



                else app.cursor = AddDaysDT(app.cursor, vk == VK_RIGHT ? 7 : -7);



                ++g_rawNavCount;



                InvalidateOccs();



                UIInvalidate();



                return 0;



            }



            if (!ctrl && !alt) {



                if (vk == VK_OEM_2 || vk == VK_DIVIDE) { OpenSlashPalette(); return 0; }

                if (HandleHotkeys(vk, ctrl, alt, false)) return 0;



















            }



        }



        break;



    }







    case WM_KEYDOWN: {







                     // IME 激活时方向键/字母会变成 VK_PROCESSKEY，取回原始虚拟键再处理



                     if (wParam == VK_PROCESSKEY) {



                         UINT realVk = ImmGetVirtualKey(hwnd);



                                      if (realVk == 0 || realVk == VK_PROCESSKEY) return 0;



                         wParam = realVk;



                     }







        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;



        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;



        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;



        if (app.hotkeysCapturing) {
            HandleHotkeyCapture(wParam, ctrl, alt, shift);
            return 0;
        }
        if (wParam == VK_F10) { OpenHotkeys(); return 0; }
        if (HandleHotkeys(wParam, ctrl, alt, shift)) return 0;
        // 无障碍键盘导航：Tab / Shift+Tab 移动焦点，Enter 激活，Esc 取消
        if (app.overlay == OV_NONE && !app.searchEditing && !ctrl && !alt) {
            if (wParam == VK_TAB) { UIFocusNext((shift != 0)); return 0; }
            if (wParam == VK_RETURN && UIFocusCmd() != 0) {
                if (UIActivateFocus()) return 0;
            }
            if (wParam == VK_ESCAPE && UIFocusCmd() != 0) { UIClearFocus(); return 0; }
        }

        if (CmdkHotkeyMatches(wParam, ctrl, alt, shift)) {



            if (app.overlay == OV_CMDK) CloseCommandPalette();



            else OpenCommandPalette();



            return 0;



        }







        if (HandleOverlayKey(wParam)) return 0;














if (ctrl && wParam == 'G') { OpenDateJump(); return 0; }







if (ctrl && wParam == 'A' && app.view == VIEW_AGENDA) {







    for (auto& o : app.occs) if (OccMatchesFilter(o)) app.selected.insert(OccKey(o));







    DebugLog(L"agenda select all count=%d", (int)app.selected.size());







    UIInvalidate();







    return 0;







}







        if (ctrl && wParam == 'Z') { UndoAction(); return 0; }







        if (ctrl && wParam == 'Y') { RedoAction(); return 0; }







        if (ctrl && shift && wParam == 'T') { OpenThemeStudio(); return 0; }



        if (ctrl && shift && wParam == 'S') { OpenDbManager(); return 0; }



        if (ctrl && shift && wParam == 'M') { OpenSyncCenter(); return 0; }



        if (ctrl && wParam == 'I') { OpenSmartImportFromFile(); return 0; }



        



        if (ctrl && wParam == 'S') { DoSaveStore(); ShowToast(L"已保存"); return 0; }










        // 搜索框右侧的 / 提示：按 / 直接进入搜索（与 Ctrl+F 等价）

        // / 打开斜杠指令面板（搜索仍用 Ctrl+F）
        if (!ctrl && !alt && (wParam == VK_OEM_2 || wParam == VK_DIVIDE) && app.overlay == OV_NONE) {
            OpenSlashPalette();
            return 0;
        }







        switch (wParam) {







        case VK_ESCAPE:







            if (app.overlay != OV_NONE) CloseOverlay();







            else if (!app.search.empty() || app.searchEditing) { app.search.clear(); app.searchIntent = false; app.searchEditing = false; AppEnableIme(false); SetFocus(g_hwnd); UIInvalidate(); }







            return 0;

















































        case VK_F1: OpenHelp(); return 0;



        case VK_F2: OpenStats(1, app.cursor); return 0;



        case VK_F3: app.overlay = OV_NONE; app.view = VIEW_AGENDA; app.agendaBoard = true; app.selected.clear(); InvalidateOccs(); UIInvalidate(); return 0;



        case VK_F4: OpenExamPanel(); return 0;

        case VK_F11: OpenGrades(); return 0;



        case VK_F5: OpenFocusPanel(); return 0;



        case VK_F6: OpenHabitsPanel(); return 0;



        case VK_F7: OpenPrintPanel(); return 0;



        case VK_F8: OpenProfilesPanel(); return 0;



        case VK_F9: OpenReminderCenter(); return 0;







        case VK_LEFT:







            if (g_rawKeyboard) return 0;







            if (app.view == VIEW_YEAR) app.cursor = MakeDT(app.cursor.y - 1, app.cursor.mo, 1);







            else if (app.view == VIEW_MONTH) app.cursor = AddMonthsDT(app.cursor, -1);







            else if (app.view == VIEW_WEEK) app.cursor = AddDaysDT(app.cursor, -7);







            else if (app.view == VIEW_DAY) app.cursor = AddDaysDT(app.cursor, -1);







            else app.cursor = AddDaysDT(app.cursor, -7);







            InvalidateOccs(); UIInvalidate(); return 0;







        case VK_RIGHT:







            if (g_rawKeyboard) return 0;







            if (app.view == VIEW_YEAR) app.cursor = MakeDT(app.cursor.y + 1, app.cursor.mo, 1);







            else if (app.view == VIEW_MONTH) app.cursor = AddMonthsDT(app.cursor, 1);







            else if (app.view == VIEW_WEEK) app.cursor = AddDaysDT(app.cursor, 7);







            else if (app.view == VIEW_DAY) app.cursor = AddDaysDT(app.cursor, 1);







            else app.cursor = AddDaysDT(app.cursor, 7);







            InvalidateOccs(); UIInvalidate(); return 0;







        case VK_DELETE: DeleteSelected(); return 0;







        }







        break;







    }







    case WM_CLOSE:







        if (!g_quitting && app.store.settings.closeToTray && !g_trayAdded) {
        AddTrayIcon();
    }
    if (!g_quitting && app.store.settings.closeToTray) {







            HideMainWindowToTray();







            return 0;







        }







        break;







    case WM_TRAYICON_MSG:







        switch (LOWORD(lParam)) {







        case WM_LBUTTONUP:







        case WM_LBUTTONDBLCLK:







        case NIN_BALLOONUSERCLICK:







            ShowMainWindowNow();







            break;







        case WM_RBUTTONUP:







        case WM_CONTEXTMENU:







            ShowTrayMenu();







            break;







        default: break;







        }







        return 0;







    case WM_DESTROY:







        RemoveTrayIcon();







        BotShutdown();

        OnAppClose();







        PostQuitMessage(0);







        return 0;







    }







    return DefWindowProcW(hwnd, msg, wParam, lParam);







}















// ---------------- 示例数据 ----------------







void SeedDemoData(Store& st) {







    st.tags = DefaultTags();







    st.settings.seededSample = true;







    st.settings.theme = L"astral";







    DateTime semStart = MakeDT(2026, 9, 7);







    auto addSeries = [&](const std::wstring& title, int wd, int h, int mi, int dur, const char* tag, bool alarm) {







        DateTime first = semStart;







        while (DayOfWeek(first.y, first.mo, first.d) != wd) first = AddDaysDT(first, 1);







        Event e;







        e.title = title;







        e.start = MakeDT(first.y, first.mo, first.d, h, mi);







        e.end = AddMinutesDT(e.start, dur);







        e.tags.push_back(tag);







        e.recur.freq = RF_WEEKLY;







        e.recur.interval = 1;







        e.recur.byDay = { wd };







        e.recur.until = MakeDT(2027, 1, 10);







        e.recur.hasUntil = true;







        if (alarm) e.alarms.push_back(Alarm{ -10, true, L"" });







        e.source = L"示例";







        st.AddEvent(e);







    };







    addSeries(L"高等数学", 1, 8, 0, 100, "course", true);







    addSeries(L"高等数学", 3, 8, 0, 100, "course", true);







    addSeries(L"大学英语", 1, 14, 0, 100, "course", false);







    addSeries(L"大学物理", 2, 10, 0, 100, "course", true);







    addSeries(L"大学物理", 4, 10, 0, 100, "course", true);







    addSeries(L"体育", 2, 16, 0, 100, "fitness", false);







    addSeries(L"电路实验", 3, 14, 0, 150, "lab", false);







    addSeries(L"程序设计基础", 5, 8, 0, 100, "course", true);







    addSeries(L"图书馆自习", 4, 19, 0, 90, "study", false);







    addSeries(L"社团例会", 5, 19, 0, 120, "club", false);







    addSeries(L"健身", 6, 16, 0, 60, "fitness", false);







    DateTime t = Today();







    auto oneOff = [&](const std::wstring& title, DateTime start, int dur, const char* tag, int prio, bool allDay) {







        Event e;







        e.title = title;







        e.start = start;







        e.end = allDay ? AddDaysDT(StartOfDay(start), 1) : AddMinutesDT(start, dur);







        e.allDay = allDay;







        e.tags.push_back(tag);







        e.priority = prio;







        e.source = L"示例";







        if (!allDay) e.alarms.push_back(Alarm{ -15, true, L"" });







        st.AddEvent(e);







    };







    oneOff(L"小组作业讨论", MakeDT(t.y, t.mo, t.d, 15, 0), 120, "group", PRIO_MED, false);







    DateTime examDay = AddDaysDT(t, 3);







    oneOff(L"物理期中考试", MakeDT(examDay.y, examDay.mo, examDay.d, 19, 0), 90, "exam", PRIO_HIGH, false);







    DateTime hwDay = AddDaysDT(t, 4);







    oneOff(L"高数作业提交", MakeDT(hwDay.y, hwDay.mo, hwDay.d, 22, 0), 0, "homework", PRIO_HIGH, true);







    DateTime bday = AddDaysDT(t, 8);







    oneOff(L"同学生日", MakeDT(bday.y, bday.mo, bday.d, 0, 0), 0, "birthday", PRIO_NONE, true);







    DateTime ddl = AddDaysDT(t, 11);







    oneOff(L"四级报名截止", MakeDT(ddl.y, ddl.mo, ddl.d, 0, 0), 0, "exam", PRIO_MED, true);







    st.water[ISODate(t)] = 6;







}























static void WaitForPreviousInstanceIfNeeded() {







    int argc = 0;







    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);







    if (!argv) return;







    DWORD pid = 0;







    for (int i = 1; i + 1 < argc; ++i) {







        if (std::wstring(argv[i]) == L"--wait-pid") { pid = (DWORD)_wtoi(argv[i + 1]); break; }







    }







    LocalFree(argv);







    if (pid == 0) return;







    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);







    if (!h) return;







    WaitForSingleObject(h, 15000);







    CloseHandle(h);







}















static bool TryRestoreLatestBackup(Store& st) {







    std::wstring bakDir = PathJoin(GetDataDir(), L"backup");







    std::vector<std::wstring> files = ListFiles(bakDir, L".json");







    std::wstring best;







    size_t bestEvents = 0;







    ULONGLONG bestTime = 0;







    for (auto& f : files) {







        WIN32_FILE_ATTRIBUTE_DATA ad{};







        ULONGLONG t = 0;







        if (GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &ad))







            t = ((ULONGLONG)ad.ftLastWriteTime.dwHighDateTime << 32) | ad.ftLastWriteTime.dwLowDateTime;







        bool ok = false;







        std::string data = ReadFileBytes(f, &ok);







        if (!ok || data.empty()) continue;







        Store tmp;







        std::wstring err;







        if (!tmp.FromJsonStr(data, &err)) continue;







        if (tmp.events.size() < 2) continue;   // 跳过只有欢迎页的覆盖文件







        if (tmp.events.size() > bestEvents || (tmp.events.size() == bestEvents && t > bestTime)) {







            best = f;







            bestEvents = tmp.events.size();







            bestTime = t;







        }







    }







    if (best.empty()) return false;







    bool ok = false;







    std::string data = ReadFileBytes(best, &ok);







    if (!ok) return false;







    std::wstring err;







    if (!st.FromJsonStr(data, &err)) return false;







    DebugLog(L"restored backup: %s", best.c_str());







    return true;







}







static void SeedWelcome(Store& st) {







    st.tags = DefaultTags();







    Event e;







    e.title = L"欢迎使用 CAELENDAR";







    e.start = MakeDT(app.now.y, app.now.mo, app.now.d, 20, 0);







    e.end = AddMinutesDT(e.start, 30);







    e.tags.push_back("idea");







    e.desc = L"在月视图里：单击选中日期、双击空白新建日程；右键日程可以复制/删除；顶部「新建」或 Ctrl+N 创建；"







             L"试试「更多 → 快速添加」输入“明天 15:00 高数作业 #作业”。这条示例日程可以直接删掉。";







    e.alarms.push_back(Alarm{ -10, true, L"" });







    st.AddEvent(e);







    st.settings.seededSample = true;







}















// ---------------- 关闭 ----------------







void OnAppClose() {







    StopReminderEngine();







    DestroyWidgetWindow();







    if (!g_noSave) app.store.Save();







    KillTimer(g_hwnd, 1);







}















// ---------------- 入口 ----------------







static void SetDpiAwareness() {







    HMODULE user32 = GetModuleHandleW(L"user32.dll");







    if (user32) {







        typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(HANDLE);







        auto fn = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(user32, "SetProcessDpiAwarenessContext");







        if (fn && fn((HANDLE)-4)) return;







        typedef BOOL(WINAPI* PFN_SetProcessDPIAware)();







        auto fn2 = (PFN_SetProcessDPIAware)GetProcAddress(user32, "SetProcessDPIAware");







        if (fn2) fn2();







    }







}















int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {







    {
        const wchar_t* cl = GetCommandLineW();
        g_testMode = (wcsstr(cl, L"--probe-") != nullptr) || (wcsstr(cl, L"--selftest") != nullptr);
        g_probeWidgetTest = (wcsstr(cl, L"--probe-widget") != nullptr);
    }

    HANDLE singleMutex = CreateMutexW(nullptr, TRUE, L"XingLi_SingleInstance_Mutex");







    g_singleMutex = singleMutex;







    bool noSingleton = wcsstr(GetCommandLineW(), L"--no-singleton") != nullptr || wcsstr(GetCommandLineW(), L"--restarted") != nullptr;   // 重启/测试允许并行







    if (!noSingleton && singleMutex && GetLastError() == ERROR_ALREADY_EXISTS) {







        bool autoStartLaunch = wcsstr(GetCommandLineW(), L"--autostart") != nullptr;







        HWND old = FindWindowW(L"XingLiMain", nullptr);







        if (old && !autoStartLaunch) {







            ShowWindow(old, SW_SHOW);







            ShowWindow(old, SW_RESTORE);







            SetForegroundWindow(old);







        }







        return 0;







    }







    int argc = 0;







    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);







    for (int i = 1; i < argc; ++i) {







        std::wstring a = argv[i];







        if (a == L"--shot" && i + 1 < argc) { g_shotPath = argv[++i]; g_shotMode = true; g_noSave = true; }







        else if (a == L"--theme" && i + 1 < argc) { g_themeArg = argv[++i]; app.store.settings.theme = g_themeArg; }







        else if (a == L"--view" && i + 1 < argc) {







            std::wstring v = argv[++i];







            if (v == L"month") app.view = VIEW_MONTH;







            else if (v == L"week") app.view = VIEW_WEEK;







            else if (v == L"day") app.view = VIEW_DAY;







            else if (v == L"year") app.view = VIEW_YEAR;







            else app.view = VIEW_AGENDA;







        }







        else if (a == L"--demo") g_demoMode = true;



        else if (a == L"--board") g_boardDemo = true;







        else if (a == L"--smart-organize") g_smartCli = true;







        else if (a == L"--probe-next") g_probeNext = true;







        else if (a == L"--probe-agenda") g_probeAgenda = true;







        else if (a == L"--agenda-select-all") g_agendaSelectAll = true;







        else if (a == L"--probe-cleanup") g_probeCleanup = true;







        else if (a == L"--probe-opacity" && i + 1 < argc) g_probeOpacity = _wtoi(argv[++i]);







        else if (a == L"--toast-text" && i + 1 < argc) g_toastTextArg = argv[++i];







        else if (a == L"--probe-selectall-delete") g_probeSelectAllDelete = true;







        else if (a == L"--probe-bulk-dedupe") g_probeBulkDedupe = true;







        else if (a == L"--probe-import-series") g_probeImportSeries = true;







        else if (a == L"--probe-widget-refresh") g_probeWidgetRefresh = true;







        else if (a == L"--probe-reschedule") g_probeReschedule = true;







        else if (a == L"--probe-shift-select") g_probeShiftSelect = true;







        else if (a == L"--probe-done-edit") g_probeDoneEdit = true;







        else if (a == L"--select-n" && i + 1 < argc) g_selectN = _wtoi(argv[++i]);







        else if (a == L"--shot-late" && i + 1 < argc) g_lateShotPath = argv[++i];







        else if (a == L"--late-reschedule") g_lateReschedule = true;







        else if (a == L"--keep-open") g_keepOpen = true;







        else if (a == L"--probe-year") g_probeYear = true;







        else if (a == L"--probe-restart") g_probeRestart = true;







        else if (a == L"--probe-search") g_probeSearch = true;







        else if (a == L"--probe-wheel") g_probeWheel = true;







        else if (a == L"--probe-editor-input") g_probeEditorInput = true;







        else if (a == L"--probe-datejump") g_probeDateJump = true;







        else if (a == L"--probe-data") g_probeData = true;







        else if (a == L"--probe-restore") g_probeRestore = true;







        else if (a == L"--probe-solar") g_probeSolar = true;







        else if (a == L"--probe-lunar-editor") g_probeLunarEditor = true;







        else if (a == L"--probe-daydetail") g_probeDayDetail = true;
        else if (a == L"--probe-clip") g_probeClip = true;
        else if (a == L"--probe-done") g_probeDone = true;
        else if (a == L"--probe-multiselect") g_probeMulti = true;
        else if (a == L"--probe-misclick") g_probeMisclick = true;
        else if (a == L"--probe-bulk-day") g_probeBulkDay = true;
        else if (a == L"--probe-quick") g_probeQuick = true;
        else if (a == L"--probe-agenda-filter") g_probeAgendaFilter = true;
        else if (a == L"--probe-clip-history") g_probeClipHistory = true;
        else if (a == L"--probe-conflict") g_probeConflict = true;
        else if (a == L"--probe-multiselect2") g_probeMulti2 = true;
        else if (a == L"--probe-templates") g_probeTemplates = true;
        else if (a == L"--probe-shift-order") g_probeShiftOrder = true;
        else if (a == L"--probe-account-sync") g_probeAccountSync = true;
        else if (a == L"--probe-tags-ui") g_probeTagsUi = true;
        else if (a == L"--probe-account-ui") g_probeAccountUi = true;
        else if (a == L"--probe-autobackup") g_probeAutoBackup = true;
        else if (a == L"--probe-recur-preview") g_probeRecurPreview = true;
        else if (a == L"--probe-multi-ddl") g_probeMultiDdl = true;
        else if (a == L"--probe-bot") g_probeBot = true;
        else if (a == L"--probe-ddl-click") g_probeDdlClick = true;
        else if (a == L"--probe-ddl-point") g_probeDdlPoint = true;
        else if (a == L"--probe-board-all") g_probeBoardAll = true;
        else if (a == L"--probe-print-png") g_probePrintPng = true;
        else if (a == L"--probe-multiselect3") g_probeMulti3 = true;
        else if (a == L"--probe-ddl-view") g_probeDdlView = true;
        else if (a == L"--probe-ringtone") g_probeRingtone = true;
        else if (a == L"--probe-multiselect4") g_probeMulti4 = true;
        else if (a == L"--probe-stats-tags") g_probeStatsTags = true;
        else if (a == L"--probe-toolbar") g_probeToolbar = true;
        else if (a == L"--probe-hotkeys") g_probeHotkeys = true;
        else if (a == L"--probe-multi-real") g_probeMultiReal = true;
        else if (a == L"--probe-multi-view") g_probeMultiView = true;
        else if (a == L"--probe-grades") g_probeGrades = true;
        else if (a == L"--probe-merge") g_probeMerge = true;
        else if (a == L"--probe-tombstone") g_probeTombstone = true;
        else if (a == L"--probe-tag-btn") g_probeTagBtn = true;
        else if (a == L"--probe-hc") g_probeHC = true;
        else if (a == L"--probe-bot2") g_probeBot2 = true;
        else if (a == L"--probe-kb-nav") g_probeKbNav = true;
        else if (a == L"--probe-health") g_probeHealth = true;
        else if (a == L"--probe-month-detail") g_probeMonthDetail = true;
        else if (a == L"--probe-a11y") g_probeA11y = true;
        else if (a == L"--probe-grades-sync") g_probeGradesSync = true;



        else if (a == L"--probe-inputfix") g_probeInputFix = true;



        else if (a == L"--probe-focusstart") g_probeFocusStart = true;



        else if (a == L"--probe-help") g_probeHelp = true;



        else if (a == L"--probe-rawkeys") g_probeRawKeys = true;



        else if (a == L"--probe-alarm") g_probeAlarm = true;

        else if (a == L"--probe-tags") g_probeTags = true;

        else if (a == L"--probe-slash") g_probeSlash = true;
        else if (a == L"--probe-slashshot") g_probeSlashShot = true;
        else if (a == L"--probe-slashui") g_probeSlashUi = true;
        else if (a == L"--probe-share") g_probeShare = true;
        else if (a == L"--probe-sharepanel") g_probeSharePanel = true;
        else if (a == L"--probe-cmdkshot") g_probeCmdkShot = true;
        else if (a == L"--probe-helpshot") g_probeHelpShot = true;
        else if (a == L"--probe-cmdkwheel") g_probeCmdkWheel = true;
        else if (a == L"--share-port" && i + 1 < argc) g_sharePort = _wtoi(argv[++i]);

        else if (a == L"--probe-tagui") g_probeTagUi = true;



        else if (a == L"--probe-board") g_probeBoard = true;



        else if (a == L"--probe-version") g_probeVersion = true;



        else if (a == L"--probe-backup") g_probeBackup = true;



        else if (a == L"--probe-widget") g_probeWidget = true;



        else if (a == L"--probe-reminder") g_probeReminder = true;



        else if (a == L"--probe-theme") g_probeTheme = true;



        else if (a == L"--probe-theme-io") g_probeThemeIo = true;



        else if (a == L"--probe-cmdk") g_probeCmdk = true;



        else if (a == L"--probe-cmdk-ui") g_probeCmdkUi = true;



        else if (a == L"--probe-import2") g_probeImport2 = true;



        else if (a == L"--probe-import2-ui") g_probeImport2Ui = true;



        else if (a == L"--probe-ocr") g_probeOcr = true;



        else if (a == L"--probe-subscription") g_probeSubscription = true;



        else if (a == L"--probe-sqlite-enable") g_probeSqliteEnable = true;



        else if (a == L"--probe-sqlite-read") g_probeSqliteRead = true;



        else if (a == L"--probe-sqlite-rollback") g_probeSqliteRollback = true;



        else if (a == L"--probe-sync") g_probeSync = true;



        else if (a == L"--probe-sync-status") g_probeSyncStatus = true;



        else if (a == L"--probe-corrupt-json") g_probeCorruptJson = true;



        else if (a == L"--probe-sqlite-fallback") g_probeSqliteFallback = true;



        else if (a == L"--probe-exams") g_probeExams = true;



        else if (a == L"--probe-focus") g_probeFocus = true;



        else if (a == L"--probe-focus-timer") g_probeFocusTimer = true;



        else if (a == L"--probe-habits") g_probeHabits = true;



        else if (a == L"--probe-print") g_probePrint = true;



        else if (a == L"--probe-profiles") g_probeProfiles = true;



        else if (a == L"--probe-override-dup") g_probeOverrideDup = true;



        else if (a == L"--sub-url" && i + 1 < argc) g_subUrlArg = argv[++i];



        else if (a == L"--rollback-now" && i + 2 < argc) {



            g_rollbackTest = true;



            g_rollbackExe = argv[++i];



            g_rollbackData = argv[++i];



        }







        else if (a == L"--cleanup-keyword" && i + 1 < argc) g_cleanupKeywordArg = argv[++i];







        else if (a == L"--widget-mode" && i + 1 < argc) g_widgetModeArg = argv[++i];







        else if (a == L"--widget-skin" && i + 1 < argc) g_widgetSkinArg = argv[++i];







        else if (a == L"--widget-scale" && i + 1 < argc) g_widgetScaleArg = _wtoi(argv[++i]);







        else if (a == L"--widget-opacity" && i + 1 < argc) g_widgetOpacityArg = _wtoi(argv[++i]);







        else if (a == L"--widget-no-date") g_widgetDateArg = 0;







        else if (a == L"--widget-no-water") g_widgetWaterArg = 0;







        else if (a == L"--widget-no-today") g_widgetTodayArg = 0;







        else if (a == L"--autostart" || a == L"--hidden") g_startHidden = true;
        else if (a == L"--offscreen") g_offscreen = true;







        else if (a == L"--import-file" && i + 1 < argc) {







            std::wstring f = argv[++i];







            bool ok = false;







            std::string data = ReadFileBytes(f, &ok);







            if (ok) {







                app.import.open = true;







                app.import.file = f;







                app.import.parsed = ParseIcs(data);







                app.import.mode = 0;







                app.overlay = OV_IMPORT;







            }







        }







        else if (a == L"--widget-shot" && i + 1 < argc) { g_widgetShotPath = argv[++i]; g_noSave = true; }



        else if (a == L"--cmdk-text" && i + 1 < argc) g_cmdkTextArg = argv[++i];







        else if (a == L"--reminder-shot" && i + 1 < argc) { g_reminderShotPath = argv[++i]; g_noSave = true; }







        else if (a == L"--overlay" && i + 1 < argc) g_startOverlay = argv[++i];







        else if (a == L"--w" && i + 1 < argc) g_forceW = _wtoi(argv[++i]);







        else if (a == L"--h" && i + 1 < argc) g_forceH = _wtoi(argv[++i]);







        else if (a == L"--selftest") {







            LocalFree(argv);







            EnsureDir(GetDataDir());







            return RunSelfTests();







        }







    }







    LocalFree(argv);















    SetDpiAwareness();







    {







        HDC screen = GetDC(nullptr);







        gfx::dpi = GetDeviceCaps(screen, LOGPIXELSX) / 96.0f;







        ReleaseDC(nullptr, screen);







    }







    GdiplusStartupInput gsi;







    GdiplusStartup(&g_gdiToken, &gsi, nullptr);







    gfx::Init();















    if (g_demoMode) {







        SeedDemoData(app.store);







        g_noSave = true;







        if (!g_themeArg.empty()) app.store.settings.theme = g_themeArg;







    } else {







        bool loaded = app.store.Load();







        bool looksOverwritten = app.store.settings.seededSample && app.store.events.size() <= 1 &&







            (app.store.events.empty() || Contains(app.store.events[0].title, L"欢迎使用 CAELENDAR"));







        if (!loaded || looksOverwritten) {







            if (TryRestoreLatestBackup(app.store)) {







                app.store.Save();







            } else {







                SeedWelcome(app.store);







                app.store.Save();







            }







    }







    }







    app.dataHealth = ValidateStoreData(app.store);



    DebugLog(L"数据校验：%s（%d 个系列）", app.dataHealth.c_str(), (int)app.store.events.size());



    if (!g_demoMode && !g_shotMode && !g_testMode) EnsureVersionSnapshot();







    if (!g_widgetModeArg.empty()) app.store.settings.widget.mode = g_widgetModeArg;







    if (!g_widgetSkinArg.empty()) app.store.settings.widget.skin = g_widgetSkinArg;







    if (g_widgetScaleArg > 0) app.store.settings.widget.scale = std::max(80, std::min(160, g_widgetScaleArg));







    if (g_widgetOpacityArg > 0) app.store.settings.widget.opacity = std::max(30, std::min(100, g_widgetOpacityArg));







    if (g_widgetDateArg == 0) app.store.settings.widget.showDate = false;







    if (g_widgetWaterArg == 0) app.store.settings.widget.showWater = false;







    if (g_widgetTodayArg == 0) app.store.settings.widget.showToday = false;







    ApplyThemeFromSettings();



    if (g_boardDemo) { app.view = VIEW_AGENDA; app.agendaBoard = true; }







    ApplyFontSettings();







    if (!g_demoMode && !g_shotMode && !g_testMode) SyncAutoStart();







    if (g_smartCli) {







        SmartOptions so;







        SmartResult sr = SmartOrganize(app.store, so);







        app.store.Save();







        DebugLog(L"smart-organize: %s", sr.summary.c_str());







        return 0;







    }







    app.cursor = Today();







    if (g_agendaSelectAll) {







        app.view = VIEW_AGENDA;







        auto occs = ExpandOccurrences(app.store, AddDaysDT(Today(), -1), AddDaysDT(Today(), 120));







        for (auto& o : occs) {







            if (o.ev.done || o.ev.status == ST_CANCELLED) continue;







            app.selected.insert(OccKey(o));







        }







        InvalidateOccs();







    }







    RefreshNow();















    WNDCLASSEXW wc{};







    wc.cbSize = sizeof(wc);







    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;







    wc.lpfnWndProc = WndProc;







    wc.hInstance = hInst;







    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);







    wc.lpszClassName = L"XingLiMain";







    RegisterClassExW(&wc);















    RECT work{};







    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);







    int logicW = g_forceW > 0 ? g_forceW : 1360;







    int logicH = g_forceH > 0 ? g_forceH : 860;







    int winW = (int)(logicW * gfx::dpi), winH = (int)(logicH * gfx::dpi);







    if (winW > (work.right - work.left) * 94 / 100) winW = (work.right - work.left) * 94 / 100;







    if (winH > (work.bottom - work.top) * 94 / 100) winH = (work.bottom - work.top) * 94 / 100;







    int x = work.left + (work.right - work.left - winW) / 2;







    int y = work.top + (work.bottom - work.top - winH) / 2;















    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    g_hwnd = CreateWindowExW(0, L"XingLiMain", L"CAELENDAR",







                             WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,







                             x, y, winW, winH, nullptr, nullptr, hInst, nullptr);







    if (!g_hwnd) { GdiplusShutdown(g_gdiToken); return 1; }







    RegisterRawKeyboard();
    ShareInit();
    if (g_probeShare) ShareProbeStart(g_sharePort);
    if (g_probeSharePanel) SetTimer(g_hwnd, 4985, 1200, nullptr);
    if (g_probeCmdkShot) SetTimer(g_hwnd, 4987, 1200, nullptr);
    if (g_probeHelpShot) SetTimer(g_hwnd, 4989, 1200, nullptr);
    if (g_probeCmdkWheel) SetTimer(g_hwnd, 4991, 800, nullptr);















    if (g_shotMode) {







        ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);







        UpdateWindow(g_hwnd);







    } else if (!g_startHidden) {







        ShowWindow(g_hwnd, SW_SHOW);







        UpdateWindow(g_hwnd);







        SetFocus(g_hwnd);







        AppEnableIme(false);







    }







    if (!g_shotMode && !g_testMode) {
        if (g_startHidden) {
            // 开机自启：桌面/任务栏可能还没就绪，延迟 1.5 秒挂托盘（失败还有 30 次重试）
            SetTimer(g_hwnd, kTrayRetryTimerId, 1500, nullptr);
        } else {
            AddTrayIcon();
        }
    }







    if (g_probeNext) SetTimer(g_hwnd, 11, 1200, nullptr);







    if (g_probeAgenda) SetTimer(g_hwnd, 13, 1200, nullptr);







    if (g_probeCleanup) SetTimer(g_hwnd, 21, 1200, nullptr);







    if (g_probeOpacity > 0) SetTimer(g_hwnd, 51, 1200, nullptr);







    if (!g_toastTextArg.empty()) ShowToast(g_toastTextArg, 60000);







    if (g_selectN > 0) {







        RefreshOccs();







        int n = 0;







        for (auto& o : app.occs) {







            app.selected.insert(OccKey(o));







            if (++n >= g_selectN) break;







        }







    }







    if (g_probeSelectAllDelete) SetTimer(g_hwnd, 71, 1200, nullptr);







    if (g_probeBulkDedupe) SetTimer(g_hwnd, 81, 1200, nullptr);







    if (g_probeImportSeries) SetTimer(g_hwnd, 91, 1500, nullptr);







    if (g_probeWidgetRefresh) SetTimer(g_hwnd, 101, 1500, nullptr);







    if (g_probeReschedule) SetTimer(g_hwnd, 111, 1500, nullptr);







    if (g_probeShiftSelect) SetTimer(g_hwnd, 121, 1500, nullptr);







    if (g_probeDoneEdit) SetTimer(g_hwnd, 131, 1500, nullptr);







    if (g_probeYear) SetTimer(g_hwnd, 1151, 1500, nullptr);







    if (g_probeRestart) SetTimer(g_hwnd, 1401, 1200, nullptr);







    if (g_probeSearch) SetTimer(g_hwnd, 1501, 1200, nullptr);







    if (g_probeWheel) SetTimer(g_hwnd, 1601, 1200, nullptr);







    if (g_probeEditorInput) SetTimer(g_hwnd, 1701, 1200, nullptr);







    if (g_probeDateJump) SetTimer(g_hwnd, 1801, 1200, nullptr);







    if (g_probeData) SetTimer(g_hwnd, 1901, 1200, nullptr);







    if (g_probeRestore) SetTimer(g_hwnd, 2001, 1200, nullptr);







    if (g_probeSolar) SetTimer(g_hwnd, 2101, 1200, nullptr);







    if (g_probeLunarEditor) SetTimer(g_hwnd, 2201, 1200, nullptr);



    if (g_probeDayDetail) SetTimer(g_hwnd, 2301, 1200, nullptr);
    if (g_probeClip) SetTimer(g_hwnd, 2410, 1200, nullptr);
    if (g_probeDone) SetTimer(g_hwnd, 2411, 1200, nullptr);
    if (g_probeMulti) SetTimer(g_hwnd, 2414, 1200, nullptr);
    if (g_probeMisclick) SetTimer(g_hwnd, 2416, 1200, nullptr);
    if (g_probeBulkDay) SetTimer(g_hwnd, 2419, 1200, nullptr);
    if (g_probeQuick) SetTimer(g_hwnd, 2420, 1200, nullptr);
    if (g_probeAgendaFilter) SetTimer(g_hwnd, 2421, 1200, nullptr);
    if (g_probeClipHistory) SetTimer(g_hwnd, 2423, 1200, nullptr);
    if (g_probeConflict) SetTimer(g_hwnd, 2425, 1200, nullptr);
    if (g_probeMulti2) SetTimer(g_hwnd, 2426, 1200, nullptr);
    if (g_probeTemplates) SetTimer(g_hwnd, 2429, 1200, nullptr);
    if (g_probeShiftOrder) SetTimer(g_hwnd, 2431, 1200, nullptr);
    if (g_probeAccountSync) SetTimer(g_hwnd, 2433, 1200, nullptr);
    if (g_probeTagsUi) SetTimer(g_hwnd, 2435, 1200, nullptr);
    if (g_probeAccountUi) SetTimer(g_hwnd, 2437, 1200, nullptr);
    if (g_probeAutoBackup) SetTimer(g_hwnd, 2439, 1200, nullptr);
    if (g_probeRecurPreview) SetTimer(g_hwnd, 2441, 1200, nullptr);
    if (g_probeMultiDdl) SetTimer(g_hwnd, 2443, 1200, nullptr);
    if (g_probeBot) SetTimer(g_hwnd, 2445, 1200, nullptr);
    if (g_probeDdlClick) SetTimer(g_hwnd, 2447, 1200, nullptr);
    if (g_probeDdlPoint) SetTimer(g_hwnd, 2449, 1200, nullptr);
    if (g_probeBoardAll) SetTimer(g_hwnd, 2451, 1200, nullptr);
    if (g_probePrintPng) SetTimer(g_hwnd, 2453, 1200, nullptr);
    if (g_probeMulti3) SetTimer(g_hwnd, 2455, 1200, nullptr);
    if (g_probeDdlView) SetTimer(g_hwnd, 2457, 1200, nullptr);
    if (g_probeRingtone) SetTimer(g_hwnd, 2459, 1200, nullptr);
    if (g_probeMulti4) SetTimer(g_hwnd, 2461, 1200, nullptr);
    if (g_probeStatsTags) SetTimer(g_hwnd, 2463, 1200, nullptr);
    if (g_probeToolbar) SetTimer(g_hwnd, 2465, 1200, nullptr);
    if (g_probeHotkeys) SetTimer(g_hwnd, 2467, 1200, nullptr);
    if (g_probeMultiReal) SetTimer(g_hwnd, 2469, 1200, nullptr);
    if (g_probeMultiView) SetTimer(g_hwnd, 2471, 1200, nullptr);
    if (g_probeGrades) SetTimer(g_hwnd, 2475, 1200, nullptr);
    if (g_probeMerge) SetTimer(g_hwnd, 2481, 1200, nullptr);
    if (g_probeTombstone) SetTimer(g_hwnd, 2483, 1200, nullptr);
    if (g_probeTagBtn) SetTimer(g_hwnd, 2485, 1200, nullptr);
    if (g_probeHC) SetTimer(g_hwnd, 2487, 1200, nullptr);
    if (g_probeBot2) SetTimer(g_hwnd, 2489, 1200, nullptr);
    if (g_probeKbNav) SetTimer(g_hwnd, 2491, 1200, nullptr);
    if (g_probeHealth) SetTimer(g_hwnd, 2493, 1200, nullptr);
    if (g_probeMonthDetail) SetTimer(g_hwnd, 2495, 1200, nullptr);
    if (g_probeA11y) SetTimer(g_hwnd, 2499, 1200, nullptr);
    if (g_probeGradesSync) SetTimer(g_hwnd, 2497, 1200, nullptr);



    if (g_probeInputFix) SetTimer(g_hwnd, 4901, 1400, nullptr);



    if (g_probeFocusStart) SetTimer(g_hwnd, 4902, 1400, nullptr);



    if (g_probeHelp) SetTimer(g_hwnd, 4950, 1200, nullptr);



    if (g_probeRawKeys) SetTimer(g_hwnd, 4960, 1200, nullptr);



    if (g_probeAlarm) SetTimer(g_hwnd, 4970, 1200, nullptr);

    if (g_probeTags) SetTimer(g_hwnd, 4980, 1200, nullptr);

    if (g_probeSlash) SetTimer(g_hwnd, 4990, 1200, nullptr);
    if (g_probeSlashShot) SetTimer(g_hwnd, 4990, 1200, nullptr);
    if (g_probeSlashUi) SetTimer(g_hwnd, 4994, 1200, nullptr);

    if (g_probeTagUi) SetTimer(g_hwnd, 4982, 1200, nullptr);







    if (g_probeBoard) SetTimer(g_hwnd, 2401, 1200, nullptr);



    if (g_probeVersion) SetTimer(g_hwnd, 2501, 1200, nullptr);



    if (g_probeBackup) SetTimer(g_hwnd, 2701, 1200, nullptr);



    if (g_probeWidget) SetTimer(g_hwnd, 2801, 1200, nullptr);



    if (g_probeReminder) SetTimer(g_hwnd, 2901, 1200, nullptr);



    if (g_probeTheme) SetTimer(g_hwnd, 3001, 1200, nullptr);



    if (g_probeThemeIo) SetTimer(g_hwnd, 3101, 1200, nullptr);



    if (g_probeCmdk) SetTimer(g_hwnd, 3201, 1200, nullptr);



    if (g_probeCmdkUi) SetTimer(g_hwnd, 3301, 1500, nullptr);



    if (g_probeImport2) SetTimer(g_hwnd, 3401, 1200, nullptr);



    if (g_probeImport2Ui) SetTimer(g_hwnd, 3501, 1600, nullptr);



    if (g_probeOcr) SetTimer(g_hwnd, 3601, 1200, nullptr);



    if (g_probeSubscription) SetTimer(g_hwnd, 3701, 1200, nullptr);



    if (g_probeSqliteEnable) SetTimer(g_hwnd, 3901, 1200, nullptr);



    if (g_probeSqliteRead) SetTimer(g_hwnd, 3911, 1200, nullptr);



    if (g_probeSqliteRollback) SetTimer(g_hwnd, 3921, 1200, nullptr);



    if (g_probeSync) SetTimer(g_hwnd, 4001, 1400, nullptr);



    if (g_probeSyncStatus) SetTimer(g_hwnd, 4011, 1200, nullptr);



    if (g_probeCorruptJson) SetTimer(g_hwnd, 4101, 1200, nullptr);



    if (g_probeSqliteFallback) SetTimer(g_hwnd, 4111, 1200, nullptr);



    if (g_probeExams) SetTimer(g_hwnd, 4201, 1200, nullptr);



    if (g_probeFocus) SetTimer(g_hwnd, 4301, 1200, nullptr);



    if (g_probeFocusTimer) SetTimer(g_hwnd, 4311, 1200, nullptr);



    if (g_probeHabits) SetTimer(g_hwnd, 4401, 1200, nullptr);



    if (g_probePrint) SetTimer(g_hwnd, 4501, 1200, nullptr);



    if (g_probeProfiles) SetTimer(g_hwnd, 4601, 1200, nullptr);



    if (g_probeOverrideDup) SetTimer(g_hwnd, 4701, 1200, nullptr);



    if (g_rollbackTest) SetTimer(g_hwnd, 2601, 1000, nullptr);















    if (((app.store.settings.widget.enabled && !g_shotMode) || !g_widgetShotPath.empty()) && (!g_testMode || g_probeWidgetTest)) CreateWidgetWindow();







    StartReminderEngine();



    SyncInit();



    FocusInit();



    HabitsInit();



    ProfilesInit();



    if (!g_demoMode && !g_shotMode) {



        LoadSubscriptions();



        if (!app.subscriptions.empty()) SetTimer(g_hwnd, 11, 15000, nullptr);

        BotInit();

        SetTimer(g_hwnd, 61, 1000, nullptr);



    }







    if (!g_reminderShotPath.empty()) ShowTestReminder();















    if (!g_startOverlay.empty()) {







        if (g_startOverlay == L"editor") {







            app.editor.LoadNew(Today());







            app.editor.open = true;







            app.overlay = OV_EDITOR;







        } else if (g_startOverlay == L"editor-edit") {







            if (!app.store.events.empty()) {







                app.editor.LoadExisting(app.store.events[0], nullptr);







                app.editor.open = true;







                app.overlay = OV_EDITOR;







            }







        } else if (g_startOverlay == L"undoredo") {
            std::vector<MenuItemDef> items;
            items.push_back({ L"撤销  Ctrl+Z", ICO_UNDO, false, false, []() {} });
            items.push_back({ L"重做  Ctrl+Y", ICO_REDO, false, false, []() {} });
            OpenMenu(0, 0, items);
        } else if (g_startOverlay == L"confirm") {
            OpenConfirm(L"智能整理课表",
                L"将扫描没有标签的日程：自动识别课程并打「课程」标签、按课程自动配色、把有规律的每周重复日程合并成可编辑的重复系列。\n\n合并后可以整组编辑（仅此次 / 此后所有 / 整个系列）。此操作可用 Ctrl+Z 撤销。",
                []() {});
        } else if (g_startOverlay == L"settings") {







            OpenSettings();







        } else if (g_startOverlay == L"daydetail") {







            OpenDayDetail(MakeDT(2026, 10, 18));







        } else if (g_startOverlay == L"stats") {







            OpenStats(1, Today());







        } else if (g_startOverlay == L"stats-year") {







            OpenStats(2, Today());







        } else if (g_startOverlay == L"versions") {







            OpenVersionsOverlay();







        } else if (g_startOverlay == L"backup") {







            OpenBackupExport();







        } else if (g_startOverlay == L"reminders") {







            SeedReminderCenterDemo();



            OpenReminderCenter();







        } else if (g_startOverlay == L"themes") {







            OpenThemeStudio();







        } else if (g_startOverlay == L"import") {







            std::wstring sample = L"<table><tr><td>时间</td><td>周一</td><td>周三</td><td>周五</td></tr>"



                                  L"<tr><td>08:00-09:40</td><td>高等数学<br>教三201<br>张老师</td><td>大学物理<br>教二105</td><td></td></tr>"



                                  L"<tr><td>10:00-11:40</td><td>大学英语<br>外语楼302</td><td></td><td>程序设计基础<br>机房A</td></tr></table>";



            LoadSmartImport(L"", sample, true, L"示例教务课表.html");







        } else if (g_startOverlay == L"ocr") {







            app.import.ocrSource = L"示例：教务课表截图.png";



            app.import.ocrText = L"时间\t周一\t周三\t周五\n08:00-09:40\t高等数学 教三201\t大学物理 教二105\t\n10:00-11:40\t大学英语 外语楼302\t\t程序设计基础 机房A";



            OpenOcrResult();







        } else if (g_startOverlay == L"db") {







            OpenDbManager();







        } else if (g_startOverlay == L"exams") {







            OpenExamPanel();







        } else if (g_startOverlay == L"focus") {







            OpenFocusPanel();







        } else if (g_startOverlay == L"print") {







            OpenPrintPanel();







        } else if (g_startOverlay == L"profiles") {







            ProfilesInit();



            ProfilesSeedDemo();



            OpenProfilesPanel();







        } else if (g_startOverlay == L"help") {







            OpenHelp();







        } else if (g_startOverlay == L"habits") {







            HabitsInit();



            HabitsSeedDemo();



            OpenHabitsPanel();







        } else if (g_startOverlay == L"sync") {







            OpenSyncCenter();







        } else if (g_startOverlay == L"subs") {







            LoadSubscriptions();



            OpenSubscriptions();







        } else if (g_startOverlay == L"cmdk") {







            OpenCommandPalette();



            if (!g_cmdkTextArg.empty()) {



                app.cmdk.text = g_cmdkTextArg;



                app.cmdk.lastText.clear();



            }







        } else if (g_startOverlay == L"quick") {







            OpenQuickAdd();







            app.quick.text = L"明天 15:00-16:00 高数作业 #作业 !!";







        } else if (g_startOverlay == L"bulk") {







            OpenBulkAdd();







        } else if (g_startOverlay == L"about") {







            app.overlay = OV_ABOUT;







        } else if (g_startOverlay == L"widget") {







            OpenWidgetSettings();







        } else if (g_startOverlay == L"series") {







            OpenSeriesColors();







        } else if (g_startOverlay == L"colorpick") {







            OpenColorPicker(L"自定义颜色", L"#6EA8FF", nullptr);







        } else if (g_startOverlay == L"cleanup") {







            OpenCleanup();







        } else if (g_startOverlay == L"fonts") {







            OpenFontSettings();







        } else if (g_startOverlay == L"tag") {







            app.tagEdit.open = true;







            app.tagEdit.isNew = true;







            app.tagEdit.name = L"健身";







            app.tagEdit.color = L"#34D399";







            app.overlay = OV_TAGEDIT;







        }







        UIInvalidate();







    }















    if (g_shotMode || !g_widgetShotPath.empty() || !g_reminderShotPath.empty()) SetTimer(g_hwnd, 9, 700, nullptr);







    if (!g_lateShotPath.empty()) SetTimer(g_hwnd, 8, 1600, nullptr);















    MSG msg;







    while (GetMessageW(&msg, nullptr, 0, 0)) {







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4701) {



            KillTimer(g_hwnd, 4701);



            int overrides = 0, noRecur = 0, total = 0, dup = 0;



            std::map<std::wstring, int> seen;



            for (auto& e : app.store.events) {



                for (auto& ov : e.overrides) {



                    ++overrides;



                    if (ov.recurId.y == 1970 && ov.recurId.mo == 1 && ov.recurId.d == 1 &&



                        ov.recurId.h == 0 && ov.recurId.mi == 0) ++noRecur;



                }



                auto occ = ExpandEventOccurrences(e, MakeDT(2026, 9, 1), MakeDT(2026, 12, 31));



                for (auto& o : occ) {



                    ++total;



                    std::wstring key = Utf8ToWide(o.seriesId) + L"|" + ISODT(o.start);



                    if (seen[key]++ > 0) ++dup;



                }



            }



            // 往返测试：新建重复系列 → 按次完成 → 序列化/反序列化 → 展开不应重复



            Store t;



            Event te;



            te.id = "ev_probe_roundtrip";



            te.title = L"往返测试";



            te.start = MakeDT(2026, 9, 7, 9, 0);



            te.end = MakeDT(2026, 9, 7, 10, 0);



            te.recur.freq = RF_WEEKLY;



            te.recur.interval = 1;



            te.recur.hasUntil = true;



            te.recur.until = MakeDT(2026, 10, 5, 9, 0);



            te.recur.byDay = { 1 };



            t.events.push_back(te);



            t.SetOccurrenceDone("ev_probe_roundtrip", te.start, te.start, te.end, true);



            std::string js = t.ToJsonStr();



            Store t2;



            std::wstring err;



            bool ok = t2.FromJsonStr(js, &err);



            int rtTotal = 0, rtDup = 0;



            std::set<std::wstring> rtSeen;



            for (auto& e2 : t2.events) {



                auto occ2 = ExpandEventOccurrences(e2, MakeDT(2026, 9, 1), MakeDT(2026, 10, 31));



                for (auto& o : occ2) {



                    ++rtTotal;



                    std::wstring key = ISODT(o.start);



                    if (!rtSeen.insert(key).second) ++rtDup;



                }



            }



            DebugLog(L"probe-override-dup: overrides=%d noRecurId=%d occurrences=%d dup=%d roundtripOk=%d rtTotal=%d rtDup=%d",



                     overrides, noRecur, total, dup, ok ? 1 : 0, rtTotal, rtDup);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4601) {



            KillTimer(g_hwnd, 4601);



            std::wstring err;



            bool ok = ProfilesProbeRun(err);



            DebugLog(L"probe-profiles: ok=%d err=%s", ok ? 1 : 0, err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4501) {



            KillTimer(g_hwnd, 4501);



            std::wstring err;



            bool ok = PrintPlanProbe(err);



            DebugLog(L"probe-print: ok=%d err=%s", ok ? 1 : 0, err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4401) {



            KillTimer(g_hwnd, 4401);



            std::wstring err;



            bool ok = HabitsProbeRun(err);



            DebugLog(L"probe-habits: ok=%d err=%s", ok ? 1 : 0, err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4311) {



            KillTimer(g_hwnd, 4311);



            FocusInit();



            g_focusProbeBefore = FocusTodayMinutes();



            app.focus.minutes = 1;



            app.focus.remainingSec = 1;



            app.focus.label = L"计时探针";



            FocusStart();



            SetTimer(g_hwnd, 4312, 1600, nullptr);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4312) {



            KillTimer(g_hwnd, 4312);



            FocusTick();



            int after = FocusTodayMinutes();



            DebugLog(L"probe-focus-timer: before=%d after=%d running=%d sessions=%d",



                     g_focusProbeBefore, after, app.focus.running ? 1 : 0, (int)app.focus.sessions.size());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4301) {



            KillTimer(g_hwnd, 4301);



            std::wstring err;



            bool ok = FocusProbeRun(err);



            DebugLog(L"probe-focus: ok=%d err=%s", ok ? 1 : 0, err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4201) {



            KillTimer(g_hwnd, 4201);



            std::wstring title;



            int days = -1;



            int count = ExamPanelProbe(title, days);



            DebugLog(L"probe-exams: count=%d first=%s days=%d", count, title.c_str(), days);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4101) {



            KillTimer(g_hwnd, 4101);



            app.store.Save();



            WriteFileBytesAtomic(GetCalendarPath(), "{ this is not valid json");



            Store test;



            bool ok = test.Load();



            bool broken = FileExists(PathJoin(GetDataDir(), L"calendar.broken.json"));



            DebugLog(L"probe-corrupt-json: ok=%d events=%d brokenBackup=%d", ok ? 1 : 0, (int)test.events.size(), broken ? 1 : 0);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4111) {



            KillTimer(g_hwnd, 4111);



            app.store.Save();



            std::wstring err;



            bool enabled = SqliteEnableStorage(err);



            WriteFileBytesAtomic(PathJoin(GetDataDir(), L"calendar.db"), "garbage-not-a-sqlite-db");



            Store test;



            bool ok = test.Load();



            DebugLog(L"probe-sqlite-fallback: enabled=%d loadOk=%d events=%d", enabled ? 1 : 0, ok ? 1 : 0, (int)test.events.size());



            SqliteRollbackToJson(err);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4001) {



            KillTimer(g_hwnd, 4001);



            SyncInit();



            std::wstring deviceId = SyncDeviceId();



            app.store.Save();



            int oplog = SyncOplogCount();



            int before = (int)app.store.events.size();



            std::string delId;



            if (!app.store.events.empty()) delId = app.store.events.back().id;



            if (!delId.empty()) app.store.DeleteEvent(delId);



            int afterDelete = (int)app.store.events.size();



            int tomb = SyncTombstoneCount();



            std::wstring pkg = SyncBuildMigrationPackage();



            std::wstring path = PathJoin(GetDataDir(), L"sync-probe.xlbak");



            std::wstring err;



            bool enc = EncryptBackupFile(path, WideToUtf8(pkg), L"test123", err);



            std::string plain;



            bool dec = enc && DecryptBackupFile(path, L"test123", plain, err);



            bool imported = dec && SyncImportMigrationPackage(plain, err);



            int afterImport = (int)app.store.events.size();



            int backups = 0;



            for (auto& f : ListFiles(PathJoin(GetDataDir(), L"backup"), L".json")) {



                size_t p = f.find_last_of(L"\\/");



                std::wstring name = p == std::wstring::npos ? f : f.substr(p + 1);



                if (name.rfind(L"manual-before-migration-", 0) == 0) ++backups;



            }



            DebugLog(L"probe-sync: device=%s deviceLen=%d oplog=%d tomb=%d before=%d afterDelete=%d enc=%d dec=%d imported=%d afterImport=%d backups=%d",



                     deviceId.c_str(), (int)deviceId.size(), oplog, tomb, before, afterDelete,



                     enc ? 1 : 0, dec ? 1 : 0, imported ? 1 : 0, afterImport, backups);



            DeleteFileW(path.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4011) {



            KillTimer(g_hwnd, 4011);



            std::wstring deviceId = SyncDeviceId();



            DebugLog(L"probe-sync-status: device=%s deviceLen=%d oplog=%d tomb=%d",



                     deviceId.c_str(), (int)deviceId.size(), SyncOplogCount(), SyncTombstoneCount());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3901) {



            KillTimer(g_hwnd, 3901);



            bool saved = app.store.Save();



            std::wstring err;



            bool ok = SqliteEnableStorage(err);



            DebugLog(L"probe-sqlite-enable: jsonSaved=%d jsonExists=%d", saved ? 1 : 0, FileExists(GetCalendarPath()) ? 1 : 0);



            DebugLog(L"probe-sqlite-enable: ok=%d engine=%d dbEvents=%d storeEvents=%d err=%s",



                     ok ? 1 : 0, DbEngineEnabled() ? 1 : 0, DbEventRowCount(), (int)app.store.events.size(), err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3911) {



            KillTimer(g_hwnd, 3911);



            std::string dbJson;



            bool loadOk = DbLoadJson(dbJson);



            DebugLog(L"probe-sqlite-read: engine=%d dbLoad=%d events=%d", DbEngineEnabled() ? 1 : 0,



                     loadOk ? 1 : 0, (int)app.store.events.size());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3921) {



            KillTimer(g_hwnd, 3921);



            std::wstring err;



            bool ok = SqliteRollbackToJson(err);



            int jsonEvents = 0;



            bool jok = false;



            std::string json = ReadFileBytes(GetCalendarPath(), &jok);



            if (jok && !json.empty()) {



                Store tmp;



                std::wstring jerr;



                if (tmp.FromJsonStr(json, &jerr)) jsonEvents = (int)tmp.events.size();



            }



            DebugLog(L"probe-sqlite-rollback: ok=%d engine=%d jsonEvents=%d storeEvents=%d err=%s",



                     ok ? 1 : 0, DbEngineEnabled() ? 1 : 0, jsonEvents, (int)app.store.events.size(), err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3701) {



            KillTimer(g_hwnd, 3701);



            std::wstring err;



            bool okAdd = AddSubscription(g_subUrlArg, L"测试订阅", err);



            int before = (int)app.store.events.size();



            int added1 = SyncSubscriptionsDue(true, false);



            int after1 = (int)app.store.events.size();



            int added2 = SyncSubscriptionsDue(true, false);



            int after2 = (int)app.store.events.size();



            for (auto& e : app.store.events) {



                if (e.uid == L"sub-test-1") {



                    e.title = L"手动改过的标题";



                    app.store.Touch();



                    break;



                }



            }



            SyncSubscriptionsDue(true, false);



            int preserved = 0;



            for (auto& e : app.store.events)



                if (e.uid == L"sub-test-1" && e.title == L"手动改过的标题") preserved = 1;



            DebugLog(L"probe-subscription: add=%d added1=%d added2=%d before=%d after1=%d after2=%d preserved=%d err=%s",



                     okAdd ? 1 : 0, added1, added2, before, after1, after2, preserved, err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3601) {



            KillTimer(g_hwnd, 3601);



            std::wstring text, err;



            bool ok = WindowsOcrImage(L"E:\\myCalendar\\ocr-sample.png", L"zh-Hans-CN", text, err);



            DebugLog(L"probe-ocr: ok=%d chars=%d err=%s", ok ? 1 : 0, (int)text.size(), err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3501) {



            KillTimer(g_hwnd, 3501);



            int before = (int)app.store.events.size();



            float hx = 0, hy = 0;



            bool found = GetHitPointByCmd(CMD_IMPORT_OK, hx, hy);



            if (found) SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)hx, (int)hy));



            int after = (int)app.store.events.size();



            DebugLog(L"probe-import2-ui: clicked=%d before=%d after=%d overlay=%d",



                     found ? 1 : 0, before, after, (int)app.overlay);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3401) {



            KillTimer(g_hwnd, 3401);



            std::wstring html = L"<table><tr><td>时间</td><td>周一</td><td>周三</td><td>周五</td></tr>"



                                L"<tr><td>08:00-09:40</td><td>高等数学<br>教三201<br>张老师</td><td>大学物理<br>教二105</td><td></td></tr>"



                                L"<tr><td>10:00-11:40</td><td>大学英语<br>外语楼302</td><td></td><td>程序设计基础<br>机房A</td></tr></table>";



            TableImportResult r1 = ParseScheduleText(html);



            std::wstring csv = L"标题,开始,结束,地点\n高等数学,2026-10-20 08:00,2026-10-20 09:40,教三201\n大学英语,2026-10-21 10:00,2026-10-21 11:40,外语楼302";



            TableImportResult r2 = ParseScheduleText(csv);



            int hh = 0, mm = 0;



            if (!r2.events.empty()) { hh = r2.events[0].start.h; mm = r2.events[0].start.mi; }



            std::wstring ics = L"BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:probe-ics-1\r\nSUMMARY:测试 ICS\r\nDTSTART:20261020T080000\r\nDTEND:20261020T090000\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";



            LoadSmartImport(L"", ics, true, L"probe.ics");



            int icsEvents = (int)app.import.parsed.events.size();



            DebugLog(L"probe-import2: htmlOk=%d htmlEvents=%d table=%d csvEvents=%d csvStart=%02d:%02d icsEvents=%d",



                     r1.ok ? 1 : 0, (int)r1.events.size(), r1.table ? 1 : 0, (int)r2.events.size(), hh, mm, icsEvents);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3301) {



            KillTimer(g_hwnd, 3301);



            OpenCommandPalette();



            app.cmdk.text = L"明天9点高数作业#作业";



            app.cmdk.lastText.clear();



            UIInvalidate();



            SetTimer(g_hwnd, 3302, 900, nullptr);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3302) {



            KillTimer(g_hwnd, 3302);



            int before = (int)app.store.events.size();



            HWND eh = CmdkEditHandleForProbe();



            if (eh) SendMessageW(eh, WM_KEYDOWN, VK_RETURN, 0);



            int after = (int)app.store.events.size();



            DebugLog(L"probe-cmdk-ui: edit=%d before=%d after=%d overlay=%d", eh ? 1 : 0, before, after, (int)app.overlay);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3201) {



            KillTimer(g_hwnd, 3201);



            Settings& s = app.store.settings;



            s.cmdkHotkey = L"Ctrl+Shift+K";



            bool custom = CmdkHotkeyMatches('K', true, false, true);



            bool defaultOff = !CmdkHotkeyMatches('K', true, false, false);



            int before = (int)app.store.events.size();



            std::wstring err;



            bool created = CmdkProbeRun(L"明天 9 点 高数作业 #作业", err);



            int after = (int)app.store.events.size();



            DebugLog(L"probe-cmdk: hotkeyCustom=%d defaultOff=%d created=%d before=%d after=%d err=%s",



                     custom ? 1 : 0, defaultOff ? 1 : 0, created ? 1 : 0, before, after, err.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3101) {



            KillTimer(g_hwnd, 3101);



            Settings& s = app.store.settings;



            s.customBg = L"#101418";



            s.customAccent = L"#55D6BE";



            s.themeRadius = 14;



            s.themeDensity = 105;



            s.customLight = false;



            std::wstring path = PathJoin(GetDataDir(), L"probe-theme.json");



            bool exp = ExportThemeToFile(path);



            std::wstring err;



            bool imp = ImportThemeFromFile(path, err);



            bool ok = exp && imp && s.customBg == L"#101418" && s.customAccent == L"#55D6BE" &&



                      s.themeRadius == 14 && s.themeDensity == 105 && !s.customLight;



            std::wstring badPath = PathJoin(GetDataDir(), L"probe-theme-bad.json");



            WriteFileBytesAtomic(badPath, "not a theme");



            std::wstring badErr;



            bool badRejected = !ImportThemeFromFile(badPath, badErr);



            DebugLog(L"probe-theme-io: exp=%d imp=%d ok=%d badRejected=%d", exp ? 1 : 0, imp ? 1 : 0,



                     ok ? 1 : 0, badRejected ? 1 : 0);



            DeleteFileW(path.c_str());



            DeleteFileW(badPath.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 3001) {



            KillTimer(g_hwnd, 3001);



            Settings& s = app.store.settings;



            s.customBg = L"#F4F1EA";



            s.customAccent = L"#B8862F";



            s.customLight = true;



            s.themeRadius = 16;



            s.themeDensity = 110;



            ApplyCustomTheme(s.customBg, s.customAccent, s.themeRadius, s.themeDensity, s.customLight);



            app.themeIdx = 9;



            int lightIdx = ThemeIndexById(L"daylight");



            int customIdx = ThemeIndexById(L"custom");



            DebugLog(L"probe-theme: lightIdx=%d customIdx=%d light=%d radius=%d density=%d count=%d",



                     lightIdx, customIdx, ThemeAt(9).light ? 1 : 0, ThemeAt(9).radius, ThemeAt(9).density, ThemeCount());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2901) {



            KillTimer(g_hwnd, 2901);



            SeedReminderCenterDemo();



            OpenReminderCenter();



            DebugLog(L"probe-reminder: before=%d", MissedReminderCount());



            SetTimer(g_hwnd, 2902, 700, nullptr);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2902) {



            KillTimer(g_hwnd, 2902);



            int idx = MissedReminderCount() - 1;



            float x = 0, y = 0;



            bool found = idx >= 0 && GetHitPointByCmd(CMD_RM_DONE_BASE + idx, x, y);



            if (found) SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));



            DebugLog(L"probe-reminder: doneClicked=%d after=%d", found ? 1 : 0, MissedReminderCount());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2801) {



            KillTimer(g_hwnd, 2801);



            DestroyWidgetWindow();



            app.store.settings.widget.enabled = true;



            app.store.settings.widget.mode = L"week";



            app.store.settings.widget.clickThrough = true;



            app.store.settings.widget.lowPower = true;



            app.store.settings.widget.x = 99999;



            app.store.settings.widget.y = 99999;



            CreateWidgetWindow();



            DebugLog(L"probe-widget: requested week+clickThrough+lowPower");



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2701) {



            KillTimer(g_hwnd, 2701);



            std::string plain = app.store.ToJsonStr();



            std::wstring path = PathJoin(GetDataDir(), L"probe-backup.xlbak");



            std::wstring err;



            bool enc = EncryptBackupFile(path, plain, L"test123", err);



            std::string back;



            bool dec = enc && DecryptBackupFile(path, L"test123", back, err);



            std::wstring wrongErr;



            std::string bad;



            bool wrong = DecryptBackupFile(path, L"wrongpass", bad, wrongErr);



            DebugLog(L"probe-backup: enc=%d dec=%d same=%d wrongRejected=%d err=%s", enc ? 1 : 0, dec ? 1 : 0,



                     (dec && back == plain) ? 1 : 0, (!wrong) ? 1 : 0, err.c_str());



            DeleteFileW(path.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2601) {



            KillTimer(g_hwnd, 2601);



            DebugLog(L"probe-rollback: target=%s", g_rollbackExe.c_str());



            RollbackToVersion(g_rollbackExe, g_rollbackData, L"test");



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2501) {



            KillTimer(g_hwnd, 2501);



            EnsureVersionSnapshot();



            auto vers = ListFiles(PathJoin(GetDataDir(), L"versions"), L".exe");



            DebugLog(L"probe-version: versions=%d health=%s", (int)vers.size(), app.dataHealth.c_str());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2401) {



            KillTimer(g_hwnd, 2401);



            app.view = VIEW_AGENDA;



            app.agendaBoard = true;



            app.cursor = Today();



            InvalidateOccs();



            UIInvalidate();



            SetTimer(g_hwnd, 2402, 700, nullptr);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2402) {



            KillTimer(g_hwnd, 2402);



            int before = 0;



            auto all = ExpandOccurrences(app.store, AddDaysDT(StartOfDay(app.now), -365), AddDaysDT(StartOfDay(app.now), 365));



            for (auto& o : all) if (o.ev.done) ++before;



            bool clicked = false;



            for (auto& hit : g_hits) {



                if (hit.cmd < CMD_BOARD_DONE + 2000 || hit.cmd >= CMD_BOARD_DONE + 3000) continue;



                float cx = hit.r.X + hit.r.Width / 2, cy = hit.r.Y + hit.r.Height / 2;



                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)cx, (int)cy));



                clicked = true;



                break;



            }



            int after = 0;



            auto all2 = ExpandOccurrences(app.store, AddDaysDT(StartOfDay(app.now), -365), AddDaysDT(StartOfDay(app.now), 365));



            for (auto& o : all2) if (o.ev.done) ++after;



            DebugLog(L"probe-board: clicked=%d doneBefore=%d doneAfter=%d", clicked ? 1 : 0, before, after);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2301) {







            KillTimer(g_hwnd, 2301);







            app.cursor = MakeDT(2026, 10, 18);







            app.view = VIEW_MONTH;







            InvalidateOccs();







            UIInvalidate();







            SetTimer(g_hwnd, 2302, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2495) {
            KillTimer(g_hwnd, 2495);
            app.view = VIEW_MONTH;
            app.cursor = app.now;
            app.store.settings.monthChips = true;
            app.store.settings.monthCounts = false;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-month-default.png"));
            app.store.settings.monthChips = false;
            app.store.settings.monthCounts = true;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-month-counts.png"));
            DebugLog(L"probe-month-detail: shots done chips=0 counts=1");
            app.store.settings.monthChips = true;
            app.store.settings.monthCounts = false;
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2497) {
            KillTimer(g_hwnd, 2497);
            LoadGrades();
            GradesApplyFromStoreJson("{\"grades\":[]}");
            GradesProbeAdd(L"SyncMath", 4, 92);
            GradesProbeAdd(L"SyncPhys", 3, 85);
            std::string payload = GradesAttachToStoreJson(app.store.ToJsonStr());
            std::wstring err;
            JValue root = JsonParse(payload, &err);
            int attached = root.Get(L"grades").IsArr() ? (int)root.Get(L"grades").arr.size() : -1;
            GradesApplyFromStoreJson("{\"grades\":[{\"course\":\"FromServer\",\"credit\":2,\"score\":80}]}");
            double gpa = GradesGpa();
            bool ok = attached == 2 && GradesCount() == 1 && gpa > 2.99 && gpa < 3.01;
            DebugLog(L"probe-grades-sync: attached=%d applied=%d gpa=%.2f ok=%d", attached, GradesCount(), gpa, ok ? 1 : 0);
            GradesApplyFromStoreJson("{\"grades\":[]}");
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2499) {
            KillTimer(g_hwnd, 2499);
            ShowWindow(g_hwnd, SW_SHOWNA);
            UpdateWindow(g_hwnd);
            Sleep(300);
            LRESULT sendLr = SendMessageW(g_hwnd, WM_GETOBJECT, 0, OBJID_CLIENT);
            LRESULT directLr = A11yHandleGetObject(g_hwnd, 0, OBJID_CLIENT);
            IAccessible* pacc = nullptr;
            HRESULT hr = ObjectFromLresult(sendLr, IID_IAccessible, 0, (void**)&pacc);
            if (FAILED(hr) || !pacc) {
                hr = AccessibleObjectFromWindow(g_hwnd, OBJID_CLIENT, IID_IAccessible, (void**)&pacc);
            }
            std::wstring name, desc;
            long role = -1, state = -1, children = -1;
            if (SUCCEEDED(hr) && pacc) {
                VARIANT vc; VariantInit(&vc); vc.vt = VT_I4; vc.lVal = CHILDID_SELF;
                BSTR b = nullptr;
                if (SUCCEEDED(pacc->get_accName(vc, &b)) && b) { name = b; SysFreeString(b); }
                VARIANT vr; VariantInit(&vr);
                if (SUCCEEDED(pacc->get_accRole(vc, &vr))) role = vr.lVal;
                VARIANT vs; VariantInit(&vs);
                if (SUCCEEDED(pacc->get_accState(vc, &vs))) state = vs.lVal;
                pacc->get_accChildCount(&children);
                BSTR d = nullptr;
                if (SUCCEEDED(pacc->get_accDescription(vc, &d)) && d) { desc = d; SysFreeString(d); }
                pacc->Release();
            }
            DebugLog(L"probe-a11y: sendLr=0x%08X directLr=0x%08X hr=0x%08X name=%s role=%d state=%d children=%d desc=%s ok=%d",
                     (unsigned)sendLr, (unsigned)directLr, (unsigned)hr, name.c_str(), (int)role, (int)state, (int)children, desc.c_str(),
                     (!name.empty() && role == ROLE_SYSTEM_WINDOW) ? 1 : 0);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2493) {
            KillTimer(g_hwnd, 2493);
            Store st;
            auto mk = [&](const char* id, const wchar_t* title, int y, int mo, int d, int h, int durMin) {
                Event e; e.id = id; e.title = title;
                e.start = MakeDT(y, mo, d, h, 0);
                e.end = AddMinutesDT(e.start, durMin);
                st.events.push_back(e);
            };
            mk("d1", L"重复", 2026, 10, 20, 9, 60);
            mk("d2", L"重复", 2026, 10, 20, 9, 60);
            mk("bad", L"结束早", 2026, 10, 21, 10, -120);
            mk("empty", L"", 2026, 10, 22, 9, 60);
            mk("long", L"超长", 2026, 10, 1, 9, 8 * 24 * 60);
            mk("ghost", L"坏标签", 2026, 10, 23, 9, 60);
            st.events[5].tags.push_back("not_exist");
            mk("alarm", L"坏提醒", 2026, 10, 24, 9, 60);
            Alarm al; al.offsetMin = 100000;
            st.events[6].alarms.push_back(al);
            HealthReport hr = ScanHealth(st);
            app.store = st;
            OpenHealth();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            DebugLog(L"probe-health: dup=%d endBefore=%d tooLong=%d empty=%d tags=%d alarms=%d issues=%d",
                     hr.duplicate, hr.endBeforeStart, hr.tooLong, hr.emptyTitle, hr.missingTags, hr.badAlarms, hr.issues());
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-health.png"));
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2491) {
            KillTimer(g_hwnd, 2491);
            app.view = VIEW_AGENDA;
            app.selected.clear();
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            int f1 = 0, f2 = 0, f3 = 0, fb = 0, act = 0, after = 0;
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_TAB, 0);
            f1 = UIFocusCmd();
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_TAB, 0);
            f2 = UIFocusCmd();
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_TAB, 0);
            f3 = UIFocusCmd();
            BYTE ks[256]; GetKeyboardState(ks); ks[VK_SHIFT] = 0x80; SetKeyboardState(ks);
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_TAB, 0);
            BYTE ks2[256]; GetKeyboardState(ks2); ks2[VK_SHIFT] = 0; SetKeyboardState(ks2);
            fb = UIFocusCmd();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            act = UIActivateFocus() ? 1 : 0;
            after = UIFocusCmd();
            DebugLog(L"probe-kb-nav: f1=%d f2=%d f3=%d back=%d activated=%d after=%d", f1, f2, f3, fb, act, after);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2489) {
            KillTimer(g_hwnd, 2489);
            app.store.events.clear();
            Event e;
            e.id = "bot_ev1";
            e.title = L"提醒测试";
            e.start = AddMinutesDT(NowDT(), 1);
            e.end = AddMinutesDT(e.start, 30);
            Alarm al; al.offsetMin = 5; al.sound = true;
            e.alarms.push_back(al);
            app.store.events.push_back(e);
            BotInit();
            app.bot.enabled = true;
            app.bot.chatId = 1;
            app.bot.token.clear();
            app.bot.sentReminders.clear();
            BotTick();
            int pushed = (int)app.bot.sentReminders.size();
            std::wstring next = BotTestCommand(L"/next");
            std::wstring search = BotTestCommand(L"/search 提醒");
            std::wstring today = BotTestCommand(L"/today");
            std::wstring done = BotTestCommand(L"/done 1");
            bool isDone = !app.store.events.empty() && app.store.events[0].done;
            DebugLog(L"probe-bot2: pushed=%d nextLen=%d searchLen=%d todayLen=%d done=%s eventDone=%d lastList=%d",
                     pushed, (int)next.size(), (int)search.size(), (int)today.size(), done.c_str(), isDone ? 1 : 0,
                     (int)app.bot.lastList.size());
            BotShutdown();
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2487) {
            KillTimer(g_hwnd, 2487);
            app.view = VIEW_MONTH;
            app.cursor = app.now;
            app.store.settings.highContrast = false;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-hc-off.png"));
            app.store.settings.highContrast = true;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-hc-on.png"));
            app.store.settings.highContrast = false;
            OpenSettings();
            app.settingsScroll = 0;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-hc-settings.png"));
            DebugLog(L"probe-hc: off/on captured hc=%d", app.store.settings.highContrast ? 1 : 0);
            app.store.settings.highContrast = false;
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2485) {
            KillTimer(g_hwnd, 2485);
            OpenSettings();
            app.settingsScroll = 330;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-tag-btn.png"));
            DebugLog(L"probe-tag-btn: shot scroll=%.0f tags=%d", app.settingsScroll, (int)app.store.tags.size());
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2483) {
            KillTimer(g_hwnd, 2483);
            Store local, remote;
            Event e1; e1.id = "e1"; e1.title = L"保留"; e1.updated = MakeDT(2026, 10, 19, 10, 0); e1.start = MakeDT(2026, 10, 19, 9, 0); e1.end = MakeDT(2026, 10, 19, 10, 0);
            local.events.push_back(e1);
            local.tombstones.push_back({ "e2", ISODT(MakeDT(2026, 10, 19, 11, 0)) });
            Event e2; e2.id = "e2"; e2.title = L"已删本地"; e2.updated = MakeDT(2026, 10, 19, 9, 0); e2.start = MakeDT(2026, 10, 19, 11, 0); e2.end = MakeDT(2026, 10, 19, 12, 0);
            Event e3; e3.id = "e3"; e3.title = L"新事件"; e3.updated = MakeDT(2026, 10, 19, 8, 0); e3.start = MakeDT(2026, 10, 20, 9, 0); e3.end = MakeDT(2026, 10, 20, 10, 0);
            remote.events.push_back(e2); remote.events.push_back(e3);
            remote.tombstones.push_back({ "e1", ISODT(MakeDT(2026, 10, 19, 12, 0)) });
            MergeStats st;
            Store merged = MergeStores(local, remote, st);
            Store rt;
            std::wstring err;
            rt.FromJsonStr(local.ToJsonStr(), &err);
            DebugLog(L"probe-tombstone: n=%d deleted=%d tombs=%d roundtrip=%d",
                     (int)merged.events.size(), st.deleted, (int)merged.tombstones.size(), (int)rt.tombstones.size());
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2481) {
            KillTimer(g_hwnd, 2481);
            Store local, remote;
            Event a; a.id = "e1"; a.title = L"A"; a.updated = MakeDT(2026, 10, 19, 10, 0); a.start = MakeDT(2026, 10, 19, 9, 0); a.end = MakeDT(2026, 10, 19, 10, 0);
            Event b; b.id = "e2"; b.title = L"local-only"; b.updated = MakeDT(2026, 10, 19, 9, 0); b.start = MakeDT(2026, 10, 19, 11, 0); b.end = MakeDT(2026, 10, 19, 12, 0);
            local.events.push_back(a); local.events.push_back(b);
            Event a2; a2.id = "e1"; a2.title = L"A2"; a2.updated = MakeDT(2026, 10, 19, 11, 0); a2.start = a.start; a2.end = a.end;
            Event c; c.id = "e3"; c.title = L"remote-only"; c.updated = MakeDT(2026, 10, 19, 8, 0); c.start = MakeDT(2026, 10, 20, 9, 0); c.end = MakeDT(2026, 10, 20, 10, 0);
            remote.events.push_back(a2); remote.events.push_back(c);
            MergeStats st;
            Store merged = MergeStores(local, remote, st);
            std::wstring m1 = merged.events.size() == 3 ? merged.events[0].title : L"?";
            Store local2 = local;
            local2.events[0].updated = MakeDT(2026, 10, 19, 12, 0);
            MergeStats st2;
            Store merged2 = MergeStores(local2, remote, st2);
            std::wstring m2 = merged2.events.empty() ? L"?" : merged2.events[0].title;
            DebugLog(L"probe-merge: n=%d first=%s updated=%d added=%d kept=%d | localNewer n=%d first=%s kept=%d",
                     (int)merged.events.size(), m1.c_str(), st.updated, st.added, st.kept,
                     (int)merged2.events.size(), m2.c_str(), st2.kept);
            OpenSettings();
            app.settingsScroll = 0;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-settings-top.png"));
            app.settingsScroll = 420;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-settings-bottom.png"));
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2475) {
            KillTimer(g_hwnd, 2475);
            DeleteFileW(PathJoin(GetDataDir(), L"grades.json").c_str());
            LoadGrades();
            GradesProbeAdd(L"高等数学", 4.0, 90);
            GradesProbeAdd(L"线性代数", 3.0, 85);
            GradesProbeAdd(L"大学物理", 2.0, 78);
            OpenGrades();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            DebugLog(L"probe-grades: count=%d credits=%.2f gpa=%.2f", GradesCount(), GradesCredits(), GradesGpa());
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-grades.png"));
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2471) {
            KillTimer(g_hwnd, 2471);
            app.view = VIEW_AGENDA;
            app.selected.clear();
            app.agendaAnchorKey.clear();
            InvalidateOccs();
            auto redraw = [&]() { RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW); };
            auto clickAt = [&](float px, float py) {
                LPARAM lp = MAKELPARAM((int)px, (int)py);
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
            };
            auto eventHits = [&]() {
                std::vector<RectF> v;
                for (auto& hh : g_hits) if (hh.cmd >= CMD_EVENT_BASE && hh.cmd < CMD_EVENT_BASE + 5000) v.push_back(hh.r);
                std::sort(v.begin(), v.end(), [](const RectF& a, const RectF& b) { return a.Y < b.Y; });
                return v;
            };
            redraw();
            auto rows = eventHits();
            int listSingle = 0;
            if (!rows.empty()) {
                clickAt(rows[0].X + rows[0].Width / 2, rows[0].Y + rows[0].Height / 2);
                redraw();
                listSingle = (int)app.selected.size();
            }
            app.selected.clear();
            app.view = VIEW_MONTH;
            app.cursor = app.now;
            InvalidateOccs();
            redraw();
            auto chips = eventHits();
            int monthShift = 0;
            if (chips.size() >= 2) {
                BYTE ks[256]; GetKeyboardState(ks); ks[VK_SHIFT] = 0x80; SetKeyboardState(ks);
                clickAt(chips[0].X + chips[0].Width / 2, chips[0].Y + chips[0].Height / 2);
                BYTE ks2[256]; GetKeyboardState(ks2); ks2[VK_SHIFT] = 0; SetKeyboardState(ks2);
                redraw();
                monthShift = (int)app.selected.size();
                BYTE ks3[256]; GetKeyboardState(ks3); ks3[VK_CONTROL] = 0x80; SetKeyboardState(ks3);
                clickAt(chips[1].X + chips[1].Width / 2, chips[1].Y + chips[1].Height / 2);
                BYTE ks4[256]; GetKeyboardState(ks4); ks4[VK_CONTROL] = 0; SetKeyboardState(ks4);
                redraw();
            }
            DebugLog(L"probe-multi-view: listSingle=%d monthShift=%d monthSame=%d",
                     listSingle, monthShift, (int)app.selected.size());
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-multi-view.png"));
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2469) {
            KillTimer(g_hwnd, 2469);
            app.view = VIEW_AGENDA;
            app.agendaBoard = false;
            app.agendaFilter.done = 0;
            app.selected.clear();
            app.agendaAnchorKey.clear();
            InvalidateOccs();
            auto redraw = [&]() { RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW); };
            auto rowsOf = [&]() {
                std::vector<RectF> v;
                for (auto& hh : g_hits) if (hh.cmd >= CMD_EVENT_BASE && hh.cmd < CMD_EVENT_BASE + 5000) v.push_back(hh.r);
                std::sort(v.begin(), v.end(), [](const RectF& a, const RectF& b) { return a.Y < b.Y; });
                return v;
            };
            auto clickAt = [&](float px, float py) {
                LPARAM lp = MAKELPARAM((int)px, (int)py);
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
            };
            redraw();
            auto rows = rowsOf();
            DebugLog(L"probe-multi-real: rows=%d", (int)rows.size());
            if (rows.size() >= 8) {
                clickAt(rows[3].X + rows[3].Width / 2, rows[3].Y + rows[3].Height / 2);
                redraw();
                DebugLog(L"probe-multi-real: single1=%d overlay=%d", (int)app.selected.size(), (int)app.overlay);
                rows = rowsOf();
                clickAt(rows[5].X + rows[5].Width / 2, rows[5].Y + rows[5].Height / 2);
                redraw();
                DebugLog(L"probe-multi-real: single2=%d anchor=%s", (int)app.selected.size(), app.agendaAnchorKey.c_str());
                rows = rowsOf();
                BYTE ks[256]; GetKeyboardState(ks); ks[VK_CONTROL] = 0x80; SetKeyboardState(ks);
                clickAt(rows[6].X + rows[6].Width / 2, rows[6].Y + rows[6].Height / 2);
                BYTE ks2[256]; GetKeyboardState(ks2); ks2[VK_CONTROL] = 0; SetKeyboardState(ks2);
                redraw();
                DebugLog(L"probe-multi-real: ctrlAdd=%d", (int)app.selected.size());
                rows = rowsOf();
                BYTE ks3[256]; GetKeyboardState(ks3); ks3[VK_SHIFT] = 0x80; SetKeyboardState(ks3);
                clickAt(rows[7].X + rows[7].Width / 2, rows[7].Y + rows[7].Height / 2);
                BYTE ks4[256]; GetKeyboardState(ks4); ks4[VK_SHIFT] = 0; SetKeyboardState(ks4);
                redraw();
                DebugLog(L"probe-multi-real: shiftRange=%d", (int)app.selected.size());
                // 月视图 Shift+点击事件块
                app.view = VIEW_MONTH;
                app.cursor = app.now;
                app.selected.clear();
                InvalidateOccs();
                redraw();
                RectF ev;
                bool foundEv = false;
                for (auto& hh : g_hits) if (hh.cmd >= CMD_EVENT_BASE && hh.cmd < CMD_EVENT_BASE + 5000) { ev = hh.r; foundEv = true; break; }
                if (foundEv) {
                    BYTE ks5[256]; GetKeyboardState(ks5); ks5[VK_SHIFT] = 0x80; SetKeyboardState(ks5);
                    clickAt(ev.X + ev.Width / 2, ev.Y + ev.Height / 2);
                    BYTE ks6[256]; GetKeyboardState(ks6); ks6[VK_SHIFT] = 0; SetKeyboardState(ks6);
                    redraw();
                }
                DebugLog(L"probe-multi-real: monthShift=%d monthOverlay=%d", (int)app.selected.size(), (int)app.overlay);
                CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-multi-real.png"));
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2467) {
            KillTimer(g_hwnd, 2467);
            app.store.settings.hotkeyMonth = L"F1";
            app.store.settings.hotkeyWeek = L"F2";
            app.view = VIEW_AGENDA;
            InvalidateOccs();
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F1, 0);
            int v1 = (int)app.view;
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F2, 0);
            int v2 = (int)app.view;
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F10, 0);
            int ovl = (int)app.overlay;
            OpenHotkeys();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            float bx = 0, by = 0;
            bool found = false;
            for (auto& hh : g_hits) {
                if (hh.cmd == CMD_HOTKEYS_ROW_BASE + 0) { bx = hh.r.X + hh.r.Width / 2; by = hh.r.Y + hh.r.Height / 2; found = true; break; }
            }
            if (found) {
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM((int)bx, (int)by));
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)bx, (int)by));
                BYTE ks[256]; GetKeyboardState(ks); ks[VK_CONTROL] = 0x80; SetKeyboardState(ks);
                SendMessageW(g_hwnd, WM_KEYDOWN, (WPARAM)"M"[0], 0);
                BYTE ks2[256]; GetKeyboardState(ks2); ks2[VK_CONTROL] = 0; SetKeyboardState(ks2);
            }
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            DebugLog(L"probe-hotkeys: v1=%d v2=%d ovl=%d rowHit=%d captured=%s",
                     v1, v2, ovl, found ? 1 : 0, app.store.settings.hotkeyMonth.c_str());
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-hotkeys.png"));
            app.store.settings.hotkeyMonth = L"M";
            app.store.settings.hotkeyWeek = L"W";
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2465) {
            KillTimer(g_hwnd, 2465);
            app.view = VIEW_AGENDA;
            app.agendaBoard = false;
            app.agendaFilter.done = 0;
            app.selected.clear();
            app.agendaAnchorKey.clear();
            InvalidateOccs();
            auto redraw = [&]() { RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW); };
            auto rowsOf = [&]() {
                std::vector<RectF> v;
                for (auto& hh : g_hits) if (hh.cmd >= CMD_EVENT_BASE && hh.cmd < CMD_EVENT_BASE + 5000) v.push_back(hh.r);
                std::sort(v.begin(), v.end(), [](const RectF& a, const RectF& b) { return a.Y < b.Y; });
                return v;
            };
            auto findHit = [&](int target) -> RectF {
                for (auto& hh : g_hits) if (hh.cmd == target) return hh.r;
                return RectF(0, 0, 0, 0);
            };
            auto clickAt = [&](float px, float py, bool dbl = false) {
                LPARAM lp = MAKELPARAM((int)px, (int)py);
                if (dbl) { SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, lp); SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp); return; }
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
            };
            redraw();
            auto rows = rowsOf();
            float baseY = rows.empty() ? 0 : rows[0].Y;
            if (rows.size() >= 6) {
                clickAt(rows[0].X + rows[0].Width / 2, rows[0].Y + rows[0].Height / 2);
                int n1 = (int)app.selected.size();
                redraw();
                RectF clearBtn = findHit(CMD_AGENDA_SELNONE);
                rows = rowsOf();
                clickAt(clearBtn.X + clearBtn.Width / 2, clearBtn.Y + clearBtn.Height / 2);
                redraw();
                int n2 = (int)app.selected.size();
                bool anchorCleared = app.agendaAnchorKey.empty();
                rows = rowsOf();
                float baseY2 = rows.empty() ? 0 : rows[0].Y;
                clickAt(clearBtn.X + clearBtn.Width / 2, clearBtn.Y + clearBtn.Height / 2, true);
                redraw();
                int ovl = (int)app.overlay;
                rows = rowsOf();
                clickAt(rows[2].X + rows[2].Width / 2, rows[2].Y + rows[2].Height / 2);
                redraw();
                int nMid = (int)app.selected.size();
                bool anchorSet = !app.agendaAnchorKey.empty();
                rows = rowsOf();
                BYTE ks[256]; GetKeyboardState(ks); ks[VK_SHIFT] = 0x80; SetKeyboardState(ks);
                clickAt(rows[5].X + rows[5].Width / 2, rows[5].Y + rows[5].Height / 2);
                BYTE ks2[256]; GetKeyboardState(ks2); ks2[VK_SHIFT] = 0; SetKeyboardState(ks2);
                int n3 = (int)app.selected.size();
                redraw();
                DebugLog(L"probe-toolbar: single=%d cleared=%d anchorCleared=%d layoutStable=%d dblOverlay=%d midSelect=%d anchorSet=%d shiftRange=%d",
                         n1, n2, anchorCleared ? 1 : 0, (baseY == baseY2) ? 1 : 0, ovl, nMid, anchorSet ? 1 : 0, n3);
                CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-toolbar.png"));
            } else {
                DebugLog(L"probe-toolbar: rows=%d not enough", (int)rows.size());
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2463) {
            KillTimer(g_hwnd, 2463);
            app.view = VIEW_MONTH;
            app.cursor = app.now;
            OpenStats(0, app.now);
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-stats-tags.png"));
            DebugLog(L"probe-stats-tags: shot");
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2461) {
            KillTimer(g_hwnd, 2461);
            app.view = VIEW_AGENDA;
            app.agendaBoard = false;
            app.selected.clear();
            app.agendaAnchorKey.clear();
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            std::vector<RectF> rows;
            for (auto& hh : g_hits) {
                if (hh.cmd >= CMD_EVENT_BASE && hh.cmd < CMD_EVENT_BASE + 5000) rows.push_back(hh.r);
            }
            std::sort(rows.begin(), rows.end(), [](const RectF& a, const RectF& b) { return a.Y < b.Y; });
            auto clickRow = [&](int index, bool ctrl, bool shift) {
                if (index < 0 || index >= (int)rows.size()) return;
                BYTE ks[256]; GetKeyboardState(ks);
                if (ctrl) ks[VK_CONTROL] = 0x80;
                if (shift) ks[VK_SHIFT] = 0x80;
                SetKeyboardState(ks);
                LPARAM lp = MAKELPARAM((int)(rows[index].X + rows[index].Width / 2),
                                       (int)(rows[index].Y + rows[index].Height / 2));
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                BYTE ks2[256]; GetKeyboardState(ks2);
                ks2[VK_CONTROL] = 0; ks2[VK_SHIFT] = 0;
                SetKeyboardState(ks2);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
            };
            if (rows.size() >= 6) {
                clickRow(0, false, false);
                int n1 = (int)app.selected.size();
                clickRow(3, false, true);
                int n2 = (int)app.selected.size();
                clickRow(5, true, false);
                int n3 = (int)app.selected.size();
                clickRow(1, false, false);
                int n4 = (int)app.selected.size();
                clickRow(4, false, true);
                int n5 = (int)app.selected.size();
                DebugLog(L"probe-multiselect4: rows=%d single=%d shiftRange=%d ctrlAdd=%d single2=%d shiftRange2=%d",
                         (int)rows.size(), n1, n2, n3, n4, n5);
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-multiselect4.png"));
            } else {
                DebugLog(L"probe-multiselect4: rows=%d not enough", (int)rows.size());
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2459) {
            KillTimer(g_hwnd, 2459);
            std::wstring wav = L"C:\\Windows\\Media\\Alarm01.wav";
            bool exists = GetFileAttributesW(wav.c_str()) != INVALID_FILE_ATTRIBUTES;
            app.store.settings.notify.soundFile = wav;
            DoSaveStore();
            bool okj = false;
            std::string js = ReadFileBytes(GetCalendarPath(), &okj);
            bool saved = js.find("soundFile") != std::string::npos;
            BOOL played = exists ? PlaySoundW(wav.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT) : FALSE;
            OpenSettings();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-ringtone.png"));
            DebugLog(L"probe-ringtone: exists=%d saved=%d played=%d", exists ? 1 : 0, saved ? 1 : 0, played ? 1 : 0);
            app.store.settings.notify.soundFile.clear();
            DoSaveStore();
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2457) {
            KillTimer(g_hwnd, 2457);
            DateTime df = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, AddDaysDT(df, -1), AddDaysDT(df, 7));
            for (auto& o : app.occs) {
                if (o.seriesId == "e_ddl_point") { app.cursor = StartOfDay(o.start); break; }
            }
            app.view = VIEW_DAY;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-ddl-view.png"));
            DebugLog(L"probe-ddl-view: shot cursor=%s", ISODT(app.cursor).c_str());
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2455) {
            KillTimer(g_hwnd, 2455);
            app.view = VIEW_AGENDA;
            app.agendaBoard = false;
            app.selected.clear();
            app.agendaAnchorKey.clear();
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            std::vector<RectF> rows;
            for (auto& h : g_hits) {
                if (h.cmd >= CMD_EVENT_BASE && h.cmd < CMD_EVENT_BASE + 5000) rows.push_back(h.r);
            }
            std::sort(rows.begin(), rows.end(), [](const RectF& a, const RectF& b) { return a.Y < b.Y; });
            auto clickRow = [&](int index, bool ctrl, bool shift) {
                if (index < 0 || index >= (int)rows.size()) return;
                POINT pt; GetCursorPos(&pt);
                BYTE ks[256]; GetKeyboardState(ks);
                if (ctrl) ks[VK_CONTROL] = 0x80;
                if (shift) ks[VK_SHIFT] = 0x80;
                SetKeyboardState(ks);
                LPARAM lp = MAKELPARAM((int)(rows[index].X + rows[index].Width / 2),
                                       (int)(rows[index].Y + rows[index].Height / 2));
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                BYTE ks2[256]; GetKeyboardState(ks2);
                ks2[VK_CONTROL] = 0; ks2[VK_SHIFT] = 0;
                SetKeyboardState(ks2);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
            };
            if (rows.size() >= 5) {
                clickRow(0, false, false);
                int n1 = (int)app.selected.size();
                clickRow(2, false, false);
                int n2 = (int)app.selected.size();
                clickRow(3, true, false);
                int n3 = (int)app.selected.size();
                clickRow(4, false, true);
                int n4 = (int)app.selected.size();
                DebugLog(L"probe-multiselect3: rows=%d single1=%d single2=%d ctrl=%d shiftRange=%d",
                         (int)rows.size(), n1, n2, n3, n4);
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-multiselect3.png"));
            } else {
                DebugLog(L"probe-multiselect3: rows=%d not enough", (int)rows.size());
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2453) {
            KillTimer(g_hwnd, 2453);
            std::wstring err, path;
            bool ok = ExportWeeklyPlanPng(err, &path);
            bool fok = false;
            std::string bytes = ReadFileBytes(path, &fok);
            DebugLog(L"probe-print-png: ok=%d file=%d size=%d err=%s path=%s",
                     ok ? 1 : 0, fok ? 1 : 0, (int)bytes.size(), err.c_str(), path.c_str());
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2451) {
            KillTimer(g_hwnd, 2451);
            app.view = VIEW_AGENDA;
            app.agendaBoard = true;
            app.boardAllEvents = true;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-board-all.png"));
            app.boardAllEvents = false;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-board-task.png"));
            std::string ics = ExportIcs(app.store);
            Store tmp;
            IcsImportResult ir = ImportIcs(tmp, ics, 0, L"probe");
            bool hasLoc = false, hasColor = false, hasTag = false;
            for (auto& e : tmp.events) {
                if (!e.location.empty()) hasLoc = true;
                if (!e.color.empty()) hasColor = true;
                if (!e.tags.empty()) hasTag = true;
            }
            DebugLog(L"probe-board-all: icsLen=%d LOCATION=%d COLOR=%d CATEGORIES=%d roundtripLoc=%d color=%d tag=%d imported=%d",
                     (int)ics.size(),
                     ics.find("LOCATION:") != std::string::npos ? 1 : 0,
                     ics.find("COLOR:") != std::string::npos ? 1 : 0,
                     ics.find("CATEGORIES:") != std::string::npos ? 1 : 0,
                     hasLoc ? 1 : 0, hasColor ? 1 : 0, hasTag ? 1 : 0, (int)tmp.events.size());
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2449) {
            KillTimer(g_hwnd, 2449);
            InvalidateOccs();
            DateTime df = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, df, AddDaysDT(df, 60));
            if (!app.occs.empty()) {
                OpenEditorForOccurrence(app.occs[0]);
                app.editor.date1 = ISODate(StartOfDay(app.now));
                app.editor.date2 = ISODate(StartOfDay(app.now));
                app.editor.time1 = L"18:00";
                app.editor.time2 = L"18:00";
                app.editor.isDdl = true;
                EditorSave();
                if (!app.store.events.empty()) {
                    const Event& e = app.store.events[0];
                    DebugLog(L"probe-ddl-point: start=%s end=%s equal=%d ddl=%d overlay=%d err=%s",
                             ISODT(e.start).c_str(), ISODT(e.end).c_str(),
                             CompareDT(e.start, e.end) == 0 ? 1 : 0, e.ddl ? 1 : 0,
                             (int)app.overlay, app.editor.error.c_str());
                }
                app.view = VIEW_AGENDA;
                InvalidateOccs();
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-ddl-point.png"));
            } else {
                DebugLog(L"probe-ddl-point: no occs");
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2447) {
            KillTimer(g_hwnd, 2447);
            InvalidateOccs();
            DateTime df = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, df, AddDaysDT(df, 60));
            if (!app.occs.empty()) {
                OpenEditorForOccurrence(app.occs[0]);
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                float bx = 0, by = 0;
                if (GetHitPointByCmd(CMD_ED_DDL, bx, by)) {
                    int c = 0, l = 0;
                    bool hit = PointInHit(bx, by, c, l);
                    DebugLog(L"probe-ddl-click: hit=%d cmd=%d isDdlStart=%d", hit ? 1 : 0, c, app.editor.isDdl ? 1 : 0);
                    std::wstring seq;
                    for (int i = 0; i < 6; ++i) {
                        int pc = 0, pl = 0;
                        PointInHit(bx, by, pc, pl);
                        seq += Fmt(L"[%d]", pc);
                        LPARAM lp = MAKELPARAM((int)bx, (int)by);
                        SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                        SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                        seq += app.editor.isDdl ? L"1" : L"0";
                    }
                    DebugLog(L"probe-ddl-click: seq=%s", seq.c_str());
                } else {
                    DebugLog(L"probe-ddl-click: hit rect not found");
                }
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2445) {
            KillTimer(g_hwnd, 2445);
            app.bot.token = L"123456:TEST-TOKEN";
            app.bot.enabled = false;
            app.bot.status = L"未启用";
            OpenBotPanel();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-bot-panel.png"));
            app.overlay = OV_NONE;
            std::wstring help = BotTestCommand(L"/help");
            std::wstring today = BotTestCommand(L"/today");
            int before = (int)app.store.events.size();
            std::wstring added = BotTestCommand(L"/add 明天 9:00 开会 #会议");
            int after = (int)app.store.events.size();
            DebugLog(L"probe-bot: helpLen=%d todayHead=%s addHead=%s events=%d->%d chat=%lld",
                     (int)help.size(), today.substr(0, 30).c_str(), added.substr(0, 30).c_str(),
                     before, after, app.bot.chatId);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2443) {
            KillTimer(g_hwnd, 2443);
            app.view = VIEW_AGENDA;
            app.selected.clear();
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            std::vector<RectF> rows;
            for (auto& h : g_hits) {
                if (h.cmd >= CMD_EVENT_BASE && h.cmd < CMD_EVENT_BASE + 5000) rows.push_back(h.r);
                if (rows.size() >= 2) break;
            }
            auto clickRow = [&](const RectF& r) {
                LPARAM lp = MAKELPARAM((int)(r.X + r.Width / 2), (int)(r.Y + r.Height / 2));
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
            };
            if (rows.size() >= 2) {
                clickRow(rows[0]);
                int s1 = (int)app.selected.size();
                clickRow(rows[1]);
                int s2 = (int)app.selected.size();
                DebugLog(L"probe-multi-ddl: singleClicks=%d,%d overlay=%d", s1, s2, (int)app.overlay);
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-multi-new.png"));
                // 双击第一行应打开编辑器
                LPARAM lp = MAKELPARAM((int)(rows[0].X + rows[0].Width / 2), (int)(rows[0].Y + rows[0].Height / 2));
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                DebugLog(L"probe-multi-ddl: dblclick overlay=%d", (int)app.overlay);
            } else {
                DebugLog(L"probe-multi-ddl: rows not found hits=%d", (int)g_hits.size());
            }
            app.editor.open = false;
            app.overlay = OV_NONE;
            // DDL 独立字段 + 编辑器截图
            if (!app.store.events.empty()) {
                app.store.events[0].ddl = true;
                app.store.Touch();
                DoSaveStore();
                InvalidateOccs();
                if (!app.occs.empty()) {
                    OpenEditorForOccurrence(app.occs[0]);
                    RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                    CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-editor-ddl.png"));
                }
            }
            app.editor.open = false;
            app.overlay = OV_NONE;
            OpenSettings();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-settings-layout.png"));
            bool okj = false;
            std::string js = ReadFileBytes(GetCalendarPath(), &okj);
            DebugLog(L"probe-multi-ddl: ddlSaved=%d", js.find("\"ddl\"") != std::string::npos ? 1 : 0);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2441) {
            KillTimer(g_hwnd, 2441);
            InvalidateOccs();
            DateTime rf = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, rf, AddDaysDT(rf, 120));
            if (!app.occs.empty()) {
                OpenEditorForOccurrence(app.occs[0]);
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-recur-preview.png"));
                float bx = 0, by = 0;
                if (GetHitPointByCmd(CMD_ED_EXDATE_BASE + 0, bx, by)) {
                    SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)bx, (int)by));
                    DebugLog(L"probe-recur-preview: exDates=%d", (int)app.editor.exDates.size());
                    RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                    CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-recur-preview-skipped.png"));
                    EditorSave();
                    bool okj = false;
                    std::string js = ReadFileBytes(GetCalendarPath(), &okj);
                    DebugLog(L"probe-recur-preview: saved exDatesField=%d overlay=%d",
                             js.find("exDates") != std::string::npos ? 1 : 0, (int)app.overlay);
                } else {
                    DebugLog(L"probe-recur-preview: chip not found");
                    app.overlay = OV_NONE;
                    app.editor.open = false;
                }
            } else {
                DebugLog(L"probe-recur-preview: no occurrences");
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2439) {
            KillTimer(g_hwnd, 2439);
            std::wstring bakDir = PathJoin(GetDataDir(), L"autobak");
            app.store.settings.autoBackupDir = bakDir;
            DoSaveStore();
            AutoBackupTick();
            AutoBackupTick();
            int files = 0;
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(PathJoin(bakDir, L"星历备份-*.json").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++files; } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            bool okj = false;
            std::string cal = ReadFileBytes(GetCalendarPath(), &okj);
            DebugLog(L"probe-autobackup: files=%d dirSaved=%d", files,
                     cal.find("autoBackupDir") != std::string::npos ? 1 : 0);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2437) {
            KillTimer(g_hwnd, 2437);
            OpenAccountSync();
            app.syncUi.server = L"http://127.0.0.1:17890";
            app.syncUi.user = L"probe";
            app.syncUi.password = L"pass123";
            app.syncUi.status = L"示例：登录后上传/拉取";
            app.syncUi.error.clear();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-account-ui.png"));
            app.overlay = OV_NONE;
            bool okr = false;
            std::string data = ReadFileBytes(GetCalendarPath(), &okr);
            ApplyPulledStore(data);
            DebugLog(L"probe-account-ui: overlayShot=1 apply=%d events=%d status=%s", okr ? 1 : 0,
                     (int)app.store.events.size(), app.syncUi.status.c_str());
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2435) {
            KillTimer(g_hwnd, 2435);
            Tag* hw = app.store.FindTag("homework");
            if (hw) {
                hw->task = true;
                hw->customIcon = L"icons/test.png";
                app.store.Touch();
                DoSaveStore();
            }
            bool okJson = false;
            std::string rawJson = ReadFileBytes(PathJoin(GetDataDir(), L"calendar.json"), &okJson);
            DebugLog(L"probe-tags-ui: savedTask=%d savedIcon=%d",
                     rawJson.find("\"task\"") != std::string::npos ? 1 : 0,
                     rawJson.find("customIcon") != std::string::npos ? 1 : 0);
            app.tagEdit.open = true;
            app.tagEdit.isNew = false;
            app.tagEdit.editingId = "homework";
            app.tagEdit.name = L"作业";
            app.tagEdit.color = L"#FFB454";
            app.tagEdit.icon = hw ? hw->icon : L"edit";
            app.tagEdit.task = true;
            app.tagEdit.customIcon = L"icons/test.png";
            app.overlay = OV_TAGEDIT;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-tags-ui.png"));
            app.overlay = OV_NONE;
            app.tagEdit.open = false;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-tags-main.png"));
            DebugLog(L"probe-tags-ui: done");
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2433) {
            KillTimer(g_hwnd, 2433);
            ShareProbeStart(17899);
            Sleep(300);
            std::wstring srv = L"http://127.0.0.1:17899";
            std::wstring err;
            bool reg = SyncAccountRegister(srv, L"probe", L"pass123", err);
            if (!reg) reg = SyncAccountLogin(srv, L"probe", L"pass123", err);
            DebugLog(L"probe-account-sync: auth=%d err=%s tokenLen=%d", reg ? 1 : 0, err.c_str(), (int)app.syncUi.token.size());
            if (reg) {
                bool pushed = SyncPushStore(srv, app.syncUi.token, app.store.ToJsonStr(), err);
                DebugLog(L"probe-account-sync: push=%d err=%s", pushed ? 1 : 0, err.c_str());
                std::string pulled;
                bool pullOk = SyncPullStore(srv, app.syncUi.token, pulled, err);
                JValue pv = JsonParse(pulled);
                int n = (pv.IsObj() && pv.Has(L"events") && pv.Get(L"events").IsArr()) ? (int)pv.Get(L"events").arr.size() : -1;
                DebugLog(L"probe-account-sync: pull=%d events=%d err=%s", pullOk ? 1 : 0, n, err.c_str());
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2431) {
            KillTimer(g_hwnd, 2431);
            app.view = VIEW_AGENDA;
            app.selected.clear();
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            float ex = 0, ey = 0;
            bool found = false;
            for (auto& h : g_hits) {
                if (h.cmd >= CMD_EVENT_BASE && h.cmd < CMD_EVENT_BASE + 5000) {
                    ex = h.r.X + h.r.Width / 2;
                    ey = h.r.Y + h.r.Height / 2;
                    found = true;
                    break;
                }
            }
            if (found) {
                LPARAM lp = MAKELPARAM((int)ex, (int)ey);
                BYTE ks[256];
                GetKeyboardState(ks);
                ks[VK_SHIFT] = 0x80;
                SetKeyboardState(ks);
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                ks[VK_SHIFT] = 0;
                SetKeyboardState(ks);   // 先松 Shift，再松鼠标
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                DebugLog(L"probe-shift-order: selected=%d overlay=%d shiftPressRecorded=%d",
                         (int)app.selected.size(), (int)app.overlay, g_shiftAtPress ? 1 : 0);
            } else {
                DebugLog(L"probe-shift-order: row not found hits=%d", (int)g_hits.size());
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2429) {
            KillTimer(g_hwnd, 2429);
            OpenTemplateNewMenu();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            DebugLog(L"probe-templates: menu overlay=%d items=%d", (int)app.overlay, (int)app.menu.items.size());
            float mx = 0, my = 0;
            if (GetHitPointByCmd(CMD_MENU_ITEM_BASE + 0, mx, my)) {
                LPARAM lp = MAKELPARAM((int)mx, (int)my);
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                DebugLog(L"probe-templates: after pick overlay=%d title=%s time1=%s", (int)app.overlay,
                         app.editor.title.c_str(), app.editor.time1.c_str());
                SaveEditorAsTemplate();
                bool fok = false;
                std::string tj = ReadFileBytes(PathJoin(GetDataDir(), L"event-templates.json"), &fok);
                DebugLog(L"probe-templates: saved ok=%d bytes=%d", fok ? 1 : 0, (int)tj.size());
                app.editor.open = false;
                app.overlay = OV_NONE;
            } else {
                DebugLog(L"probe-templates: menu item hit not found");
            }
            OpenLocationPresetMenu();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            DebugLog(L"probe-templates: locmenu overlay=%d items=%d", (int)app.overlay, (int)app.menu.items.size());
            app.overlay = OV_NONE;
            app.menu.open = false;
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2426) {
            KillTimer(g_hwnd, 2426);
            app.view = VIEW_AGENDA;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            float ex = 0, ey = 0;
            if (GetHitPointByCmd(CMD_EVENT_BASE + 0, ex, ey)) {
                keybd_event(VK_SHIFT, 0, 0, 0);
                LPARAM lp = MAKELPARAM((int)ex, (int)ey);
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
                DebugLog(L"probe-multiselect2: shiftClick selected=%d", (int)app.selected.size());
            } else {
                DebugLog(L"probe-multiselect2: row hit not found");
            }
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureWindowPng(g_hwnd, PathJoin(GetDataDir(), L"probe-multi2.png"));
            float bx = 0, by2 = 0;
            if (GetHitPointByCmd(CMD_AGENDA_BOARD_VIEW, bx, by2)) {
                LPARAM lp2 = MAKELPARAM((int)bx, (int)by2);
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp2);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp2);
                SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, lp2);
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp2);
                DebugLog(L"probe-multiselect2: dblclickButton board=%d swallow=%d", app.agendaBoard ? 1 : 0, g_swallowNextUp ? 1 : 0);
            } else {
                DebugLog(L"probe-multiselect2: board hit not found");
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2425) {
            KillTimer(g_hwnd, 2425);
            InvalidateOccs();
            DateTime cf = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, cf, AddDaysDT(cf, 60));
            Event testEv;
            testEv.title = L"冲突测试";
            testEv.start = MakeDT(2026, 10, 19, 18, 15);
            testEv.end = MakeDT(2026, 10, 19, 18, 45);
            auto conflicts = FindEventConflicts(testEv);
            DebugLog(L"probe-conflict: detect count=%d first=%s", (int)conflicts.size(),
                     conflicts.empty() ? L"-" : conflicts[0].c_str());
            if (!app.occs.empty()) {
                OpenEditorForOccurrence(app.occs[0]);
                app.editor.time1 = L"18:30";
                app.editor.time2 = L"19:30";
                EditorSave();
                DebugLog(L"probe-conflict: editor save overlay=%d", (int)app.overlay);
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2423) {
            KillTimer(g_hwnd, 2423);
            InvalidateOccs();
            DateTime cf = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, cf, AddDaysDT(cf, 60));
            app.eventClipHistory.clear();
            for (int k = 0; k < (int)app.occs.size() && k < 3; ++k) CopyEventToClip(app.occs[k], 0);
            DebugLog(L"probe-clip-history: size=%d", (int)app.eventClipHistory.size());
            for (auto& c : app.eventClipHistory)
                DebugLog(L"probe-clip-history: item=%s start=%s", c.label.c_str(), ISODT(c.ev.start).c_str());
            if (app.eventClipHistory.size() >= 2) {
                EventClipState pick = app.eventClipHistory[1];
                DateTime target = AddDaysDT(StartOfDay(pick.ev.start), 3);
                PasteClipAt(pick, target);
                if (!app.store.events.empty()) {
                    const Event& ne = app.store.events.back();
                    DebugLog(L"probe-clip-history: paste=%s start=%s target=%s", ne.title.c_str(),
                             ISODT(ne.start).c_str(), ISODT(target).c_str());
                }
                ShowPasteHistoryMenu(target);
                DebugLog(L"probe-clip-history: menu overlay=%d items=%d", (int)app.overlay, (int)app.menu.items.size());
                app.overlay = OV_NONE;
                app.menu.open = false;
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2421) {
            KillTimer(g_hwnd, 2421);
            app.agendaFilter.range = 2;
            app.agendaFilter.done = 1;
            app.agendaScroll = 0;
            app.view = VIEW_AGENDA;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SetTimer(g_hwnd, 2422, 400, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2422) {
            KillTimer(g_hwnd, 2422);
            std::wstring path = PathJoin(GetDataDir(), L"probe-agenda-filter.png");
            CaptureWindowPng(g_hwnd, path);
            DebugLog(L"probe-agenda-filter: shot=%s range=%d done=%d", path.c_str(), app.agendaFilter.range, app.agendaFilter.done);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2420) {
            KillTimer(g_hwnd, 2420);
            const wchar_t* samples[] = {
                L"明天 15:00-16:00 高数作业 #作业 地点 A306 提前10分钟",
                L"每周一三五 19:00 晚自习 在 图书馆 提前半小时",
                L"每月5号 10:00 交报告 标签 作业",
                L"每两周 周三 14:00 组会 教室 B202",
            };
            for (auto* sp : samples) {
                QuickResult q = ParseQuick(sp, Today(), &app.store);
                if (!q.ok) { DebugLog(L"probe-quick: ERR %s => %s", sp, q.error.c_str()); continue; }
                DebugLog(L"probe-quick: %s => title=%s loc=%s start=%s end=%s freq=%d byDay=%d byMonthDay=%d alarms=%d tags=%d",
                    sp, q.ev.title.c_str(), q.ev.location.c_str(), ISODT(q.ev.start).c_str(), ISODT(q.ev.end).c_str(),
                    q.ev.recur.freq, (int)q.ev.recur.byDay.size(), q.ev.recur.byMonthDay, (int)q.ev.alarms.size(), (int)q.ev.tags.size());
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2419) {
            KillTimer(g_hwnd, 2419);
            OpenBulkAdd();
            int d20 = DaySerial(2026, 10, 20);
            int d22 = DaySerial(2026, 10, 22);
            app.bulk.days = { d20, d22 };
            app.bulk.title = L"批量覆盖测试";
            app.bulk.location = L"A101";
            app.bulk.startText = L"18:00";
            app.bulk.endText = L"20:00";
            app.bulk.perEditSerial = d22;
            app.bulk.editStart = L"19:00";
            app.bulk.editEnd = L"21:00";
            app.bulk.editLocation = L"B202";
            app.overlay = OV_BULKADD;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            float bx = 0, byy = 0;
            if (GetHitPointByCmd(CMD_BULK_OK, bx, byy)) {
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)bx, (int)byy));
                for (auto& e : app.store.events) {
                    if (e.title != L"批量覆盖测试") continue;
                    DebugLog(L"probe-bulk-day: start=%s end=%s loc=%s", ISODT(e.start).c_str(),
                             ISODT(e.end).c_str(), e.location.c_str());
                }
            } else {
                DebugLog(L"probe-bulk-day: create button not found");
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2416) {
            KillTimer(g_hwnd, 2416);
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            float ex = 0, ey = 0;
            if (GetHitPointByCmd(CMD_EVENT_BASE + 0, ex, ey)) {
                LPARAM lp = MAKELPARAM((int)ex, (int)ey);
                SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
                DebugLog(L"probe-misclick: dblclick event overlay=%d isNew=%d", (int)app.overlay, app.editor.isNew ? 1 : 0);
            } else {
                DebugLog(L"probe-misclick: event chip not found");
            }
            app.editor.open = false;
            app.overlay = OV_NONE;
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            int before = (int)app.store.events.size();
            OpenConfirm(L"误触测试", L"关闭确认框后快速第二下不应穿透到底层。", []() {}, L"确定", L"取消");
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            float bx = 0, byy = 0;
            if (GetHitPointByCmd(CMD_CONFIRM_OK, bx, byy)) {
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)bx, (int)byy));
                SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(800, 500));
                SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(800, 500));
                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM(800, 500));
                DebugLog(L"probe-misclick: penetrate overlay=%d editor=%d events=%d(before=%d)",
                         (int)app.overlay, app.editor.open ? 1 : 0, (int)app.store.events.size(), before);
            } else {
                DebugLog(L"probe-misclick: confirm ok not found overlay=%d", (int)app.overlay);
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2414) {
            KillTimer(g_hwnd, 2414);
            InvalidateOccs();
            DateTime mf = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, mf, AddDaysDT(mf, 60));
            for (int k = 0; k < (int)app.occs.size() && k < 3; ++k) {
                app.selected.insert(OccKey(app.occs[k]));
            }
            app.view = VIEW_AGENDA;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SetTimer(g_hwnd, 2415, 400, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2415) {
            KillTimer(g_hwnd, 2415);
            std::wstring path = PathJoin(GetDataDir(), L"probe-multi.png");
            CaptureWindowPng(g_hwnd, path);
            DebugLog(L"probe-multi: shot=%s selected=%d", path.c_str(), (int)app.selected.size());
            {
                float bx = 0, by = 0;
                if (GetHitPointByCmd(CMD_SELECTED_BASE + 0, bx, by)) {
                    LPARAM lp = MAKELPARAM((int)bx, (int)by);
                    SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                    SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                    SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
                    SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp);
                    DebugLog(L"probe-multi: dblclick donebox overlay=%d done=%d", (int)app.overlay,
                             app.occs.empty() ? -1 : (app.occs[0].ev.done ? 1 : 0));
                } else {
                    DebugLog(L"probe-multi: donebox not found");
                }
            }
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2411) {
            KillTimer(g_hwnd, 2411);
            InvalidateOccs();
            DateTime df = StartOfDay(app.now);
            app.occs = ExpandOccurrences(app.store, df, AddDaysDT(df, 60));
            if (!app.occs.empty()) {
                Occurrence o = app.occs[0];
                bool before = o.ev.done;
                ToggleOccurrenceDone(o, true);
                app.occs = ExpandOccurrences(app.store, df, AddDaysDT(df, 60));
                bool found = false;
                for (auto& oo : app.occs) {
                    if (oo.seriesId == o.seriesId && CompareDT(oo.start, o.start) == 0) {
                        found = true;
                        DebugLog(L"probe-done: before=%d after=%d title=%s", before ? 1 : 0, oo.ev.done ? 1 : 0, oo.ev.title.c_str());
                        break;
                    }
                }
                if (!found) DebugLog(L"probe-done: occurrence not found after toggle");
            }
            app.view = VIEW_AGENDA;
            InvalidateOccs();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SetTimer(g_hwnd, 2413, 400, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2413) {
            KillTimer(g_hwnd, 2413);
            std::wstring path = PathJoin(GetDataDir(), L"probe-done.png");
            CaptureWindowPng(g_hwnd, path);
            DebugLog(L"probe-done: shot=%s", path.c_str());
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2410) {
            KillTimer(g_hwnd, 2410);
            InvalidateOccs();
            {
                DateTime pfrom = StartOfDay(app.now);
                app.occs = ExpandOccurrences(app.store, pfrom, AddDaysDT(pfrom, 60));
            }
            int before = (int)app.store.events.size();
            if (!app.occs.empty()) {
                Occurrence o = app.occs[0];
                for (auto& oo : app.occs) { if (!oo.ev.allDay) { o = oo; break; } }
                CopyEventToClip(o, 0);
                DateTime target = AddDaysDT(StartOfDay(o.start), 2);
                PasteEventClip(target);
                if (!app.store.events.empty()) {
                    const Event& ne = app.store.events.back();
                    int tdiff = (ne.start.h * 60 + ne.start.mi) - (o.start.h * 60 + o.start.mi);
                    int durSrc = MinutesBetween(o.start, o.end);
                    int durNew = MinutesBetween(ne.start, ne.end);
                    DebugLog(L"probe-clip: single src=%s target=%s new=%s newEnd=%s timeDelta=%d durSrc=%d durNew=%d loc=%s color=%s tags=%d",
                        ISODT(o.start).c_str(), ISODT(target).c_str(), ISODT(ne.start).c_str(), ISODT(ne.end).c_str(), tdiff,
                        durSrc, durNew, ne.location.c_str(), ne.color.c_str(), (int)ne.tags.size());
                }
            }
            for (auto& base : app.store.events) {
                if (!base.IsRecurring()) continue;
                for (auto& o : app.occs) {
                    if (o.seriesId != base.id) continue;
                    CopyEventToClip(o, 1);
                    DateTime target = AddDaysDT(StartOfDay(o.start), 7);
                    PasteEventClip(target);
                    if (!app.store.events.empty()) {
                        const Event& ne = app.store.events.back();
                        int tdiff = (ne.start.h * 60 + ne.start.mi) - (o.start.h * 60 + o.start.mi);
                        DebugLog(L"probe-clip: series src=%s target=%s new=%s freq=%d timeDelta=%d color=%s",
                            ISODT(o.start).c_str(), ISODT(target).c_str(), ISODT(ne.start).c_str(), ne.recur.freq, tdiff, ne.color.c_str());
                    }
                    break;
                }
                break;
            }
            DebugLog(L"probe-clip: events before=%d after=%d", before, (int)app.store.events.size());
            ShowDayMenu(DaySerial(2026, 10, 22));
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SetTimer(g_hwnd, 2412, 350, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2412) {
            KillTimer(g_hwnd, 2412);
            std::wstring path = PathJoin(GetDataDir(), L"probe-clip.png");
            CaptureWindowPng(g_hwnd, path);
            DebugLog(L"probe-clip: shot=%s overlay=%d", path.c_str(), (int)app.overlay);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2302) {







            KillTimer(g_hwnd, 2302);







            float x = 0, y = 0;







            int serial = DaySerial(2026, 10, 4);



            ShowDayMenu(serial);



            DebugLog(L"probe-daydetail: after ShowDayMenu overlay=%d date=%d-%d-%d", (int)app.overlay, app.dayDetail.y, app.dayDetail.mo, app.dayDetail.d);







            // 真实消息路径：右键一个没有日程的日期格，必须也能打开当天详情



            app.overlay = OV_NONE;



            app.cursor = MakeDT(2026, 10, 18);



            app.view = VIEW_MONTH;



            InvalidateOccs();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            DateTime blank;



            bool found = false;



            for (int d = 1; d <= 31 && !found; ++d) {



                DateTime day = MakeDT(2026, 10, d);



                if (ExpandOccurrences(app.store, day, AddDaysDT(day, 1)).empty()) { blank = day; found = true; }



            }



            bool hit = false;



            if (found) hit = GetHitPointByCmd(CMD_DAY_BASE + DaySerial(blank.y, blank.mo, blank.d), x, y);



            if (hit) SendMessageW(g_hwnd, WM_RBUTTONUP, 0, MAKELPARAM((int)x, (int)y));



            DebugLog(L"probe-daydetail: rightClickBlank day=%d-%d-%d found=%d hit=%d overlay=%d",



                     blank.y, blank.mo, blank.d, found ? 1 : 0, hit ? 1 : 0, (int)app.overlay);







            SetTimer(g_hwnd, 2303, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2303) {







            KillTimer(g_hwnd, 2303);







            DebugLog(L"probe-daydetail: overlay=%d date=%d-%d-%d occs=%d info=%s", (int)app.overlay,







                     app.dayDetail.y, app.dayDetail.mo, app.dayDetail.d, (int)app.dayDetailOccs.size(),







                     SolarTermNameOn(app.dayDetail).c_str());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2201) {







            KillTimer(g_hwnd, 2201);







            OpenEditorNew(Today(), 9, 0);







            app.editor.title = L"农历生日测试";







            SetTimer(g_hwnd, 2202, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2202) {







            KillTimer(g_hwnd, 2202);







            // 生日标签下标（默认标签表里 birthday 是第 15 个，索引 14）







            int idx = -1;







            for (int i = 0; i < (int)app.store.tags.size(); ++i)







                if (app.store.tags[i].id == "birthday") idx = i;







            float x = 0, y = 0;







            bool found = idx >= 0 && GetHitPointByCmd(CMD_ED_TAG_BASE + idx, x, y);







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            DebugLog(L"probe-lunar-editor: birthdayTag=%d idx=%d freq=%d", found ? 1 : 0, idx, app.editor.freq);







            SetTimer(g_hwnd, 2203, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2203) {







            DebugLog(L"probe-lunar-editor: after tag click freq=%d tags=%d", app.editor.freq, (int)app.editor.tags.size());







            KillTimer(g_hwnd, 2203);







            app.editor.freq = RF_YEARLY;







            app.editor.lunar = true;







            app.editor.lunarDate = L"2005-08-15";







            DebugLog(L"probe-lunar-editor: before save freq=%d lunar=%d lunarDate=%s", app.editor.freq, app.editor.lunar ? 1 : 0, app.editor.lunarDate.c_str());







            EditorSave();







            SetTimer(g_hwnd, 2204, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2204) {







            KillTimer(g_hwnd, 2204);







            const Event* ev = nullptr;







            for (auto& e : app.store.events) if (e.title == L"农历生日测试") ev = &e;







            if (ev) DebugLog(L"probe-lunar-editor: saved start=%d-%d-%d lunar=%d tags=%d", ev->start.y, ev->start.mo, ev->start.d, ev->recur.lunar ? 1 : 0, (int)ev->tags.size());







            else DebugLog(L"probe-lunar-editor: saved NOT FOUND");







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2101) {







            KillTimer(g_hwnd, 2101);







            auto terms = SolarTermDatesOfYear(2026);







            for (size_t i = 0; i < terms.size(); ++i)







                DebugLog(L"probe-solar: %04d-%02d-%02d", terms[i].y, terms[i].mo, terms[i].d);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2001) {







            KillTimer(g_hwnd, 2001);







            OpenSettings();







            SetTimer(g_hwnd, 2002, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2002) {







            KillTimer(g_hwnd, 2002);







            float x = 0, y = 0;







            bool found = GetHitPointByCmd(CMD_SET_RESTORE_BACKUP, x, y);







            DebugLog(L"probe-restore: restoreBtn=%d events=%d", found ? 1 : 0, (int)app.store.events.size());







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            SetTimer(g_hwnd, 2003, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2003) {







            KillTimer(g_hwnd, 2003);







            float x = 0, y = 0;







            bool found = GetHitPointByCmd(CMD_CONFIRM_OK, x, y);







            DebugLog(L"probe-restore: confirmBtn=%d", found ? 1 : 0);







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            SetTimer(g_hwnd, 2004, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 2004) {







            KillTimer(g_hwnd, 2004);







            DebugLog(L"probe-restore: after events=%d", (int)app.store.events.size());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4901) {



            KillTimer(g_hwnd, 4901);



            g_rawKeyboard = false;



            // 焦点回归测试：弹层里的 EDIT 失焦后，关闭弹层焦点应回到主窗口，方向键继续切月



            OpenFocusPanel();



            UIInvalidate();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            UISetEditFocus(9980, false);



            bool editFocus = (GetFocus() != g_hwnd);



            CloseOverlay();



            bool mainFocus = (GetFocus() == g_hwnd);



            app.view = VIEW_MONTH;



            app.cursor = MakeDT(2026, 10, 18);



            InvalidateOccs();



            int m0 = app.cursor.mo;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_RIGHT, 0);



            int m1 = app.cursor.mo;



            DebugLog(L"probe-inputfix: editFocus=%d mainFocus=%d arrowChanged=%d (%d->%d)", editFocus ? 1 : 0, mainFocus ? 1 : 0, (m0 != m1) ? 1 : 0, m0, m1);



            // 月视图空白格双击新建



            app.overlay = OV_NONE;



            app.view = VIEW_MONTH;



            app.cursor = MakeDT(2026, 10, 18);



            InvalidateOccs();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            int bserial = -1;



            for (int d = 1; d <= 31; ++d) { DateTime dd = MakeDT(2026, 10, d); if (ExpandOccurrences(app.store, dd, AddDaysDT(dd, 1)).empty()) { bserial = DaySerial(2026, 10, d); break; } }



            float bx = 0, by = 0;



            bool bhit = (bserial >= 0) && GetHitPointByCmd(CMD_DAY_BASE + bserial, bx, by);



            if (bhit) SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, 0, MAKELPARAM((int)bx, (int)by));



            DebugLog(L"probe-inputfix: dblMonth hit=%d overlay=%d editorOpen=%d", bhit ? 1 : 0, (int)app.overlay, app.editor.open ? 1 : 0);



            CloseOverlay();



            // 周视图空白时间段双击新建



            app.view = VIEW_WEEK;



            InvalidateOccs();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            bool wfound = false; DateTime wd; int wmin = 0; float wx = 0, wy = 0;



            for (float yy = S(200); yy < S(900) && !wfound; yy += S(30))



                for (float xx = S(420); xx < S(1500) && !wfound; xx += S(40))



                    if (WeekViewHitTime(xx, yy, wd, wmin)) { wx = xx; wy = yy; wfound = true; }



            if (wfound) SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, 0, MAKELPARAM((int)wx, (int)wy));



            DebugLog(L"probe-inputfix: dblWeek found=%d overlay=%d editorOpen=%d", wfound ? 1 : 0, (int)app.overlay, app.editor.open ? 1 : 0);



            CloseOverlay();



            if (wfound) SendMessageW(g_hwnd, WM_RBUTTONUP, 0, MAKELPARAM((int)wx, (int)wy));



            DebugLog(L"probe-inputfix: rbtnWeek found=%d overlay=%d", wfound ? 1 : 0, (int)app.overlay);



            CloseOverlay();







            CloseOverlay();



            // 日视图空白时间段双击新建



            app.view = VIEW_DAY;



            InvalidateOccs();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            bool dfound = false; DateTime dd; int dmin = 0; float dx = 0, dy = 0;



            for (float yy = S(200); yy < S(900) && !dfound; yy += S(30))



                for (float xx = S(420); xx < S(1500) && !dfound; xx += S(40))



                    if (WeekViewHitTime(xx, yy, dd, dmin)) { dx = xx; dy = yy; dfound = true; }



            if (dfound) SendMessageW(g_hwnd, WM_LBUTTONDBLCLK, 0, MAKELPARAM((int)dx, (int)dy));



            DebugLog(L"probe-inputfix: dblDay found=%d overlay=%d editorOpen=%d", dfound ? 1 : 0, (int)app.overlay, app.editor.open ? 1 : 0);



            CloseOverlay();



            if (dfound) SendMessageW(g_hwnd, WM_RBUTTONUP, 0, MAKELPARAM((int)dx, (int)dy));



            DebugLog(L"probe-inputfix: rbtnDay found=%d overlay=%d", dfound ? 1 : 0, (int)app.overlay);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4902) {



            KillTimer(g_hwnd, 4902);



            g_rawKeyboard = false;   // 该探针测普通消息路径



            HWND f = GetFocus();



            wchar_t cls[64] = {0};



            if (f) GetClassNameW(f, cls, 63);



            int fid = f ? GetDlgCtrlID(f) : -1;



            int m0 = app.cursor.mo;



            // 模拟真实键盘：把方向键发给当前焦点窗口



            SendMessageW(f ? f : g_hwnd, WM_KEYDOWN, VK_RIGHT, 0);



            int m1 = app.cursor.mo;



            DebugLog(L"probe-focusstart: focusMain=%d focusId=%d class=%s monthChanged=%d (%d->%d) searchLen=%d",



                     (f == g_hwnd) ? 1 : 0, fid, cls, (m0 != m1) ? 1 : 0, m0, m1, (int)app.search.size());







            // 焦点逻辑测试：搜索框被程序/系统抢到焦点但没有用户意图时，定时器必须把焦点还给主窗口



            app.searchEditing = true;



            app.searchIntent = false;



            UIInvalidate();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            HWND se = GetDlgItem(g_hwnd, 18001);



            if (se) SetFocus(se);



            SendMessageW(g_hwnd, WM_TIMER, 1, 0);



            bool autoGuard = (GetFocus() == g_hwnd);



            // 用户主动点搜索框时，焦点保留在搜索框，方向键用于文本光标



            app.searchEditing = true;



            app.searchIntent = true;



            if (se) SetFocus(se);



            SendMessageW(g_hwnd, WM_TIMER, 1, 0);



            bool intentKept = (se && GetFocus() == se);



            app.searchIntent = false;



            app.searchEditing = false;



            SetFocus(g_hwnd);







        AppEnableIme(false);



            // 周/日视图：焦点在主窗口时方向键必须切换日期



            app.view = VIEW_WEEK;



            app.cursor = MakeDT(2026, 10, 18);



            int wd0 = DaySerial(app.cursor.y, app.cursor.mo, app.cursor.d);



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_RIGHT, 0);



            int wd1 = DaySerial(app.cursor.y, app.cursor.mo, app.cursor.d);



            app.view = VIEW_DAY;



            app.cursor = MakeDT(2026, 10, 18);



            int dd0 = DaySerial(app.cursor.y, app.cursor.mo, app.cursor.d);



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_RIGHT, 0);



            int dd1 = DaySerial(app.cursor.y, app.cursor.mo, app.cursor.d);



            DebugLog(L"probe-focusstart: autoStolenGuard=%d intentKept=%d weekArrow=%d dayArrow=%d",



                     autoGuard ? 1 : 0, intentKept ? 1 : 0, (wd1 != wd0) ? 1 : 0, (dd1 != dd0) ? 1 : 0);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4982) {

            KillTimer(g_hwnd, 4982);

            OpenSettings();

            UIInvalidate();

            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

            SetTimer(g_hwnd, 4983, 6000, nullptr);

            continue;

        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4983) {

            KillTimer(g_hwnd, 4983);

            g_quitting = true;

            DestroyWindow(g_hwnd);

            continue;

        }

        

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4993) {
            KillTimer(g_hwnd, 4993);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4994) {
            KillTimer(g_hwnd, 4994);
            g_rawKeyboard = false;
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_OEM_2, 0);
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SetTimer(g_hwnd, 4995, 700, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4995) {
            KillTimer(g_hwnd, 4995);
            HWND e = GetDlgItem(g_hwnd, 18701);
            SetWindowTextW(e, L"/");
            app.slash.text = L"/";
            SendMessageW(e, EM_SETSEL, 1, 1);
            SendMessageW(e, WM_CHAR, (WPARAM)L'v', 0);
            SendMessageW(e, WM_CHAR, (WPARAM)L'i', 0);
            SendMessageW(e, WM_CHAR, (WPARAM)L'e', 0);
            SendMessageW(e, WM_CHAR, (WPARAM)L'w', 0);
            SendMessageW(e, WM_KEYDOWN, VK_TAB, 0);
            SendMessageW(e, WM_KEYDOWN, VK_TAB, 0);
            std::wstring tabText = app.slash.text;
            SetWindowTextW(e, L"/");
            app.slash.text = L"/";
            SendMessageW(e, EM_SETSEL, 1, 1);
            app.slash.text = L"/";
            SendMessageW(e, EM_SETSEL, 1, 1);
            app.slash.lastText.clear();
            int wheelBefore = app.slash.sel;
            SendMessageW(e, WM_MOUSEWHEEL, MAKEWPARAM(0, (WORD)(-WHEEL_DELTA)), MAKELPARAM(500, 400));
            int wheelAfter = app.slash.sel;
            SetWindowTextW(e, L"/");
            app.slash.text = L"/";
            SendMessageW(e, EM_SETSEL, 1, 1);
            app.slash.text = L"/";
            SendMessageW(e, EM_SETSEL, 1, 1);
            SendMessageW(e, WM_CHAR, (WPARAM)L'/', 0);
            SendMessageW(e, WM_CHAR, (WPARAM)L'h', 0);
            SendMessageW(e, WM_CHAR, (WPARAM)L'e', 0);
            SendMessageW(e, WM_CHAR, (WPARAM)L'l', 0);
            SendMessageW(e, WM_CHAR, (WPARAM)L'p', 0);
            std::wstring typedText = app.slash.text;
            SendMessageW(e, WM_KEYDOWN, VK_RETURN, 0);
            bool helpOverlay = (app.overlay == OV_HELP);
            DebugLog(L"probe-slashui: tabText=[%s] wheelBefore=%d wheelAfter=%d typedText=[%s] helpOverlay=%d",
                     tabText.c_str(), wheelBefore, wheelAfter, typedText.c_str(), helpOverlay ? 1 : 0);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }
        
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4990) {
            KillTimer(g_hwnd, 4990);
            g_rawKeyboard = false;
            SendMessageW(g_hwnd, WM_KEYDOWN, VK_OEM_2, 0);
            bool opened = (app.overlay == OV_SLASH);
            if (g_probeSlashShot) {
                DebugLog(L"probe-slash: shot opened=%d", opened ? 1 : 0);
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                SaveScreenshot(L"E:\\myCalendar\\shots\\slash-panel.png");
                if (HWND se = GetDlgItem(g_hwnd, 18701)) {
                    SendMessageW(se, WM_CHAR, (WPARAM)L'g', 0);
                    SendMessageW(se, WM_CHAR, (WPARAM)L'o', 0);
                    SendMessageW(se, WM_CHAR, (WPARAM)L't', 0);
                }
                RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                SaveScreenshot(L"E:\\myCalendar\\shots\\slash-typed.png");
                DebugLog(L"probe-slash: typedText=%s", app.slash.text.c_str());
                SetTimer(g_hwnd, 4993, 8000, nullptr);
                continue;
            }
            bool searchEditingBefore = app.searchEditing;
            bool searchEditExists = GetDlgItem(g_hwnd, 18001) != nullptr;
            DebugLog(L"probe-slash: opened=%d searchEditing=%d searchEditExists=%d", opened ? 1 : 0, searchEditingBefore ? 1 : 0, searchEditExists ? 1 : 0);
            CloseSlashPalette();
            std::wstring err;
            bool helpOk = SlashProbeRun(L"help goto", err);
            bool todayOk = SlashProbeRun(L"today", err);
            bool gotoOk = SlashProbeRun(L"goto 2026-10-18", err);
            bool viewOkCmd = SlashProbeRun(L"view week", err);
            bool monthOk = SlashProbeRun(L"month", err);
            bool dumpOkCmd = SlashProbeRun(L"dump probe-dump.json", err);
            bool checkOk = SlashProbeRun(L"check", err);
            std::wstring badErr;
            bool badRejected = (!SlashProbeRun(L"nope", badErr)) && !badErr.empty();
            bool dateOk = (app.cursor.y == 2026 && app.cursor.mo == 10 && app.cursor.d == 18);
            bool finalViewOk = (app.view == VIEW_MONTH);
            bool dumpFile = FileExists(PathJoin(GetDataDir(), L"probe-dump.json"));
            DebugLog(L"probe-slash: help=%d today=%d goto=%d view=%d month=%d dump=%d check=%d bad=%d dateOk=%d finalViewOk=%d dumpFile=%d",
                     helpOk ? 1 : 0, todayOk ? 1 : 0, gotoOk ? 1 : 0, viewOkCmd ? 1 : 0, monthOk ? 1 : 0,
                     dumpOkCmd ? 1 : 0, checkOk ? 1 : 0, badRejected ? 1 : 0, dateOk ? 1 : 0, finalViewOk ? 1 : 0, dumpFile ? 1 : 0);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }
        
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4991) {
            KillTimer(g_hwnd, 4991);
            OpenCommandPalette();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            for (int k = 0; k < 2; ++k)
                SendMessageW(g_hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, (WORD)(-WHEEL_DELTA)), MAKELPARAM(600, 400));
            DebugLog(L"probe-cmdkwheel: open=1 sel=%d", app.cmdk.sel);
            SetTimer(g_hwnd, 4992, 800, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4992) {
            KillTimer(g_hwnd, 4992);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4989) {
            KillTimer(g_hwnd, 4989);
            OpenHelp();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SaveScreenshot(L"E:\\myCalendar\\shots\\help-new.png");
            SetTimer(g_hwnd, 4990, 1200, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4987) {
            KillTimer(g_hwnd, 4987);
            OpenCommandPalette();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SaveScreenshot(L"E:\\myCalendar\\shots\\cmdk-all.png");
            SetTimer(g_hwnd, 4988, 1200, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4990) {
            KillTimer(g_hwnd, 4990);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4988) {
            KillTimer(g_hwnd, 4988);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4985) {
            KillTimer(g_hwnd, 4985);
            OpenSharePanel();
            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            SaveScreenshot(L"E:\\myCalendar\\shots\\share-panel.png");
            SetTimer(g_hwnd, 4986, 1500, nullptr);
            continue;
        }
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4986) {
            KillTimer(g_hwnd, 4986);
            g_quitting = true;
            DestroyWindow(g_hwnd);
            continue;
        }
        
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4980) {

            KillTimer(g_hwnd, 4980);

            int total = (int)app.store.tags.size();

            bool hasInterview = app.store.FindTag("interview") != nullptr;

            bool allIcons = true;

            for (auto& t : app.store.tags) if (t.icon.empty()) allIcons = false;

            Tag* tag = app.store.FindTag("course");

            std::wstring before = tag ? tag->icon : L"";

            DebugLog(L"probe-tags: total=%d interview=%d allIcons=%d courseIcon=%s", total, hasInterview ? 1 : 0, allIcons ? 1 : 0, before.c_str());

            app.tagEdit.open = true;

            app.tagEdit.isNew = false;

            app.tagEdit.editingId = "course";

            app.tagEdit.name = L"课程";

            app.tagEdit.color = L"#6EA8FF";

            app.tagEdit.icon = before;

            app.tagEdit.error.clear();

            app.overlay = OV_TAGEDIT;

            UIInvalidate();

            SetTimer(g_hwnd, 4981, 4000, nullptr);

            continue;

        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4981) {

            KillTimer(g_hwnd, 4981);

            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

            std::wstring chosen = TagIconChoices()[8];

            float ix = 0, iy = 0;

            bool iconHit = GetHitPointByCmd(CMD_TAGEDIT_ICON_BASE + 8, ix, iy);

            if (iconHit) SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)ix, (int)iy));

            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

            float ox = 0, oy = 0;

            if (GetHitPointByCmd(CMD_TAGEDIT_OK, ox, oy))

                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)ox, (int)oy));

            Tag* saved = app.store.FindTag("course");

            bool changed = saved && saved->icon == chosen;

            Store copy;

            std::wstring jerr;

            std::string js = app.store.ToJsonStr();

            copy.FromJsonStr(js, &jerr);

            const Tag* round = copy.FindTag("course");

            bool persisted = round && round->icon == chosen;

            DebugLog(L"probe-tags: changed=%d persisted=%d icon=%s overlaySettings=%d", changed ? 1 : 0,

                     persisted ? 1 : 0, chosen.c_str(), (app.overlay == OV_SETTINGS) ? 1 : 0);

            g_quitting = true;

            DestroyWindow(g_hwnd);

            continue;

        }

        

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4970) {



            KillTimer(g_hwnd, 4970);



            OpenEditorNew(Today(), 9, 0);



            SetTimer(g_hwnd, 4971, 600, nullptr);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4971) {



            KillTimer(g_hwnd, 4971);



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            float ax = 0, ay = 0;



            bool hit = GetHitPointByCmd(CMD_ED_ALARM_ADD, ax, ay);



            if (hit) SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)ax, (int)ay));



            DebugLog(L"probe-alarm: addHit=%d menuOpen=%d", hit ? 1 : 0, app.menu.open ? 1 : 0);



            SetTimer(g_hwnd, 4972, 600, nullptr);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4972) {



            KillTimer(g_hwnd, 4972);



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            float mx = 0, my = 0;



            bool hit = GetHitPointByCmd(CMD_MENU_ITEM_BASE + 2, mx, my);



            if (hit) SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)mx, (int)my));



            DebugLog(L"probe-alarm: itemHit=%d overlayEditor=%d editorOpen=%d alarms=%d",



                     hit ? 1 : 0, (app.overlay == OV_EDITOR) ? 1 : 0, app.editor.open ? 1 : 0, (int)app.editor.alarms.size());



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4960) {



            KillTimer(g_hwnd, 4960);



            g_rawProbeActive = true;
            {
                HWND fg = GetForegroundWindow();
                DWORD fgTid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
                DWORD myTid = GetCurrentThreadId();
                if (fgTid && fgTid != myTid) {
                    AttachThreadInput(fgTid, myTid, TRUE);
                    SetForegroundWindow(g_hwnd);
                    SetFocus(g_hwnd);
                    AttachThreadInput(fgTid, myTid, FALSE);
                }
            }
            SetForegroundWindow(g_hwnd);



            SetFocus(g_hwnd);



            int before = DaySerial(app.cursor.y, app.cursor.mo, app.cursor.d);



            INPUT in[6] = {};



            for (int k = 0; k < 3; ++k) {



                in[k * 2].type = INPUT_KEYBOARD;



                in[k * 2].ki.wVk = VK_RIGHT;



                in[k * 2].ki.wScan = 0x4D;



                in[k * 2].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;



                in[k * 2 + 1].type = INPUT_KEYBOARD;



                in[k * 2 + 1].ki.wVk = VK_RIGHT;



                in[k * 2 + 1].ki.wScan = 0x4D;



                in[k * 2 + 1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;



            }



            SendInput(6, in, sizeof(INPUT));



            DebugLog(L"probe-rawkeys: before=%d nav=%d", before, g_rawNavCount);



            SetTimer(g_hwnd, 4961, 700, nullptr);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4961) {



            KillTimer(g_hwnd, 4961);



            DebugLog(L"probe-rawkeys: nav=%d cursor=%d-%d-%d", g_rawNavCount, app.cursor.y, app.cursor.mo, app.cursor.d);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 4950) {



            KillTimer(g_hwnd, 4950);



            int n = HelpEntryCount();



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F1, 0);



            bool f1 = (app.overlay == OV_HELP);



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F2, 0);



            bool f2 = (app.overlay == OV_STATS);



            app.overlay = OV_NONE;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F3, 0);



            bool f3 = (app.view == VIEW_AGENDA && app.agendaBoard);



            app.overlay = OV_NONE;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F4, 0);



            bool f4 = (app.overlay == OV_EXAMS);



            app.overlay = OV_NONE;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F5, 0);



            bool f5 = (app.overlay == OV_FOCUS);



            app.overlay = OV_NONE;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F6, 0);



            bool f6 = (app.overlay == OV_HABITS);



            app.overlay = OV_NONE;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F7, 0);



            bool f7 = (app.overlay == OV_PRINT);



            app.overlay = OV_NONE;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F8, 0);



            bool f8 = (app.overlay == OV_PROFILES);



            app.overlay = OV_NONE;



            SendMessageW(g_hwnd, WM_KEYDOWN, VK_F9, 0);



            bool f9 = (app.overlay == OV_REMINDERS);



            OpenHelp();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);



            float hx = 0, hy = 0;



            bool hit = GetHitPointByCmd(CMD_HELP_ITEM_BASE + 0, hx, hy);



            if (hit) SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)hx, (int)hy));



            bool click = (app.overlay == OV_CMDK);



            DebugLog(L"probe-help: entries=%d f1=%d f2=%d f3=%d f4=%d f5=%d f6=%d f7=%d f8=%d f9=%d clickCmdK=%d",



                     n, f1 ? 1 : 0, f2 ? 1 : 0, f3 ? 1 : 0, f4 ? 1 : 0, f5 ? 1 : 0, f6 ? 1 : 0, f7 ? 1 : 0, f8 ? 1 : 0, f9 ? 1 : 0, click ? 1 : 0);



            g_quitting = true;



            DestroyWindow(g_hwnd);



            continue;



        }



        



        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1901) {







            KillTimer(g_hwnd, 1901);







            RefreshOccs();







            DebugLog(L"probe-data: events=%d cursor=%d-%d occs=%d", (int)app.store.events.size(), app.cursor.y, app.cursor.mo, (int)app.occs.size());







            if (!app.store.events.empty()) {







                auto& e0 = app.store.events[0];







                DebugLog(L"probe-data: e0=%s start=%d-%d-%d %d:%d recur=%d", e0.title.c_str(), e0.start.y, e0.start.mo, e0.start.d, e0.start.h, e0.start.mi, e0.recur.freq);







            }







            auto wideOcc = ExpandOccurrences(app.store, MakeDT(2026, 9, 1), MakeDT(2026, 12, 31));







            DebugLog(L"probe-data: wideOcc=%d", (int)wideOcc.size());







            for (int i = 0; i < (int)app.occs.size() && i < 3; ++i)







                DebugLog(L"probe-data: occ %s %d-%d-%d", app.occs[i].ev.title.c_str(), app.occs[i].start.y, app.occs[i].start.mo, app.occs[i].start.d);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1801) {







            KillTimer(g_hwnd, 1801);







            app.cursor = MakeDT(1500, 1, 1);







            InvalidateOccs();







            RefreshOccs();







            DebugLog(L"probe-datejump: clamped year=%d", app.cursor.y);







            SetTimer(g_hwnd, 1802, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1802) {







            KillTimer(g_hwnd, 1802);







            OpenDateJump();







            app.dateJump = L"1582-10-05";







            UIInvalidate();







            SetTimer(g_hwnd, 1803, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1803) {







            KillTimer(g_hwnd, 1803);







            float x = 0, y = 0;







            bool found = GetHitPointByCmd(CMD_DJ_OK, x, y);







            DebugLog(L"probe-datejump: out-of-range okBtn=%d", found ? 1 : 0);







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            SetTimer(g_hwnd, 1804, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1804) {







            KillTimer(g_hwnd, 1804);







            DebugLog(L"probe-datejump: after 1582 overlay=%d cursor=%d-%d-%d", (int)app.overlay, app.cursor.y, app.cursor.mo, app.cursor.d);







            app.dateJump = L"2026-12-01";







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_DJ_OK, x, y)) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            SetTimer(g_hwnd, 1805, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1805) {







            KillTimer(g_hwnd, 1805);







            DebugLog(L"probe-datejump: after jump overlay=%d cursor=%d-%d-%d", (int)app.overlay, app.cursor.y, app.cursor.mo, app.cursor.d);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1701) {







            KillTimer(g_hwnd, 1701);







            OpenEditorNew(Today(), 9, 0);







            SetTimer(g_hwnd, 1702, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1702) {







            KillTimer(g_hwnd, 1702);







            HWND de = GetDlgItem(g_hwnd, 9000 + 9102);







            HWND te = GetDlgItem(g_hwnd, 9000 + 9101);







            DebugLog(L"probe-editor-input: dateEdit=%p titleEdit=%p", de, te);







            if (de) {







                SetFocus(de);







                SendMessageW(de, EM_SETSEL, 0, -1);







                SendMessageW(de, WM_CLEAR, 0, 0);







                std::wstring typed = L"2026-12-01";







                for (wchar_t ch : typed) SendMessageW(de, WM_CHAR, (WPARAM)ch, 1);







            }







            if (te) {







                SendMessageW(te, EM_SETSEL, 0, -1);







                std::wstring typed = L"日期输入测试";







                for (wchar_t ch : typed) SendMessageW(te, WM_CHAR, (WPARAM)ch, 1);







            }







            DebugLog(L"probe-editor-input: date1=%s title=%s", app.editor.date1.c_str(), app.editor.title.c_str());







            SetTimer(g_hwnd, 1703, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1703) {







            KillTimer(g_hwnd, 1703);







            DebugLog(L"probe-editor-input: final date1=%s title=%s", app.editor.date1.c_str(), app.editor.title.c_str());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1601) {







            KillTimer(g_hwnd, 1601);







            app.view = VIEW_YEAR;







            InvalidateOccs();







            UIInvalidate();







            DebugLog(L"probe-wheel: year before=%d", app.cursor.y);







            SetTimer(g_hwnd, 1602, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1602) {







            KillTimer(g_hwnd, 1602);







            POINT pt{ 800, 400 };







            ClientToScreen(g_hwnd, &pt);







            SendMessageW(g_hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, (WORD)(-WHEEL_DELTA)), MAKELPARAM(pt.x, pt.y));







            DebugLog(L"probe-wheel: year after=%d", app.cursor.y);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1501) {







            KillTimer(g_hwnd, 1501);







            app.search = L"高数";



            app.searchEditing = true;



            app.searchIntent = true;



            app.searchFocusPending = true;







            UIInvalidate();



            RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);







            if (HWND se0 = GetDlgItem(g_hwnd, 18001)) SetFocus(se0);







            SetTimer(g_hwnd, 1502, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1502) {







            KillTimer(g_hwnd, 1502);







            HWND se = GetDlgItem(g_hwnd, 18001);







            DebugLog(L"probe-search: focused=%d searchLen=%d", (se && GetFocus() == se) ? 1 : 0, (int)app.search.size());







            PostMessageW(g_hwnd, WM_KEYDOWN, VK_ESCAPE, 0);







            SetTimer(g_hwnd, 1503, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1503) {







            KillTimer(g_hwnd, 1503);







            HWND se = GetDlgItem(g_hwnd, 18001);







            DebugLog(L"probe-search: after esc focused=%d searchEmpty=%d", (se && GetFocus() == se) ? 1 : 0, app.search.empty() ? 1 : 0);







            if (se) SetFocus(se);







            PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM(20, 20));







            SetTimer(g_hwnd, 1504, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1504) {







            KillTimer(g_hwnd, 1504);







            HWND se = GetDlgItem(g_hwnd, 18001);







            DebugLog(L"probe-search: after click-outside focused=%d", (se && GetFocus() == se) ? 1 : 0);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1401) {







            KillTimer(g_hwnd, 1401);







            app.overlay = OV_ABOUT;







            UIInvalidate();







            SetTimer(g_hwnd, 1402, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1402) {







            KillTimer(g_hwnd, 1402);







            float x = 0, y = 0;







            bool found = GetHitPointByCmd(CMD_ABOUT_RESTART, x, y);







            DebugLog(L"probe-restart: found=%d at (%d,%d)", found ? 1 : 0, (int)x, (int)y);







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1151) {







            KillTimer(g_hwnd, 1151);







            app.view = VIEW_YEAR;







            app.cursor = Today();







            InvalidateOccs();







            UIInvalidate();







            SetTimer(g_hwnd, 1152, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1152) {







            KillTimer(g_hwnd, 1152);







            float x = 0, y = 0;







            bool found = GetHitPointByCmd(CMD_YEAR_MONTH_BASE + 9, x, y);







            DebugLog(L"probe-year: month hit=%d", found ? 1 : 0);







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            SetTimer(g_hwnd, 1153, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1153) {







            KillTimer(g_hwnd, 1153);







            DebugLog(L"probe-year: after month view=%d cursor=%04d-%02d", (int)app.view, app.cursor.y, app.cursor.mo);







            app.view = VIEW_YEAR;







            InvalidateOccs();







            UIInvalidate();







            SetTimer(g_hwnd, 1154, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1154) {







            KillTimer(g_hwnd, 1154);







            float x = 0, y = 0;







            int serial = DaySerial(2026, 10, 20);







            bool found = GetHitPointByCmd(CMD_YEAR_DAY_BASE + serial % 400, x, y);







            DebugLog(L"probe-year: day hit=%d", found ? 1 : 0);







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            SetTimer(g_hwnd, 1155, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 1155) {







            KillTimer(g_hwnd, 1155);







            DebugLog(L"probe-year: after day view=%d cursor=%04d-%02d-%02d", (int)app.view, app.cursor.y, app.cursor.mo, app.cursor.d);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 8) {







            KillTimer(g_hwnd, 8);







            RefreshOccs();







            if (g_selectN > 0) {







                int n = 0;







                for (auto& o : app.occs) {







                    app.selected.insert(OccKey(o));







                    if (++n >= g_selectN) break;







                }







            }







            if (g_lateReschedule) {







                std::vector<Occurrence> occs;







                for (auto& o : app.occs) { occs.push_back(o); if (occs.size() >= 2) break; }







                if (!occs.empty()) OpenRescheduleDialog(occs);







            }







            RenderFrame();







            SaveScreenshot(g_lateShotPath);







            DebugLog(L"late-shot: occs=%d selected=%d overlay=%d view=%d", (int)app.occs.size(), (int)app.selected.size(), (int)app.overlay, (int)app.view);







            if (g_keepOpen) continue;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 111) {















            KillTimer(g_hwnd, 111);







            RefreshOccs();







            std::vector<Occurrence> occs;







            for (auto& o : app.occs) { occs.push_back(o); if (occs.size() >= 2) break; }







            DebugLog(L"probe-reschedule: targets=%d", (int)occs.size());







            if (!occs.empty()) OpenRescheduleDialog(occs);







            SetTimer(g_hwnd, 112, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 112) {







            KillTimer(g_hwnd, 112);







            float x = 0, y = 0;







            bool found = GetHitPointByCmd(CMD_RS_OK, x, y);







            DebugLog(L"probe-reschedule: ok found=%d overlay=%d", found ? 1 : 0, (int)app.overlay);







            if (found) PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            SetTimer(g_hwnd, 113, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 113) {







            KillTimer(g_hwnd, 113);







            int ovs = 0;







            for (auto& e : app.store.events) ovs += (int)e.overrides.size();







            DebugLog(L"probe-reschedule: after overlay=%d overrides=%d events=%d", (int)app.overlay, ovs, (int)app.store.events.size());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 121) {







            KillTimer(g_hwnd, 121);







            RefreshOccs();







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_EVENT_BASE, x, y)) {







                keybd_event(VK_SHIFT, 0, 0, 0);







                SendMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







                keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);







                DebugLog(L"probe-shift: after shift click selected=%d", (int)app.selected.size());







                SetTimer(g_hwnd, 122, 700, nullptr);







            } else {







                DebugLog(L"probe-shift: no event hit");







                g_quitting = true; DestroyWindow(g_hwnd);







            }







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 122) {







            KillTimer(g_hwnd, 122);







            DebugLog(L"probe-shift: selected now=%d", (int)app.selected.size());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 131) {







            KillTimer(g_hwnd, 131);







            RefreshOccs();







            if (!app.occs.empty()) {







                Occurrence o = app.occs[0];







                app.store.SetOccurrenceDone(o.seriesId, o.origStart, o.start, o.end, true);







                InvalidateOccs();







                RefreshOccs();







                if (!app.occs.empty()) OpenEditorForOccurrence(app.occs[0]);







                DebugLog(L"probe-done-edit: overlay=%d editorDone=%d", (int)app.overlay, app.editor.done ? 1 : 0);







            }







            SetTimer(g_hwnd, 132, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 132) {







            KillTimer(g_hwnd, 132);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 101) {







            KillTimer(g_hwnd, 101);







            if (!app.store.events.empty()) {







                app.store.events[0].title += L" ✓改";







                app.store.Touch();







                DebugLog(L"probe-widget: touch rev=%d", app.store.revision);







                DoSaveStore();







            }







            SetTimer(g_hwnd, 102, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 102) {







            KillTimer(g_hwnd, 102);







            DebugLog(L"probe-widget: after rev=%d title=%s", app.store.revision,







                     app.store.events.empty() ? L"" : app.store.events[0].title.c_str());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 91) {







            KillTimer(g_hwnd, 91);







            float x = 0, y = 0;







            bool found = GetHitPointByCmd(CMD_IMPORT_OK, x, y);







            DebugLog(L"probe-import: overlay=%d found=%d at (%d,%d) file=%s parsed=%d", (int)app.overlay,







                     found ? 1 : 0, (int)x, (int)y, app.import.file.c_str(), (int)app.import.parsed.events.size());







            if (found) {







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            }







            SetTimer(g_hwnd, 92, 1200, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 92) {







            KillTimer(g_hwnd, 92);







            DebugLog(L"probe-import: after import overlay=%d events=%d", (int)app.overlay, (int)app.store.events.size());







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_SERIES_COLOR_SWATCH_BASE, x, y)) {







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







                DebugLog(L"probe-import: click first swatch");







            }







            SetTimer(g_hwnd, 93, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 93) {







            KillTimer(g_hwnd, 93);







            int colored = 0;







            for (auto& e : app.store.events) if (!e.color.empty()) ++colored;







            DebugLog(L"probe-import: colored=%d overlay=%d", colored, (int)app.overlay);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 71) {







            KillTimer(g_hwnd, 71);







            app.view = VIEW_AGENDA;







            InvalidateOccs();







            UIInvalidate();







            SetTimer(g_hwnd, 72, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 72) {







            KillTimer(g_hwnd, 72);







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_SELECTED_BASE, x, y)) {







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            }







            SetTimer(g_hwnd, 73, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 73) {







            KillTimer(g_hwnd, 73);







            DebugLog(L"probe-selectall: before select-all=%d", (int)app.selected.size());







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_AGENDA_SELALL, x, y)) {







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            }







            SetTimer(g_hwnd, 74, 700, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 74) {







            KillTimer(g_hwnd, 74);







            DebugLog(L"probe-selectall: after select-all=%d events=%d", (int)app.selected.size(), (int)app.store.events.size());







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_AGENDA_DELETE, x, y)) {







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            }







            SetTimer(g_hwnd, 75, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 75) {







            KillTimer(g_hwnd, 75);







            DebugLog(L"probe-selectall: overlay=%d", (int)app.overlay);







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_CONFIRM_OK, x, y)) {







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            }







            SetTimer(g_hwnd, 76, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 76) {







            KillTimer(g_hwnd, 76);







            DebugLog(L"probe-selectall: after delete events=%d selected=%d", (int)app.store.events.size(), (int)app.selected.size());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 81) {







            KillTimer(g_hwnd, 81);







            app.bulk.open = true;







            app.bulk.text = L"2026-10-20 08:00-09:00 重复测试\n2026-10-20 08:00-09:00 重复测试\n2026-10-21 10:00-11:00 唯一测试";







            app.bulk.recurMode = 0;







            app.bulk.repeatCount = 12;







            app.bulk.colorIdx = -1;







            app.overlay = OV_BULKADD;







            UIInvalidate();







            SetTimer(g_hwnd, 82, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 82) {







            KillTimer(g_hwnd, 82);







            DebugLog(L"probe-bulk: events before=%d", (int)app.store.events.size());







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_BULK_OK, x, y)) {







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            }







            SetTimer(g_hwnd, 83, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 83) {







            KillTimer(g_hwnd, 83);







            DebugLog(L"probe-bulk: events after=%d", (int)app.store.events.size());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 51) {







            KillTimer(g_hwnd, 51);







            OpenWidgetSettings();







            SetTimer(g_hwnd, 52, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 52) {







            KillTimer(g_hwnd, 52);







            HWND edit = GetDlgItem(g_hwnd, 9000 + 9502);







            if (edit) {







                std::wstring v = Fmt(L"%d", g_probeOpacity);







                SetWindowTextW(edit, v.c_str());







                PostMessageW(g_hwnd, WM_COMMAND, MAKEWPARAM((WORD)(9000 + 9502), EN_CHANGE), (LPARAM)edit);







                DebugLog(L"probe-opacity: set edit text=%s", v.c_str());







            } else {







                DebugLog(L"probe-opacity: edit not found");







            }







            SetTimer(g_hwnd, 53, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 53) {







            KillTimer(g_hwnd, 53);







            DebugLog(L"probe-opacity: applied opacity=%d", app.store.settings.widget.opacity);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 21) {







            KillTimer(g_hwnd, 21);







            OpenCleanup();







            if (!g_cleanupKeywordArg.empty()) app.cleanup.keyword = g_cleanupKeywordArg;







            DebugLog(L"probe-cleanup: overlay opened keyword=%s", g_cleanupKeywordArg.c_str());







            SetTimer(g_hwnd, 22, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 22) {







            KillTimer(g_hwnd, 22);







            DebugLog(L"probe-cleanup: events=%d", (int)app.store.events.size());







            float x = 0, y = 0;







            int targetCmd = g_cleanupKeywordArg.empty() ? CMD_CLEAN_ALL : CMD_CLEAN_APPLY;







            if (GetHitPointByCmd(targetCmd, x, y)) {







                DebugLog(L"probe-cleanup: click action cmd=%d at (%d,%d)", targetCmd, (int)x, (int)y);







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            } else {







                DebugLog(L"probe-cleanup: action hit not found");







            }







            SetTimer(g_hwnd, 23, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 23) {







            KillTimer(g_hwnd, 23);







            DebugLog(L"probe-cleanup: overlay=%d events=%d", (int)app.overlay, (int)app.store.events.size());







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_CONFIRM_OK, x, y)) {







                DebugLog(L"probe-cleanup: click confirm at (%d,%d)", (int)x, (int)y);







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            } else {







                DebugLog(L"probe-cleanup: confirm hit not found");







            }







            SetTimer(g_hwnd, 24, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 24) {







            KillTimer(g_hwnd, 24);







            int left = 0;







            for (auto& e : app.store.events) if (!g_cleanupKeywordArg.empty() && e.title.find(g_cleanupKeywordArg) != std::wstring::npos) ++left;







            DebugLog(L"probe-cleanup: after events=%d overlay=%d keywordLeft=%d", (int)app.store.events.size(), (int)app.overlay, left);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 13) {







            KillTimer(g_hwnd, 13);







            app.view = VIEW_AGENDA;







            InvalidateOccs();







            UIInvalidate();







            DebugLog(L"probe-agenda: view switched");







            SetTimer(g_hwnd, 14, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 14) {







            KillTimer(g_hwnd, 14);







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_SELECTED_BASE, x, y)) {







                DebugLog(L"probe-agenda: click checkbox at (%d,%d)", (int)x, (int)y);







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            } else {







                DebugLog(L"probe-agenda: checkbox hit not found");







            }







            SetTimer(g_hwnd, 15, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 15) {







            KillTimer(g_hwnd, 15);







            DebugLog(L"probe-agenda: selected=%d", (int)app.selected.size());







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_AGENDA_DELETE, x, y)) {







                DebugLog(L"probe-agenda: click delete at (%d,%d)", (int)x, (int)y);







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            } else {







                DebugLog(L"probe-agenda: delete button not found");







            }







            SetTimer(g_hwnd, 16, 800, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 16) {







            KillTimer(g_hwnd, 16);







            DebugLog(L"probe-agenda: overlay=%d events=%d", (int)app.overlay, (int)app.store.events.size());







            float x = 0, y = 0;







            if (GetHitPointByCmd(CMD_CONFIRM_OK, x, y)) {







                DebugLog(L"probe-agenda: click confirm at (%d,%d)", (int)x, (int)y);







                PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM((int)x, (int)y));







            } else {







                DebugLog(L"probe-agenda: confirm button not found");







            }







            SetTimer(g_hwnd, 17, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 17) {







            KillTimer(g_hwnd, 17);







            DebugLog(L"probe-agenda: after events=%d selected=%d", (int)app.store.events.size(), (int)app.selected.size());







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 61) {
            BotTick();
            continue;
        }

        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 11) {







            KillTimer(g_hwnd, 11);







            RECT rc; GetClientRect(g_hwnd, &rc);







            float sx = S(SIDEBAR_W) + S(22);







            float sy = S(TITLE_H) + S(14);







            float cw = (float)rc.right - sx - S(22);







            int nx = (int)(sx + cw - S(62) - S(34) + S(14));







            int ny = (int)(sy + S(3) + S(14));







            DebugLog(L"probe: click next-month at (%d,%d) overlay=%d", nx, ny, (int)app.overlay);







            PostMessageW(g_hwnd, WM_LBUTTONUP, 0, MAKELPARAM(nx, ny));







            SetTimer(g_hwnd, 12, 900, nullptr);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 12) {







            KillTimer(g_hwnd, 12);







            DebugLog(L"probe: after click cursor=%04d-%02d overlay=%d", app.cursor.y, app.cursor.mo, (int)app.overlay);







            g_quitting = true;







            DestroyWindow(g_hwnd);







            continue;







        }







        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == 9) {







            KillTimer(g_hwnd, 9);







            if (!g_widgetShotPath.empty()) {







                HWND w = FindWindowW(L"XingLiWidget", nullptr);







                CaptureWindowPng(w, g_widgetShotPath);







                DestroyWindow(g_hwnd);







                continue;







            }







            if (!g_reminderShotPath.empty()) {







                HWND r = FindWindowW(L"XingLiReminder", nullptr);







                CaptureWindowPng(r, g_reminderShotPath);







                DestroyWindow(g_hwnd);







                continue;







            }







            RenderFrame();







            SaveScreenshot(g_shotPath);







            DestroyWindow(g_hwnd);







            continue;







        }







        if (g_hwnd) {







            HWND se = GetDlgItem(g_hwnd, 18001);







            if (se && GetFocus() == se) {







                if (app.overlay != OV_NONE) {







                    SetFocus(g_hwnd);







        AppEnableIme(false);







                } else if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {







                    app.search.clear();







                    app.searchFocused = false;







                    SetFocus(g_hwnd);







        AppEnableIme(false);







                    UIInvalidate();







                    continue;







                }







            }







        }







        bool passToDialog = true;







        if (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) passToDialog = false;
        if (msg.message == WM_KEYDOWN && app.overlay == OV_SLASH &&
            (msg.wParam == VK_TAB || msg.wParam == VK_UP || msg.wParam == VK_DOWN)) passToDialog = false;







        if (g_hwnd && passToDialog && IsDialogMessageW(g_hwnd, &msg)) continue;







        TranslateMessage(&msg);







        DispatchMessageW(&msg);







    }















    ShareShutdown();

    if (g_memdc) DeleteDC(g_memdc);







    if (g_hbmp) DeleteObject(g_hbmp);







    gfx::Shutdown();







    GdiplusShutdown(g_gdiToken);







    return 0;







}

