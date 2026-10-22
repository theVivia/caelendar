// habits.cpp - 习惯打卡（本地优先，与统计联动）
#include "app.h"
#include "json.h"
#include <algorithm>
#include <ctime>

using namespace Gdiplus;
using namespace gfx;

namespace {

std::wstring HabitsPath() { return PathJoin(GetDataDir(), L"habits.json"); }

std::string KeyOf(const DateTime& d) {
    return WideToUtf8(Fmt(L"%04d-%02d-%02d", d.y, d.mo, d.d));
}

void Save() {
    EnsureDir(GetDataDir());
    JValue root = JValue::Obj();
    root.Set(L"version") = JValue(1);
    JValue arr = JValue::Arr();
    for (auto& h : app.habits.list) {
        JValue o = JValue::Obj();
        o.Set(L"id") = JValue(Utf8ToWide(h.id));
        o.Set(L"name") = JValue(h.name);
        o.Set(L"target") = JValue(h.targetPerWeek);
        JValue c = JValue::Obj();
        for (auto& kv : h.checks) if (kv.second) c.Set(Utf8ToWide(kv.first)) = JValue(true);
        o.Set(L"checks") = c;
        arr.Push(o);
    }
    root.Set(L"habits") = arr;
    WriteFileBytesAtomic(HabitsPath(), JsonDump(root, true));
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

DateTime WeekStart(const DateTime& today) {
    int wd = DayOfWeek(today.y, today.mo, today.d);
    int offset = app.store.settings.weekStart == 1 ? ((wd + 6) % 7) : wd;
    return AddDaysDT(today, -offset);
}

bool IsSameDay(const DateTime& a, const DateTime& b) {
    return a.y == b.y && a.mo == b.mo && a.d == b.d;
}

}  // namespace

void HabitsInit() {
    app.habits.list.clear();
    bool ok = false;
    std::string raw = ReadFileBytes(HabitsPath(), &ok);
    if (!ok || raw.empty()) return;
    std::wstring err;
    JValue root = JsonParse(raw, &err);
    if (!root.IsObj()) return;
    for (auto& v : root.Get(L"habits").arr) {
        if (!v.IsObj()) continue;
        Habit h;
        h.id = WideToUtf8(v.Get(L"id").AsStr());
        h.name = v.Get(L"name").AsStr();
        h.targetPerWeek = std::max(1, std::min(7, v.Get(L"target").AsInt(7)));
        for (auto& kv : v.Get(L"checks").obj) {
            if (kv.second.AsBool(false)) h.checks[WideToUtf8(kv.first)] = true;
        }
        if (h.id.empty()) h.id = WideToUtf8(Fmt(L"hb%lld", (long long)time(nullptr)));
        if (h.name.empty()) h.name = L"习惯";
        app.habits.list.push_back(h);
    }
    DebugLog(L"习惯加载: %d 个", (int)app.habits.list.size());
}

void HabitsSave() { Save(); }

void HabitsToggle(int habitIndex, const DateTime& day) {
    if (habitIndex < 0 || habitIndex >= (int)app.habits.list.size()) return;
    if (DaySerial(day.y, day.mo, day.d) > DaySerial(app.now.y, app.now.mo, app.now.d)) return;  // 不能打未来的卡
    Habit& h = app.habits.list[habitIndex];
    std::string k = KeyOf(day);
    if (h.checks.count(k)) h.checks.erase(k);
    else h.checks[k] = true;
    Save();
    DebugLog(L"习惯打卡: %s %s", h.name.c_str(), Utf8ToWide(k).c_str());
}

int HabitStreak(const Habit& h) {
    int streak = 0;
    DateTime d = Today();
    for (int i = 0; i < 400; ++i) {
        std::string k = KeyOf(d);
        if (h.checks.count(k) && h.checks.at(k)) {
            ++streak;
            d = AddDaysDT(d, -1);
        } else {
            if (i == 0) { d = AddDaysDT(d, -1); continue; }  // 今天没打卡，从昨天开始算连续
            break;
        }
    }
    return streak;
}

int HabitWeekCount(const Habit& h) {
    DateTime start = WeekStart(Today());
    int n = 0;
    for (int i = 0; i < 7; ++i) {
        std::string k = KeyOf(AddDaysDT(start, i));
        if (h.checks.count(k) && h.checks.at(k)) ++n;
    }
    return n;
}

int HabitsDoneThisWeek() {
    DateTime start = WeekStart(Today());
    int n = 0;
    for (auto& h : app.habits.list) {
        for (int i = 0; i < 7; ++i) {
            std::string k = KeyOf(AddDaysDT(start, i));
            if (h.checks.count(k) && h.checks.at(k)) ++n;
        }
    }
    return n;
}

void HabitsSeedDemo() {
    if (!app.habits.list.empty()) return;
    auto add = [](const wchar_t* name, int target) {
        Habit h;
        h.id = WideToUtf8(Fmt(L"hb%lld%02d", (long long)time(nullptr), (int)app.habits.list.size()));
        h.name = name;
        h.targetPerWeek = target;
        app.habits.list.push_back(h);
    };
    add(L"早起", 7);
    add(L"背单词", 5);
    add(L"运动 30 分钟", 4);
    DateTime start = WeekStart(Today());
    for (int i = 0; i < 3 && i < (int)app.habits.list.size(); ++i) {
        for (int d = 0; d < 3; ++d) {
            DateTime day = AddDaysDT(start, d);
            if (DaySerial(day.y, day.mo, day.d) > DaySerial(app.now.y, app.now.mo, app.now.d)) break;
            app.habits.list[i].checks[KeyOf(day)] = true;
        }
    }
    Save();
}

void OpenHabitsPanel() {
    HabitsInit();
    app.overlay = OV_HABITS;
    UIInvalidate();
}

bool HabitsProbeRun(std::wstring& err) {
    HabitsInit();
    // 自包含测试数据：不依赖 demo 种子与“今天星期几”
    app.habits.list.clear();
    Habit h;
    h.id = "hb_probe";
    h.name = L"早起";
    h.targetPerWeek = 7;
    app.habits.list.push_back(h);
    DateTime today = Today();
    DateTime monday = WeekStart(today);
    app.habits.list[0].checks[KeyOf(today)] = true;
    app.habits.list[0].checks[KeyOf(AddDaysDT(today, -1))] = true;
    app.habits.list[0].checks[KeyOf(monday)] = true;
    Save();
    int week = HabitWeekCount(app.habits.list[0]);
    int streak = HabitStreak(app.habits.list[0]);
    int done = HabitsDoneThisWeek();
    bool file = FileExists(HabitsPath());
    DebugLog(L"probe-habits: week=%d streak=%d done=%d file=%d", week, streak, done, file ? 1 : 0);
    err.clear();
    return week >= 1 && streak >= 2 && file;
}

void DrawHabitsPanelOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(780), ph = S(620);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Txt(g, L"习惯打卡", S(17), true, th.text, RectF(x + S(24), y + S(16), S(240), S(26)));
    Txt(g, L"点击本周圆圈即可打卡；记录保存在 data/habits.json，并与学习统计联动。", S(9), false, th.dim2,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    DateTime weekStart = WeekStart(Today());
    static const wchar_t* wdNames[7] = { L"一", L"二", L"三", L"四", L"五", L"六", L"日" };
    // 周标题
    float headerY = y + S(82);
    Txt(g, L"习惯", S(9), false, th.dim2, RectF(x + S(28), headerY, S(200), S(16)));
    for (int i = 0; i < 7; ++i) {
        DateTime day = AddDaysDT(weekStart, i);
        bool today = IsSameDay(day, Today());
        float cx = x + S(330) + i * S(46);
        Txt(g, Fmt(L"%s", wdNames[i]), S(8.5f), today, today ? th.accent : th.dim2,
            RectF(cx, headerY, S(36), S(14)), ALIGN_CENTER);
        Txt(g, Fmt(L"%d", day.d), S(8), false, today ? th.accent : th.dim2,
            RectF(cx, headerY + S(14), S(36), S(14)), ALIGN_CENTER);
    }
    Txt(g, L"本周", S(9), false, th.dim2, RectF(x + S(664), headerY, S(80), S(16)), ALIGN_CENTER);

    float rowY = y + S(118);
    float rowH = S(60);
    if (app.habits.list.empty()) {
        Txt(g, L"还没有习惯。在下方添加「早起 / 背单词 / 运动」等，点击本周格子打卡。", S(10.5f), false, th.dim2,
            RectF(x + S(24), rowY + S(16), pw - S(48), S(40)), ALIGN_LEFT, false, true);
    }
    for (int i = 0; i < (int)app.habits.list.size() && i < 6; ++i) {
        auto& h = app.habits.list[i];
        float ry = rowY + i * rowH;
        RectF row(x + S(24), ry, pw - S(48), rowH - S(6));
        FillRoundRect(g, row, S(8), WithAlpha(th.panel2, 150));
        Txt(g, h.name, S(11), true, th.text, RectF(row.X + S(12), row.Y, S(180), row.Height), ALIGN_VCENTER, false, true);
        Txt(g, Fmt(L"连续 %d 天", HabitStreak(h)), S(8.5f), false, th.dim2,
            RectF(row.X + S(200), row.Y, S(96), row.Height), ALIGN_VCENTER);
        int weekN = HabitWeekCount(h);
        for (int d = 0; d < 7; ++d) {
            DateTime day = AddDaysDT(weekStart, d);
            bool done = h.checks.count(KeyOf(day)) && h.checks.at(KeyOf(day));
            bool today = IsSameDay(day, app.now);
            float cx = row.X + S(318) + d * S(46);
            float cy = row.Y + row.Height / 2;
            if (done) {
                FillCircle(g, cx, cy, S(11), th.accent);
                Icon(g, ICO_CHECK, RectF(cx - S(7), cy - S(7), S(14), S(14)), th.onAccent, 2.0f);
            } else {
                FillCircle(g, cx, cy, S(11), WithAlpha(th.panel, 230));
                StrokeCircle(g, cx, cy, S(11), today ? WithAlpha(th.accent, 160) : th.borderSoft, S(1));
            }
            int idx = i;
            AddHit(cx - S(15), cy - S(15), S(30), S(30), 0, [idx, day]() { HabitsToggle(idx, day); UIInvalidate(); });
        }
        Txt(g, Fmt(L"%d/%d", weekN, h.targetPerWeek), S(9), weekN >= h.targetPerWeek, 
            weekN >= h.targetPerWeek ? th.success : th.dim,
            RectF(row.X + S(646), row.Y, S(60), row.Height), ALIGN_CENTER | ALIGN_VCENTER);
        UI::IconBtn(g, RectF(RX(row) - S(28), row.Y + row.Height / 2 - S(12), S(22), S(22)), CMD_IMPORT_OK + 100 + i,
                    ICO_TRASH, 10, [i]() {
                        if (i >= 0 && i < (int)app.habits.list.size()) {
                            app.habits.list.erase(app.habits.list.begin() + i);
                            HabitsSave();
                            UIInvalidate();
                        }
                    });
    }

    // 新增
    float fy = y + ph - S(150);
    Txt(g, L"添加习惯", S(10.5f), false, th.dim2, RectF(x + S(24), fy, S(120), S(18)));
    RectF nameBox(x + S(24), fy + S(22), S(260), S(34));
    FillRoundRect(g, nameBox, S(8), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, nameBox, S(8), th.borderSoft, S(1));
    UIEdit(9990, RectF(nameBox.X + S(10), nameBox.Y + S(7), nameBox.Width - S(20), S(20)), &app.habits.newName, false, S(11));
    UI::Stepper(g, RectF(x + S(296), fy + S(22), S(150), S(34)), CMD_IMPORT_OK + 200, CMD_IMPORT_OK + 201,
                Fmt(L"每周 %d 天", app.habits.newTarget), []() {
                    app.habits.newTarget = std::max(1, app.habits.newTarget - 1);
                    UIInvalidate();
                }, []() {
                    app.habits.newTarget = std::min(7, app.habits.newTarget + 1);
                    UIInvalidate();
                }, 9.5f);
    UI::Btn(g, RectF(x + S(458), fy + S(22), S(120), S(34)), CMD_IMPORT_OK + 202, L"添加", 0, ICO_PLUS, []() {
        std::wstring name = Trim(app.habits.newName);
        if (name.empty()) { ShowToast(L"先输入习惯名称"); return; }
        Habit h;
        h.id = WideToUtf8(Fmt(L"hb%lld%02d", (long long)time(nullptr), (int)app.habits.list.size()));
        h.name = name;
        h.targetPerWeek = std::max(1, std::min(7, app.habits.newTarget));
        app.habits.list.push_back(h);
        app.habits.newName.clear();
        HabitsSave();
        ShowToast(L"已添加习惯");
        UIInvalidate();
    });
    Txt(g, L"建议目标：每周 3–7 天；今天以后的日期不能提前打卡。", S(8.5f), false, th.dim2,
        RectF(x + S(24), fy + S(66), pw - S(48), S(16)));
    Txt(g, L"本周合计：", S(10), false, th.dim, RectF(x + S(24), fy + S(92), S(100), S(18)));
    Txt(g, Fmt(L"%d 次打卡", HabitsDoneThisWeek()), S(11), true, th.accent,
        RectF(x + S(104), fy + S(92), S(160), S(18)));

    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
