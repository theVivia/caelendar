// model.cpp - 数据模型实现
#include "model.h"
#include "json.h"
#include "recur.h"
#include <algorithm>
#include <cmath>

bool DbEngineEnabled();
bool DbLoadJson(std::string& out);
bool DbSaveJson(const std::string& json);
void SyncRecordSave(const std::string& storeJson);
void SyncRecordTombstone(const std::string& id, const std::wstring& uid, const std::wstring& title);

// ---------- 小工具 ----------
static JValue DTToJ(const DateTime& t) { return JValue(ISODT(t)); }
static DateTime JToDT(const JValue& v) {
    bool ok = false;
    DateTime t = ParseISODT(v.AsStr(), &ok);
    return ok ? t : DateTime{ 0,0,0,0,0 };
}
static std::wstring Two(int v) { return Pad2(v); }

const std::vector<Tag>& DefaultTags() {
    static std::vector<Tag> tags;
    if (tags.empty()) {
        auto add = [&](const char* id, const wchar_t* name, const wchar_t* color, const wchar_t* icon, bool builtin) {
            Tag t; t.id = id; t.name = name; t.color = color; t.icon = icon; t.builtin = builtin;
            tags.push_back(t);
        };
        add("course", L"课程", L"#6EA8FF", L"book", true);
        add("study", L"学习", L"#8E7BFF", L"pen", true);
        add("homework", L"作业", L"#FFB454", L"edit", true);
    // 作业/考试默认计入 DDL 看板（用户可在标签编辑里改）
    for (auto& t : tags) if (t.id == "homework" || t.id == "exam") t.task = true;
        add("exam", L"考试", L"#FF6B7A", L"target", true);
        add("lab", L"实验", L"#4FD1C5", L"flask", true);
        add("group", L"小组作业", L"#F9A8D4", L"users", true);
        add("club", L"社团", L"#C084FC", L"users", true);
        add("life", L"生活", L"#A3E635", L"home", true);
        add("fitness", L"锻炼", L"#34D399", L"heart", true);
        add("water", L"喝水", L"#38BDF8", L"drop", true);
        add("meeting", L"会议", L"#FB923C", L"briefcase", true);
        add("parttime", L"兼职", L"#FACC15", L"money", true);
        add("commute", L"通勤", L"#94A3B8", L"bus", true);
        add("finance", L"财务", L"#4ADE80", L"bank", true);
        add("birthday", L"生日", L"#F472B6", L"gift", true);
        add("contest", L"竞赛", L"#E3C177", L"trophy", true);
        add("volunteer", L"志愿", L"#60A5FA", L"heart", true);
        add("idea", L"灵感", L"#FCD34D", L"bulb", true);
        add("interview", L"面试", L"#F97316", L"user", true);
        add("research", L"科研", L"#22D3EE", L"flask", true);
        add("project", L"项目", L"#818CF8", L"target", true);
        add("thesis", L"论文", L"#A78BFA", L"pen", true);
        add("internship", L"实习", L"#FBBF24", L"briefcase", true);
        add("report", L"汇报", L"#FB7185", L"mic", true);
    }
    return tags;
}

const wchar_t* DefaultTagIcon(const std::string& id) {
    for (auto& t : DefaultTags()) if (t.id == id) return t.icon.c_str();
    return L"tag";
}

std::wstring Store::NowStamp() {
    DateTime n = NowDT();
    return Fmt(L"%04d-%02d-%02dT%02d:%02d", n.y, n.mo, n.d, n.h, n.mi);
}
int SemesterWeek(const Settings& s, const DateTime& d) {
    if (!s.semester.enabled) return 0;
    int diff = DaySerial(d.y, d.mo, d.d) - DaySerial(s.semester.start.y, s.semester.start.mo, s.semester.start.d);
    if (diff < 0) return 0;
    int w = diff / 7 + 1;
    if (s.semester.weeks > 0 && w > s.semester.weeks) return 0;
    return w;
}
const wchar_t* PriorityName(int p) {
    switch (p) {
    case PRIO_LOW: return L"低";
    case PRIO_MED: return L"中";
    case PRIO_HIGH: return L"高";
    default: return L"无";
    }
}
const wchar_t* StatusName(int s) {
    switch (s) {
    case ST_TENTATIVE: return L"待定";
    case ST_CANCELLED: return L"已取消";
    default: return L"确认";
    }
}
const wchar_t* RecurFreqName(int f) {
    switch (f) {
    case RF_DAILY: return L"每天";
    case RF_WEEKLY: return L"每周";
    case RF_MONTHLY: return L"每月";
    case RF_YEARLY: return L"每年";
    default: return L"不重复";
    }
}

// ---------- 序列化 ----------
static JValue RecurToJ(const Recur& r) {
    JValue o = JValue::Obj();
    const wchar_t* f = L"none";
    if (r.freq == RF_DAILY) f = L"daily";
    else if (r.freq == RF_WEEKLY) f = L"weekly";
    else if (r.freq == RF_MONTHLY) f = L"monthly";
    else if (r.freq == RF_YEARLY) f = L"yearly";
    o.Set(L"freq") = JValue(f);
    o.Set(L"interval") = JValue(r.interval);
    if (!r.byDay.empty()) {
        JValue a = JValue::Arr();
        for (int d : r.byDay) a.Push(JValue(d));
        o.Set(L"byDay") = a;
    }
    if (r.byMonthDay) o.Set(L"byMonthDay") = JValue(r.byMonthDay);
    if (r.byMonth) o.Set(L"byMonth") = JValue(r.byMonth);
    if (r.nthWeekday) o.Set(L"nthWeekday") = JValue(r.nthWeekday);
    if (r.hasUntil) o.Set(L"until") = DTToJ(r.until);
    if (r.count) o.Set(L"count") = JValue(r.count);
    if (r.lunar) o.Set(L"lunar") = JValue(true);
    if (!r.exDates.empty()) {
        JValue a = JValue::Arr();
        for (auto& d : r.exDates) a.Push(DTToJ(d));
        o.Set(L"exDates") = a;
    }
    if (r.wkst != 1) o.Set(L"wkst") = JValue(r.wkst);
    return o;
}
static Recur RecurFromJ(const JValue& v) {
    Recur r;
    if (!v.IsObj()) return r;
    std::wstring f = v.Get(L"freq").AsStr(L"none");
    if (f == L"daily") r.freq = RF_DAILY;
    else if (f == L"weekly") r.freq = RF_WEEKLY;
    else if (f == L"monthly") r.freq = RF_MONTHLY;
    else if (f == L"yearly") r.freq = RF_YEARLY;
    r.interval = std::max(1, v.Get(L"interval").AsInt(1));
    for (auto& d : v.Get(L"byDay").arr) r.byDay.push_back(d.AsInt());
    r.byMonthDay = v.Get(L"byMonthDay").AsInt(0);
    r.byMonth = v.Get(L"byMonth").AsInt(0);
    r.nthWeekday = v.Get(L"nthWeekday").AsInt(0);
    if (v.Has(L"until")) { r.until = JToDT(v.Get(L"until")); r.hasUntil = true; }
    r.count = v.Get(L"count").AsInt(0);
    for (auto& d : v.Get(L"exDates").arr) r.exDates.push_back(JToDT(d));
    r.wkst = v.Get(L"wkst").AsInt(1);
    r.lunar = v.Get(L"lunar").AsBool(false);
    return r;
}
static JValue EventToJ(const Event& e, bool withOverrides = true) {
    JValue o = JValue::Obj();
    o.Set(L"id") = JValue(Utf8ToWide(e.id));
    if (!e.uid.empty()) o.Set(L"uid") = JValue(e.uid);
    o.Set(L"title") = JValue(e.title);
    if (!e.desc.empty()) o.Set(L"desc") = JValue(e.desc);
    if (!e.location.empty()) o.Set(L"location") = JValue(e.location);
    if (!e.url.empty()) o.Set(L"url") = JValue(e.url);
    o.Set(L"start") = DTToJ(e.start);
    o.Set(L"end") = DTToJ(e.end);
    if (e.allDay) o.Set(L"allDay") = JValue(true);
    if (!e.tags.empty()) {
        JValue a = JValue::Arr();
        for (auto& t : e.tags) a.Push(JValue(Utf8ToWide(t)));
        o.Set(L"tags") = a;
    }
    if (e.priority) o.Set(L"priority") = JValue(e.priority);
    if (e.status) o.Set(L"status") = JValue(e.status);
    if (e.done) o.Set(L"done") = JValue(true);
    if (e.ddl) o.Set(L"ddl") = JValue(true);
    if (!e.recur.IsNone() || !e.recur.exDates.empty()) o.Set(L"recur") = RecurToJ(e.recur);
    if (!e.alarms.empty()) {
        JValue a = JValue::Arr();
        for (auto& al : e.alarms) {
            JValue ao = JValue::Obj();
            ao.Set(L"offsetMin") = JValue(al.offsetMin);
            ao.Set(L"sound") = JValue(al.sound);
            if (!al.text.empty()) ao.Set(L"text") = JValue(al.text);
            a.Push(ao);
        }
        o.Set(L"alarms") = a;
    }
    if (!e.color.empty()) o.Set(L"color") = JValue(e.color);
    if (!e.source.empty()) o.Set(L"source") = JValue(e.source);
    // recurId / deleted 属于 override 自身的字段，必须无论 withOverrides 都序列化，
    // 否则每次保存都会丢失“对应哪一次重复实例”，重启后同一节课会出现原实例+覆盖实例两份。
    bool hasRecurId = !(e.recurId.y == 1970 && e.recurId.mo == 1 && e.recurId.d == 1 &&
                        e.recurId.h == 0 && e.recurId.mi == 0);
    if (hasRecurId) o.Set(L"recurId") = DTToJ(e.recurId);
    if (e.deletedOverride) o.Set(L"deleted") = JValue(true);
    if (e.created.y != 0) o.Set(L"created") = DTToJ(e.created);
    if (e.updated.y != 0) o.Set(L"updated") = DTToJ(e.updated);
    if (withOverrides && !e.overrides.empty()) {
        JValue a = JValue::Arr();
        for (auto& ov : e.overrides) a.Push(EventToJ(ov, false));
        o.Set(L"overrides") = a;
    }
    return o;
}
static Event EventFromJ(const JValue& o) {
    Event e;
    e.id = WideToUtf8(o.Get(L"id").AsStr());
    e.uid = o.Get(L"uid").AsStr();
    e.title = o.Get(L"title").AsStr();
    e.desc = o.Get(L"desc").AsStr();
    e.location = o.Get(L"location").AsStr();
    e.url = o.Get(L"url").AsStr();
    e.start = JToDT(o.Get(L"start"));
    e.end = JToDT(o.Get(L"end"));
    e.allDay = o.Get(L"allDay").AsBool(false);
    for (auto& t : o.Get(L"tags").arr) e.tags.push_back(WideToUtf8(t.AsStr()));
    e.priority = o.Get(L"priority").AsInt(PRIO_NONE);
    e.status = o.Get(L"status").AsInt(ST_CONFIRMED);
    e.done = o.Get(L"done").AsBool(false);
    e.ddl = o.Get(L"ddl").AsBool(false);
    if (o.Has(L"recur")) e.recur = RecurFromJ(o.Get(L"recur"));
    for (auto& al : o.Get(L"alarms").arr) {
        Alarm a;
        a.offsetMin = al.Get(L"offsetMin").AsInt(-10);
        a.sound = al.Get(L"sound").AsBool(true);
        a.text = al.Get(L"text").AsStr();
        e.alarms.push_back(a);
    }
    e.color = o.Get(L"color").AsStr();
    e.source = o.Get(L"source").AsStr();
    if (o.Has(L"recurId")) e.recurId = JToDT(o.Get(L"recurId"));
    if (o.Get(L"created").IsStr()) e.created = JToDT(o.Get(L"created"));
    if (o.Get(L"updated").IsStr()) e.updated = JToDT(o.Get(L"updated"));
    e.deletedOverride = o.Get(L"deleted").AsBool(false);
    for (auto& ov : o.Get(L"overrides").arr) e.overrides.push_back(EventFromJ(ov));
    return e;
}

static bool RepairLegacyOverrides(Store& st) {
    bool changed = false;
    int fixed = 0, unmatched = 0;
    for (auto& e : st.events) {
        if (!e.IsRecurring()) continue;
        for (auto& ov : e.overrides) {
            bool hasRecurId = !(ov.recurId.y == 1970 && ov.recurId.mo == 1 && ov.recurId.d == 1 &&
                            ov.recurId.h == 0 && ov.recurId.mi == 0);
            if (hasRecurId) continue;
            DateTime orig;
            if (FindRecurStartNear(e, ov.start, orig)) {
                ov.recurId = orig;
                changed = true;
                ++fixed;
            } else {
                ++unmatched;
            }
        }
    }
    if (changed) {
        DebugLog(L"旧版覆盖记录修复: 补全 recurId=%d 条，未匹配=%d 条", fixed, unmatched);
        st.dirty = true;
        st.Save();
    } else if (unmatched > 0) {
        DebugLog(L"旧版覆盖记录: %d 条无法匹配重复槽位（已保留原样）", unmatched);
    }
    return changed;
}

static JValue SettingsToJ(const Settings& s) {
    JValue o = JValue::Obj();
    o.Set(L"theme") = JValue(s.theme);
    if (!s.autoBackupDir.empty()) o.Set(L"autoBackupDir") = JValue(s.autoBackupDir);
    o.Set(L"weekStart") = JValue(s.weekStart);
    o.Set(L"timeZoom") = JValue(s.timeZoom);
    {
        JValue sem = JValue::Obj();
        sem.Set(L"enabled") = JValue(s.semester.enabled);
        sem.Set(L"start") = DTToJ(s.semester.start);
        sem.Set(L"weeks") = JValue(s.semester.weeks);
        sem.Set(L"showParity") = JValue(s.semester.showParity);
        o.Set(L"semester") = sem;
        JValue w = JValue::Obj();
        w.Set(L"enabled") = JValue(s.water.enabled);
        w.Set(L"intervalMin") = JValue(s.water.intervalMin);
        w.Set(L"from") = JValue(Fmt(L"%02d:%02d", s.water.fromMin / 60, s.water.fromMin % 60));
        w.Set(L"to") = JValue(Fmt(L"%02d:%02d", s.water.toMin / 60, s.water.toMin % 60));
        w.Set(L"targetCups") = JValue(s.water.targetCups);
        w.Set(L"cupMl") = JValue(s.water.cupMl);
        w.Set(L"sound") = JValue(s.water.sound);
        o.Set(L"water") = w;
        JValue n = JValue::Obj();
        n.Set(L"enabled") = JValue(s.notify.enabled);
        n.Set(L"sound") = JValue(s.notify.sound);
        n.Set(L"defaultLead") = JValue(s.notify.defaultLead);
        n.Set(L"snoozeMin") = JValue(s.notify.snoozeMin);
        n.Set(L"focusMode") = JValue(s.notify.focusMode);
        if (!s.notify.soundFile.empty()) n.Set(L"soundFile") = JValue(s.notify.soundFile);
        o.Set(L"notify") = n;
        JValue wd = JValue::Obj();
        wd.Set(L"enabled") = JValue(s.widget.enabled);
        wd.Set(L"mode") = JValue(s.widget.mode);
        wd.Set(L"skin") = JValue(s.widget.skin);
        wd.Set(L"scale") = JValue(s.widget.scale);
        wd.Set(L"opacity") = JValue(s.widget.opacity);
        wd.Set(L"x") = JValue(s.widget.x);
        wd.Set(L"y") = JValue(s.widget.y);
        wd.Set(L"showWater") = JValue(s.widget.showWater);
        wd.Set(L"showDate") = JValue(s.widget.showDate);
        wd.Set(L"showToday") = JValue(s.widget.showToday);
        wd.Set(L"alwaysOnTop") = JValue(s.widget.alwaysOnTop);
        wd.Set(L"clickThrough") = JValue(s.widget.clickThrough);
        wd.Set(L"lowPower") = JValue(s.widget.lowPower);
        wd.Set(L"showTasks") = JValue(s.widget.showTasks);
        o.Set(L"widget") = wd;
    }
    {
        JValue ct = JValue::Obj();
        ct.Set(L"bg") = JValue(s.customBg);
        ct.Set(L"accent") = JValue(s.customAccent);
        ct.Set(L"radius") = JValue(s.themeRadius);
        ct.Set(L"density") = JValue(s.themeDensity);
        ct.Set(L"light") = JValue(s.customLight);
        o.Set(L"customTheme") = ct;
        o.Set(L"cmdkHotkey") = JValue(s.cmdkHotkey);
        JValue hk = JValue::Obj();
        hk.Set(L"month") = JValue(s.hotkeyMonth);
        hk.Set(L"week") = JValue(s.hotkeyWeek);
        hk.Set(L"day") = JValue(s.hotkeyDay);
        hk.Set(L"year") = JValue(s.hotkeyYear);
        hk.Set(L"agenda") = JValue(s.hotkeyAgenda);
        hk.Set(L"today") = JValue(s.hotkeyToday);
        hk.Set(L"search") = JValue(s.hotkeySearch);
        hk.Set(L"new") = JValue(s.hotkeyNew);
        o.Set(L"hotkeys") = hk;
    }
    o.Set(L"monthChips") = JValue(s.monthChips);
    o.Set(L"monthCounts") = JValue(s.monthCounts);
    o.Set(L"lunar") = JValue(s.lunar);
    o.Set(L"weekNumbers") = JValue(s.weekNumbers);
    o.Set(L"reduceMotion") = JValue(s.reduceMotion);
        o.Set(L"highContrast") = JValue(s.highContrast);
    o.Set(L"seededSample") = JValue(s.seededSample);
    {
        JValue f = JValue::Obj();
        f.Set(L"ui") = JValue(s.fontUI);
        f.Set(L"num") = JValue(s.fontNum);
        f.Set(L"scale") = JValue(s.fontScale);
        JValue parts = JValue::Arr();
        for (int i = 0; i < kFontPartCount; ++i) {
            JValue p = JValue::Obj();
            if (!s.fontParts[i].family.empty()) p.Set(L"family") = JValue(s.fontParts[i].family);
            p.Set(L"size") = JValue(s.fontParts[i].size);
            parts.Push(p);
        }
        f.Set(L"parts") = parts;
        o.Set(L"fonts") = f;
        o.Set(L"closeToTray") = JValue(s.closeToTray);
        o.Set(L"autoStart") = JValue(s.autoStart);
    }
    return o;
}
static void HHMM(const std::wstring& s, int& minutesOut) {
    int h = 0, m = 0;
    swscanf_s(s.c_str(), L"%d:%d", &h, &m);
    minutesOut = h * 60 + m;
}
static Settings SettingsFromJ(const JValue& o) {
    Settings s;
    if (!o.IsObj()) return s;
    s.theme = o.Get(L"theme").AsStr(L"astral");
    s.autoBackupDir = o.Get(L"autoBackupDir").AsStr(L"");
    s.weekStart = o.Get(L"weekStart").AsInt(1);
    s.timeZoom = std::max(25, std::min(160, o.Get(L"timeZoom").AsInt(100)));
    const JValue& sem = o.Get(L"semester");
    if (sem.IsObj()) {
        s.semester.enabled = sem.Get(L"enabled").AsBool(true);
        if (sem.Has(L"start")) s.semester.start = JToDT(sem.Get(L"start"));
        s.semester.weeks = sem.Get(L"weeks").AsInt(18);
        s.semester.showParity = sem.Get(L"showParity").AsBool(true);
    }
    const JValue& w = o.Get(L"water");
    if (w.IsObj()) {
        s.water.enabled = w.Get(L"enabled").AsBool(true);
        s.water.intervalMin = std::max(5, w.Get(L"intervalMin").AsInt(45));
        HHMM(w.Get(L"from").AsStr(L"08:00"), s.water.fromMin);
        HHMM(w.Get(L"to").AsStr(L"22:30"), s.water.toMin);
        s.water.targetCups = std::max(1, w.Get(L"targetCups").AsInt(8));
        s.water.cupMl = std::max(50, w.Get(L"cupMl").AsInt(250));
        s.water.sound = w.Get(L"sound").AsBool(true);
    }
    const JValue& n = o.Get(L"notify");
    if (n.IsObj()) {
        s.notify.enabled = n.Get(L"enabled").AsBool(true);
        s.notify.sound = n.Get(L"sound").AsBool(true);
        s.notify.defaultLead = n.Get(L"defaultLead").AsInt(10);
        s.notify.snoozeMin = std::max(1, n.Get(L"snoozeMin").AsInt(10));
        s.notify.focusMode = n.Get(L"focusMode").AsBool(false);
        s.notify.soundFile = n.Get(L"soundFile").AsStr(L"");
    }
    const JValue& wd = o.Get(L"widget");
    if (wd.IsObj()) {
        s.widget.enabled = wd.Get(L"enabled").AsBool(true);
        s.widget.mode = wd.Get(L"mode").AsStr(L"card");
        s.widget.skin = wd.Get(L"skin").AsStr(L"theme");
        s.widget.scale = std::min(160, std::max(80, wd.Get(L"scale").AsInt(100)));
        s.widget.opacity = std::min(100, std::max(30, wd.Get(L"opacity").AsInt(94)));
        s.widget.x = wd.Get(L"x").AsInt(-1);
        s.widget.y = wd.Get(L"y").AsInt(-1);
        s.widget.showWater = wd.Get(L"showWater").AsBool(true);
        s.widget.showDate = wd.Get(L"showDate").AsBool(true);
        s.widget.showToday = wd.Get(L"showToday").AsBool(true);
        s.widget.alwaysOnTop = wd.Get(L"alwaysOnTop").AsBool(true);
        s.widget.clickThrough = wd.Get(L"clickThrough").AsBool(false);
        s.widget.lowPower = wd.Get(L"lowPower").AsBool(true);
        s.widget.showTasks = wd.Get(L"showTasks").AsBool(true);
    }
    {
        const JValue& ct = o.Get(L"customTheme");
        if (ct.IsObj()) {
            s.customBg = ct.Get(L"bg").AsStr(L"#07080D");
            s.customAccent = ct.Get(L"accent").AsStr(L"#E3C177");
            s.themeRadius = std::max(0, std::min(20, ct.Get(L"radius").AsInt(10)));
            s.themeDensity = std::max(85, std::min(115, ct.Get(L"density").AsInt(100)));
            s.customLight = ct.Get(L"light").AsBool(false);
        }
    }
    s.cmdkHotkey = o.Get(L"cmdkHotkey").AsStr(L"Ctrl+K");
    {
        const JValue& hk = o.Get(L"hotkeys");
        if (hk.IsObj()) {
            s.hotkeyMonth = hk.Get(L"month").AsStr(L"M");
            s.hotkeyWeek = hk.Get(L"week").AsStr(L"W");
            s.hotkeyDay = hk.Get(L"day").AsStr(L"D");
            s.hotkeyYear = hk.Get(L"year").AsStr(L"Y");
            s.hotkeyAgenda = hk.Get(L"agenda").AsStr(L"A");
            s.hotkeyToday = hk.Get(L"today").AsStr(L"T");
            s.hotkeySearch = hk.Get(L"search").AsStr(L"Ctrl+F");
            s.hotkeyNew = hk.Get(L"new").AsStr(L"Ctrl+N");
        }
    }
    if (s.cmdkHotkey.empty()) s.cmdkHotkey = L"Ctrl+K";
    s.monthChips = o.Get(L"monthChips").AsBool(true);
    s.monthCounts = o.Get(L"monthCounts").AsBool(false);
    s.lunar = o.Get(L"lunar").AsBool(true);
    s.weekNumbers = o.Get(L"weekNumbers").AsBool(true);
    s.reduceMotion = o.Get(L"reduceMotion").AsBool(false);
        s.highContrast = o.Get(L"highContrast").AsBool(false);
    s.seededSample = o.Get(L"seededSample").AsBool(false);
    {
        const JValue& f = o.Get(L"fonts");
        if (f.IsObj()) {
            s.fontUI = f.Get(L"ui").AsStr(L"Noto Sans SC");
            s.fontNum = f.Get(L"num").AsStr(L"Bahnschrift");
            s.fontScale = std::min(200, std::max(50, f.Get(L"scale").AsInt(100)));
            const auto& arr = f.Get(L"parts").arr;
            for (int i = 0; i < kFontPartCount && i < (int)arr.size(); ++i) {
                s.fontParts[i].family = arr[i].Get(L"family").AsStr();
                s.fontParts[i].size = std::min(200, std::max(50, arr[i].Get(L"size").AsInt(100)));
            }
        }
        s.closeToTray = o.Get(L"closeToTray").AsBool(true);
        s.autoStart = o.Get(L"autoStart").AsBool(false);
    }
    return s;
}

std::string Store::ToJsonStr() const {
    JValue root = JValue::Obj();
    root.Set(L"version") = JValue(version);
    root.Set(L"updatedAt") = JValue(updatedAt.empty() ? NowStamp() : updatedAt);
    root.Set(L"settings") = SettingsToJ(settings);
    {
        JValue tagsJ = JValue::Arr();
        for (auto& t : tags) {
            JValue o = JValue::Obj();
            o.Set(L"id") = JValue(Utf8ToWide(t.id));
            o.Set(L"name") = JValue(t.name);
            o.Set(L"color") = JValue(t.color);
            if (!t.icon.empty()) o.Set(L"icon") = JValue(t.icon);
            if (t.builtin) o.Set(L"builtin") = JValue(true);
            if (t.task) o.Set(L"task") = JValue(true);
            if (!t.customIcon.empty()) o.Set(L"customIcon") = JValue(t.customIcon);
            tagsJ.Push(o);
        }
        root.Set(L"tags") = tagsJ;
    }
    {
        JValue evs = JValue::Arr();
        for (auto& e : events) evs.Push(EventToJ(e));
        root.Set(L"events") = evs;
    }
    {
        JValue w = JValue::Obj();
        for (auto& kv : water) w.Set(kv.first) = JValue(kv.second);
        root.Set(L"water") = w;
    }
    if (!tombstones.empty()) {
        JValue arr = JValue::Arr();
        for (auto& t : tombstones) {
            JValue o = JValue::Obj();
            o.Set(L"id") = JValue(Utf8ToWide(t.first));
            o.Set(L"deletedAt") = JValue(t.second);
            arr.Push(o);
        }
        root.Set(L"tombstones") = arr;
    }
    return JsonDump(root, true);
}
bool Store::FromJsonStr(const std::string& s, std::wstring* err) {
    JValue root = JsonParse(s, err);
    if (!root.IsObj()) return false;
    version = root.Get(L"version").AsInt(1);
    if (version > kDataFormatVersion) DebugLog(L"数据版本 %d 高于程序支持版本 %d，将尝试兼容读取", version, kDataFormatVersion);
    else if (version < kDataFormatVersion) {
        version = kDataFormatVersion;
        dirty = true;
        DebugLog(L"数据格式已迁移到版本 %d", kDataFormatVersion);
    }
    updatedAt = root.Get(L"updatedAt").AsStr(NowStamp());
    settings = SettingsFromJ(root.Get(L"settings"));
    tags.clear();
    for (auto& t : root.Get(L"tags").arr) {
        Tag tag;
        tag.id = WideToUtf8(t.Get(L"id").AsStr());
        tag.name = t.Get(L"name").AsStr();
        tag.color = t.Get(L"color").AsStr(L"#94A3B8");
        tag.icon = t.Get(L"icon").AsStr(L"");
        tag.builtin = t.Get(L"builtin").AsBool(false);
        tag.task = t.Get(L"task").AsBool(false);
        tag.customIcon = t.Get(L"customIcon").AsStr(L"");
        if (!tag.id.empty()) tags.push_back(tag);
    }
    {
        // 旧数据补图标；同时把新增的内置标签合并进来（不动用户已有标签）
        for (auto& dt : DefaultTags()) {
            auto it = std::find_if(tags.begin(), tags.end(), [&](const Tag& x) { return x.id == dt.id; });
            if (it == tags.end()) {
                tags.push_back(dt);
                dirty = true;
            } else if (it->icon.empty()) {
                it->icon = dt.icon;
                dirty = true;
            }
        }
        for (auto& t : tags) {
            if (t.icon.empty()) { t.icon = L"tag"; dirty = true; }
        }
    }
    events.clear();
    for (auto& e : root.Get(L"events").arr) {
        Event ev = EventFromJ(e);
        if (!ev.id.empty()) events.push_back(ev);
    }
    water.clear();
    tombstones.clear();
    for (auto& t : root.Get(L"tombstones").arr) {
        if (!t.IsObj()) continue;
        std::string tid = WideToUtf8(t.Get(L"id").AsStr());
        std::wstring at = t.Get(L"deletedAt").AsStr();
        if (!tid.empty()) tombstones.push_back({ tid, at });
    }
    for (auto& kv : root.Get(L"water").obj) water[kv.first] = kv.second.AsInt(0);
    return true;
}
void Store::EnsureDefaults() {
    if (tags.empty()) tags = DefaultTags();
    dirty = true;
}
bool Store::Load() {
    if (DbEngineEnabled()) {
        std::string dbJson;
        if (DbLoadJson(dbJson) && !dbJson.empty()) {
            std::wstring derr;
            if (FromJsonStr(dbJson, &derr)) {
                if (tags.empty()) tags = DefaultTags();
                dirty = false;
                RepairLegacyOverrides(*this);
                DebugLog(L"存储引擎: SQLite 载入 events=%d", (int)events.size());
                return true;
            }
            DebugLog(L"SQLite 载入失败，回退 JSON: %s", derr.c_str());
        } else {
            DebugLog(L"SQLite 读取失败，回退 JSON");
        }
    }
    bool ok = false;
    std::string data = ReadFileBytes(GetCalendarPath(), &ok);
    if (!ok || data.empty()) {
        EnsureDefaults();
        updatedAt = NowStamp();
        return Save();
    }
    std::wstring err;
    if (!FromJsonStr(data, &err)) {
        DebugLog(L"calendar.json 解析失败: %s", err.c_str());
        // 备份坏文件后重建
        std::wstring bak = PathJoin(GetDataDir(), L"calendar.broken.json");
        CopyFileSimple(GetCalendarPath(), bak);
        EnsureDefaults();
        updatedAt = NowStamp();
        return Save();
    }
    // 补齐缺失标签
    if (tags.empty()) tags = DefaultTags();
    dirty = false;
    RepairLegacyOverrides(*this);
    return true;
}
bool Store::Save() {
    EnsureDir(GetDataDir());
    std::wstring bakDir = PathJoin(GetDataDir(), L"backup");
    EnsureDir(bakDir);
    updatedAt = NowStamp();
    std::string json = ToJsonStr();
    // 写盘前把上一版留成 auto-previous.json，避免误操作/异常退出无处可退
    if (FileExists(GetCalendarPath()))
        CopyFileSimple(GetCalendarPath(), PathJoin(bakDir, L"auto-previous.json"));
    bool ok = WriteFileBytesAtomic(GetCalendarPath(), json);
    if (ok) {
        dirty = false;
        // 每天留一份备份
        std::wstring day = ISODate(Today());
        std::wstring bak = PathJoin(bakDir, L"calendar-" + day + L".json");
        if (!FileExists(bak)) WriteFileBytesAtomic(bak, json);
        // 只清理每日备份（calendar-*.json），manual / auto-previous 永久保留
        auto files = ListFiles(bakDir, L".json");
        std::vector<std::wstring> daily;
        for (auto& f : files) {
            size_t p = f.find_last_of(L"\\/");
            std::wstring name = (p == std::wstring::npos) ? f : f.substr(p + 1);
            if (name.rfind(L"calendar-", 0) == 0) daily.push_back(f);
        }
        std::sort(daily.begin(), daily.end());
        if (daily.size() > 14) {
            size_t del = daily.size() - 14;
            for (size_t i = 0; i < del; ++i) DeleteFileW(daily[i].c_str());
        }
    } else {
        DebugLog(L"保存失败: %s", GetCalendarPath().c_str());
    }
    if (ok && DbEngineEnabled()) {
        if (!DbSaveJson(json)) DebugLog(L"SQLite 镜像写入失败（JSON 已保存，可继续使用）");
    }
    if (ok) SyncRecordSave(json);
    return ok;
}
void Store::Snapshot() {
    undoStack.push_back(ToJsonStr());
    if (undoStack.size() > 50) undoStack.erase(undoStack.begin());
    redoStack.clear();
}
bool Store::Undo() {
    if (undoStack.empty()) return false;
    redoStack.push_back(ToJsonStr());
    std::string s = undoStack.back();
    undoStack.pop_back();
    FromJsonStr(s);
    dirty = true;
    return true;
}
bool Store::Redo() {
    if (redoStack.empty()) return false;
    undoStack.push_back(ToJsonStr());
    std::string s = redoStack.back();
    redoStack.pop_back();
    FromJsonStr(s);
    dirty = true;
    return true;
}

// ---------- 事件 CRUD ----------
Event* Store::Find(const std::string& id) {
    for (auto& e : events) if (e.id == id) return &e;
    return nullptr;
}
const Event* Store::Find(const std::string& id) const {
    for (auto& e : events) if (e.id == id) return &e;
    return nullptr;
}
std::string Store::AddEvent(Event e) {
    if (e.id.empty()) e.id = NewId("ev");
    if (CompareDT(e.created, DateTime{ 0,0,0,0,0 }) == 0) e.created = NowDT();
    e.updated = NowDT();
    if (e.end.y == 0) e.end = e.start;
    events.push_back(std::move(e));
    Touch();
    return events.back().id;
}
bool Store::UpdateEvent(const Event& e) {
    for (auto& x : events) {
        if (x.id == e.id) {
            Event copy = e;
            copy.updated = NowDT();
            x = copy;
            Touch();
            return true;
        }
    }
    return false;
}
bool Store::DeleteEvent(const std::string& id) {
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].id == id) {
            SyncRecordTombstone(events[i].id, events[i].uid, events[i].title);
            std::wstring now = ISODT(NowDT());
            bool found = false;
            for (auto& t : tombstones) {
                if (t.first == id) { t.second = now; found = true; break; }
            }
            if (!found) tombstones.push_back({ id, now });
            while (tombstones.size() > 300) tombstones.erase(tombstones.begin());
            events.erase(events.begin() + i);
            Touch();
            return true;
        }
    }
    return false;
}
std::string Store::DuplicateEvent(const std::string& id, DateTime newStart) {
    Event* src = Find(id);
    if (!src) return "";
    Event copy = *src;
    copy.id = NewId("ev");
    copy.uid.clear();
    copy.recur.exDates.clear();
    copy.overrides.clear();
    if (newStart.y != 0) {
        int dur = copy.DurationMin();
        copy.start = newStart;
        copy.end = AddMinutesDT(newStart, dur);
    }
    copy.created = NowDT();
    copy.updated = NowDT();
    events.push_back(copy);
    Touch();
    return events.back().id;
}
void Store::ToggleDone(const std::string& id, bool done) {
    Event* e = Find(id);
    if (!e) return;
    e->done = done;
    e->updated = NowDT();
    Touch();
}

bool Store::SetOccurrenceDone(const std::string& seriesId, const DateTime& origStart,
                              const DateTime& occStart, const DateTime& occEnd, bool done) {
    Event* e = Find(seriesId);
    if (!e) return false;
    if (!e->IsRecurring()) {
        e->done = done;
        e->updated = NowDT();
        Touch();
        return true;
    }
    for (auto& ov : e->overrides) {
        if (CompareDT(ov.recurId, origStart) == 0) {
            ov.done = done;
            ov.updated = NowDT();
            Touch();
            return true;
        }
    }
    Event ov = *e;
    ov.id = e->id + "#" + std::to_string(DaySerial(origStart.y, origStart.mo, origStart.d)) + "_" +
            std::to_string(origStart.h * 60 + origStart.mi);
    ov.recurId = origStart;
    ov.recur = Recur();
    ov.overrides.clear();
    ov.uid.clear();
    ov.start = occStart;
    ov.end = occEnd;
    ov.done = done;
    ov.created = NowDT();
    ov.updated = NowDT();
    e->overrides.push_back(ov);
    Touch();
    return true;
}

static void CopyEditableFields(Event& dst, const Event& src) {
    dst.title = src.title;
    dst.desc = src.desc;
    dst.location = src.location;
    dst.url = src.url;
    dst.allDay = src.allDay;
    dst.tags = src.tags;
    dst.priority = src.priority;
    dst.status = src.status;
    dst.done = src.done;
    dst.alarms = src.alarms;
    dst.color = src.color;
    dst.start = src.start;
    dst.end = src.end;
}

bool Store::UpdateOccurrence(const std::string& seriesId, const DateTime& origStart, const Event& effective, int scope) {
    Event* series = Find(seriesId);
    if (!series) return false;
    if (scope == 2) {   // 整个系列
        Event copy = *series;
        bool startChanged = CompareDT(copy.start, effective.start) != 0 || CompareDT(copy.end, effective.end) != 0;
        CopyEditableFields(copy, effective);
        copy.recur = effective.recur;
        if (startChanged) { copy.recur.exDates.clear(); copy.overrides.clear(); }
        return UpdateEvent(copy);
    }
    if (scope == 1) {   // 此后所有：拆分为新系列
        Event newSeries = *series;
        newSeries.id = NewId("ev");
        newSeries.uid.clear();
        CopyEditableFields(newSeries, effective);
        newSeries.recur = series->recur;
        newSeries.recur.exDates.clear();
        newSeries.overrides.clear();
        newSeries.start = effective.start;
        newSeries.end = effective.end;
        // 原系列截至本次之前
        DateTime cut = origStart;
        if (series->allDay) {
            series->recur.until = AddDaysDT(StartOfDay(cut), -1);
        } else {
            series->recur.until = AddMinutesDT(cut, -1);
        }
        series->recur.hasUntil = true;
        series->recur.count = 0;
        for (auto it = series->overrides.begin(); it != series->overrides.end();) {
            if (CompareDT(it->recurId, origStart) >= 0) it = series->overrides.erase(it);
            else ++it;
        }
        series->updated = NowDT();
        newSeries.created = NowDT();
        newSeries.updated = NowDT();
        events.push_back(newSeries);
        Touch();
        return true;
    }
    // 仅本次：写 override
    Event ov = effective;
    ov.id = series->id + "@" + WideToUtf8(ISODT(origStart));
    ov.recurId = origStart;
    ov.recur = Recur();
    ov.overrides.clear();
    ov.uid.clear();
    // 相同 origStart 的旧覆盖替换
    bool replaced = false;
    for (auto& old : series->overrides) {
        if (CompareDT(old.recurId, origStart) == 0) { old = ov; replaced = true; break; }
    }
    if (!replaced) series->overrides.push_back(ov);
    series->updated = NowDT();
    Touch();
    return true;
}
bool Store::RescheduleOccurrence(const std::string& seriesId, const DateTime& origStart, const Event& effective,
                                 const DateTime& occStart, const DateTime& occEnd, const Date& newDay) {
    Event* series = Find(seriesId);
    if (!series) return false;
    int delta = DaySerial(newDay.y, newDay.m, newDay.d) - DaySerial(origStart.y, origStart.mo, origStart.d);
    if (delta == 0) return true;
    if (!series->IsRecurring()) {
        Event copy = *series;
        copy.start = AddDaysDT(copy.start, delta);
        copy.end = AddDaysDT(copy.end, delta);
        return UpdateEvent(copy);
    }
    Event eff = effective;
    eff.start = AddDaysDT(occStart, delta);
    eff.end = AddDaysDT(occEnd, delta);
    return UpdateOccurrence(seriesId, origStart, eff, 0);
}

bool Store::DeleteOccurrence(const std::string& seriesId, const DateTime& origStart, int scope) {
    Event* series = Find(seriesId);
    if (!series) return false;
    if (scope == 2) return DeleteEvent(seriesId);
    if (scope == 1) {
        DateTime cut = origStart;
        if (series->allDay) series->recur.until = AddDaysDT(StartOfDay(cut), -1);
        else series->recur.until = AddMinutesDT(cut, -1);
        series->recur.hasUntil = true;
        series->recur.count = 0;
        for (auto it = series->overrides.begin(); it != series->overrides.end();) {
            if (CompareDT(it->recurId, origStart) >= 0) it = series->overrides.erase(it);
            else ++it;
        }
        series->updated = NowDT();
        Touch();
        return true;
    }
    // 仅本次：加入 exDates；若已有 override 则删除它
    for (auto it = series->overrides.begin(); it != series->overrides.end();) {
        if (CompareDT(it->recurId, origStart) == 0) { it = series->overrides.erase(it); }
        else ++it;
    }
    bool exists = false;
    for (auto& d : series->recur.exDates) if (CompareDT(d, origStart) == 0) exists = true;
    if (!exists) series->recur.exDates.push_back(origStart);
    // 若整个系列只有这一次（无重复）则直接删
    if (!series->IsRecurring()) return DeleteEvent(seriesId);
    series->updated = NowDT();
    Touch();
    return true;
}

// ---------- 标签 ----------
static std::string SlugId(const std::wstring& name) {
    // 用名称哈希做稳定 id（跨导入保持一致）
    unsigned h = 2166136261u;
    for (wchar_t c : name) { h ^= (unsigned)c; h *= 16777619u; }
    char buf[32];
    snprintf(buf, sizeof(buf), "t_%08x", h);
    return buf;
}
std::string TagIdForName(const std::wstring& name) {
    unsigned h = 2166136261u;
    for (wchar_t c : name) { h ^= (unsigned)c; h *= 16777619u; }
    char buf[32];
    snprintf(buf, sizeof(buf), "t_%08x", h);
    return buf;
}
std::string Store::AddTag(const std::wstring& name, const std::wstring& color, const std::wstring& icon) {
    std::string id = SlugId(name);
    for (auto& t : tags) if (t.id == id) return t.id;
    Tag t; t.id = id; t.name = name; t.color = color.empty() ? L"#94A3B8" : color; t.icon = icon.empty() ? L"tag" : icon; t.builtin = false;
    tags.push_back(t);
    Touch();
    return id;
}
Tag* Store::FindTag(const std::string& id) {
    for (auto& t : tags) if (t.id == id) return &t;
    return nullptr;
}
const Tag* Store::FindTag(const std::string& id) const {
    for (auto& t : tags) if (t.id == id) return &t;
    return nullptr;
}
std::wstring Store::TagName(const std::string& id) const {
    const Tag* t = FindTag(id);
    return t ? t->name : Utf8ToWide(id);
}
std::wstring Store::TagColor(const std::string& id) const {
    const Tag* t = FindTag(id);
    return t ? t->color : L"#94A3B8";
}
std::string Store::EnsureTag(const std::wstring& name, const std::wstring& color) {
    for (auto& t : tags) if (t.name == name) return t.id;
    const auto& palette = PaletteHex();
    std::wstring c = color;
    if (c.empty()) c = palette[tags.size() % palette.size()];
    return AddTag(name, c, L"tag");
}
bool Store::DeleteTag(const std::string& id) {
    for (size_t i = 0; i < tags.size(); ++i) {
        if (tags[i].id == id) {
            tags.erase(tags.begin() + i);
            for (auto& e : events) {
                auto& v = e.tags;
                v.erase(std::remove(v.begin(), v.end(), id), v.end());
            }
            Touch();
            return true;
        }
    }
    return false;
}

// ---------- 喝水 ----------
int Store::WaterToday() const {
    auto it = water.find(ISODate(Today()));
    return it == water.end() ? 0 : it->second;
}
void Store::AddWaterCup() {
    SetWater(ISODate(Today()), WaterToday() + 1);
}
void Store::SetWater(const std::wstring& date, int cups) {
    water[date] = std::max(0, cups);
    Touch();
}
std::wstring ValidateStoreData(const Store& st) {
    int issues = 0;
    std::wstring first;
    auto note = [&](const std::wstring& msg) {
        ++issues;
        if (first.empty()) first = msg;
        if (issues <= 5) DebugLog(L"数据校验：%s", msg.c_str());
    };
    std::set<std::string> ids;
    for (auto& e : st.events) {
        if (e.id.empty()) note(L"存在空 id 的日程");
        else if (!ids.insert(e.id).second) note(L"重复的日程 id");
        if (e.title.empty()) note(L"存在空标题日程");
        if (!IsSupportedDate(e.start) || !IsSupportedDate(e.end)) note(L"日程日期超出 1901–2100");
        if (CompareDT(e.end, e.start) < 0) note(Fmt(L"结束早于开始：%s", e.title.c_str()));
        int dur = MinutesBetween(e.start, e.end);
        if (dur > 366 * 24 * 60) note(Fmt(L"时长异常：%s", e.title.c_str()));
        if (e.recur.freq != RF_NONE) {
            if (e.recur.interval < 1) note(Fmt(L"重复间隔异常：%s", e.title.c_str()));
            if (e.recur.hasUntil && !IsSupportedDate(e.recur.until)) note(Fmt(L"重复结束日期异常：%s", e.title.c_str()));
            for (int d : e.recur.byDay) if (d < 0 || d > 6) note(Fmt(L"重复星期异常：%s", e.title.c_str()));
            if (e.recur.byMonth < 0 || e.recur.byMonth > 12) note(Fmt(L"重复月份异常：%s", e.title.c_str()));
            if (e.recur.byMonthDay < 0 || e.recur.byMonthDay > 31) note(Fmt(L"重复日期异常：%s", e.title.c_str()));
        }
        for (auto& ov : e.overrides) {
            if (!IsSupportedDate(ov.start) || !IsSupportedDate(ov.end)) note(Fmt(L"例外实例日期异常：%s", e.title.c_str()));
        }
    }
    if (issues == 0) return L"通过";
    return Fmt(L"%d 个问题（%s）", issues, first.c_str());
}