// editor.cpp - 事件编辑器
#include "app.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

// 农历日期字符串（年-月-日），用于编辑器直接输入
static std::wstring LunarDateString(const DateTime& d) {
    int ly = 0, lm = 0, ld = 0;
    bool leap = false;
    if (!LunarDateOf(d, ly, lm, ld, leap)) return L"";
    if (leap) return Fmt(L"%04d-闰%02d-%02d", ly, lm, ld);
    return Fmt(L"%04d-%02d-%02d", ly, lm, ld);
}

// 解析直接输入的农历日期：YYYY-MM-DD / YYYY/MM/DD / YYYY-闰MM-DD
static bool ParseLunarDateInput(const std::wstring& input, DateTime& out, bool& leapOut, std::wstring& err) {
    std::wstring s = Trim(input);
    if (s.empty()) { err = L"请填写农历日期（年-月-日）"; return false; }
    if (s.rfind(L"农历", 0) == 0) s = Trim(s.substr(2));
    bool leap = false;
    size_t leapPos = s.find(L'闰');
    if (leapPos != std::wstring::npos) {
        leap = true;
        s.erase(leapPos, 1);
    }
    for (auto& ch : s) if (ch == L'/' || ch == L'.' || ch == L'年' || ch == L'月') ch = L'-';
    if (!s.empty() && s.back() == L'日') s.pop_back();
    int nums[3] = { 0, 0, 0 };
    int n = 0;
    size_t pos = 0;
    while (pos < s.size() && n < 3) {
        while (pos < s.size() && (s[pos] == L'-' || s[pos] == L' ')) ++pos;
        size_t start = pos;
        while (pos < s.size() && iswdigit(s[pos])) ++pos;
        if (start == pos) break;
        nums[n++] = _wtoi(s.substr(start, pos - start).c_str());
    }
    if (n < 3) { err = L"农历日期格式：年-月-日，例如 2005-08-15 或 2023-闰02-01"; return false; }
    if (nums[0] < 1901 || nums[0] > 2100) { err = L"农历年份仅支持 1901–2100"; return false; }
    if (nums[1] < 1 || nums[1] > 12) { err = L"农历月份必须是 1–12"; return false; }
    if (nums[2] < 1 || nums[2] > 30) { err = L"农历日期必须是 1–30"; return false; }
    Date solar{};
    if (!LunarToSolarDate(nums[0], nums[1], nums[2], leap, solar)) {
        if (leap) err = Fmt(L"%d 年没有闰%d月", nums[0], nums[1]);
        else err = L"这个农历日期不存在（小月没有三十）";
        return false;
    }
    leapOut = leap;
    out = MakeDT(nums[0], nums[1], nums[2], 0, 0);   // 返回农历分量，由调用方换算公历
    return true;
}


// ---------------- 日期/时间输入解析 ----------------
bool DateInputParse(const std::wstring& s, const DateTime& base, DateTime& out) {
    std::wstring t = Trim(s);
    if (t.empty()) return false;
    DateTime today = Today();
    if (t == L"今天") { out = today; return true; }
    if (t == L"明天") { out = AddDaysDT(today, 1); return true; }
    if (t == L"后天") { out = AddDaysDT(today, 2); return true; }
    if (t == L"大后天") { out = AddDaysDT(today, 3); return true; }
    if (t == L"昨天") { out = AddDaysDT(today, -1); return true; }
    // 周三 / 星期三 / 礼拜三
    size_t p = t.find(L'周');
    if (p == std::wstring::npos) p = t.find(L"星期");
    if (p == std::wstring::npos) p = t.find(L"礼拜");
    if (p != std::wstring::npos) {
        std::wstring tail = t.substr(p + (t[p] == L'周' ? 1 : 2));
        bool nextWeek = p >= 1 && (t[0] == L'下');
        int wd = -1;
        static const wchar_t* names = L"日一二三四五六天";
        if (!tail.empty()) {
            for (int i = 0; i < 7; ++i) {
                if (tail[0] == names[i]) { wd = (i == 6 ? 0 : i); break; }
            }
        }
        if (wd < 0) wd = 0;
        int cur = DayOfWeek(today.y, today.mo, today.d);
        int diff = (wd - cur + 7) % 7;
        if (nextWeek) diff += 7;
        out = AddDaysDT(today, diff);
        return true;
    }
    int y = 0, mo = 0, d = 0;
    if (swscanf_s(t.c_str(), L"%d-%d-%d", &y, &mo, &d) == 3 ||
        swscanf_s(t.c_str(), L"%d/%d/%d", &y, &mo, &d) == 3) {
        if (mo >= 1 && mo <= 12 && d >= 1 && d <= 31) {
            if (y < 100) y += 2000;
            out = MakeDT(y, mo, d);
            return true;
        }
    }
    int a = 0, b = 0;
    if (swscanf_s(t.c_str(), L"%d月%d日", &a, &b) == 2 || swscanf_s(t.c_str(), L"%d月%d号", &a, &b) == 2) {
        if (a >= 1 && a <= 12 && b >= 1 && b <= 31) { out = MakeDT(today.y, a, b); return true; }
    }
    if (t.back() == L'日' || t.back() == L'号') {
        std::wstring num = t.substr(0, t.size() - 1);
        int dd = 0;
        if (ToInt(num, dd) && dd >= 1 && dd <= 31) { out = MakeDT(base.y, base.mo, dd); return true; }
    }
    int one = 0;
    if (ToInt(t, one) && one >= 1 && one <= 31) { out = MakeDT(base.y, base.mo, one); return true; }
    // 10-20 / 10/20
    if (swscanf_s(t.c_str(), L"%d-%d", &a, &b) == 2 || swscanf_s(t.c_str(), L"%d/%d", &a, &b) == 2) {
        if (a >= 1 && a <= 12 && b >= 1 && b <= 31) { out = MakeDT(today.y, a, b); return true; }
    }
    return false;
}

bool TimeInputParse(const std::wstring& s, int& h, int& mi) {
    std::wstring t = Trim(s);
    if (t.empty()) return false;
    bool pm = Contains(t, L"下午") || Contains(t, L"晚上") || Contains(t, L"傍晚") || Contains(t, L"夜");
    bool am = Contains(t, L"上午") || Contains(t, L"早上") || Contains(t, L"凌晨") || Contains(t, L"早晨");
    bool noon = Contains(t, L"中午");
    t = ReplaceAll(t, L"上午", L"");
    t = ReplaceAll(t, L"下午", L"");
    t = ReplaceAll(t, L"早上", L"");
    t = ReplaceAll(t, L"早晨", L"");
    t = ReplaceAll(t, L"晚上", L"");
    t = ReplaceAll(t, L"傍晚", L"");
    t = ReplaceAll(t, L"凌晨", L"");
    t = ReplaceAll(t, L"中午", L"");
    t = ReplaceAll(t, L"点半", L":30");
    t = ReplaceAll(t, L"点", L":");
    t = ReplaceAll(t, L"时", L":");
    t = ReplaceAll(t, L"分", L"");
    t = Trim(t);
    if (t.empty()) return false;
    int hh = 0, mm = 0;
    if (swscanf_s(t.c_str(), L"%d:%d", &hh, &mm) >= 1) {
        // ok
    } else if (t.back() == L'半') {
        int v = 0;
        if (!ToInt(t.substr(0, t.size() - 1), v)) return false;
        hh = v; mm = 30;
    } else {
        int v = 0;
        if (!ToInt(t, v)) return false;
        hh = v;
    }
    if (hh < 0 || hh > 24 || mm < 0 || mm > 59) return false;
    if ((pm || noon) && hh < 12) hh += 12;
    if (am && hh == 12) hh = 0;
    if (hh == 24 && mm == 0) hh = 24;
    h = std::min(hh, 23); mi = mm;
    return true;
}
std::wstring TimeInputText(const DateTime& t) { return Fmt(L"%02d:%02d", t.h, t.mi); }

// ---------------- 编辑器状态 ----------------
void EditorState::ParseTimeTexts() {
    DateTime s, e;
    if (!DateInputParse(date1, start, s)) return;
    if (!allDay) {
        int h = s.h, mi = s.mi;
        if (TimeInputParse(time1, h, mi)) { s.h = h; s.mi = mi; }
    } else { s.h = 0; s.mi = 0; }
    if (!DateInputParse(date2.empty() ? date1 : date2, s, e)) return;
    if (allDay) {
        e.h = 0; e.mi = 0;
        if (CompareDT(e, s) < 0) e = AddDaysDT(s, 1);
    } else {
        int h2 = e.h, m2 = e.mi;
        if (TimeInputParse(time2, h2, m2)) { e.h = h2; e.mi = m2; }
    }
    start = s;
    end = e;
}

void EditorState::SyncTimeTexts() {
    date1 = ISODate(start);
    time1 = TimeInputText(start);
    DateTime endShow = end;
    if (allDay) {
        // 全天：end 是排他日期，显示最后一天
        if (CompareDT(end, AddDaysDT(StartOfDay(start), 1)) > 0) endShow = AddDaysDT(StartOfDay(end), -1);
        else endShow = StartOfDay(start);
    }
    date2 = ISODate(endShow);
    time2 = allDay ? L"" : TimeInputText(endShow);
}
void EditorState::LoadNew(const DateTime& day) {
    isNew = true;
    seriesId.clear();
    seriesRecurring = false;
    scope = 2;
    DateTime base = (day.y != 0) ? day : Today();
    int h = app.lastClickMin / 60, mi = (app.lastClickMin % 60) / 5 * 5;
    if (base.y == 0) base = Today();
    start = MakeDT(base.y, base.mo, base.d, h, mi);
    end = AddMinutesDT(start, 60);
    allDay = false;
    priority = PRIO_NONE;
    status = ST_CONFIRMED;
    done = false;
    isDdl = false;
    alarms.clear();
    alarms.push_back(Alarm{ -app.store.settings.notify.defaultLead, true, L"" });
    tags.clear();
    color.clear();
    originalColor.clear();
    freq = RF_NONE;
    interval = 1;
    byDay.clear();
    monthlyMode = 0;
    nth = 1;
    endMode = 0;
    count = 10;
    untilStr.clear();
    exDates.clear();
    source = L"本地";
    location.clear();
    url.clear();
    desc.clear();
    title.clear();
    error.clear();
    SyncTimeTexts();
}
void EditorState::OpenFromEvent(const Event& ev) {
    isNew = true;
    seriesId.clear();
    seriesRecurring = false;
    scope = 2;
    title = ev.title;
    desc = ev.desc;
    location = ev.location;
    url = ev.url;
    allDay = ev.allDay;
    start = ev.start;
    end = ev.end;
    priority = ev.priority;
    status = ev.status;
    done = ev.done;
    isDdl = ev.ddl;
    tags.clear();
    for (auto& t : ev.tags) tags.insert(t);
    alarms = ev.alarms;
    color = ev.color;
    originalColor = color;
    freq = ev.recur.freq;
    interval = ev.recur.interval;
    byDay.clear();
    for (int d : ev.recur.byDay) byDay.insert(d);
    nth = ev.recur.nthWeekday;
    lunar = ev.recur.lunar;
    monthlyMode = ev.recur.nthWeekday != 0 ? 1 : 0;
    endMode = ev.recur.count > 0 ? 1 : (ev.recur.hasUntil ? 2 : 0);
    count = ev.recur.count > 0 ? ev.recur.count : 10;
    untilStr = ev.recur.hasUntil ? ISODate(ev.recur.until) : L"";
    exDates = ev.recur.exDates;
    source = ev.source;
    error.clear();
    SyncTimeTexts();
    lunarDate = ev.recur.lunar ? LunarDateString(ev.start) : L"";
}
void EditorState::LoadExisting(const Event& base, const Occurrence* occ) {
    isNew = false;
    seriesId = base.id;
    seriesRecurring = base.IsRecurring();
    scope = seriesRecurring ? 0 : 2;
    const Event& eff = occ ? occ->ev : base;
    origStart = occ ? occ->origStart : base.start;
    title = eff.title;
    desc = eff.desc;
    location = eff.location;
    url = eff.url;
    allDay = eff.allDay;
    start = occ ? occ->start : eff.start;
    end = occ ? occ->end : eff.end;
    priority = eff.priority;
    status = eff.status;
    done = eff.done;
    isDdl = eff.ddl;
    tags.clear();
    for (auto& t : eff.tags) tags.insert(t);
    alarms = eff.alarms;
    color = eff.color;
    originalColor = color;
    freq = base.recur.freq;
    interval = base.recur.interval;
    byDay.clear();
    for (int d : base.recur.byDay) byDay.insert(d);
    nth = base.recur.nthWeekday;
    lunar = base.recur.lunar;
    monthlyMode = base.recur.nthWeekday != 0 ? 1 : 0;
    endMode = base.recur.count > 0 ? 1 : (base.recur.hasUntil ? 2 : 0);
    count = base.recur.count > 0 ? base.recur.count : 10;
    untilStr = base.recur.hasUntil ? ISODate(base.recur.until) : L"";
    exDates = base.recur.exDates;
    source = base.source;
    error.clear();
    SyncTimeTexts();
    lunarDate = base.recur.lunar ? LunarDateString(base.start) : L"";
}
bool EditorState::Collect(Event& out, std::wstring& err) {
    std::wstring titleTrim = Trim(title);
    if (titleTrim.empty()) { err = L"标题不能为空"; return false; }
    DateTime s, e;
    if (!DateInputParse(date1, start, s)) { err = L"开始日期格式不对（例：2026-10-20 / 明天 / 周三）"; return false; }
    if (!IsSupportedDate(s)) { err = L"日期范围仅支持 1901–2100 年"; return false; }
    int h1 = start.h, m1 = start.mi;
    if (!allDay) {
        if (!TimeInputParse(time1, h1, m1)) { err = L"开始时间格式不对（例：08:00 / 8点 / 下午3点）"; return false; }
    }
    s.h = allDay ? 0 : h1;
    s.mi = allDay ? 0 : m1;
    if (date2.empty()) date2 = date1;
    if (!DateInputParse(date2, s, e)) { err = L"结束日期格式不对"; return false; }
    if (!IsSupportedDate(e)) { err = L"日期范围仅支持 1901–2100 年"; return false; }
    if (allDay) {
        e.h = 0; e.mi = 0;
        if (CompareDT(e, s) <= 0) e = AddDaysDT(s, 1);
    } else {
        int h2 = e.h, m2 = e.mi;
        if (!TimeInputParse(time2, h2, m2)) { err = L"结束时间格式不对"; return false; }
        e.h = h2; e.mi = m2;
        if (CompareDT(e, s) < 0) { err = L"结束时间不能早于开始时间（相同 = 时间点）"; return false; }
    }
    // 每年 + 按农历：直接输入的农历日期换算成公历（保留时间和时长）
    if (freq == RF_YEARLY && lunar) {
        if (Trim(lunarDate).empty()) lunarDate = LunarDateString(s);
        DateTime ld;
        bool lunarLeap = false;
        std::wstring lerr;
        if (!ParseLunarDateInput(lunarDate, ld, lunarLeap, lerr)) { err = lerr; return false; }
        int dur = MinutesBetween(s, e);
        Date solar{};
        if (!LunarToSolarDate(ld.y, ld.mo, ld.d, lunarLeap, solar)) { err = L"农历日期换算失败"; return false; }
        s = MakeDT(solar.y, solar.m, solar.d, s.h, s.mi);
        e = allDay ? AddDaysDT(s, 1) : AddMinutesDT(s, dur);
    }
    out = Event();
    out.title = titleTrim;
    out.desc = desc;
    out.location = location;
    out.url = url;
    out.allDay = allDay;
    out.start = s;
    out.end = e;
    out.priority = priority;
    out.status = status;
    out.done = done;
    out.ddl = isDdl;
    out.color = color;
    out.tags.clear();
    for (auto& t : tags) out.tags.push_back(t);
    out.alarms = alarms;
    out.source = source.empty() ? L"本地" : source;
    Recur r;
    r.freq = freq;
    r.interval = std::max(1, interval);
    if (freq == RF_WEEKLY) {
        if (byDay.empty()) byDay.insert(DayOfWeek(s.y, s.mo, s.d));
        for (int d : byDay) r.byDay.push_back(d);
    }
    if (freq == RF_MONTHLY) {
        if (monthlyMode == 1) {
            r.nthWeekday = nth;
            r.byDay = { DayOfWeek(s.y, s.mo, s.d) };
        } else {
            r.byMonthDay = s.d;
        }
    }
    if (freq == RF_YEARLY) {
        r.lunar = lunar;
        r.byMonth = s.mo;
        r.byMonthDay = s.d;
    }
    if (freq != RF_NONE) {
        if (endMode == 1) r.count = std::max(1, count);
        else if (endMode == 2) {
            DateTime u;
            if (!DateInputParse(untilStr, s, u)) { err = L"重复结束日期格式不对"; return false; }
            if (!IsSupportedDate(u)) { err = L"重复结束日期仅支持 1901–2100 年"; return false; }
            r.until = u;
            r.hasUntil = true;
        }
        r.exDates = exDates;
    }
    out.recur = r;
    return true;
}

// ---------------- 打开编辑器 ----------------
void OpenEditorNew(const DateTime& day, int hour, int minute) {
    app.lastClickMin = std::max(0, std::min(23 * 60 + 45, hour * 60 + minute));
    app.editor = EditorState();
    app.editor.LoadNew(day);
    app.editor.open = true;
    app.overlay = OV_EDITOR;
    app.lastClickDay = day;
    UIInvalidate();
}
void OpenEditorForOccurrence(const Occurrence& o) {
    const Event* base = app.store.Find(o.seriesId);
    if (!base) return;
    app.editor = EditorState();
    app.editor.LoadExisting(*base, &o);
    app.editor.open = true;
    app.overlay = OV_EDITOR;
    UIInvalidate();
}
void OpenEditorExisting(const std::string& seriesId, const DateTime& origStart) {
    const Event* base = app.store.Find(seriesId);
    if (!base) return;
    app.editor = EditorState();
    app.editor.LoadExisting(*base, nullptr);
    app.editor.origStart = origStart;
    app.editor.open = true;
    app.overlay = OV_EDITOR;
    UIInvalidate();
}

// ---------------- 编辑器保存/删除 ----------------
static void EditorDelete();
static void EditorDuplicate();

static void EditorSaveCommit();

void EditorSave() {
    if (!app.editor.open) return;
    Event out;
    std::wstring err;
    if (!app.editor.Collect(out, err)) {
        app.editor.error = err;
        UIInvalidate();
        return;
    }
    auto conflicts = FindEventConflicts(out, app.editor.seriesId);
    if (!conflicts.empty()) {
        std::wstring desc = L"与「" + conflicts[0] + L"」";
        if (conflicts.size() > 1) desc += Fmt(L" 等 %d 条日程", (int)conflicts.size());
        desc += L"时间重叠，仍要保存吗？";
        OpenConfirm(L"时间冲突", desc, []() { EditorSaveCommit(); }, L"仍然保存", L"取消");
        return;
    }
    EditorSaveCommit();
}

static void EditorSaveCommit() {
    if (!app.editor.open) return;
    Event out;
    std::wstring err;
    if (!app.editor.Collect(out, err)) {
        app.editor.error = err;
        UIInvalidate();
        return;
    }
    // 用户没有主动改颜色时，不要让“仅某一次的 override 颜色”覆盖整个系列的颜色
    bool colorEdited = (out.color != app.editor.originalColor);
    if (!colorEdited && !app.editor.isNew && app.editor.seriesRecurring && app.editor.scope != 0) {
        const Event* base = app.store.Find(app.editor.seriesId);
        if (base) out.color = base->color;
    }
    app.store.Snapshot();
    if (app.editor.isNew) {
        app.store.AddEvent(out);
        ShowToast(L"已创建日程");
    } else if (app.editor.seriesRecurring) {
        app.store.UpdateOccurrence(app.editor.seriesId, app.editor.origStart, out, app.editor.scope);
        ShowToast(app.editor.scope == 0 ? L"已修改这一次" : (app.editor.scope == 1 ? L"已修改此后所有" : L"已修改整个系列"));
    } else {
        const Event* old = app.store.Find(app.editor.seriesId);
        if (old) {
            out.id = old->id;
            out.uid = old->uid;
            out.created = old->created;
            out.overrides = old->overrides;
        } else {
            out.id = app.editor.seriesId;
        }
        app.store.UpdateEvent(out);
        ShowToast(L"已保存");
    }
    app.editor.open = false;
    app.overlay = OV_NONE;
    InvalidateOccs();
    DoSaveStore();
    UIInvalidate();
}

static void EditorDelete() {
    if (app.editor.isNew) {
        app.editor.open = false;
        app.overlay = OV_NONE;
        UIInvalidate();
        return;
    }
    std::string sid = app.editor.seriesId;
    DateTime os = app.editor.origStart;
    bool rec = app.editor.seriesRecurring;
    if (!rec) {
        OpenConfirm(L"删除日程", L"确定删除这条日程吗？可用 Ctrl+Z 撤销。", [sid]() {
            app.store.Snapshot();
            app.store.DeleteEvent(sid);
            app.editor.open = false;
            app.overlay = OV_NONE;
            InvalidateOccs();
            DoSaveStore();
            ShowToast(L"已删除");
        }, L"删除");
        return;
    }
    std::vector<MenuItemDef> items;
    items.push_back({ L"仅删除这一次", ICO_TRASH, true, false, [sid, os]() {
        app.store.Snapshot();
        app.store.DeleteOccurrence(sid, os, 0);
        app.editor.open = false;
        app.overlay = OV_NONE;
        InvalidateOccs();
        DoSaveStore();
        ShowToast(L"已删除这一次");
    } });
    items.push_back({ L"删除此后所有", ICO_TRASH, true, false, [sid, os]() {
        app.store.Snapshot();
        app.store.DeleteOccurrence(sid, os, 1);
        app.editor.open = false;
        app.overlay = OV_NONE;
        InvalidateOccs();
        DoSaveStore();
        ShowToast(L"已删除此后所有");
    } });
    items.push_back({ L"删除整个系列", ICO_TRASH, true, false, [sid]() {
        app.store.Snapshot();
        app.store.DeleteEvent(sid);
        app.editor.open = false;
        app.overlay = OV_NONE;
        InvalidateOccs();
        DoSaveStore();
        ShowToast(L"已删除整个系列");
    } });
    OpenMenu(0, 0, items);
}
static void EditorDuplicate() {
    if (app.editor.isNew) return;
    Event out;
    std::wstring err;
    if (!app.editor.Collect(out, err)) {
        app.editor.error = err;
        UIInvalidate();
        return;
    }
    app.eventClip.valid = true;
    app.eventClip.ev = out;
    app.eventClip.ev.recur = Recur();
    app.eventClip.ev.overrides.clear();
    app.eventClip.mode = 0;
    app.eventClip.sourceStart = out.start;
    app.eventClip.label = out.title;
    ShowToast(L"已复制日程，右键目标日期粘贴");
}

// ---------------- 绘制 ----------------
static HWND EditInBox(Graphics& g, int id, RectF r, std::wstring* buf, bool multiline = false, float px = 12.5f) {
    const Theme& th = TH();
    FillRoundRect(g, r, S(8), th.panel2);
    HWND h = UIEdit(id, RectF(r.X + S(10), r.Y + (multiline ? S(8) : (r.Height - S(21)) / 2),
                              r.Width - S(20), multiline ? r.Height - S(16) : S(21)), buf, multiline, S(px));
    bool focus = h && GetFocus() == h;
    StrokeRoundRect(g, r, S(8), focus ? WithAlpha(th.accent, 150) : th.borderSoft, S(1));
    return h;
}
static void FieldLabel(Graphics& g, const wchar_t* label, float x, float y) {
    Text(g, label, S(10), false, TH().dim2, RectF(x, y, S(200), S(15)));
}

static std::vector<DateTime> RecurPreviewDates(const EditorState& ed, int maxN) {
    std::vector<DateTime> out;
    if (ed.freq == RF_NONE || ed.start.y < 1901) return out;
    DateTime start = StartOfDay(ed.start);
    DateTime until{};
    bool hasUntil = false;
    if (ed.endMode == 2 && !ed.untilStr.empty()) {
        DateTime u;
        if (DateInputParse(ed.untilStr, start, u)) { until = StartOfDay(u); hasUntil = true; }
    }
    int startSerial = DaySerial(start.y, start.mo, start.d);
    int startWd = DayOfWeek(start.y, start.mo, start.d);
    int maxCount = (ed.endMode == 1) ? std::max(1, ed.count) : 400;
    int produced = 0;
    for (int diff = 1; diff <= 1461; ++diff) {   // 最多预览 4 年
        DateTime cur = AddDaysDT(start, diff);
        if (hasUntil && CompareDT(StartOfDay(cur), until) > 0) break;
        bool match = false;
        if (ed.freq == RF_DAILY) {
            match = (diff % std::max(1, ed.interval) == 0);
        } else if (ed.freq == RF_WEEKLY) {
            int wd = DayOfWeek(cur.y, cur.mo, cur.d);
            bool dayOk = ed.byDay.empty() ? (wd == startWd) : (ed.byDay.count(wd) > 0);
            int weekIdx = diff / 7;
            match = dayOk && (weekIdx % std::max(1, ed.interval) == 0);
        } else if (ed.freq == RF_MONTHLY) {
            int monthDiff = (cur.y * 12 + cur.mo) - (start.y * 12 + start.mo);
            if (monthDiff > 0 && monthDiff % std::max(1, ed.interval) == 0) {
                if (ed.monthlyMode == 0) {
                    match = (cur.d == start.d);
                } else {
                    int wd = DayOfWeek(cur.y, cur.mo, cur.d);
                    if (wd == startWd) {
                        if (ed.nth == -1) {
                            DateTime nextWeek = AddDaysDT(cur, 7);
                            match = (nextWeek.mo != cur.mo);
                        } else {
                            match = ((cur.d - 1) / 7 + 1) == ed.nth;
                        }
                    }
                }
            }
        } else if (ed.freq == RF_YEARLY) {
            int yearDiff = cur.y - start.y;
            if (yearDiff > 0 && yearDiff % std::max(1, ed.interval) == 0)
                match = (cur.mo == start.mo && cur.d == start.d);
        }
        if (!match) continue;
        out.push_back(cur);
        if ((int)out.size() >= maxN || ++produced >= maxCount) break;
    }
    return out;
}

void DrawEditorOverlay(Graphics& g, float W, float H) {
    gfx::FontPartScope fontPart(gfx::FP_EDITOR);
    const Theme& th = TH();
    EditorState& ed = app.editor;
    float pw = S(688), ph = S(810);
    float x, y;
    {
        pw = std::min(pw, W - S(40));
        ph = std::min(ph, H - S(24));
        x = (W - pw) / 2;
        y = (H - ph) / 2;
        Shadow(g, RectF(x, y, pw, ph), S(16), 8, 26);
        FillRoundRect(g, RectF(x, y, pw, ph), S(16), th.card);
        StrokeRoundRect(g, RectF(x, y, pw, ph), S(16), th.border, S(1));
    }
    float pad = S(24), cw = pw - pad * 2;
    float cx = x + pad;
    float yy = y + S(14);
    // 标题行
    Txt(g, ed.isNew ? L"新建日程" : L"编辑日程", S(16), true, th.text, RectF(cx, yy, S(200), S(26)));
    if (!ed.isNew && ed.seriesRecurring) {
        // 作用范围
        float sx = cx + S(150);
        const wchar_t* sc[3] = { L"仅此次", L"此后所有", L"整个系列" };
        for (int i = 0; i < 3; ++i) {
            RectF r(sx + i * S(78), yy + S(1), S(72), S(26));
            UI::Chip(g, r, CMD_ED_SCOPE_THIS + i, sc[i], ed.scope == i, th.accent, [i]() {
                app.editor.scope = i;
                UIInvalidate();
            }, 10.5f);
        }
    }
    UI::IconBtn(g, RectF(x + pw - S(46), yy - S(2), S(28), S(28)), CMD_ED_CANCEL, ICO_CLOSE, 13, []() {
        app.editor.open = false;
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    yy += S(36);
    // 标题
    FieldLabel(g, L"标题", cx, yy);
    EditInBox(g, 9101, RectF(cx, yy + S(16), cw, S(34)), &ed.title, false, 14.0f);
    yy += S(58);
    // 时间
    FieldLabel(g, L"时间", cx, yy);
    float rowY = yy + S(16);
    UI::CheckBox(g, RectF(cx, rowY, S(72), S(32)), CMD_ED_ALLDAY, L"全天", ed.allDay, []() {
        app.editor.allDay = !app.editor.allDay;
        app.editor.SyncTimeTexts();
        UIInvalidate();
    });
    float fx = cx + S(78);
    float dateW = S(122), timeW = S(78);
    EditInBox(g, 9102, RectF(fx, rowY, dateW, S(32)), &ed.date1);
    EditInBox(g, 9103, RectF(fx + dateW + S(6), rowY, timeW, S(32)), &ed.time1);
    Txt(g, L"至", S(12), false, th.dim2, RectF(fx + dateW + timeW + S(8), rowY, S(18), S(32)), ALIGN_CENTER | ALIGN_VCENTER);
    float fx2 = fx + dateW + timeW + S(30);
    EditInBox(g, 9104, RectF(fx2, rowY, dateW, S(32)), &ed.date2);
    EditInBox(g, 9105, RectF(fx2 + dateW + S(6), rowY, timeW, S(32)), &ed.time2);
    yy += S(58);
    // 重复
    FieldLabel(g, L"重复", cx, yy);
    const wchar_t* freqs[5] = { L"不重复", L"每天", L"每周", L"每月", L"每年" };
    for (int i = 0; i < 5; ++i) {
        RectF r(cx + i * S(74), yy + S(16), S(68), S(26));
        UI::Chip(g, r, CMD_ED_RECUR_BASE + i, freqs[i], ed.freq == i, th.accent, [i]() {
            app.editor.freq = i;
            if (i == RF_WEEKLY && app.editor.byDay.empty()) {
                app.editor.byDay.insert(DayOfWeek(app.editor.start.y, app.editor.start.mo, app.editor.start.d));
            }
            UIInvalidate();
        }, 10.5f);
    }
    yy += S(50);
    if (ed.freq != RF_NONE) {
        // 间隔 / 周几 / 结束
        UI::Stepper(g, RectF(cx, yy, S(128), S(30)), CMD_ED_INT_DEC, CMD_ED_INT_INC,
                    Fmt(L"间隔 %d", ed.interval), []() {
                        app.editor.interval = std::max(1, app.editor.interval - 1);
                        UIInvalidate();
                    }, []() {
                        app.editor.interval = std::min(99, app.editor.interval + 1);
                        UIInvalidate();
                    }, 10.5f);
        float wx = cx + S(140);
        if (ed.freq == RF_WEEKLY) {
            for (int i = 0; i < 7; ++i) {
                int wd = (app.store.settings.weekStart + i) % 7;
                RectF r(wx + i * S(38), yy, S(34), S(30));
                bool act = ed.byDay.count(wd) > 0;
                UI::Chip(g, r, CMD_ED_WEEKDAY_BASE + wd, WeekdayShortCN(wd), act, th.accent, [wd]() {
                    if (app.editor.byDay.count(wd)) app.editor.byDay.erase(wd);
                    else app.editor.byDay.insert(wd);
                    UIInvalidate();
                }, 10.5f);
            }
        } else if (ed.freq == RF_MONTHLY) {
            UI::Chip(g, RectF(wx, yy, S(96), S(30)), CMD_ED_MONTHMODE_BASE + 0, L"按开始日", ed.monthlyMode == 0, th.accent, []() {
                app.editor.monthlyMode = 0;
                UIInvalidate();
            }, 10);
            UI::Chip(g, RectF(wx + S(102), yy, S(110), S(30)), CMD_ED_MONTHMODE_BASE + 1, L"第 N 个周几", ed.monthlyMode == 1, th.accent, []() {
                app.editor.monthlyMode = 1;
                UIInvalidate();
            }, 10);
            if (ed.monthlyMode == 1) {
                UI::Stepper(g, RectF(wx + S(220), yy, S(120), S(30)), CMD_ED_NTH_DEC, CMD_ED_NTH_INC,
                            ed.nth < 0 ? L"最后一个" : Fmt(L"第 %d 个", ed.nth), []() {
                                app.editor.nth = app.editor.nth <= 1 ? -1 : app.editor.nth - 1;
                                UIInvalidate();
                            }, []() {
                                app.editor.nth = app.editor.nth < 0 ? 1 : std::min(5, app.editor.nth + 1);
                                UIInvalidate();
                            }, 10);
                Txt(g, Fmt(L"周%s", WeekdayShortCN(DayOfWeek(ed.start.y, ed.start.mo, ed.start.d))),
                    S(10), false, th.dim2, RectF(wx + S(346), yy, S(100), S(30)), ALIGN_VCENTER);
            }
        } else if (ed.freq == RF_YEARLY) {
            if (ed.lunar) {
                Txt(g, L"农历", S(9.5f), false, th.dim2, RectF(wx, yy, S(30), S(30)), ALIGN_VCENTER);
                EditInBox(g, 9121, RectF(wx + S(32), yy, S(126), S(30)), &ed.lunarDate, false, 11);
                UI::Chip(g, RectF(wx + S(168), yy, S(92), S(30)), CMD_ED_LUNAR, L"按农历", true, th.accent, []() {
                    app.editor.lunar = false;
                    UIInvalidate();
                }, 10);
                Txt(g, L"年-月-日，闰月写 闰06，自动换算", S(8.5f), false, WithAlpha(th.dim2, 200),
                    RectF(wx + S(266), yy, S(230), S(30)), ALIGN_VCENTER);
            } else {
                Txt(g, Fmt(L"每年 %d 月 %d 日", ed.start.mo, ed.start.d), S(10.5f), false, th.dim2,
                    RectF(wx, yy, S(220), S(30)), ALIGN_VCENTER);
                UI::Chip(g, RectF(wx + S(250), yy, S(96), S(30)), CMD_ED_LUNAR, L"按农历", false, th.accent, []() {
                    app.editor.lunar = true;
                    app.editor.ParseTimeTexts();
                    app.editor.lunarDate = LunarDateString(app.editor.start);
                    UIInvalidate();
                }, 10);
            }
        } else {
            Txt(g, ed.freq == RF_DAILY ? L"每天循环" : L"", S(10.5f), false, th.dim2, RectF(wx, yy, S(160), S(30)), ALIGN_VCENTER);
        }
        yy += S(36);
        // 结束条件
        const wchar_t* ends[3] = { L"永不结束", L"重复次数", L"结束日期" };
        for (int i = 0; i < 3; ++i) {
            RectF r(cx + i * S(96), yy, S(90), S(30));
            UI::Chip(g, r, CMD_ED_ENDMODE_BASE + i, ends[i], ed.endMode == i, th.accent, [i]() {
                app.editor.endMode = i;
                UIInvalidate();
            }, 10.5f);
        }
        if (ed.endMode == 1) {
            UI::Stepper(g, RectF(cx + S(300), yy, S(130), S(30)), CMD_ED_ENDCOUNT_DEC, CMD_ED_ENDCOUNT_INC,
                        Fmt(L"%d 次", ed.count), []() {
                            app.editor.count = std::max(1, app.editor.count - 1);
                            UIInvalidate();
                        }, []() {
                            app.editor.count = std::min(999, app.editor.count + 1);
                            UIInvalidate();
                        }, 10.5f);
        } else if (ed.endMode == 2) {
            EditInBox(g, 9109, RectF(cx + S(300), yy, S(130), S(30)), &ed.untilStr, false, 11);
        }
        yy += S(40);
        // 例外日预览：点日期可跳过/恢复该次
        {
            Txt(g, L"例外日：点日期跳过该次，再点恢复", S(9.5f), false, th.dim2,
                RectF(cx, yy, S(320), S(18)));
            yy += S(22);
            auto dates = RecurPreviewDates(ed, 9);
            if (dates.empty()) {
                Txt(g, L"（未来没有可预览的重复日期）", S(9), false, WithAlpha(th.dim2, 180),
                    RectF(cx, yy, S(300), S(22)), ALIGN_VCENTER);
                yy += S(26);
            } else {
                float bx = cx;
                for (int i = 0; i < (int)dates.size(); ++i) {
                    const DateTime d = dates[i];
                    bool skipped = false;
                    for (auto& ex : ed.exDates) if (SameDay(ex, d)) { skipped = true; break; }
                    std::wstring label = Fmt(L"%d/%d", d.mo, d.d);
                    if (skipped) label += L"×";
                    UI::Chip(g, RectF(bx, yy, S(54), S(26)), CMD_ED_EXDATE_BASE + i, label, !skipped,
                             skipped ? th.danger : th.accent, [d]() {
                                 auto it = std::find_if(app.editor.exDates.begin(), app.editor.exDates.end(),
                                                       [&](const DateTime& e) { return SameDay(e, d); });
                                 if (it != app.editor.exDates.end()) app.editor.exDates.erase(it);
                                 else app.editor.exDates.push_back(d);
                                 UIInvalidate();
                             }, 9.5f);
                    // 例外日属于系列规则，自动切到“整个系列”，避免只改单次
                    app.editor.scope = 2;
                    bx += S(58);
                    if (bx > cx + S(560) && i + 1 < (int)dates.size()) {
                        bx = cx;
                        yy += S(30);
                    }
                }
                yy += S(32);
            }
        }
    } else {
        yy += S(6);
    }
    // 标签
    FieldLabel(g, L"标签", cx, yy);
    UI::CheckBox(g, RectF(x + pw - S(164), yy - S(4), S(140), S(24)), CMD_ED_DDL, L"设为 DDL", ed.isDdl, []() {
        app.editor.isDdl = !app.editor.isDdl;
        UIInvalidate();
    });
    yy += S(18);
    {
        float tagTop = yy;
        float gap = S(5);
        float chipW = (cw - gap * 5) / 6;
        float chipH = S(24);
        int n = (int)app.store.tags.size();
        for (int i = 0; i < n; ++i) {
            const Tag& tag = app.store.tags[i];
            RectF r(cx + (i % 6) * (chipW + gap), tagTop + (i / 6) * (chipH + S(4)), chipW, chipH);
            std::string tid = tag.id;
            int tagIcon = IconFromName(tag.icon.empty() ? std::wstring(DefaultTagIcon(tag.id)) : tag.icon);
            UI::TagChip(g, r, CMD_ED_TAG_BASE + i, tag.name, Hex(tag.color), ed.tags.count(tid) > 0, [tid]() {
                if (app.editor.tags.count(tid)) {
                    app.editor.tags.erase(tid);
                } else {
                    app.editor.tags.insert(tid);
                    // 点「生日」标签 = 生日日程，自动按每年重复
                    if (tid == "birthday" && app.editor.freq != RF_YEARLY) {
                        app.editor.freq = RF_YEARLY;
                        ShowToast(L"生日已自动设为每年重复");
                    }
                }
                UIInvalidate();
            }, tagIcon);
        }
        int rows = (n + 5) / 6;
        yy = tagTop + rows * (chipH + S(4)) + S(2);
    }
    // 优先级 / 状态
    FieldLabel(g, L"优先级", cx, yy);
    for (int i = 0; i < 4; ++i) {
        RectF r(cx + S(54) + i * S(56), yy - S(2), S(50), S(24));
        Color pc = (i == PRIO_HIGH) ? th.danger : (i == PRIO_MED ? th.warn : (i == PRIO_LOW ? th.success : th.dim));
        UI::Chip(g, r, CMD_ED_PRIO_BASE + i, PriorityName(i), ed.priority == i, pc, [i]() {
            app.editor.priority = i;
            UIInvalidate();
        }, 10);
    }
    float stx = cx + S(300);
    FieldLabel(g, L"状态", stx, yy);
    for (int i = 0; i < 3; ++i) {
        RectF r(stx + S(44) + i * S(58), yy - S(2), S(52), S(24));
        UI::Chip(g, r, CMD_ED_STATUS_BASE + i, StatusName(i), ed.status == i, th.accent2, [i]() {
            app.editor.status = i;
            UIInvalidate();
        }, 10);
    }
    UI::CheckBox(g, RectF(x + pw - S(130), yy - S(4), S(100), S(24)), CMD_ED_ALLDAY + 1, L"已完成", ed.done, []() {
        app.editor.done = !app.editor.done;
        UIInvalidate();
    });
    yy += S(30);
    // 提醒
    FieldLabel(g, L"提醒", cx, yy);
    {
        float ax = cx + S(40);
        for (int i = 0; i < (int)ed.alarms.size(); ++i) {
            Alarm& a = ed.alarms[i];
            std::wstring label = a.offsetMin == 0 ? L"准时" :
                (a.offsetMin > 0 ? Fmt(L"延后 %d 分钟", a.offsetMin) : Fmt(L"提前 %d 分钟", -a.offsetMin));
            if (-a.offsetMin >= 1440 && -a.offsetMin % 1440 == 0) label = Fmt(L"提前 %d 天", -a.offsetMin / 1440);
            else if (-a.offsetMin > 60 && -a.offsetMin % 60 == 0) label = Fmt(L"提前 %d 小时", -a.offsetMin / 60);
            SizeF sz; MeasureText(g, label, S(10), false, sz);
            float cw2 = sz.Width + S(30);
            RectF chip(ax, yy - S(2), cw2, S(24));
            FillRoundRect(g, chip, S(6), WithAlpha(th.accent2, 26));
            StrokeRoundRect(g, chip, S(6), WithAlpha(th.accent2, 60), S(1));
            Txt(g, label, S(10), false, WithAlpha(th.text, 220), RectF(chip.X + S(8), chip.Y, cw2 - S(26), chip.Height), ALIGN_VCENTER);
            Txt(g, L"×", S(11), false, th.dim2, RectF(RX(chip) - S(18), chip.Y, S(14), chip.Height), ALIGN_CENTER | ALIGN_VCENTER);
            int idx = i;
            AddHit(chip.X, chip.Y, chip.Width, chip.Height, CMD_ED_ALARM_DEL_BASE + i, [idx]() {
                if (idx < (int)app.editor.alarms.size()) app.editor.alarms.erase(app.editor.alarms.begin() + idx);
                UIInvalidate();
            });
            ax += cw2 + S(6);
            if (ax > x + pw - S(180)) break;
        }
        // 添加提醒
        RectF add(ax, yy - S(2), S(92), S(24));
        UI::Chip(g, add, CMD_ED_ALARM_ADD, L"+ 提醒", false, th.accent, []() {
            std::vector<MenuItemDef> items;
            auto addAlarm = [](int offset) {
                app.editor.alarms.push_back(Alarm{ offset, true, L"" });
                UIInvalidate();
            };
            items.push_back({ L"准时", ICO_BELL, false, false, [addAlarm]() { addAlarm(0); } });
            items.push_back({ L"提前 5 分钟", ICO_BELL, false, false, [addAlarm]() { addAlarm(-5); } });
            items.push_back({ L"提前 10 分钟", ICO_BELL, false, false, [addAlarm]() { addAlarm(-10); } });
            items.push_back({ L"提前 15 分钟", ICO_BELL, false, false, [addAlarm]() { addAlarm(-15); } });
            items.push_back({ L"提前 30 分钟", ICO_BELL, false, false, [addAlarm]() { addAlarm(-30); } });
            items.push_back({ L"提前 1 小时", ICO_BELL, false, false, [addAlarm]() { addAlarm(-60); } });
            items.push_back({ L"提前 2 小时", ICO_BELL, false, false, [addAlarm]() { addAlarm(-120); } });
            items.push_back({ L"提前 1 天", ICO_BELL, false, false, [addAlarm]() { addAlarm(-1440); } });
            OpenMenu(0, 0, items, OV_EDITOR);
        }, 10);
    }
    yy += S(30);
    // 地点 / 链接
    FieldLabel(g, L"地点", cx, yy);
    FieldLabel(g, L"链接", cx + cw / 2 + S(8), yy);
    EditInBox(g, 9106, RectF(cx, yy + S(16), cw / 2 - S(60), S(32)), &ed.location);
    UI::Btn(g, RectF(cx + cw / 2 - S(54), yy + S(16), S(48), S(32)), CMD_ED_LOCATION_PRESET, L"常用", 1, -1, []() {
        OpenLocationPresetMenu();
    });
    EditInBox(g, 9107, RectF(cx + cw / 2 + S(8), yy + S(16), cw / 2 - S(8), S(32)), &ed.url);
    yy += S(56);
    // 备注
    FieldLabel(g, L"备注", cx, yy);
    EditInBox(g, 9108, RectF(cx, yy + S(16), cw, S(66)), &ed.desc, true, 11.5f);
    yy += S(90);
    // 颜色
    FieldLabel(g, L"颜色", cx, yy);
    {
        const auto& colors = PaletteHex();
        float swx = cx + S(40);
        UI::Chip(g, RectF(swx, yy - S(2), S(50), S(22)), CMD_ED_COLOR_BASE, L"自动", ed.color.empty(), th.accent, []() {
            app.editor.color.clear();
            UIInvalidate();
        }, 9.5f);
        swx += S(56);
        float swStep = S(24);
        for (int i = 0; i < (int)colors.size(); ++i) {
            RectF sw(swx + i * swStep, yy - S(2), S(20), S(20));
            FillRoundRect(g, sw, S(5), Hex(colors[i]));
            if (ed.color == colors[i]) StrokeRoundRect(g, sw, S(5), th.text, S(2));
            std::wstring c = colors[i];
            AddHit(sw.X, sw.Y, sw.Width, sw.Height, CMD_ED_COLOR_BASE + 1 + i, [c]() {
                app.editor.color = c;
                UIInvalidate();
            });
        }
        {
            RectF cu(swx + (int)colors.size() * swStep + S(4), yy - S(2), S(66), S(20));
            FillRoundRect(g, cu, S(5), WithAlpha(th.panel2, 200));
            StrokeRoundRect(g, cu, S(5), WithAlpha(th.text, 80), S(1));
            Txt(g, L"自定义…", S(9), false, th.dim, cu, ALIGN_CENTER | ALIGN_VCENTER);
            AddHit(cu.X, cu.Y, cu.Width, cu.Height, CMD_ED_COLOR_CUSTOM, []() {
                OpenColorPicker(L"日程颜色", app.editor.color, [](const std::wstring& hex) {
                    app.editor.color = hex;
                    UIInvalidate();
                });
            });
        }
        yy += S(30);
    }
    // 错误提示
    if (!ed.error.empty()) {
        Txt(g, ed.error, S(10.5f), false, th.danger, RectF(cx, y + ph - S(76), cw - S(260), S(18)));
    }
    // 底部按钮
    float by = y + ph - S(54);
    UI::Btn(g, RectF(x + pw - pad - S(96), by, S(96), S(36)), CMD_ED_SAVE, L"保存", 0, ICO_CHECK, []() { EditorSave(); });
    UI::Btn(g, RectF(x + pw - pad - S(96) * 2 - S(8), by, S(96), S(36)), CMD_ED_CANCEL, L"取消", 1, -1, []() {
        app.editor.open = false;
        app.overlay = OV_NONE;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pad + (ed.isNew ? S(0) : S(106)), by, S(96), S(36)), CMD_ED_TEMPLATE_SAVE, L"存为模板", 1, ICO_EDIT,
            []() { SaveEditorAsTemplate(); });
    if (!ed.isNew) {
        UI::Btn(g, RectF(x + pw - pad - S(96) * 3 - S(16), by, S(96), S(36)), CMD_ED_DUPLICATE, L"复制", 1, ICO_COPY, []() { EditorDuplicate(); });
        UI::Btn(g, RectF(x + pad, by, S(96), S(36)), CMD_ED_DELETE, L"删除", 3, ICO_TRASH, []() { EditorDelete(); });
    }
}