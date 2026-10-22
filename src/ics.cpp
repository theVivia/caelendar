// ics.cpp - ICS (RFC 5545 常用子集) 解析与导出
#include "ics.h"
#include "util.h"
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace {

struct PProp {
    std::wstring name;                                   // 大写
    std::map<std::wstring, std::wstring> params;         // 键大写
    std::wstring value;
};
struct Comp {
    std::wstring name;
    std::vector<PProp> props;
    std::vector<int> children;
    const PProp* Prop(const wchar_t* n) const {
        for (auto& p : props) if (p.name == n) return &p;
        return nullptr;
    }
    std::wstring Val(const wchar_t* n, const std::wstring& def = L"") const {
        const PProp* p = Prop(n);
        return p ? p->value : def;
    }
    std::wstring Param(const wchar_t* n, const std::wstring& key) const {
        const PProp* p = Prop(n);
        if (!p) return L"";
        auto it = p->params.find(key);
        return it == p->params.end() ? L"" : it->second;
    }
};

static std::vector<std::wstring> Unfold(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstring cur;
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'\r') continue;
        if (c == L'\n') { lines.push_back(cur); cur.clear(); continue; }
        cur += c;
    }
    if (!cur.empty()) lines.push_back(cur);
    std::vector<std::wstring> out;
    for (auto& l : lines) {
        if (!l.empty() && (l[0] == L' ' || l[0] == L'\t') && !out.empty()) out.back() += l.substr(1);
        else out.push_back(l);
    }
    return out;
}

static std::wstring Upper(const std::wstring& s) {
    std::wstring o = s;
    for (auto& c : o) c = (wchar_t)towupper(c);
    return o;
}

static void ParseLine(const std::wstring& line, PProp& out) {
    // 找第一个不在引号里的 ':'
    size_t colon = std::wstring::npos;
    bool inQuote = false;
    for (size_t i = 0; i < line.size(); ++i) {
        wchar_t c = line[i];
        if (c == L'"') inQuote = !inQuote;
        else if (c == L':' && !inQuote) { colon = i; break; }
    }
    if (colon == std::wstring::npos) { out.name.clear(); return; }
    std::wstring head = line.substr(0, colon);
    out.value = line.substr(colon + 1);
    // head: NAME;PARAM=VAL;...
    std::vector<std::wstring> parts;
    std::wstring cur;
    bool q = false;
    for (wchar_t c : head) {
        if (c == L'"') { q = !q; continue; }
        if (c == L';' && !q) { parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    parts.push_back(cur);
    if (parts.empty()) return;
    out.name = Upper(parts[0]);
    for (size_t i = 1; i < parts.size(); ++i) {
        size_t eq = parts[i].find(L'=');
        if (eq == std::wstring::npos) { out.params[Upper(parts[i])] = L""; continue; }
        out.params[Upper(parts[i].substr(0, eq))] = parts[i].substr(eq + 1);
    }
}

// 上面为了简单，改用文件级保存解析结果
static std::vector<Comp> g_comps;

static std::vector<int> ParseFile(const std::wstring& text, std::vector<std::wstring>& warnings) {
    g_comps.clear();
    g_comps.push_back(Comp{ L"ROOT", {}, {} });
    std::vector<int> stack{ 0 };
    auto lines = Unfold(text);
    for (auto& line : lines) {
        if (Trim(line).empty()) continue;
        PProp p;
        ParseLine(line, p);
        if (p.name.empty()) { warnings.push_back(L"无法解析行：" + line); continue; }
        if (p.name == L"BEGIN") {
            Comp c;
            c.name = Upper(Trim(p.value));
            int idx = (int)g_comps.size();
            g_comps.push_back(std::move(c));
            g_comps[stack.back()].children.push_back(idx);
            stack.push_back(idx);
        } else if (p.name == L"END") {
            if (stack.size() > 1) stack.pop_back();
        } else {
            g_comps[stack.back()].props.push_back(std::move(p));
        }
    }
    return g_comps[0].children;
}

static std::wstring Unescape(const std::wstring& s) {
    std::wstring out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t n = s[i + 1];
            if (n == L'n' || n == L'N') { out += L'\n'; ++i; }
            else if (n == L',') { out += L','; ++i; }
            else if (n == L';') { out += L';'; ++i; }
            else if (n == L'\\') { out += L'\\'; ++i; }
            else out += s[i];
        } else out += s[i];
    }
    return out;
}
static std::wstring Escape(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c == L'\\') out += L"\\\\";
        else if (c == L';') out += L"\\;";
        else if (c == L',') out += L"\\,";
        else if (c == L'\n') out += L"\\n";
        else if (c == L'\r') continue;
        else out += c;
    }
    return out;
}

struct DTVal {
    DateTime dt;
    bool allDay = false;
    bool utc = false;
    std::wstring tzid;
    bool valid = false;
};

static int TZOffsetFromString(const std::wstring& s) {
    // +0800 / -0500
    int sign = 1, h = 0, m = 0;
    std::wstring t = Trim(s);
    size_t i = 0;
    if (!t.empty() && (t[0] == L'+' || t[0] == L'-')) { sign = t[0] == L'-' ? -1 : 1; i = 1; }
    if (t.size() >= i + 4) {
        h = (t[i] - L'0') * 10 + (t[i + 1] - L'0');
        m = (t[i + 2] - L'0') * 10 + (t[i + 3] - L'0');
    }
    return sign * (h * 60 + m);
}

static DTVal ParseDT(const PProp& p, bool endProp = false) {
    DTVal v;
    auto it = p.params.find(L"TZID");
    if (it != p.params.end()) v.tzid = it->second;
    auto vt = p.params.find(L"VALUE");
    std::wstring val = Trim(p.value);
    bool dateOnly = (vt != p.params.end() && Upper(vt->second) == L"DATE") ||
                    (val.size() == 8 && val.find(L'T') == std::wstring::npos);
    if (dateOnly) {
        if (val.size() < 8) return v;
        v.dt = MakeDT(_wtoi(val.substr(0, 4).c_str()), _wtoi(val.substr(4, 2).c_str()), _wtoi(val.substr(6, 2).c_str()));
        v.allDay = true;
    } else {
        // YYYYMMDDTHHMMSS[Z] 或 YYYYMMDDTHHMM
        if (val.size() < 13) return v;
        int y = _wtoi(val.substr(0, 4).c_str());
        int mo = _wtoi(val.substr(4, 2).c_str());
        int d = _wtoi(val.substr(6, 2).c_str());
        int h = _wtoi(val.substr(9, 2).c_str());
        int mi = _wtoi(val.substr(11, 2).c_str());
        if (mo < 1 || mo > 12 || d < 1 || d > 31) return v;
        v.dt = MakeDT(y, mo, d, h, mi);
        if (!val.empty() && (val.back() == L'Z' || val.back() == L'z')) v.utc = true;
        (void)endProp;
    }
    v.valid = true;
    return v;
}

static DateTime UtcToLocal(const DateTime& utc) {
    SYSTEMTIME st{};
    st.wYear = (WORD)utc.y; st.wMonth = (WORD)utc.mo; st.wDay = (WORD)utc.d;
    st.wHour = (WORD)utc.h; st.wMinute = (WORD)utc.mi;
    st.wDayOfWeek = (WORD)DayOfWeek(utc.y, utc.mo, utc.d);
    SYSTEMTIME lt{};
    if (SystemTimeToTzSpecificLocalTime(nullptr, &st, &lt)) {
        return MakeDT(lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute);
    }
    return AddMinutesDT(utc, 480);
}
static DateTime LocalToUtc(const DateTime& loc) {
    SYSTEMTIME st{};
    st.wYear = (WORD)loc.y; st.wMonth = (WORD)loc.mo; st.wDay = (WORD)loc.d;
    st.wHour = (WORD)loc.h; st.wMinute = (WORD)loc.mi;
    st.wDayOfWeek = (WORD)DayOfWeek(loc.y, loc.mo, loc.d);
    SYSTEMTIME ut{};
    if (TzSpecificLocalTimeToSystemTime(nullptr, &st, &ut)) {
        return MakeDT(ut.wYear, ut.wMonth, ut.wDay, ut.wHour, ut.wMinute);
    }
    return AddMinutesDT(loc, -480);
}

static DateTime ToLocalTime(const DTVal& v, const std::map<std::wstring, int>& tzOffsets, std::vector<std::wstring>& warnings) {
    if (v.allDay) return v.dt;
    if (v.utc) return UtcToLocal(v.dt);
    if (v.tzid.empty()) return v.dt;
    std::wstring lower = Lower(v.tzid);
    if (Contains(lower, L"shanghai") || Contains(lower, L"prc") || Contains(lower, L"beijing") || Contains(lower, L"china"))
        return v.dt;
    for (auto& kv : tzOffsets) {
        if (Lower(kv.first) == lower) return AddMinutesDT(v.dt, 480 - kv.second);
    }
    warnings.push_back(L"未知时区 " + v.tzid + L"，按本地时间处理");
    return v.dt;
}

// 时长：P1DT2H30M → 分钟
static int ParseDurationMinutes(const std::wstring& s) {
    int sign = 1;
    size_t i = 0;
    std::wstring t = s;
    if (!t.empty() && (t[0] == L'+' || t[0] == L'-')) { sign = t[0] == L'-' ? -1 : 1; i = 1; }
    if (i < t.size() && (t[i] == L'P' || t[i] == L'p')) ++i;
    int total = 0;
    int num = 0;
    bool inTime = false;
    for (; i < t.size(); ++i) {
        wchar_t c = t[i];
        if (c == L'T' || c == L't') { inTime = true; continue; }
        if (c >= L'0' && c <= L'9') { num = num * 10 + (c - L'0'); continue; }
        int mult = 0;
        if (c == L'W') mult = 7 * 24 * 60;
        else if (c == L'D' || c == L'd') mult = 24 * 60;
        else if (c == L'H' || c == L'h') mult = 60;
        else if (c == L'M' || c == L'm') mult = inTime ? 1 : 0;
        else if (c == L'S' || c == L's') mult = 0;
        total += num * mult;
        num = 0;
    }
    return sign * total;
}

static int WeekdayFromCode(const std::wstring& code) {
    if (code == L"SU") return 0;
    if (code == L"MO") return 1;
    if (code == L"TU") return 2;
    if (code == L"WE") return 3;
    if (code == L"TH") return 4;
    if (code == L"FR") return 5;
    if (code == L"SA") return 6;
    return -1;
}
static const wchar_t* WeekdayCode(int wd) {
    static const wchar_t* w[7] = { L"SU", L"MO", L"TU", L"WE", L"TH", L"FR", L"SA" };
    return w[(wd % 7 + 7) % 7];
}

static void ParseRRule(const std::wstring& value, Recur& r, std::vector<std::wstring>& warnings,
                       const std::map<std::wstring, int>& tzOffsets) {
    for (auto& part : Split(value, L';')) {
        size_t eq = part.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = Upper(part.substr(0, eq));
        std::wstring val = part.substr(eq + 1);
        if (key == L"FREQ") {
            if (val == L"DAILY") r.freq = RF_DAILY;
            else if (val == L"WEEKLY") r.freq = RF_WEEKLY;
            else if (val == L"MONTHLY") r.freq = RF_MONTHLY;
            else if (val == L"YEARLY") r.freq = RF_YEARLY;
            else { warnings.push_back(L"不支持的重复频率：" + val); r.freq = RF_NONE; }
        } else if (key == L"INTERVAL") {
            r.interval = std::max(1, _wtoi(val.c_str()));
        } else if (key == L"COUNT") {
            r.count = std::max(0, _wtoi(val.c_str()));
        } else if (key == L"UNTIL") {
            PProp p; p.name = L"UNTIL"; p.value = val;
            DTVal dv = ParseDT(p);
            if (dv.valid) {
                r.until = ToLocalTime(dv, tzOffsets, warnings);
                if (dv.allDay) { r.until.h = 0; r.until.mi = 0; }
                r.hasUntil = true;
            }
        } else if (key == L"BYDAY") {
            for (auto& dayStr : Split(val, L',')) {
                std::wstring d = Trim(dayStr);
                int ord = 0;
                size_t pos = 0;
                if (!d.empty() && (d[0] == L'+' || d[0] == L'-' || (d[0] >= L'0' && d[0] <= L'9'))) {
                    std::wstring num;
                    while (pos < d.size() && (d[pos] == L'+' || d[pos] == L'-' || (d[pos] >= L'0' && d[pos] <= L'9'))) num += d[pos++];
                    ord = _wtoi(num.c_str());
                }
                int wd = WeekdayFromCode(Upper(d.substr(pos)));
                if (wd >= 0) {
                    r.byDay.push_back(wd);
                    if (ord != 0) r.nthWeekday = ord;
                }
            }
        } else if (key == L"BYMONTHDAY") {
            r.byMonthDay = _wtoi(val.c_str());
        } else if (key == L"BYMONTH") {
            r.byMonth = _wtoi(val.c_str());
        } else if (key == L"WKST") {
            int wd = WeekdayFromCode(Upper(val));
            if (wd >= 0) r.wkst = wd;
        } else {
            warnings.push_back(L"忽略不支持的 RRULE 项：" + key);
        }
    }
    if (r.freq == RF_NONE) return;
    if (r.freq == RF_WEEKLY && r.byDay.empty()) {
        // 由 DTSTART 决定
    }
}

static int ParsePriority(int prio) {
    if (prio <= 0) return PRIO_NONE;
    if (prio <= 4) return PRIO_HIGH;
    if (prio == 5) return PRIO_MED;
    return PRIO_LOW;
}

static void ParseAlarms(int compIdx, Event& ev, const std::map<std::wstring, int>& tzOffsets,
                        std::vector<std::wstring>& warnings, int durationMin) {
    for (int childIdx : g_comps[compIdx].children) {
        Comp& c = g_comps[childIdx];
        if (c.name != L"VALARM") continue;
        std::wstring action = Upper(c.Val(L"ACTION"));
        if (action == L"EMAIL" || action == L"AUDIO") {
            warnings.push_back(L"忽略不支持的提醒类型：" + action);
            continue;
        }
        const PProp* trig = c.Prop(L"TRIGGER");
        if (!trig) continue;
        Alarm a;
        std::wstring text = c.Val(L"DESCRIPTION");
        a.text = Unescape(text);
        a.sound = c.Val(L"ACTION") != L"NONE";
        std::wstring v = Trim(trig->value);
        bool relatedEnd = false;
        auto relIt = trig->params.find(L"RELATED");
        if (relIt != trig->params.end() && Upper(relIt->second) == L"END") relatedEnd = true;
        if (!v.empty() && (v[0] == L'-' || v[0] == L'+' || v[0] == L'P')) {
            int mins = ParseDurationMinutes(v);
            a.offsetMin = relatedEnd ? (durationMin + mins) : mins;
        } else {
            PProp p; p.name = L"TRIGGER"; p.value = v;
            DTVal dv = ParseDT(p);
            if (dv.valid) {
                DateTime at = ToLocalTime(dv, tzOffsets, warnings);
                a.offsetMin = MinutesBetween(ev.start, at);
            }
        }
        ev.alarms.push_back(a);
    }
}

static int ParseVEvent(int idx, const std::wstring& sourceName, std::vector<std::wstring>& warnings,
                       std::map<std::wstring, int>& tzOffsets, IcsParseResult& result) {
    Comp& c = g_comps[idx];
    Event ev;
    ev.source = sourceName;
    ev.uid = Unescape(c.Val(L"UID"));
    ev.title = Unescape(c.Val(L"SUMMARY"));
    if (ev.title.empty()) ev.title = L"（无标题）";
    ev.desc = Unescape(c.Val(L"DESCRIPTION"));
    ev.location = Unescape(c.Val(L"LOCATION"));
    ev.url = Unescape(c.Val(L"URL"));
    // 时间
    DTVal ds, de;
    if (const PProp* p = c.Prop(L"DTSTART")) ds = ParseDT(*p);
    if (const PProp* p = c.Prop(L"DTEND")) de = ParseDT(*p, true);
    if (!ds.valid) {
        warnings.push_back(L"日程缺少 DTSTART，已跳过：" + ev.title);
        return -1;
    }
    ev.allDay = ds.allDay;
    ev.start = ToLocalTime(ds, tzOffsets, warnings);
    if (ev.allDay) ev.start.h = ev.start.mi = 0;
    if (de.valid) {
        ev.end = ToLocalTime(de, tzOffsets, warnings);
        if (ev.allDay) ev.end.h = ev.end.mi = 0;
    } else if (const PProp* dur = c.Prop(L"DURATION")) {
        int mins = ParseDurationMinutes(dur->value);
        ev.end = AddMinutesDT(ev.start, mins);
    } else {
        ev.end = ev.allDay ? AddDaysDT(ev.start, 1) : ev.start;
    }
    int durMin = MinutesBetween(ev.start, ev.end);
    // 重复
    if (const PProp* rr = c.Prop(L"RRULE")) ParseRRule(rr->value, ev.recur, warnings, tzOffsets);
    // EXDATE
    for (auto& p : c.props) {
        if (p.name != L"EXDATE") continue;
        auto ivt = p.params.find(L"VALUE");
        bool isDate = ivt != p.params.end() && Upper(ivt->second) == L"DATE";
        for (auto& one : Split(p.value, L',')) {
            PProp tmp; tmp.name = L"EXDATE"; tmp.value = Trim(one);
            if (isDate) tmp.params[L"VALUE"] = L"DATE";
            auto tzit = p.params.find(L"TZID");
            if (tzit != p.params.end()) tmp.params[L"TZID"] = tzit->second;
            DTVal dv = ParseDT(tmp);
            if (!dv.valid) continue;
            DateTime local = ToLocalTime(dv, tzOffsets, warnings);
            if (ev.allDay) { local.h = 0; local.mi = 0; }
            ev.recur.exDates.push_back(local);
        }
    }
    // RDATE：转成独立事件（简化处理）
    std::vector<DateTime> rdates;
    for (auto& p : c.props) {
        if (p.name != L"RDATE") continue;
        for (auto& one : Split(p.value, L',')) {
            PProp tmp; tmp.name = L"RDATE"; tmp.value = Trim(one);
            DTVal dv = ParseDT(tmp);
            if (dv.valid) rdates.push_back(ToLocalTime(dv, tzOffsets, warnings));
        }
    }
    // 状态 / 优先级
    std::wstring status = Upper(c.Val(L"STATUS"));
    if (status == L"TENTATIVE") ev.status = ST_TENTATIVE;
    else if (status == L"CANCELLED") ev.status = ST_CANCELLED;
    ev.priority = ParsePriority(_wtoi(c.Val(L"PRIORITY", L"0").c_str()));
    // 颜色
    std::wstring color = c.Val(L"COLOR");
    if (!color.empty() && color[0] == L'#') ev.color = color;
    // 标签
    for (auto& p : c.props) {
        if (p.name != L"CATEGORIES") continue;
        for (auto& cat : Split(p.value, L',')) {
            std::wstring name = Trim(Unescape(cat));
            if (name.empty()) continue;
            std::string id = TagIdForName(name);
            ev.tags.push_back(id);
            result.tagNames[id] = name;
        }
    }
    if (!ev.tags.empty()) ev.tags.erase(std::unique(ev.tags.begin(), ev.tags.end()), ev.tags.end());
    ParseAlarms(idx, ev, tzOffsets, warnings, durMin);
    // RECURRENCE-ID
    DateTime recurId;
    bool hasRecurId = false;
    if (const PProp* p = c.Prop(L"RECURRENCE-ID")) {
        DTVal dv = ParseDT(*p);
        if (dv.valid) {
            recurId = ToLocalTime(dv, tzOffsets, warnings);
            if (ev.allDay) { recurId.h = 0; recurId.mi = 0; }
            hasRecurId = true;
        }
    }
    if (hasRecurId) {
        // 暂存为覆盖实例，最后挂到系列
        ev.recurId = recurId;
        result.events.push_back(ev);   // 先全部放进去，后面合并
        return (int)result.events.size() - 1;
    }
    ev.id = NewId("ev");
    int mainIdx = (int)result.events.size();
    result.events.push_back(ev);
    // rdate → 独立事件
    for (auto& rd : rdates) {
        Event extra = ev;
        extra.id = NewId("ev");
        extra.uid.clear();
        extra.recur = Recur();
        DateTime s = MakeDT(rd.y, rd.mo, rd.d, ev.start.h, ev.start.mi);
        if (ev.allDay) s = MakeDT(rd.y, rd.mo, rd.d, 0, 0);
        extra.start = s;
        extra.end = AddMinutesDT(s, durMin);
        extra.overrides.clear();
        result.events.push_back(extra);
    }
    // 范围
    if (!result.hasRange) { result.minDate = ev.start; result.maxDate = ev.DisplayEnd(); result.hasRange = true; }
    else {
        if (CompareDT(ev.start, result.minDate) < 0) result.minDate = ev.start;
        if (CompareDT(ev.DisplayEnd(), result.maxDate) > 0) result.maxDate = ev.DisplayEnd();
    }
    return mainIdx;
}

} // namespace

// 需要在使用前声明（ParseVEvent 里用）


IcsParseResult ParseIcs(const std::string& utf8) {
    IcsParseResult result;
    std::wstring text;
    // BOM / 编码
    if (utf8.size() >= 3 && (unsigned char)utf8[0] == 0xEF && (unsigned char)utf8[1] == 0xBB && (unsigned char)utf8[2] == 0xBF)
        text = Utf8ToWide(utf8.substr(3));
    else
        text = Utf8ToWide(utf8);
    if (text.empty()) { result.warnings.push_back(L"文件为空"); return result; }
    std::vector<int> roots = ParseFile(text, result.warnings);
    // 时区表
    std::map<std::wstring, int> tzOffsets;
    for (int ri : roots) {
        Comp& cal = g_comps[ri];
        if (cal.name != L"VCALENDAR") continue;
        if (result.calendarName.empty()) {
            for (auto& p : cal.props) if (p.name == L"X-WR-CALNAME") result.calendarName = Unescape(p.value);
        }
        for (int ci : cal.children) {
            Comp& c = g_comps[ci];
            if (c.name == L"VTIMEZONE") {
                std::wstring tzid = c.Val(L"TZID");
                int off = 0;
                for (int si : c.children) {
                    Comp& sub = g_comps[si];
                    if (sub.name == L"STANDARD" || sub.name == L"DAYLIGHT") {
                        off = TZOffsetFromString(sub.Val(L"TZOFFSETTO"));
                        if (sub.name == L"STANDARD") break;
                    }
                }
                if (!tzid.empty()) tzOffsets[tzid] = off;
            }
        }
        // 先收集系列（无 RECURRENCE-ID），再挂覆盖
        std::vector<int> seriesIdx;
        for (int ci : cal.children) {
            Comp& c = g_comps[ci];
            if (c.name != L"VEVENT") continue;
            // 跳过 RECURRENCE-ID
            if (c.Prop(L"RECURRENCE-ID")) continue;
            int mainIdx = ParseVEvent(ci, result.calendarName, result.warnings, tzOffsets, result);
            if (mainIdx >= 0) seriesIdx.push_back(mainIdx);
        }
        for (int ci : cal.children) {
            Comp& c = g_comps[ci];
            if (c.name != L"VEVENT") continue;
            if (!c.Prop(L"RECURRENCE-ID")) continue;
            size_t before = result.events.size();
            ParseVEvent(ci, result.calendarName, result.warnings, tzOffsets, result);
            if (result.events.size() == before) continue;
            Event ov = result.events.back();
            result.events.pop_back();
            // 找系列
            int target = -1;
            for (int si : seriesIdx) {
                if (!ov.uid.empty() && result.events[si].uid == ov.uid) { target = si; break; }
            }
            if (target < 0) {
                // 孤儿覆盖 → 当普通事件
                ov.id = NewId("ev");
                result.events.push_back(ov);
                continue;
            }
            Event& series = result.events[target];
            Event store = ov;
            store.recurId = ov.recurId;
            store.recur = Recur();
            store.overrides.clear();
            store.id = series.id + "@" + WideToUtf8(ISODT(ov.recurId));
            series.overrides.push_back(store);
        }
    }
    // 给没有 uid 的事件补 uid
    for (auto& e : result.events) {
        if (e.uid.empty()) e.uid = Utf8ToWide(e.id) + L"@xingli";
        if (e.allDay && CompareDT(e.end, e.start) <= 0) e.end = AddDaysDT(e.start, 1);
    }
    if (result.events.empty() && result.warnings.empty()) result.warnings.push_back(L"没有找到日程（需要 VCALENDAR / VEVENT）");
    return result;
}

IcsImportResult ImportIcs(Store& st, const std::string& utf8, int mode, const std::wstring& sourceName) {
    IcsParseResult pr = ParseIcs(utf8);
    IcsImportResult res;
    res.warnings = pr.warnings;
    // 确保标签存在，并把解析出的标签 id 映射到实际 id
    std::map<std::string, std::string> tagMap;
    for (auto& kv : pr.tagNames) {
        std::string actual = st.EnsureTag(kv.second);
        tagMap[kv.first] = actual;
    }
    std::function<void(Event&)> remapTags = [&](Event& e) {
        for (auto& t : e.tags) {
            auto it = tagMap.find(t);
            if (it != tagMap.end()) t = it->second;
        }
        std::sort(e.tags.begin(), e.tags.end());
        e.tags.erase(std::unique(e.tags.begin(), e.tags.end()), e.tags.end());
        for (auto& ov : e.overrides) remapTags(ov);
    };
    for (auto& e : pr.events) remapTags(e);
    auto findExisting = [&](const std::wstring& uid, const Event& ev) -> Event* {
        if (uid.empty()) return nullptr;
        for (auto& e : st.events) if (!e.uid.empty() && e.uid == uid) return &e;
        return nullptr;
    };
    std::set<std::wstring> existingKeys;
    for (auto& e : st.events) {
        existingKeys.insert(e.title + L"|" + ISODT(e.start) + L"|" + ISODT(e.end) + L"|" + (e.allDay ? L"1" : L"0"));
    }
    std::set<std::wstring> importedKeys;
    for (auto& ev : pr.events) {
        Event e = ev;
        e.source = sourceName.empty() ? (pr.calendarName.empty() ? L"ICS 导入" : pr.calendarName) : sourceName;
        Event* existing = findExisting(e.uid, e);
        // 内容完全重复（标题 + 开始 + 结束 + 全天）→ 跳过，不再造重复日程
        if (mode != 1 && !existing) {
            std::wstring key = e.title + L"|" + ISODT(e.start) + L"|" + ISODT(e.end) + L"|" + (e.allDay ? L"1" : L"0");
            if (existingKeys.count(key) || importedKeys.count(key)) {
                ++res.skipped;
                continue;
            }
            importedKeys.insert(key);
        }
        if (mode == 1) {
            // 全部追加：UID 去重
            if (findExisting(e.uid, e)) {
                e.uid = e.uid + L"-dup";
                e.uid += Utf8ToWide(NewId("x").substr(1, 4));
            }
            e.id = NewId("ev");
            st.events.push_back(e);
            res.addedIds.push_back(e.id);
            ++res.added;
        } else if (mode == 2) {
            bool skip = existing != nullptr;
            if (!skip) {
                for (auto& old : st.events) {
                    if (old.title == e.title && CompareDT(old.start, e.start) == 0) { skip = true; break; }
                }
            }
            if (skip) ++res.skipped;
            else { e.id = NewId("ev"); st.events.push_back(e); res.addedIds.push_back(e.id); ++res.added; }
        } else {
            // 合并
            if (existing) {
                std::string id = existing->id;
                DateTime created = existing->created;
                DateTime updated = existing->updated;
                *existing = e;
                existing->id = id;
                existing->created = created;
                existing->updated = NowDT();
                res.updatedIds.push_back(id);
                ++res.updated;
            } else {
                e.id = NewId("ev");
                e.created = NowDT();
                e.updated = NowDT();
                st.events.push_back(e);
                res.addedIds.push_back(e.id);
                ++res.added;
            }
        }
    }
    st.Touch();
    return res;
}

// ---------------- 导出 ----------------
namespace {

std::string FoldLine(const std::wstring& line) {
    std::string utf8 = WideToUtf8(line);
    if (utf8.size() <= 75) return utf8 + "\r\n";
    std::string out;
    size_t pos = 0;
    bool first = true;
    while (pos < utf8.size()) {
        size_t budget = first ? 75 : 74;
        size_t take = std::min(budget, utf8.size() - pos);
        // 不要截断 UTF-8 多字节序列
        while (take > 0 && pos + take < utf8.size() &&
               ((unsigned char)utf8[pos + take] & 0xC0) == 0x80) --take;
        if (take == 0) take = std::min(budget, utf8.size() - pos);
        if (!first) out += ' ';
        out += utf8.substr(pos, take);
        out += "\r\n";
        pos += take;
        first = false;
    }
    return out;
}

std::string LocalDTStr(const DateTime& t) {
    return WideToUtf8(Fmt(L"%04d%02d%02dT%02d%02d%02d", t.y, t.mo, t.d, t.h, t.mi, 0));
}
std::string DateStr(const DateTime& t) {
    return WideToUtf8(Fmt(L"%04d%02d%02d", t.y, t.mo, t.d));
}
std::string UtcDTStr(const DateTime& loc) {
    DateTime utc = LocalToUtc(loc);
    return WideToUtf8(Fmt(L"%04d%02d%02dT%02d%02d%02dZ", utc.y, utc.mo, utc.d, utc.h, utc.mi, 0));
}

void AddAlarms(std::wstring& out, const Event& ev) {
    for (auto& a : ev.alarms) {
        out += L"BEGIN:VALARM\r\n";
        out += L"ACTION:DISPLAY\r\n";
        out += L"DESCRIPTION:" + Escape(a.text.empty() ? ev.title : a.text) + L"\r\n";
        int mins = a.offsetMin;
        std::wstring trig;
        if (mins == 0) trig = L"PT0S";
        else {
            bool neg = mins < 0;
            int m = std::abs(mins);
            std::wstring dur = L"P";
            if (m >= 1440) { dur += Fmt(L"%dD", m / 1440); m %= 1440; }
            if (m > 0) {
                dur += L"T";
                if (m >= 60) { dur += Fmt(L"%dH", m / 60); m %= 60; }
                if (m > 0) dur += Fmt(L"%dM", m);
            }
            if (dur == L"P") dur = L"PT0S";
            trig = (neg ? L"-" : L"+") + dur;
        }
        out += L"TRIGGER:" + trig + L"\r\n";
        out += L"END:VALARM\r\n";
    }
}

std::wstring RruleText(const Event& ev) {
    const Recur& r = ev.recur;
    if (r.freq == RF_NONE) return L"";
    std::wstring s = L"FREQ=";
    switch (r.freq) {
    case RF_DAILY: s += L"DAILY"; break;
    case RF_WEEKLY: s += L"WEEKLY"; break;
    case RF_MONTHLY: s += L"MONTHLY"; break;
    case RF_YEARLY: s += L"YEARLY"; break;
    default: return L"";
    }
    if (r.interval > 1) s += Fmt(L";INTERVAL=%d", r.interval);
    if (r.freq == RF_WEEKLY && !r.byDay.empty()) {
        s += L";BYDAY=";
        for (size_t i = 0; i < r.byDay.size(); ++i) {
            if (i) s += L",";
            s += WeekdayCode(r.byDay[i]);
        }
    }
    if (r.freq == RF_MONTHLY) {
        if (r.nthWeekday != 0 && !r.byDay.empty()) {
            s += Fmt(L";BYDAY=%d", r.nthWeekday);
            s += WeekdayCode(r.byDay[0]);
        } else if (r.byMonthDay != 0) {
            s += Fmt(L";BYMONTHDAY=%d", r.byMonthDay);
        }
    }
    if (r.freq == RF_YEARLY) {
        if (r.byMonth > 0) s += Fmt(L";BYMONTH=%d", r.byMonth);
        if (r.byMonthDay != 0) s += Fmt(L";BYMONTHDAY=%d", r.byMonthDay);
        if (r.nthWeekday != 0 && !r.byDay.empty()) {
            s += Fmt(L";BYDAY=%d", r.nthWeekday);
            s += WeekdayCode(r.byDay[0]);
        }
    }
    if (r.count > 0) s += Fmt(L";COUNT=%d", r.count);
    else if (r.hasUntil) s += L";UNTIL=" + Utf8ToWide(UtcDTStr(r.until));
    return s;
}

void EventBody(std::wstring& out, const Event& ev, const std::wstring& uid, bool includeRrule, const Store* st) {
    out += L"UID:" + Escape(uid) + L"\r\n";
    out += L"DTSTAMP:" + Utf8ToWide(UtcDTStr(NowDT())) + L"\r\n";
    if (st && !ev.tags.empty()) {
        std::wstring cats;
        for (size_t i = 0; i < ev.tags.size(); ++i) {
            if (i) cats += L",";
            cats += Escape(st->TagName(ev.tags[i]));
        }
        out += L"CATEGORIES:" + cats + L"\r\n";
    }
    if (ev.allDay) {
        out += L"DTSTART;VALUE=DATE:" + Utf8ToWide(DateStr(ev.start)) + L"\r\n";
        out += L"DTEND;VALUE=DATE:" + Utf8ToWide(DateStr(ev.end)) + L"\r\n";
    } else {
        out += L"DTSTART;TZID=Asia/Shanghai:" + Utf8ToWide(LocalDTStr(ev.start)) + L"\r\n";
        out += L"DTEND;TZID=Asia/Shanghai:" + Utf8ToWide(LocalDTStr(ev.end)) + L"\r\n";
    }
    if (includeRrule) {
        std::wstring rrule = RruleText(ev);
        if (!rrule.empty()) out += L"RRULE:" + rrule + L"\r\n";
        if (!ev.recur.exDates.empty()) {
            out += L"EXDATE";
            out += ev.allDay ? L";VALUE=DATE:" : L";TZID=Asia/Shanghai:";
            for (size_t i = 0; i < ev.recur.exDates.size(); ++i) {
                if (i) out += L",";
                out += ev.allDay ? Utf8ToWide(DateStr(ev.recur.exDates[i])) : Utf8ToWide(LocalDTStr(ev.recur.exDates[i]));
            }
            out += L"\r\n";
        }
    }
    out += L"SUMMARY:" + Escape(ev.title) + L"\r\n";
    if (!ev.desc.empty()) out += L"DESCRIPTION:" + Escape(ev.desc) + L"\r\n";
    if (!ev.location.empty()) out += L"LOCATION:" + Escape(ev.location) + L"\r\n";
    if (!ev.url.empty()) out += L"URL:" + Escape(ev.url) + L"\r\n";
    // 状态/优先级
    const wchar_t* status = ev.status == ST_TENTATIVE ? L"TENTATIVE" : (ev.status == ST_CANCELLED ? L"CANCELLED" : L"CONFIRMED");
    out += L"STATUS:" + std::wstring(status) + L"\r\n";
    int prio = ev.priority == PRIO_HIGH ? 1 : (ev.priority == PRIO_MED ? 5 : (ev.priority == PRIO_LOW ? 9 : 0));
    if (prio) out += Fmt(L"PRIORITY:%d\r\n", prio);
    if (!ev.color.empty()) out += L"COLOR:" + ev.color + L"\r\n";
    AddAlarms(out, ev);
}

} // namespace

std::string ExportIcs(const Store& st, const std::vector<std::string>& ids, const std::wstring& calName) {
    std::wstring out;
    out += L"BEGIN:VCALENDAR\r\n";
    out += L"VERSION:2.0\r\n";
    out += L"PRODID:-//XingLi//Calendar 1.0//ZH\r\n";
    out += L"CALSCALE:GREGORIAN\r\n";
    out += L"METHOD:PUBLISH\r\n";
    out += L"X-WR-CALNAME:" + Escape(calName) + L"\r\n";
    out += L"X-WR-TIMEZONE:Asia/Shanghai\r\n";
    // 时区定义
    out += L"BEGIN:VTIMEZONE\r\n";
    out += L"TZID:Asia/Shanghai\r\n";
    out += L"BEGIN:STANDARD\r\n";
    out += L"DTSTART:19700101T000000\r\n";
    out += L"TZOFFSETFROM:+0800\r\n";
    out += L"TZOFFSETTO:+0800\r\n";
    out += L"TZNAME:CST\r\n";
    out += L"END:STANDARD\r\n";
    out += L"END:VTIMEZONE\r\n";
    auto want = [&](const Event& e) {
        if (ids.empty()) return true;
        for (auto& id : ids) if (id == e.id) return true;
        return false;
    };
    for (auto& ev : st.events) {
        if (!want(ev)) continue;
        std::wstring uid = ev.uid.empty() ? (Utf8ToWide(ev.id) + L"@xingli") : ev.uid;
        out += L"BEGIN:VEVENT\r\n";
        EventBody(out, ev, uid, true, &st);
        out += L"END:VEVENT\r\n";
        // 覆盖实例
        for (auto& ov : ev.overrides) {
            out += L"BEGIN:VEVENT\r\n";
            std::wstring body;
            EventBody(body, ov, uid, false, &st);
            out += body;
            // RECURRENCE-ID 放在 UID 之后（顺序不严格）
            out += L"RECURRENCE-ID";
            if (ev.allDay) out += L";VALUE=DATE:";
            else out += L";TZID=Asia/Shanghai:";
            out += ev.allDay ? Utf8ToWide(DateStr(ov.recurId)) : Utf8ToWide(LocalDTStr(ov.recurId));
            out += L"\r\n";
            out += L"END:VEVENT\r\n";
        }
    }
    out += L"END:VCALENDAR\r\n";
    // 折行
    std::string result;
    for (auto& line : Split(out, L'\n')) {
        std::wstring l = line;
        if (!l.empty() && l.back() == L'\r') l.pop_back();
        if (l.empty()) continue;
        result += FoldLine(l);
    }
    return result;
}