// profiles.cpp - 多学期 / 多身份课表档案（只切换视图与学期设置，不搬移事件）
#include "app.h"
#include "json.h"
#include <algorithm>
#include <ctime>

using namespace Gdiplus;
using namespace gfx;

namespace {

std::wstring ProfilesPath() { return PathJoin(GetDataDir(), L"profiles.json"); }

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

DateTime TodayOr(const std::wstring& s) {
    DateTime d;
    if (!s.empty() && DateInputParse(s, Today(), d)) return StartOfDay(d);
    return StartOfDay(Today());
}

std::wstring TagLabel(const std::string& tagId) {
    if (tagId.empty()) return L"全部（不过滤）";
    std::wstring n = app.store.TagName(tagId);
    return n.empty() ? Utf8ToWide(tagId) : n;
}

void Save() {
    EnsureDir(GetDataDir());
    JValue root = JValue::Obj();
    root.Set(L"version") = JValue(1);
    root.Set(L"active") = JValue(Utf8ToWide(app.profiles.activeId));
    JValue arr = JValue::Arr();
    for (auto& p : app.profiles.list) {
        JValue o = JValue::Obj();
        o.Set(L"id") = JValue(Utf8ToWide(p.id));
        o.Set(L"name") = JValue(p.name);
        o.Set(L"start") = JValue(p.start);
        o.Set(L"weeks") = JValue(p.weeks);
        o.Set(L"tag") = JValue(Utf8ToWide(p.tagId));
        arr.Push(o);
    }
    root.Set(L"profiles") = arr;
    WriteFileBytesAtomic(ProfilesPath(), JsonDump(root, true));
}

void EnsureDefault() {
    if (!app.profiles.list.empty()) return;
    CalendarProfile p;
    p.id = "all";
    p.name = L"全部课表";
    p.start = ISODate(app.store.settings.semester.start);
    p.weeks = std::max(1, app.store.settings.semester.weeks);
    p.tagId.clear();
    app.profiles.list.push_back(p);
    app.profiles.activeId = p.id;
}

}  // namespace

void ProfilesInit() {
    app.profiles.list.clear();
    bool ok = false;
    std::string raw = ReadFileBytes(ProfilesPath(), &ok);
    if (ok && !raw.empty()) {
        std::wstring err;
        JValue root = JsonParse(raw, &err);
        if (root.IsObj()) {
            app.profiles.activeId = WideToUtf8(root.Get(L"active").AsStr());
            for (auto& v : root.Get(L"profiles").arr) {
                if (!v.IsObj()) continue;
                CalendarProfile p;
                p.id = WideToUtf8(v.Get(L"id").AsStr());
                p.name = v.Get(L"name").AsStr();
                p.start = v.Get(L"start").AsStr();
                p.weeks = std::max(1, std::min(30, v.Get(L"weeks").AsInt(18)));
                p.tagId = WideToUtf8(v.Get(L"tag").AsStr());
                if (p.id.empty()) continue;
                if (p.name.empty()) p.name = L"课表";
                app.profiles.list.push_back(p);
            }
        }
    }
    EnsureDefault();
    if (app.profiles.activeId.empty()) app.profiles.activeId = app.profiles.list[0].id;
    Save();
    // 恢复当前档案（只设置过滤/学期，不写事件）
    for (auto& p : app.profiles.list) {
        if (p.id == app.profiles.activeId) {
            app.profileName = p.name;
            app.profileTag = p.tagId;
            break;
        }
    }
    DebugLog(L"课表档案: %d 个，当前=%s tag=%s", (int)app.profiles.list.size(),
             app.profileName.c_str(), Utf8ToWide(app.profileTag).c_str());
}

void ProfilesSave() { Save(); }
std::wstring ActiveProfileName() { return app.profileName.empty() ? L"全部课表" : app.profileName; }

void ApplyProfile(const std::string& id) {
    if (app.profiles.list.empty()) ProfilesInit();
    for (auto& p : app.profiles.list) {
        if (p.id != id) continue;
        app.profiles.activeId = p.id;
        app.profileName = p.name;
        app.profileTag = p.tagId;
        DateTime start = TodayOr(p.start);
        app.store.settings.semester.start = start;
        app.store.settings.semester.weeks = std::max(1, std::min(30, p.weeks));
        app.store.settings.semester.enabled = true;
        app.cursor = start;
        app.selected.clear();
        app.store.Touch();
        DoSaveStore();
        Save();
        InvalidateOccs();
        ShowToast(Fmt(L"已切换到课表档案：%s", p.name.c_str()), 2500);
        UIInvalidate();
        return;
    }
}

void ProfilesSeedDemo() {
    ProfilesInit();
    auto ensure = [](const wchar_t* name, const wchar_t* tag, const wchar_t* start, int weeks) {
        for (auto& p : app.profiles.list) if (p.name == name) return;
        CalendarProfile p;
        p.id = WideToUtf8(Fmt(L"pf%lld%02d", (long long)time(nullptr), (int)app.profiles.list.size()));
        p.name = name;
        p.start = start;
        p.weeks = weeks;
        p.tagId = (tag && *tag) ? app.store.EnsureTag(tag) : std::string();
        app.profiles.list.push_back(p);
    };
    ensure(L"本科课表", L"课程", L"2026-09-07", 18);
    ensure(L"辅修课表", L"辅修", L"2026-09-07", 16);
    Save();
}

bool ProfilesProbeRun(std::wstring& err) {
    ProfilesInit();
    ProfilesSeedDemo();
    std::string target;
    for (auto& p : app.profiles.list) if (!p.tagId.empty()) { target = p.id; break; }
    if (target.empty()) { err = L"没有带标签的档案"; return false; }
    ApplyProfile(target);
    int tagged = 0;
    for (auto& e : app.store.events)
        for (auto& t : e.tags) if (t == app.profileTag) { ++tagged; break; }
    bool file = FileExists(ProfilesPath());
    std::wstring active = ActiveProfileName();
    DebugLog(L"probe-profiles: profiles=%d active=%s tag=%s taggedEvents=%d file=%d",
             (int)app.profiles.list.size(), active.c_str(), Utf8ToWide(app.profileTag).c_str(), tagged, file ? 1 : 0);
    ApplyProfile(app.profiles.list[0].id);
    bool allReset = app.profileTag.empty();
    DebugLog(L"probe-profiles: back=%s tagEmpty=%d", ActiveProfileName().c_str(), allReset ? 1 : 0);
    err.clear();
    return file && allReset && !active.empty();
}

void OpenProfilesPanel() {
    ProfilesInit();
    app.overlay = OV_PROFILES;
    UIInvalidate();
}

void DrawProfilesPanelOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(760), ph = S(620);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Txt(g, L"课表档案 / 多学期", S(17), true, th.text, RectF(x + S(24), y + S(16), S(360), S(26)));
    Txt(g, L"档案 = 学期设置 + 可选标签过滤；切换只改变视图，不移动、不删除任何日程。", S(9), false, th.dim2,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    DateTime ws = TodayOr(app.profiles.activeId.empty() ? L"" :
                          (app.profiles.list.empty() ? L"" : app.profiles.list[0].start));
    (void)ws;
    Txt(g, L"当前：" + ActiveProfileName(), S(10), false, th.accent, RectF(x + S(24), y + S(76), S(320), S(18)));
    if (!app.profileTag.empty())
        Txt(g, L"过滤标签：" + TagLabel(app.profileTag), S(9), false, th.dim2,
            RectF(x + S(340), y + S(76), S(360), S(18)));

    float ly = y + S(104);
    float rowH = S(62);
    for (int i = 0; i < (int)app.profiles.list.size() && i < 5; ++i) {
        auto& p = app.profiles.list[i];
        float ry = ly + i * rowH;
        bool active = (p.id == app.profiles.activeId);
        RectF row(x + S(24), ry, pw - S(48), rowH - S(6));
        FillRoundRect(g, row, S(8), WithAlpha(active ? th.accent : th.panel2, active ? 22 : 150));
        StrokeRoundRect(g, row, S(8), active ? WithAlpha(th.accent, 130) : th.borderSoft, S(1));
        Txt(g, p.name, S(12), true, active ? th.accent : th.text,
            RectF(row.X + S(14), row.Y + S(8), S(220), S(20)), ALIGN_LEFT, false, true);
        Txt(g, Fmt(L"%s 起 · %d 周 · %s", p.start.c_str(), p.weeks, TagLabel(p.tagId).c_str()),
            S(9), false, th.dim2, RectF(row.X + S(14), row.Y + S(30), S(400), S(18)), ALIGN_LEFT, false, true);
        if (active) {
            Txt(g, L"当前", S(9), true, th.accent, RectF(RX(row) - S(160), row.Y, S(60), row.Height), ALIGN_RIGHT | ALIGN_VCENTER);
        } else {
            UI::Btn(g, RectF(RX(row) - S(90), row.Y + S(12), S(66), S(28)), CMD_IMPORT_OK + 20 + i, L"切换", 1, -1, [p]() {
                ApplyProfile(p.id);
            });
        }
        if (app.profiles.list.size() > 1) {
            UI::IconBtn(g, RectF(RX(row) - S(32), row.Y + row.Height / 2 - S(11), S(22), S(22)),
                        CMD_IMPORT_OK + 40 + i, ICO_TRASH, 10, [i]() {
                            if (i < (int)app.profiles.list.size()) {
                                app.profiles.list.erase(app.profiles.list.begin() + i);
                                if (app.profiles.list.empty()) ProfilesInit();
                                ProfilesSave();
                                UIInvalidate();
                            }
                        });
        }
        AddHit(row.X, row.Y, row.Width - S(100), row.Height, 0, [p]() { ApplyProfile(p.id); });
    }

    // 新增
    float fy = y + ph - S(190);
    Txt(g, L"新建课表档案", S(10.5f), false, th.dim2, RectF(x + S(24), fy, S(160), S(18)));
    RectF nameBox(x + S(24), fy + S(22), S(240), S(34));
    FillRoundRect(g, nameBox, S(8), WithAlpha(th.panel2, 200));
    StrokeRoundRect(g, nameBox, S(8), th.borderSoft, S(1));
    UIEdit(9995, RectF(nameBox.X + S(10), nameBox.Y + S(7), nameBox.Width - S(20), S(20)), &app.profiles.newName, false, S(11));
    UI::Stepper(g, RectF(x + S(276), fy + S(22), S(140), S(34)), CMD_IMPORT_OK + 60, CMD_IMPORT_OK + 61,
                Fmt(L"%d 周", app.profiles.newWeeks), []() {
                    app.profiles.newWeeks = std::max(1, app.profiles.newWeeks - 1);
                    UIInvalidate();
                }, []() {
                    app.profiles.newWeeks = std::min(30, app.profiles.newWeeks + 1);
                    UIInvalidate();
                }, 9.5f);
    UI::Btn(g, RectF(x + S(428), fy + S(22), S(150), S(34)), CMD_IMPORT_OK + 62,
            L"标签：" + TagLabel(app.profiles.newTag), 1, -1, []() {
                // 循环切换：全部 → 每个标签 → 全部
                std::vector<std::string> ids;
                ids.push_back("");
                for (auto& t : app.store.tags) ids.push_back(t.id);
                size_t cur = 0;
                for (size_t i = 0; i < ids.size(); ++i) if (ids[i] == app.profiles.newTag) cur = i;
                app.profiles.newTag = ids[(cur + 1) % ids.size()];
                UIInvalidate();
            });
    UI::Btn(g, RectF(x + S(588), fy + S(22), S(120), S(34)), CMD_IMPORT_OK + 63, L"添加", 0, ICO_PLUS, []() {
        std::wstring name = Trim(app.profiles.newName);
        if (name.empty()) { ShowToast(L"先输入档案名称"); return; }
        CalendarProfile p;
        p.id = WideToUtf8(Fmt(L"pf%lld%02d", (long long)time(nullptr), (int)app.profiles.list.size()));
        p.name = name;
        p.start = ISODate(app.cursor);
        p.weeks = std::max(1, std::min(30, app.profiles.newWeeks));
        p.tagId = app.profiles.newTag;
        app.profiles.list.push_back(p);
        app.profiles.newName.clear();
        ProfilesSave();
        ShowToast(Fmt(L"已添加档案：%s", name.c_str()));
        UIInvalidate();
    });
    Txt(g, L"提示：要区分本科/辅修，先给课程加不同标签（如「课程」「辅修」），再给档案绑定标签。", S(8.5f), false, th.dim2,
        RectF(x + S(24), fy + S(70), pw - S(48), S(16)), ALIGN_LEFT, false, true);
    Txt(g, L"档案只影响日历视图过滤与学期周显示；学习统计、打印等仍统计全部日程。", S(8.5f), false, th.dim2,
        RectF(x + S(24), fy + S(90), pw - S(48), S(16)), ALIGN_LEFT, false, true);

    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
