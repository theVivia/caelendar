// app.h - 全局状态、UI 基础设施声明
#pragma once
#include "util.h"
#include "gfx.h"
#include "theme.h"
#include "model.h"
#include "recur.h"
#include "ics.h"
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <set>
#include <map>

// ---------- 版本 / 基础信息 ----------
constexpr const wchar_t* kAppVersion = L"3.36";
constexpr const wchar_t* kAppBuildDate = L"2026-10-18";

// ---------- 命令 ID ----------
enum Cmd {
    CMD_NONE = 0,
    CMD_MIN = 1, CMD_MAX, CMD_CLOSE,
    CMD_VIEW_BASE = 10,          // +0..4（月/周/日/年/日程）
    CMD_PREV = 20, CMD_NEXT, CMD_TODAY,
    CMD_NEW = 30, CMD_THEME, CMD_SETTINGS, CMD_MORE, CMD_IMPORT, CMD_EXPORT,
    CMD_HELP, CMD_UNDO, CMD_REDO, CMD_QUICKADD, CMD_BATCHADD, CMD_WIDGET_TOGGLE,
    CMD_WATER = 200, CMD_WATER_DEC, CMD_WATER_RESET, CMD_WATER_SET_BASE = 220,
    CMD_TAG_BASE = 1000,         // +tag 下标
    CMD_AGENDA_DAY_BASE = 3000,  // 日程视图日期头
    CMD_DAY_BASE = 20000,        // +DaySerial
    CMD_EVENT_BASE = 40000,      // +app.occs 下标
    CMD_SELECTED_BASE = 45000,   // 日程视图选择框 + ocс 下标
    CMD_QUICK_ITEM_BASE = 47000,
    CMD_THEME_ITEM_BASE = 48000,
    CMD_MENU_ITEM_BASE = 50000,
    CMD_YEAR_MONTH_BASE = 60000,   // +0..11：年视图月份卡片
    CMD_YEAR_DAY_BASE = 60200,     // +0..400：年视图日期格
    CMD_SEARCH_EDIT = 54412,
    CMD_SEARCH_CLEAR = 54410,
    CMD_DATEJUMP = 54810, CMD_DJ_OK = 54800, CMD_DJ_CANCEL,
    CMD_DAYDETAIL_CLOSE = 54900, CMD_DD_NEW, CMD_DD_QUICK, CMD_DD_DAYVIEW,
    CMD_AGENDA_LIST_VIEW = 54950, CMD_AGENDA_BOARD_VIEW,
    CMD_ACC_REGISTER = 55600, CMD_ACC_LOGIN, CMD_ACC_PUSH, CMD_ACC_PULL, CMD_ACC_CLOSE,
    CMD_ACC_MERGE = 55655,
    CMD_SET_AUTOBACKUP_DIR = 55620, CMD_SET_AUTOBACKUP_CLEAR,
    CMD_SET_NOTIFY_SOUND_FILE = 55640, CMD_SET_NOTIFY_SOUND_CLEAR,
    CMD_SET_HIGHCONTRAST = 55650, CMD_SET_MONTHCHIPS = 55651, CMD_SET_MONTHCOUNTS = 55652,
    CMD_HOTKEYS_ROW_BASE = 55660, CMD_HOTKEYS_RESET = 55670, CMD_HOTKEYS_CLOSE = 55671,
    CMD_GRADES_ADD = 55800, CMD_GRADES_CLOSE = 55801, CMD_GRADES_DEL_BASE = 55810,
    CMD_HEALTH_CLOSE = 55830, CMD_HEALTH_CLEANUP = 55831,
    CMD_BOT_TOGGLE = 55700, CMD_BOT_SAVE_TOKEN, CMD_BOT_CLOSE,
    CMD_SET_ACCOUNT_SYNC = 55610,
    CMD_AGENDA_RANGE_BASE = 55500,     // +0..3 范围筛选
    CMD_AGENDA_DONE_BASE = 55510,      // +0..2 完成状态筛选
    CMD_AGENDA_FILTER_APPLY = 55520,
    CMD_STATS_OPEN = 54559, CMD_STATS_CLOSE = 54560, CMD_STATS_TAB_BASE = 54562,
    CMD_STATS_MONTH_BASE = 54570, CMD_STATS_PREV = 54582, CMD_STATS_NEXT,
    CMD_BACKUP_OPEN_EXPORT = 54750, CMD_BACKUP_OPEN_IMPORT, CMD_BACKUP_CANCEL,
    CMD_THEMESTUDIO_OPEN = 54830, CMD_TS_CLOSE, CMD_TS_THEME_BASE = 54833,
    CMD_TS_ACCENT = 54860, CMD_TS_BG, CMD_TS_LIGHT, CMD_TS_RADIUS_DEC, CMD_TS_RADIUS_INC,
    CMD_TS_DENSITY_DEC, CMD_TS_DENSITY_INC, CMD_TS_EXPORT, CMD_TS_IMPORT,
    CMD_BACKUP_CONFIRM, CMD_BACKUP_PICK_FILE,
    CMD_BOARD_DONE = 54970, CMD_BOARD_UNDONE, CMD_BOARD_CLEAR, CMD_BOARD_GROUP,
    CMD_BOARD_FILTER_BASE = 54960, CMD_BOARD_COURSE_FILTER, CMD_BOARD_ALL_EVENTS = 54980,
    CMD_HELP_BUTTON = 58550,
    CMD_HELP_ITEM_BASE = 58500,   // +0..帮助功能地图行
    CMD_ABOUT_VERSIONS = 54403, CMD_VER_CLOSE = 54620, CMD_VER_ROLLBACK_BASE = 54630,
    // 编辑器
    CMD_ED_SAVE = 51001, CMD_ED_CANCEL, CMD_ED_DELETE, CMD_ED_DUPLICATE,
    CMD_ED_SCOPE_THIS, CMD_ED_SCOPE_FOLLOW, CMD_ED_SCOPE_ALL,
    CMD_ED_ALLDAY, CMD_ED_DUR_BASE, CMD_ED_RECUR_BASE,
    // 注意：这些 +i 的基址必须彼此隔开（之前连续枚举导致颜色和标签命令撞车，
    // 选颜色时上面的标签会被随机高亮）
    CMD_ED_WEEKDAY_BASE = 51020,     // +0..6
    CMD_ED_TAG_BASE = 51040,         // +0..63
    CMD_ED_PRIO_BASE = 51110,        // +0..3
    CMD_ED_STATUS_BASE = 51120,      // +0..2
    CMD_ED_TEMPLATE_SAVE = 51156, CMD_ED_LOCATION_PRESET = 51157,
    CMD_ED_ALARM_ADD = 51130,
    CMD_ED_ALARM_DEL_BASE = 51140,   // +0..15
    CMD_ED_COLOR_BASE = 51160,       // +0..31（0=自动，1..=调色板）
    CMD_ED_COLOR_CUSTOM = 51189,
    CMD_ED_LUNAR = 51188,
    CMD_ED_NEWTAG = 51190,
    CMD_ED_ENDMODE_BASE = 51191,     // +0..2
    CMD_ED_ENDCOUNT_DEC = 51196, CMD_ED_ENDCOUNT_INC = 51197,
    CMD_ED_MONTHMODE_BASE = 51198,   // +0..2
    CMD_ED_NTH_DEC = 51202, CMD_ED_NTH_INC = 51203,
    CMD_ED_INT_DEC = 51210, CMD_ED_INT_INC = 51211, CMD_ED_ALARM_PRESET_BASE = 51212,  // +0..7
    CMD_ED_EXDATE_BASE = 51230,   // +0..9 重复例外日切换
    CMD_ED_DDL = 51250,           // 设为 DDL
    // 设置
    CMD_SET_THEME_BASE = 52000, CMD_SET_WEEKSTART_BASE, CMD_SET_SEM_ENABLE,
    CMD_SET_SEM_DEC, CMD_SET_SEM_INC, CMD_SET_SEM_DATE_BASE, CMD_SET_SEM_PARITY,
    CMD_SET_WATER_ENABLE, CMD_SET_WATER_INT_DEC, CMD_SET_WATER_INT_INC,
    CMD_SET_WATER_TARGET_DEC, CMD_SET_WATER_TARGET_INC,
    CMD_SET_WATER_FROM_BASE, CMD_SET_WATER_TO_BASE, CMD_SET_WATER_SOUND,
    CMD_SET_NOTIFY_ENABLE, CMD_SET_NOTIFY_SOUND, CMD_SET_NOTIFY_LEAD_DEC, CMD_SET_NOTIFY_LEAD_INC,
    CMD_SET_NOTIFY_FOCUS, CMD_REMINDERS_OPEN = 54760, CMD_RM_CLOSE,
    CMD_RM_DONE_BASE = 54762, CMD_RM_EDIT_BASE = 54780, CMD_RM_DISMISS_BASE = 54798, CMD_RM_CLEAR = 54816,
    CMD_SET_LUNAR, CMD_SET_WEEKNUM, CMD_SET_REDUCEMOTION, CMD_SET_CLOSE,
    CMD_SET_BACKUP, CMD_SET_RESTORE_BACKUP, CMD_SET_OPEN_DATA_DIR, CMD_SET_TAG_ADD, CMD_SET_TAG_DEL_BASE,
    CMD_SET_TAG_EDIT_BASE, CMD_SET_WIDGET_ENABLE, CMD_SET_WIDGET_OPACITY_DEC, CMD_SET_WIDGET_OPACITY_INC,
    CMD_SET_WIDGET_MODE_BASE,
    CMD_SET_FONTS, CMD_SET_CLOSE2TRAY, CMD_SET_AUTOSTART,
    CMD_FONT_RESET = 54600, CMD_FONT_CLOSE, CMD_FONT_PICK_BASE = 54700, CMD_FONT_PICK_DEFAULT,
    CMD_FONT_GLOBAL_DEC = 55000, CMD_FONT_GLOBAL_INC,
    CMD_FONT_SIZE_DEC_BASE = 55100, CMD_FONT_SIZE_INC_BASE = 55200,
    // 导入
    CMD_IMPORT_MODE_BASE = 53000,   // +0..2（合并方式 chips）
    CMD_IMPORT_OK = 53010, CMD_IMPORT_CANCEL, CMD_IMPORT_PICK, CMD_EXPORT_OK,
    CMD_IMPORT_COLOR_BASE = 53150,
    // 确认框
    CMD_CONFIRM_OK = 54000, CMD_CONFIRM_CANCEL,
    // 批量添加
    CMD_BULK_OK = 54100, CMD_BULK_CANCEL,
    CMD_BULK_REPEAT_BASE = 54110, CMD_BULK_COUNT_DEC = 54130, CMD_BULK_COUNT_INC,
    CMD_BULK_COLOR_BASE = 54140,     // +1..18 色板
    CMD_BULK_COLOR_CUSTOM = 54160,
    // 批量添加（日历多选）
    CMD_BULK_CAL_PREV = 55300, CMD_BULK_CAL_NEXT, CMD_BULK_CAL_TODAY,
    CMD_BULK_CAL_DAY_BASE = 55310,     // +0..41
    CMD_BULK_TAG_BASE = 55360,         // +标签索引
    CMD_BULK_PRESET_APPLY_BASE = 55400,
    CMD_BULK_PRESET_DEL_BASE = 55420,
    CMD_BULK_SAVE_PRESET = 55440, CMD_BULK_CLEAR_DAYS = 55441,
    CMD_BULK_MODE_CAL = 55442, CMD_BULK_MODE_TEXT = 55443,
    CMD_BULK_TARGET_BASE = 55460,     // +已选日期索引（切换逐日调整目标）
    // 快速添加
    CMD_QA_OK = 54200, CMD_QA_CANCEL, CMD_QA_DETAIL,
    // 标签编辑
    CMD_TAGEDIT_OK = 54300, CMD_TAGEDIT_CANCEL, CMD_TAGEDIT_COLOR_BASE, CMD_TAGEDIT_COLOR_CUSTOM,
    CMD_TAGEDIT_TASK = 54319, CMD_TAGEDIT_ICON_IMPORT = 54320, CMD_TAGEDIT_ICON_CLEAR = 54321,
    CMD_TAGEDIT_ICON_BASE = 54360,
    CMD_SHARE_TOGGLE = 54700, CMD_SHARE_RESET, CMD_SHARE_COPY, CMD_SHARE_CLOSE, CMD_SHARE_REQ_CLEAR, CMD_SHARE_ALLOW,
    CMD_SHARE_OPEN_LOCAL, CMD_SHARE_TAILSCALE, CMD_SHARE_DOWNLOAD,
    CMD_ZOOM_OUT = 54720, CMD_ZOOM_IN, CMD_ZOOM_FIT,
    CMD_ABOUT_CLOSE = 54400, CMD_ABOUT_RESTART,
    // 日程批量
    CMD_AGENDA_SELALL = 54500, CMD_AGENDA_SELNONE, CMD_AGENDA_DELETE, CMD_AGENDA_MOVE, CMD_AGENDA_TAGS, CMD_AGENDA_PRIO, CMD_AGENDA_DONE, CMD_AGENDA_INVERT, CMD_AGENDA_RESCHEDULE,
    CMD_AGENDA_COLOR = 54520,
    CMD_IMPORT_SMART = 54530,
    CMD_SMART_ORGANIZE = 54540,
    // 挂件 / 提示窗外观
    CMD_WS_ENABLE = 57000, CMD_WS_LAYOUT_BASE = 57010, CMD_WS_SKIN_BASE = 57020,
    CMD_WS_SCALE_DEC = 57030, CMD_WS_SCALE_INC, CMD_WS_OPACITY_DEC, CMD_WS_OPACITY_INC,
    CMD_WS_TOGGLE_WATER, CMD_WS_TOGGLE_DATE, CMD_WS_TOGGLE_TODAY, CMD_WS_TOGGLE_TOPMOST,
    CMD_WS_TOGGLE_TASKS, CMD_WS_TOGGLE_CLICK, CMD_WS_TOGGLE_LOW,
    CMD_WS_RESET_POS, CMD_WS_RESET, CMD_WS_CLOSE,
    // 批量清理日程
    CMD_CLEAN_CLOSE = 57210, CMD_CLEAN_ALL, CMD_CLEAN_DONE, CMD_CLEAN_IMPORTED,
    CMD_CLEAN_MODE_BASE = 57220, CMD_CLEAN_TYPE_BASE = 57230, CMD_CLEAN_STATUS_BASE = 57240,
    CMD_CLEAN_TAG_BASE = 57300, CMD_CLEAN_COLOR_BASE = 57350, CMD_CLEAN_NOCOLOR = 57390,
    CMD_CLEAN_ALLDAY = 57400, CMD_CLEAN_ALARM, CMD_CLEAN_APPLY, CMD_CLEAN_RESET,
    // 颜色编辑器 / 系列配色
    CMD_CP_OK = 57500, CMD_CP_CANCEL, CMD_CP_HEX_EDIT = 57510,
    CMD_CP_R_DEC = 57520, CMD_CP_R_INC, CMD_CP_G_DEC, CMD_CP_G_INC, CMD_CP_B_DEC, CMD_CP_B_INC,
    CMD_CP_C_DEC = 57530, CMD_CP_C_INC, CMD_CP_M_DEC, CMD_CP_M_INC, CMD_CP_Y_DEC, CMD_CP_Y_INC, CMD_CP_K_DEC, CMD_CP_K_INC,
    CMD_CP_RESET = 57540,
    CMD_SERIES_COLOR_CLOSE = 57550, CMD_SERIES_COLOR_AUTO, CMD_SERIES_COLOR_CLEAR, CMD_SERIES_COLOR_SWATCH_BASE = 57600, CMD_SERIES_COLOR_CUSTOM_BASE = 57700,
    // 调休 / 迁移 + 跨视图批量条
    CMD_RS_OK = 57800, CMD_RS_CANCEL, CMD_RS_DATE_EDIT = 57810, CMD_RS_QUICK_BASE = 57820,
    CMD_RS_DAY_DEC = 57831, CMD_RS_DAY_INC,
    CMD_BATCH_DONE = 57840, CMD_BATCH_RESCHEDULE, CMD_BATCH_DELETE, CMD_BATCH_INVERT, CMD_BATCH_CLEAR,
};
// ---------- 布局常量（逻辑像素，用 S() 缩放） ----------
constexpr float TITLE_H = 58;
constexpr float SIDEBAR_W = 244;

enum ViewKind { VIEW_MONTH = 0, VIEW_WEEK, VIEW_DAY, VIEW_YEAR, VIEW_AGENDA };
enum OverlayKind { OV_NONE = 0, OV_EDITOR, OV_QUICKADD, OV_BULKADD, OV_SETTINGS, OV_IMPORT, OV_CONFIRM, OV_MENU, OV_SEARCH, OV_TAGEDIT, OV_FONTS, OV_ABOUT, OV_WIDGET, OV_CLEANUP, OV_COLORPICK, OV_SERIESCOLOR, OV_RESCHEDULE, OV_DATEJUMP, OV_DAYDETAIL, OV_STATS, OV_VERSIONS, OV_BACKUP, OV_REMINDERS, OV_THEMESTUDIO, OV_CMDK, OV_OCR, OV_SUBS, OV_DB, OV_SYNC, OV_EXAMS, OV_FOCUS, OV_HABITS, OV_PRINT, OV_PROFILES, OV_HELP, OV_SLASH, OV_SHARE, OV_ACCOUNT, OV_BOT, OV_HOTKEYS, OV_GRADES, OV_HEALTH };

// ---------- 全局运行状态 ----------
extern bool g_quitting;
extern HANDLE g_singleMutex;

// ---------- 命中区域 ----------
struct Hit {
    Gdiplus::RectF r;
    int layer = 0;
    int cmd = 0;
    std::function<void()> fn;
};
extern std::vector<Hit> g_hits;
extern int g_hoverCmd;
extern int g_drawLayer;
extern int g_inputCmdLayer;   // 用于键盘等

void AddHit(float x, float y, float w, float h, int cmd, std::function<void()> fn);
bool GetHitPointByCmd(int cmd, float& x, float& y);   // 调试探针用：取某命令命中区中心
void ClearHits();
void UIInvalidate();
void AppEnableIme(bool on);
bool IsHovered(int cmd);
void UIFocusNext(bool backward);
void A11yInit();
LRESULT A11yHandleGetObject(HWND hwnd, WPARAM wParam, LPARAM lParam);
int UIFocusCmd();
bool UIActivateFocus();
void UIClearFocus();
bool PointInHit(float px, float py, int& outCmd, int& outLayer);

// ---------- 窗口基础设施 ----------
extern HWND g_hwnd;
extern HDC g_memdc;
extern HBITMAP g_hbmp;
extern void* g_bits;
extern int g_bufW, g_bufH;
extern bool g_shotMode;
extern std::wstring g_shotPath;
extern bool g_demoMode;
extern bool g_noSave;
extern bool g_maximized;
extern bool g_running;

void RequestRepaint();
void RenderFrame();
void ShowToast(const std::wstring& msg, int ms = 2600);

// ---------- 主题 / 标签色 ----------
const Theme& TH();
Gdiplus::Color TagColorOf(const std::string& tagId);
std::wstring TagHex(const std::string& tagId);
Gdiplus::Color Blend(Gdiplus::Color a, Gdiplus::Color b, float t);

// ---------- UI 控件（ui.cpp） ----------
namespace UI {
void Panel(Gdiplus::Graphics& g, Gdiplus::RectF r, float radius, bool raised = false);
void Dim(Gdiplus::Graphics& g, float W, float H, BYTE alpha);
void Btn(Gdiplus::Graphics& g, Gdiplus::RectF r, int cmd, const std::wstring& label, int style,
         int icon, std::function<void()> fn);
void IconBtn(Gdiplus::Graphics& g, Gdiplus::RectF r, int cmd, int icon, float iconSize,
             std::function<void()> fn);
void Chip(Gdiplus::Graphics& g, Gdiplus::RectF r, int cmd, const std::wstring& label, bool active,
          Gdiplus::Color accent, std::function<void()> fn, float px = 11.0f);
void TagChip(Gdiplus::Graphics& g, Gdiplus::RectF r, int cmd, const std::wstring& name,
             Gdiplus::Color color, bool active, std::function<void()> fn, int icon = -1);
void CheckBox(Gdiplus::Graphics& g, Gdiplus::RectF r, int cmd, const std::wstring& label,
              bool checked, std::function<void()> fn);
void SectionTitle(Gdiplus::Graphics& g, const std::wstring& label, Gdiplus::RectF r);
void Divider(Gdiplus::Graphics& g, float x, float y, float w);
void Scrollbar(Gdiplus::Graphics& g, Gdiplus::RectF track, float viewH, float contentH, float scroll);
void Stepper(Gdiplus::Graphics& g, Gdiplus::RectF r, int cmdDec, int cmdInc, const std::wstring& value,
             std::function<void()> dec, std::function<void()> inc, float px = 11.0f);
void FieldLabel(Gdiplus::Graphics& g, const std::wstring& label, float x, float y, float w);
void EditFrame(Gdiplus::Graphics& g, Gdiplus::RectF r);
}
HWND UIEdit(int id, Gdiplus::RectF r, std::wstring* buf, bool multiline = false, float px = 13, bool bold = false);
void UIEditEndFrame();
bool UIHandleCommand(WPARAM wParam, LPARAM lParam);
INT_PTR UICtlColorEdit(HWND hwnd, HDC hdc);
void UISetEditFocus(int id, bool selectAll = false);
HFONT UIGdiFont(float px, bool bold, bool display = false);
void CloseAllEdits();
// ---------- 各视图绘制 ----------
bool MonthDateAtPoint(float px, float py, DateTime& out);
void DrawMonthView(Gdiplus::Graphics& g, float x, float y, float w, float h);
void DrawWeekView(Gdiplus::Graphics& g, float x, float y, float w, float h, bool singleDay);
void DrawAgendaView(Gdiplus::Graphics& g, float x, float y, float w, float h);
bool WeekViewHitTime(float px, float py, DateTime& day, int& minute);
void DrawSidebar(Gdiplus::Graphics& g, float W, float H);
void DrawTitleBar(Gdiplus::Graphics& g, float W);
void DrawOverlays(Gdiplus::Graphics& g, float W, float H);
void DrawToast(Gdiplus::Graphics& g, float W, float H);

// ---------- 编辑器 / 对话框 ----------
struct EditorState {
    bool open = false;
    bool isNew = false;
    std::string seriesId;
    DateTime origStart;        // 正在编辑的实例原开始
    bool seriesRecurring = false;
    int scope = 0;             // 0=仅本次 1=此后所有 2=整个系列
    // 字段
    std::wstring title, date1, time1, date2, time2, location, url, desc;
    bool allDay = false;
    int priority = 0, status = 0;
    bool done = false;
    bool isDdl = false;   // 独立 DDL 标记
    std::set<std::string> tags;
    std::vector<Alarm> alarms;
    std::wstring color;
    std::wstring originalColor;   // 打开编辑器时的颜色，用来判断用户是否真的改了颜色
    // 重复
    int freq = 0, interval = 1;
    std::set<int> byDay;
    int monthDay = 0;
    int endMode = 0;           // 0=永不 1=次数 2=日期
    int count = 10;
    std::wstring untilStr;
    std::vector<DateTime> exDates;
    std::wstring source;
    int monthlyMode = 0;   // 0=按开始日 1=第 nth 个周几
    int nth = 1;
    bool lunar = false;      // 每年重复按农历
    std::wstring lunarDate;  // 直接输入的农历日期 YYYY-MM-DD
    std::wstring error;
    DateTime start, end;
    void OpenFromEvent(const Event& e);
    void LoadNew(const DateTime& day);
    void LoadExisting(const Event& base, const Occurrence* occ);
    bool Collect(Event& out, std::wstring& err);
    void SyncTimeTexts();
    void ParseTimeTexts();     // 尽量解析回 start/end
};

struct QuickState { bool open = false; std::wstring text; std::vector<Event> previews; };
struct ShareUiState {
    bool enabled = false;
    bool allowRequests = true;
    bool useTailscale = false;
    int port = 17890;
    std::wstring token;
    std::wstring publicUrl;
};
struct SlashState {
    bool open = false;
    std::wstring text;
    std::wstring lastText;
    std::wstring error;
    int sel = 0;
};
struct CmdkState {
    bool open = false;
    std::wstring text;
    std::wstring lastText;
    int sel = 0;
    bool captureHotkey = false;
    std::wstring error;
};
struct BulkPreset {
    std::wstring name;
    std::wstring title;
    std::wstring location;
    int startMin = 18 * 60;
    int endMin = 20 * 60;
    int colorIdx = -1;
    std::wstring customColor;
    std::vector<std::string> tags;
};
struct BulkDayOverride {
    int startMin = -1;
    int endMin = -1;
    std::wstring location;
};
struct EventTemplate {
    std::wstring name, title, location, color;
    int startMin = 18 * 60;
    int endMin = 20 * 60;
    std::vector<std::string> tags;
    std::vector<Alarm> alarms;
};
struct BulkState {
    bool open = false;
    // 文本模式（旧）
    std::wstring text;
    int recurMode = 0;
    int repeatCount = 12;
    int colorIdx = -1;
    std::wstring customColor;
    // 日历多选模式
    bool calMode = true;
    int calY = 0, calM = 0;
    std::set<int> days;
    std::wstring title, location;
    std::wstring startText = L"18:00", endText = L"20:00";
    std::set<std::string> tags;
    std::vector<BulkPreset> presets;
    std::wstring presetName;
    // 逐日覆盖：perEditSerial=0 表示当前编辑公共字段，否则编辑该日
    int perEditSerial = 0;
    std::wstring editLocation, editStart = L"18:00", editEnd = L"20:00";
    std::map<int, BulkDayOverride> dayOverrides;
};
struct ImportState {
    bool open = false;
    std::wstring file;
    std::wstring label;
    bool tableMode = false;
    std::vector<Event> tableEvents;
    std::vector<std::wstring> tableWarnings;
    std::wstring summary;
    std::wstring rawBody;
    std::wstring ocrText;
    std::wstring ocrSource;
    int conflicts = 0;
    IcsParseResult parsed;
    int mode = 0;
    bool smart = true;          // 智能识别课程 / 配色 / 合并重复
    int colorIdx = -1;          // 导入后统一颜色；-1 = 自动/智能配色
    std::wstring colorCustom;   // 自定义统一颜色（优先于 colorIdx）
    std::wstring error;
};
struct BotState {
    bool enabled = false;
    std::wstring token;
    long long chatId = 0;
    std::wstring status;
    std::vector<std::wstring> lastList;          // /today /week 的列表，供 /done 使用
    std::vector<std::wstring> sentReminders;     // 已推送提醒去重
};
struct SyncUiState {
    std::wstring password;
    std::wstring lastExport;
    std::wstring error;
    std::wstring summary;
    // 账号同步（M1）
    std::wstring server = L"http://127.0.0.1:17890";
    std::wstring user;
    std::wstring token;
    bool loggedIn = false;
    std::wstring status;
};
struct FocusSession {
    std::string id;
    DateTime start, end;
    int minutes = 0;
    std::wstring label;
};
struct FocusState {
    int minutes = 25;
    int remainingSec = 25 * 60;
    bool running = false;
    unsigned long long lastTick = 0;
    std::wstring label;
    std::vector<FocusSession> sessions;
};
struct Habit {
    std::string id;
    std::wstring name;
    int targetPerWeek = 7;
    std::map<std::string, bool> checks;   // "YYYY-MM-DD" -> done
};
struct HabitsState {
    std::vector<Habit> list;
    std::wstring newName;
    int newTarget = 7;
};
struct CalendarProfile {
    std::string id;
    std::wstring name;
    std::wstring start;      // YYYY-MM-DD
    int weeks = 18;
    std::string tagId;       // 空 = 全部
};
struct ProfilesState {
    std::vector<CalendarProfile> list;
    std::string activeId;
    std::wstring newName;
    int newWeeks = 18;
    std::string newTag;
};
struct Subscription {
    std::string id;
    std::wstring name;
    std::wstring url;
    long long lastSync = 0;
    bool enabled = true;
};
struct SubscriptionUi {
    std::wstring newUrl;
    std::wstring newName;
    int sel = -1;
    std::wstring error;
};
struct ConfirmState {
    bool open = false;
    std::wstring title, body, okText, cancelText;
    std::function<void()> onOk;
};
struct CleanupState {
    std::wstring from, to, keyword;
    std::set<std::string> tags;
    std::set<std::wstring> colors;
    int typeMode = 0;      // 0=全部 1=单次 2=重复
    int statusMode = 0;    // 0=全部 1=已完成 2=未完成
    bool onlyAllDay = false;
    bool onlyWithAlarm = false;
    int delMode = 0;       // 0=整个系列 1=仅范围内实例
};
struct ColorPickState {
    bool open = false;
    std::wstring title;
    std::wstring hex = L"#6EA8FF";
    int returnOverlay = 0;
    std::function<void(const std::wstring&)> onOk;
};
struct SeriesColorState { bool open = false; int scroll = 0; };
struct RescheduleTarget {
    std::string seriesId;
    DateTime origStart;
    DateTime occStart;
    DateTime occEnd;
    Event effective;
};
struct RescheduleState {
    bool open = false;
    std::wstring date;
    std::vector<RescheduleTarget> targets;
};
struct MenuItemDef { std::wstring label; int icon = -1; bool danger = false; bool disabled = false; std::function<void()> fn; std::wstring color; };
struct MenuState { bool open = false; float x = 0, y = 0; std::vector<MenuItemDef> items; int anchorCmd = 0; OverlayKind returnOverlay = OV_NONE; };
struct TagEditState {
    bool open = false;
    std::wstring name;
    std::wstring color;
    std::wstring icon = L"tag";
    std::string editingId;
    bool isNew = true;
    std::wstring error;
    bool task = false;              // 计入 DDL 看板
    std::wstring customIcon;        // 自定义图标路径（相对 data）
};

extern bool g_widgetMenuOpen;
extern bool g_offscreen;   // 挂件原生右键菜单打开期间，主窗口不能来抢前台

struct EventClipState {
    bool valid = false;
    Event ev;
    int mode = 0;              // 0=这一次 1=整个系列 2=此后所有
    DateTime sourceStart{};
    std::wstring label;
};
struct AgendaFilterState {
    int range = 0;      // 0=全部（未来60天）1=今天 2=未来7天 3=自定义
    int done = 0;       // 0=全部 1=未完成 2=已完成
    std::wstring fromText, toText;
};
struct AppState {
    EventClipState eventClip;
    std::vector<EventClipState> eventClipHistory;   // 最近 5 条复制过的日程
    AgendaFilterState agendaFilter;
    Store store;
    int view = VIEW_MONTH;
    DateTime cursor = Today();
    DateTime now = NowDT();
    int themeIdx = 0;
    std::wstring search;
    bool searchFocused = false;
    bool searchIntent = false;   // 只有用户主动点击/Ctrl+F 才允许搜索框持有焦点
    int tagScroll = 0;
    bool searchEditing = false;  // 搜索框是否处于编辑态（决定是否创建原生 EDIT）
    bool searchFocusPending = false;
    std::set<std::string> tagFilter;
    OverlayKind overlay = OV_NONE;
    // 展开缓存
    std::vector<Occurrence> occs;
    DateTime occFrom, occTo;
    bool occValid = false;
    // 视图状态
    int scrollMin = 6 * 60;      // 周/日视图顶部（分钟）
    int agendaScroll = 0;
    int agendaTopDay = 0;        // 日程视图起始日偏移
    int lastClickMin = 9 * 60;   // 上次点击的空位时间（双击新建用）
    DateTime lastClickDay;
    std::set<std::wstring> selected;   // 选中的 occurrence key
    // 编辑相关
    EditorState editor;
    QuickState quick;
    CmdkState cmdk;
    SlashState slash;
    ShareUiState share;
    std::vector<Subscription> subscriptions;
    SyncUiState syncUi;
    BotState bot;
    FocusState focus;
    HabitsState habits;
    ProfilesState profiles;
    std::wstring profileName;      // 当前档案显示名
    std::string profileTag;        // 当前档案标签过滤，空=全部

    SubscriptionUi subUi;
    BulkState bulk;
    ImportState import;
    ConfirmState confirm;
    CleanupState cleanup;
    ColorPickState colorPick;
    SeriesColorState seriesColor;
    RescheduleState reschedule;
    std::wstring dateJump;
    std::wstring dataHealth;   // 启动数据校验结果
    bool backupImport = false;
    std::wstring backupPath;
    std::wstring backupPassword;
    std::wstring backupError;
    int reminderScroll = 0;
    DateTime dayDetail;
    std::vector<Occurrence> dayDetailOccs;
    std::wstring gradeCourse, gradeCredit, gradeScore;   // 成绩面板输入
    bool hotkeysCapturing = false;     // 快捷键设置：正在等待按键
    int hotkeyCaptureIdx = -1;
    std::wstring hotkeyError;
    float settingsScroll = 0;          // 设置面板滚动
    std::wstring agendaAnchorKey;      // 列表多选的 Shift 范围锚点
    bool agendaBoard = false;
    int statsTab = 1;
    DateTime statsAnchor;
    bool boardAllEvents = false;      // 看板显示全部日程（不只 DDL/任务）
    int boardFilter = 0;              // 0=全部 1=作业 2=考试
    std::string boardCourseFilter;    // 空=全部课程
    std::set<std::wstring> boardSelected;
    MenuState menu;
    TagEditState tagEdit;
    bool settingsOpen = false;
    std::wstring setSemStart, setWaterFrom, setWaterTo;
    // 字体选择器
    bool fontPickerOpen = false;
    int fontPickerTarget = -1;
    int fontPickerScroll = 0;
    std::vector<std::wstring> fontList;
    bool sidebarCollapsed = false;
    // 提醒
    DWORD toastUntil = 0;
    std::wstring toastText;
    // 数据变更
    bool needSave = false;
};

extern AppState app;

// 工具
std::wstring OccKey(const Occurrence& o);
bool OccMatchesFilter(const Occurrence& o);
void InvalidateOccs();
bool ShiftSelectPressed();
bool CtrlSelectPressed();
void OpenTemplateNewMenu();
void SaveEditorAsTemplate();
void OpenLocationPresetMenu();
std::wstring TodayLabel();
std::wstring FormatTimeRange(const Occurrence& o);
std::vector<std::wstring> FindEventConflicts(const Event& ev, const std::string& ignoreSeriesId = "");
void DrawTagIcon(Gdiplus::Graphics& g, const Tag& tag, Gdiplus::RectF box, Gdiplus::Color fallback, float weight);
std::wstring RelativeDayLabel(const DateTime& d);
bool DateInputParse(const std::wstring& s, const DateTime& base, DateTime& out);
bool TimeInputParse(const std::wstring& s, int& h, int& mi);
std::wstring TimeInputText(const DateTime& t);

// 视图/覆盖层
void OpenEditorNew(const DateTime& day, int hour = 9, int minute = 0);
void OpenEditorForOccurrence(const Occurrence& o);
void OpenEditorExisting(const std::string& seriesId, const DateTime& origStart);
void CloseOverlay();
void OpenQuickAdd();
void OpenCommandPalette();
void SetTimeZoom(int delta);
void FitTimeZoom();
bool WeekViewInZoomControl(float px, float py);
void CmdkScroll(int delta);
void ShareInit();
void ShareShutdown();
void ShareTick();
void OpenSharePanel();
void DrawShareOverlay(Gdiplus::Graphics& g, float W, float H);
void ShareProbeStart(int port);
void OpenSharePanel();

void OpenSlashPalette(const std::wstring& prefill = L"");
void CloseSlashPalette();
void SlashExecuteCurrent();
void SlashScroll(int delta);
bool SlashProbeRun(const std::wstring& line, std::wstring& err);
void DrawSlashOverlay(Gdiplus::Graphics& g, float W, float H);
void CloseCommandPalette();
void DrawCommandPaletteOverlay(Gdiplus::Graphics& g, float W, float H);
bool CmdkHotkeyMatches(WPARAM key, bool ctrl, bool alt, bool shift);
bool CmdkProbeRun(const std::wstring& text, std::wstring& err);
HWND CmdkEditHandleForProbe();
void OpenBulkAdd();
void OpenSettings();
void OpenConfirm(const std::wstring& title, const std::wstring& body, std::function<void()> onOk,
                 const std::wstring& okText = L"确定", const std::wstring& cancelText = L"取消");
void OpenMenu(float x, float y, std::vector<MenuItemDef> items, OverlayKind returnOverlay = OV_NONE);
void DoSaveStore();
void ApplyThemeFromSettings();
void ApplyFontSettings();
void OpenFontSettings();
void OpenWidgetSettings();
void OpenCleanup();
void OpenColorPicker(const std::wstring& title, const std::wstring& initialHex,
                     std::function<void(const std::wstring&)> onOk);
void OpenSeriesColors();
void OpenRescheduleDialog(const std::vector<Occurrence>& occs);
void OpenRescheduleFromSelection();
void DrawRescheduleOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawSelectionBar(Gdiplus::Graphics& g, float W, float H);
void DrawColorPickOverlay(Gdiplus::Graphics& g, float W, float H);
void OpenDateJump();
void DrawDateJumpOverlay(Gdiplus::Graphics& g, float W, float H);
void OpenDayDetail(const DateTime& day);
void DrawDayDetailOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawAgendaBoard(Gdiplus::Graphics& g, float x, float y, float w, float h);
void OpenStats(int tab = -1, const DateTime& anchor = DateTime{0, 1, 1, 0, 0});
void DrawStatsOverlay(Gdiplus::Graphics& g, float W, float H);

// 版本快照 / 回滚
void EnsureVersionSnapshot();
void OpenVersionsOverlay();
void DrawVersionsOverlay(Gdiplus::Graphics& g, float W, float H);
void RollbackToVersion(const std::wstring& exePath, const std::wstring& dataPath, const std::wstring& label);

// 加密备份包（AES-256-GCM + PBKDF2）
bool EncryptBackupFile(const std::wstring& path, const std::string& plain, const std::wstring& password, std::wstring& err);
bool DecryptBackupFile(const std::wstring& path, const std::wstring& password, std::string& plain, std::wstring& err);
void OpenBackupExport();
void OpenBackupImport();
void DrawBackupOverlay(Gdiplus::Graphics& g, float W, float H);
struct TableImportResult {
    std::vector<Event> events;
    std::vector<std::wstring> warnings;
    std::wstring summary;
    bool ok = false;
    bool table = false;
};
TableImportResult ParseScheduleText(const std::wstring& text);
std::wstring ReadClipboardText();
void LoadSmartImport(const std::wstring& file, const std::wstring& text, bool fromClipboard, const std::wstring& label);
void DrawSmartImportOverlay(Gdiplus::Graphics& g, float W, float H);
bool WindowsOcrImage(const std::wstring& imagePath, const std::wstring& lang, std::wstring& outText, std::wstring& err);
void OpenOcrResult();
void OpenOcrFromImageFile();
void DrawOcrResultOverlay(Gdiplus::Graphics& g, float W, float H);
void LoadSubscriptions();
void SaveSubscriptions();
void OpenSubscriptions();
void DrawSubscriptionsOverlay(Gdiplus::Graphics& g, float W, float H);
int SyncSubscriptionsDue(bool force, bool userVisible);
bool AddSubscription(const std::wstring& url, const std::wstring& name, std::wstring& err);
void DeleteSubscription(int index);
bool DbEngineEnabled();
bool DbLoadJson(std::string& out);
bool DbSaveJson(const std::string& json);
bool DbAvailable();
int DbEventRowCount();
bool SqliteEnableStorage(std::wstring& err);
bool SqliteRollbackToJson(std::wstring& err);
void OpenDbManager();
void DrawDbManagerOverlay(Gdiplus::Graphics& g, float W, float H);
void SyncInit();
void SyncRecordSave(const std::string& storeJson);
void SyncRecordTombstone(const std::string& id, const std::wstring& uid, const std::wstring& title);
const std::wstring& SyncDeviceId();
const std::wstring& SyncDeviceName();
int SyncOplogCount();
int SyncTombstoneCount();
std::wstring SyncLastOpText();
std::wstring SyncBuildMigrationPackage();
bool SyncImportMigrationPackage(const std::string& plain, std::wstring& err);
void OpenSyncCenter();
void OpenAccountSync();
void DrawAccountSyncOverlay(Gdiplus::Graphics& g, float W, float H);
void AutoBackupTick();
void BotInit();
void BotShutdown();
void BotTick();
std::wstring BotTestCommand(const std::wstring& raw);
void BotSetEnabled(bool on);
void BotSaveToken(const std::wstring& token);
void OpenBotPanel();
void DrawBotOverlay(Gdiplus::Graphics& g, float W, float H);
void ApplyPulledStore(const std::string& storeJson);
bool SyncAccountRegister(const std::wstring& server, const std::wstring& user, const std::wstring& pass, std::wstring& err);
bool SyncAccountLogin(const std::wstring& server, const std::wstring& user, const std::wstring& pass, std::wstring& err);
bool SyncPushStore(const std::wstring& server, const std::wstring& token, const std::string& store, std::wstring& err);
bool SyncPullStore(const std::wstring& server, const std::wstring& token, std::string& store, std::wstring& err);
void DrawSyncCenterOverlay(Gdiplus::Graphics& g, float W, float H);
void OpenExamPanel();
void DrawExamPanelOverlay(Gdiplus::Graphics& g, float W, float H);
int ExamPanelProbe(std::wstring& firstTitle, int& days);
void FocusInit();
void FocusStart();
void FocusPause();
void FocusFinish(bool record);
void FocusReset();
void FocusTick();
void FocusRecordSession(int minutes, const std::wstring& label);
int FocusTodayMinutes();
int FocusWeekMinutes();
int FocusWeekCount();
void OpenFocusPanel();
void DrawFocusPanelOverlay(Gdiplus::Graphics& g, float W, float H);
bool FocusProbeRun(std::wstring& err);
void HabitsInit();
void HabitsSave();
void HabitsToggle(int habitIndex, const DateTime& day);
int HabitStreak(const Habit& h);
int HabitWeekCount(const Habit& h);
int HabitsDoneThisWeek();
void OpenHabitsPanel();
void DrawHabitsPanelOverlay(Gdiplus::Graphics& g, float W, float H);
bool HabitsProbeRun(std::wstring& err);
void OpenPrintPanel();
void OpenHotkeys();
void DrawHotkeysOverlay(Gdiplus::Graphics& g, float W, float H);
struct HealthReport {
    int totalEvents = 0;
    int totalTags = 0;
    int duplicate = 0;
    int endBeforeStart = 0;
    int tooLong = 0;
    int emptyTitle = 0;
    int missingTags = 0;
    int badAlarms = 0;
    int issues() const { return duplicate + endBeforeStart + tooLong + emptyTitle + missingTags + badAlarms; }
};
HealthReport ScanHealth(const Store& st);
void OpenHealth();
void DrawHealthOverlay(Gdiplus::Graphics& g, float W, float H);

struct MergeStats {
    int added = 0;        // 服务器有、本地没有
    int updated = 0;      // 服务器版本更新，采用服务器
    int kept = 0;         // 本地较新，保留本地
    int localOnly = 0;    // 本地独有
    int remoteOnly = 0;   // 服务器独有
    int tagsAdded = 0;
    int deleted = 0;   // 墓碑删除传播
};
Store MergeStores(const Store& local, const Store& remote, MergeStats& st);
void OpenGrades();
void DrawGradesOverlay(Gdiplus::Graphics& g, float W, float H);
void LoadGrades();
double GradesGpa();
double GradesCredits();
int GradesCount();
bool GradesProbeAdd(const std::wstring& course, double credit, double score);
std::string GradesToJsonArray();
std::string GradesAttachToStoreJson(const std::string& storeJson);
void GradesApplyFromStoreJson(const std::string& storeJson);
bool ExportWeeklyPlanPng(std::wstring& err, std::wstring* outPath = nullptr);
void DrawPrintPanelOverlay(Gdiplus::Graphics& g, float W, float H);
bool ExportWeeklyPlanHtml(std::wstring& err, std::wstring* outPath = nullptr);
bool PrintPlanProbe(std::wstring& err);
void ProfilesInit();
void ProfilesSave();
void ApplyProfile(const std::string& id);
void ProfilesSeedDemo();
void OpenProfilesPanel();
void DrawProfilesPanelOverlay(Gdiplus::Graphics& g, float W, float H);
bool ProfilesProbeRun(std::wstring& err);
std::wstring ActiveProfileName();
void HabitsSeedDemo();
void OpenSmartImportFromFile();
void OpenReminderCenter();
void OpenThemeStudio();
void DrawThemeStudioOverlay(Gdiplus::Graphics& g, float W, float H);
bool ExportThemeToFile(const std::wstring& path);
bool ImportThemeFromFile(const std::wstring& path, std::wstring& err);
void DrawReminderCenterOverlay(Gdiplus::Graphics& g, float W, float H);
int MissedReminderCount();
void SeedReminderCenterDemo();
void DrawYearView(Gdiplus::Graphics& g, float x, float y, float w, float h);
void ViewHeader(Gdiplus::Graphics& g, float x, float y, float w);
void DrawSeriesColorsOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawCleanupOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawWidgetSettingsOverlay(Gdiplus::Graphics& g, float W, float H);
void SyncAutoStart();
void HideMainWindowToTray();
void UndoAction();
void EditorSave();
void RedoAction();
void DeleteSelected();
void ToggleSelected(const std::wstring& key);
bool MultiSelectPressed();   // 左键按下时记录的 Shift/Ctrl（避免松开时序导致 Shift 多选失效）
void ToggleOccurrenceDone(const Occurrence& o, bool done);
void RefreshNow();
void RestartApp();
void SeedDemoData(Store& st);

// 子窗口
void CreateWidgetWindow();
void DestroyWidgetWindow();
void RecreateWidgetWindow();
void RefreshWidgetNow();
void WidgetHealthTick();
void UpdateWidgetFullscreenVisibility();
void ToggleWidgetWindow();
bool WidgetEnabled();
void ShowTestReminder();
void StartReminderEngine();
void StopReminderEngine();
void OnTimerTick();
void OnAppClose();

// 农历
std::wstring LunarText(const DateTime& d);
bool LunarDateOf(const DateTime& d, int& lunarYear, int& month, int& day, bool& leap);
bool LunarToSolarDate(int lunarYear, int month, int day, bool leap, Date& out);
int LunarYearOf(const DateTime& d);
std::wstring DisplayEventTitle(const Occurrence& o);
struct HolidayInfo {
    std::wstring name;
    std::wstring category;   // 传统 / 国家 / 纪念 / 国际 / 节气
};
std::wstring LunarFestival(const DateTime& d);
std::vector<HolidayInfo> HolidaysOn(const DateTime& d);
std::wstring LunarFullText(const DateTime& d);
const wchar_t* LunarMonthCN(int m, bool leap);
const wchar_t* LunarDayCN(int d);

// 二十四节气（返回该日期对应的节气名；空=不是节气）
std::wstring SolarTermNameOn(const DateTime& d);
// 某年 24 个节气日期（小寒起，按时间顺序）
std::vector<DateTime> SolarTermDatesOfYear(int year);

// 设置页 / 导入 / 快速添加等绘制（dialogs.cpp）
void DrawEditorOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawQuickAddOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawBulkAddOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawSettingsOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawImportOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawConfirmOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawMenuOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawTagEditOverlay(Gdiplus::Graphics& g, float W, float H);
int HelpEntryCount();
void OpenHelp();
void DrawHelpOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawAboutOverlay(Gdiplus::Graphics& g, float W, float H);
void DrawFontOverlay(Gdiplus::Graphics& g, float W, float H);
bool HandleOverlayKey(WPARAM key);
void CloseAllEdits();
