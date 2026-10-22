// model.h - 数据模型、存储、增删改
#pragma once
#include "util.h"
#include "json.h"
#include <string>
#include <vector>
#include <map>
#include <set>

struct Tag {
    std::string id;
    std::wstring name;
    std::wstring color;
    std::wstring icon;
    bool builtin = false;
    bool task = false;             // 计入 DDL / 作业看板
    std::wstring customIcon;       // 自定义图标（相对 data 目录，如 icons/xxx.png）
};

enum Priority { PRIO_NONE = 0, PRIO_LOW = 1, PRIO_MED = 2, PRIO_HIGH = 3 };
enum EventStatus { ST_CONFIRMED = 0, ST_TENTATIVE = 1, ST_CANCELLED = 2 };
enum RecurFreq { RF_NONE = 0, RF_DAILY = 1, RF_WEEKLY = 2, RF_MONTHLY = 3, RF_YEARLY = 4 };

struct Alarm {
    int offsetMin = -10;   // 距开始时间的分钟数（负=提前）
    bool sound = true;
    std::wstring text;
};

constexpr int kDataFormatVersion = 2;   // 数据格式版本（用于未来迁移）

struct Recur {
    int freq = RF_NONE;
    int interval = 1;
    std::vector<int> byDay;   // 0=周日..6=周六
    int byMonthDay = 0;       // 每月几号（0=用开始日期）
    int byMonth = 0;          // 每年几月（0=用开始月份）
    int nthWeekday = 0;       // 每月第几个周几（0=不启用；-1=最后一个）
    DateTime until; bool hasUntil = false;
    int count = 0;            // 0=无限
    std::vector<DateTime> exDates;
    int wkst = 1;
    bool lunar = false;      // 每年重复时按农历同月同日
    bool IsNone() const { return freq == RF_NONE; }
};

struct Event {
    std::string id;
    std::wstring uid;
    std::wstring title;
    std::wstring desc;
    std::wstring location;
    std::wstring url;
    DateTime start, end;
    bool allDay = false;
    std::vector<std::string> tags;
    int priority = PRIO_NONE;
    int status = ST_CONFIRMED;
    bool done = false;
    bool ddl = false;    // 独立标记：计入 DDL 看板（与标签无关）
    Recur recur;
    std::vector<Alarm> alarms;
    std::wstring color;      // 空 = 按标签自动
    std::wstring source;     // 本地 / ICS 文件名
    DateTime created, updated;
    std::vector<Event> overrides;   // 重复规则里被单独修改的实例
    DateTime recurId;               // 仅 overrides 内部使用
    bool deletedOverride = false;   // 仅 overrides 内部使用

    int DurationMin() const { return MinutesBetween(start, end); }
    DateTime DisplayEnd() const {
        if (CompareDT(end, start) <= 0) return AddMinutesDT(start, allDay ? 1440 : 1);
        return end;
    }
    bool IsRecurring() const { return recur.freq != RF_NONE; }
    bool HasTag(const std::string& t) const {
        for (auto& x : tags) if (x == t) return true;
        return false;
    }
};

struct Semester { bool enabled = true; DateTime start = MakeDT(2026, 9, 7); int weeks = 18; bool showParity = true; };
struct WaterSettings {
    bool enabled = true;
    int intervalMin = 45;
    int fromMin = 8 * 60;
    int toMin = 22 * 60 + 30;
    int targetCups = 8;
    int cupMl = 250;
    bool sound = true;
};
struct NotifySettings { bool enabled = true; bool sound = true; int defaultLead = 10; int snoozeMin = 10; bool focusMode = false; std::wstring soundFile; };
struct WidgetSettings {
    bool enabled = true;
    std::wstring mode = L"card";      // card / bar / list / clock / week
    std::wstring skin = L"theme";     // theme / glass / solid / neon
    int scale = 100;                  // 80 ~ 160
    int opacity = 94;
    int x = -1, y = -1;
    bool showWater = true;
    bool showDate = true;
    bool showToday = true;
    bool alwaysOnTop = true;
    bool clickThrough = false;    // 鼠标穿透
    bool lowPower = true;        // 低刷新率省电
    bool showTasks = true;       // 今日剩余任务/DDL
};

constexpr int kFontPartCount = 8;   // 与 gfx::FontPartId 顺序一致

struct FontPartSetting {
    std::wstring family;   // 空 = 跟随全局
    int size = 100;        // 百分比
};

struct Settings {
    std::wstring theme = L"astral";
    std::wstring customBg = L"#07080D";
    std::wstring customAccent = L"#E3C177";
    int themeRadius = 10;      // 自定义圆角 0-20
    int themeDensity = 100;    // 信息密度 85-115
    bool customLight = false;
    std::wstring cmdkHotkey = L"Ctrl+K";
    std::wstring hotkeyMonth = L"M";
    std::wstring hotkeyWeek = L"W";
    std::wstring hotkeyDay = L"D";
    std::wstring hotkeyYear = L"Y";
    std::wstring hotkeyAgenda = L"A";
    std::wstring hotkeyToday = L"T";
    std::wstring hotkeySearch = L"Ctrl+F";
    std::wstring hotkeyNew = L"Ctrl+N";
    std::wstring fontUI = L"Noto Sans SC";
    std::wstring fontNum = L"Bahnschrift";
    int fontScale = 100;
    FontPartSetting fontParts[kFontPartCount];
    bool closeToTray = true;
    std::wstring autoBackupDir;   // 自动备份文件夹（空=关闭）
    bool autoStart = false;
    int weekStart = 1;
    int timeZoom = 100;      // 周/日视图时间轴缩放 25-160
    Semester semester;
    WaterSettings water;
    NotifySettings notify;
    WidgetSettings widget;
    bool highContrast = false;   // 无障碍：高对比度
    bool monthChips = true;     // 月视图显示事件条
    bool monthCounts = false;   // 月视图显示每日事件数
    bool lunar = true;
    bool weekNumbers = true;
    bool reduceMotion = false;
    bool seededSample = false;
};

struct Occurrence {
    std::string seriesId;
    Event ev;              // 生效事件（覆盖后的）
    DateTime start, end;   // 本次的真实时间
    bool isRecurring = false;
    bool isOverride = false;
    DateTime origStart;
};

class Store {
public:
    int version = 1;
    std::vector<Event> events;
    std::vector<Tag> tags;
    Settings settings;
    std::map<std::wstring, int> water;   // "2026-10-17" -> 杯数
    // 删除墓碑（同步用）：事件 id -> 删除时间（ISO），随 JSON 上传，用来把删除传播到其他设备
    std::vector<std::pair<std::string, std::wstring>> tombstones;
    std::wstring updatedAt;
    std::wstring path;
    bool dirty = false;

    void EnsureDefaults();
    bool Load();
    bool Save();
    std::string ToJsonStr() const;
    bool FromJsonStr(const std::string& s, std::wstring* err = nullptr);

    void Snapshot();
    bool Undo();
    bool Redo();
    bool CanUndo() const { return !undoStack.empty(); }
    bool CanRedo() const { return !redoStack.empty(); }
    std::vector<std::string> undoStack, redoStack;

    Event* Find(const std::string& id);
    const Event* Find(const std::string& id) const;
    std::string AddEvent(Event e);
    bool UpdateEvent(const Event& e);
    bool DeleteEvent(const std::string& id);
    std::string DuplicateEvent(const std::string& id, DateTime newStart = DateTime{ 0,0,0,0,0 });
    void ToggleDone(const std::string& id, bool done);
    // 只完成某一次（重复事件写 override，不动整个系列）
    bool SetOccurrenceDone(const std::string& seriesId, const DateTime& origStart,
                           const DateTime& occStart, const DateTime& occEnd, bool done);

    // 重复事件：scope 0=仅本次 1=此后所有 2=全部
    bool UpdateOccurrence(const std::string& seriesId, const DateTime& origStart, const Event& effective, int scope);
    bool DeleteOccurrence(const std::string& seriesId, const DateTime& origStart, int scope);
    bool RescheduleOccurrence(const std::string& seriesId, const DateTime& origStart, const Event& effective,
                              const DateTime& occStart, const DateTime& occEnd, const Date& newDay);

    std::string AddTag(const std::wstring& name, const std::wstring& color, const std::wstring& icon = L"");
    Tag* FindTag(const std::string& id);
    const Tag* FindTag(const std::string& id) const;
    std::wstring TagName(const std::string& id) const;
    std::wstring TagColor(const std::string& id) const;
    std::string EnsureTag(const std::wstring& name, const std::wstring& color = L"");
    bool DeleteTag(const std::string& id);

    int WaterToday() const;
    void AddWaterCup();
    void SetWater(const std::wstring& date, int cups);

    int revision = 0;   // 每次内容变更 +1：挂件等订阅方据此立即刷新
    void Touch() { updatedAt = NowStamp(); dirty = true; ++revision; }
    static std::wstring NowStamp();
};

const wchar_t* DefaultTagIcon(const std::string& id);

// 只读数据校验：返回“通过”或问题摘要
std::wstring ValidateStoreData(const Store& st);

// 默认标签
std::string TagIdForName(const std::wstring& name);
const std::vector<Tag>& DefaultTags();
// 学期周（0 = 不在学期内）
int SemesterWeek(const Settings& s, const DateTime& d);
const wchar_t* PriorityName(int p);
const wchar_t* StatusName(int s);
const wchar_t* RecurFreqName(int f);