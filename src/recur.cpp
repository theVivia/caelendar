// recur.cpp - 重复规则展开实现
#include "recur.h"
#include <algorithm>
#include <map>
#include <set>

bool LunarDateOf(const DateTime& d, int& lunarYear, int& month, int& day, bool& leap);
bool LunarToSolarDate(int lunarYear, int month, int day, bool leap, Date& out);
int LunarYearOf(const DateTime& d);

static bool PastUntil(const Event& e, const DateTime& t) {
    if (!e.recur.hasUntil) return false;
    DateTime u = e.recur.until;
    if (u.h == 0 && u.mi == 0) u = AddMinutesDT(u, 24 * 60 - 1);
    return CompareDT(t, u) > 0;
}

// 每月第 n 个星期几（n>0）/ 最后一个（n<0）
static int NthWeekdayOfMonth(int y, int m, int weekday, int n) {
    int dim = DaysInMonth(y, m);
    if (n > 0) {
        int firstWd = DayOfWeek(y, m, 1);
        int day = 1 + ((weekday - firstWd + 7) % 7) + (n - 1) * 7;
        return day <= dim ? day : 0;
    }
    if (n < 0) {
        int lastWd = DayOfWeek(y, m, dim);
        int day = dim - ((lastWd - weekday + 7) % 7) + (n + 1) * 7;
        return day >= 1 ? day : 0;
    }
    return 0;
}

static std::vector<int> WeekOrder(const std::vector<int>& days, int wkst) {
    std::vector<int> d = days;
    std::sort(d.begin(), d.end(), [&](int a, int b) {
        return ((a - wkst + 7) % 7) < ((b - wkst + 7) % 7);
    });
    return d;
}

std::vector<DateTime> RecurStartList(const Event& e, const DateTime& from, const DateTime& to) {
    std::vector<DateTime> out;
    if (e.recur.freq == RF_NONE) {
        DateTime end = e.DisplayEnd();
        if (CompareDT(e.start, to) < 0 && CompareDT(end, from) > 0) out.push_back(e.start);
        return out;
    }
    const Recur& r = e.recur;
    const int interval = std::max(1, r.interval);
    int produced = 0;
    int maxIter = 30000;
    auto addCandidate = [&](const DateTime& t) -> bool {
        if (r.count > 0 && produced >= r.count) return false;
        ++produced;
        if (PastUntil(e, t)) return false;
        if (CompareDT(t, from) >= 0 && CompareDT(t, to) <= 0) out.push_back(t);
        return true;
    };
    const DateTime fromDay = StartOfDay(from);
    const DateTime toDay = StartOfDay(to);

    switch (r.freq) {
    case RF_DAILY: {
        int diff = DaySerial(fromDay.y, fromDay.mo, fromDay.d) - DaySerial(e.start.y, e.start.mo, e.start.d);
        int skip = diff > 0 ? diff / interval : 0;
        if (r.count > 0 && skip >= r.count) return out;
        produced = skip;
        DateTime cur = AddDaysDT(e.start, skip * interval);
        for (int it = 0; it < maxIter; ++it) {
            if (!addCandidate(cur)) break;
            if (CompareDT(StartOfDay(cur), toDay) > 0) break;
            cur = AddDaysDT(cur, interval);
        }
        break;
    }
    case RF_WEEKLY: {
        std::vector<int> days = r.byDay;
        if (days.empty()) days.push_back(DayOfWeek(e.start.y, e.start.mo, e.start.d));
        days = WeekOrder(days, r.wkst);
        DateTime week0 = StartOfWeek(e.start, r.wkst);
        int diff = DaySerial(fromDay.y, fromDay.mo, fromDay.d) - DaySerial(week0.y, week0.mo, week0.d);
        int weeks = diff > 0 ? diff / 7 : 0;
        int skipWeeks = (weeks / interval) > 1 ? weeks / interval - 1 : 0;
        produced = skipWeeks * (int)days.size();
        if (r.count > 0 && produced >= r.count) return out;
        DateTime wk = AddDaysDT(week0, skipWeeks * interval * 7);
        bool stop = false;
        for (int it = 0; it < maxIter && !stop; ++it) {
            for (int d : days) {
                DateTime t = AddDaysDT(wk, (d - r.wkst + 7) % 7);
                t.h = e.start.h; t.mi = e.start.mi;
                if (!addCandidate(t)) { stop = true; break; }
            }
            if (CompareDT(wk, toDay) > 0) break;
            wk = AddDaysDT(wk, interval * 7);
        }
        break;
    }
    case RF_MONTHLY: {
        if (r.nthWeekday != 0 && !r.byDay.empty()) {
            int weekday = r.byDay[0];
            int m0 = e.start.y * 12 + (e.start.mo - 1);
            int mFrom = fromDay.y * 12 + (fromDay.mo - 1);
            int diff = mFrom - m0;
            int skip = diff > 0 ? diff / interval : 0;
            if (skip > 1) skip -= 1;
            if (skip < 0) skip = 0;
            if (r.count > 0 && skip >= r.count) return out;
            produced = skip;
            for (int it = 0; it < 12000; ++it) {
                int idx = m0 + (skip + it) * interval;
                int y = idx / 12, m = idx % 12 + 1;
                int day = NthWeekdayOfMonth(y, m, weekday, r.nthWeekday);
                if (day > 0) {
                    DateTime t = MakeDT(y, m, day, e.start.h, e.start.mi);
                    if (!addCandidate(t)) break;
                }
                if (CompareDT(MakeDT(y, m, 1), MakeDT(toDay.y, toDay.mo, 1)) > 0) break;
            }
        } else {
            int m0 = e.start.y * 12 + (e.start.mo - 1);
            int mFrom = fromDay.y * 12 + (fromDay.mo - 1);
            int diff = mFrom - m0;
            int skip = diff > 0 ? diff / interval : 0;
            if (skip > 1) skip -= 1;
            if (skip < 0) skip = 0;
            if (r.count > 0 && skip >= r.count) return out;
            produced = skip;
            int baseDay = r.byMonthDay > 0 ? r.byMonthDay : e.start.d;
            for (int it = 0; it < 12000; ++it) {
                int idx = m0 + (skip + it) * interval;
                int y = idx / 12, m = idx % 12 + 1;
                int dim = DaysInMonth(y, m);
                int day = baseDay;
                if (baseDay < 0) day = std::max(1, dim + 1 + baseDay);
                if (day <= dim) {
                    DateTime t = MakeDT(y, m, day, e.start.h, e.start.mi);
                    if (!addCandidate(t)) break;
                }
                if (CompareDT(MakeDT(y, m, 1), MakeDT(toDay.y, toDay.mo, 1)) > 0) break;
            }
        }
        break;
    }
    case RF_YEARLY: {
        if (r.lunar) {
            int ly0 = 0, lm = 0, ld = 0;
            bool leap = false;
            if (LunarDateOf(e.start, ly0, lm, ld, leap)) {
                int lyFrom = LunarYearOf(fromDay);
                if (lyFrom == 0) lyFrom = fromDay.y;
                int diff = lyFrom - ly0;
                int sk = diff > 0 ? diff / interval : 0;
                if (sk > 1) sk -= 1;
                if (sk < 0) sk = 0;
                if (r.count > 0 && sk >= r.count) return out;
                produced = sk;
                for (int it = 0; it < 2000; ++it) {
                    int ly = ly0 + (sk + it) * interval;
                    Date nd{};
                    bool ok = LunarToSolarDate(ly, lm, ld, leap, nd);
                    if (!ok && leap) ok = LunarToSolarDate(ly, lm, ld, false, nd);
                    if (!ok) { if (ly > toDay.y + 1) break; else continue; }
                    DateTime t = MakeDT(nd.y, nd.m, nd.d, e.start.h, e.start.mi);
                    if (!addCandidate(t)) break;
                    if (t.y > toDay.y + 1) break;
                }
                break;
            }
        }
        int month = r.byMonth > 0 ? r.byMonth : e.start.mo;
        int y0 = e.start.y;
        int diff = fromDay.y - y0;
        int skip = diff > 0 ? diff / interval : 0;
        if (skip > 1) skip -= 1;
        if (skip < 0) skip = 0;
        if (r.count > 0 && skip >= r.count) return out;
        produced = skip;
        for (int it = 0; it < 2000; ++it) {
            int y = y0 + (skip + it) * interval;
            int day = 0;
            if (r.nthWeekday != 0 && !r.byDay.empty()) day = NthWeekdayOfMonth(y, month, r.byDay[0], r.nthWeekday);
            else {
                int baseDay = r.byMonthDay > 0 ? r.byMonthDay : e.start.d;
                int dim = DaysInMonth(y, month);
                if (baseDay < 0) baseDay = dim + 1 + baseDay;
                if (baseDay <= dim) day = baseDay;
            }
            if (day > 0) {
                DateTime t = MakeDT(y, month, day, e.start.h, e.start.mi);
                if (!addCandidate(t)) break;
            }
            if (y > toDay.y) break;
        }
        break;
    }
    default: break;
    }
    return out;
}

static std::wstring OccKey(const std::string& seriesId, const DateTime& t) {
    return Utf8ToWide(seriesId) + L"|" + ISODT(t);
}

bool IsRecurringStart(const Event& e, const DateTime& t) {
    if (!e.IsRecurring()) return false;
    auto starts = RecurStartList(e, AddDaysDT(t, -1), AddDaysDT(t, 1));
    for (auto& s : starts) if (CompareDT(s, t) == 0) return true;
    return false;
}

bool FindRecurStartNear(const Event& e, const DateTime& t, DateTime& out) {
    if (!e.IsRecurring()) return false;
    auto starts = RecurStartList(e, AddDaysDT(t, -1), AddDaysDT(t, 1));
    for (auto& s : starts) if (CompareDT(s, t) == 0) { out = s; return true; }
    for (auto& s : starts) if (SameDay(s, t)) { out = s; return true; }
    return false;
}

std::vector<Occurrence> ExpandEventOccurrences(const Event& e, const DateTime& from, const DateTime& to) {
    std::vector<Occurrence> out;
    int durMin = MinutesBetween(e.start, e.end);
    DateTime bufFrom = AddDaysDT(from, -31);
    std::vector<DateTime> starts = RecurStartList(e, bufFrom, to);
    std::set<std::wstring> exSet;
    for (auto& d : e.recur.exDates) exSet.insert(ISODT(d));
    // 旧版本数据里 override 可能没有保存 recurId：用它的 start 作为兜底键，
    // 避免同一次课程既画原实例、又画覆盖实例（表现为每天重复一排）。
    auto ovKey = [](const Event& ov) {
        bool validRecurId = CompareDT(ov.recurId, DateTime{ 0, 0, 0, 0, 0 }) != 0;
        return ISODT(validRecurId ? ov.recurId : ov.start);
    };
    std::map<std::wstring, const Event*> ovMap;
    for (auto& ov : e.overrides) ovMap[ovKey(ov)] = &ov;
    std::set<std::wstring> generatedKeys;
    std::set<std::wstring> emittedStarts;

    auto pushOcc = [&](const Event& ev, const DateTime& s, const DateTime& en, bool isOv, const DateTime& orig) {
        DateTime end = en;
        // 时间点事件（原始 start==end，例如 DDL）：保持结束=开始，不强行加 1 分钟
        bool pointEvent = (CompareDT(ev.start, ev.end) == 0);
        if (CompareDT(end, s) <= 0 && !pointEvent) end = AddMinutesDT(s, 1);
        if (CompareDT(s, to) < 0 && CompareDT(end, from) > 0) {
            std::wstring startKey = ISODT(s);
            if (!emittedStarts.insert(startKey).second) return;  // 同系列同一开始时间只画一次
            Occurrence o;
            o.seriesId = e.id;
            o.ev = ev;
            o.start = s;
            o.end = end;
            o.isRecurring = e.IsRecurring();
            o.isOverride = isOv;
            o.origStart = orig;
            out.push_back(std::move(o));
        }
    };

    for (auto& s : starts) {
        std::wstring key = ISODT(s);
        generatedKeys.insert(key);
        if (exSet.count(key)) continue;
        auto it = ovMap.find(key);
        if (it != ovMap.end()) {
            const Event& ov = *it->second;
            if (ov.deletedOverride) continue;
            DateTime en = ov.end;
            if (CompareDT(en, ov.start) <= 0) en = AddMinutesDT(ov.start, 1);
            pushOcc(ov, ov.start, en, true, s);
        } else {
            DateTime en = AddMinutesDT(s, durMin);
            pushOcc(e, s, en, false, s);
        }
    }
    // 原位置不在范围内的覆盖实例（被移动进范围）
    for (auto& ov : e.overrides) {
        if (ov.deletedOverride) continue;
        std::wstring key = ovKey(ov);
        if (generatedKeys.count(key)) continue;
        DateTime en = ov.end;
        if (CompareDT(en, ov.start) <= 0) en = AddMinutesDT(ov.start, 1);
        pushOcc(ov, ov.start, en, true, ov.recurId);
    }
    std::sort(out.begin(), out.end(), [](const Occurrence& a, const Occurrence& b) {
        if (a.ev.allDay != b.ev.allDay) return a.ev.allDay;
        int c = CompareDT(a.start, b.start);
        if (c != 0) return c < 0;
        return a.ev.title < b.ev.title;
    });
    return out;
}

std::vector<Occurrence> ExpandOccurrences(const Store& st, const DateTime& from, const DateTime& to) {
    std::vector<Occurrence> out;
    for (auto& e : st.events) {
        auto v = ExpandEventOccurrences(e, from, to);
        out.insert(out.end(), v.begin(), v.end());
    }
    std::sort(out.begin(), out.end(), [](const Occurrence& a, const Occurrence& b) {
        if (a.ev.allDay != b.ev.allDay) return a.ev.allDay;
        int c = CompareDT(a.start, b.start);
        if (c != 0) return c < 0;
        return a.ev.title < b.ev.title;
    });
    return out;
}

std::vector<AlarmDue> UpcomingAlarms(const Store& st, const DateTime& now, int withinMinutes) {
    std::vector<AlarmDue> out;
    if (!st.settings.notify.enabled) return out;
    auto occs = ExpandOccurrences(st, AddDaysDT(now, -2), AddDaysDT(now, 2));
    for (auto& o : occs) {
        for (size_t i = 0; i < o.ev.alarms.size(); ++i) {
            const Alarm& a = o.ev.alarms[i];
            DateTime fire = AddMinutesDT(o.start, a.offsetMin);
            int diff = MinutesBetween(now, fire);
            if (diff >= -2 && diff <= withinMinutes) {
                AlarmDue d;
                d.seriesId = o.seriesId;
                d.evId = o.ev.id;
                d.occurrenceStart = o.origStart;
                d.occStart = o.start;
                d.occEnd = o.end;
                d.fireAt = fire;
                d.title = o.ev.title;
                d.body = a.text;
                d.alarmIndex = (int)i;
                d.key = Utf8ToWide(o.seriesId) + L"|" + ISODT(o.origStart) + L"|" + Fmt(L"%d", (int)i) + L"|" + ISODT(fire);
                out.push_back(d);
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const AlarmDue& a, const AlarmDue& b) {
        return CompareDT(a.fireAt, b.fireAt) < 0;
    });
    return out;
}

std::wstring RecurDescription(const Event& e) {
    const Recur& r = e.recur;
    if (r.freq == RF_NONE) return L"不重复";
    std::wstring s;
    if (r.interval > 1) s += Fmt(L"每 %d ", r.interval);
    else s += L"每";
    switch (r.freq) {
    case RF_DAILY: s += L"天"; break;
    case RF_WEEKLY: s += L"周"; break;
    case RF_MONTHLY: s += L"月"; break;
    case RF_YEARLY: s += L"年"; break;
    default: break;
    }
    if (r.freq == RF_WEEKLY && !r.byDay.empty()) {
        s += L" · ";
        for (size_t i = 0; i < r.byDay.size(); ++i) {
            if (i) s += L"、";
            s += WeekdayShortCN(r.byDay[i]);
        }
    }
    if (r.freq == RF_MONTHLY) {
        if (r.nthWeekday != 0 && !r.byDay.empty()) {
            s += L" · 第";
            if (r.nthWeekday < 0) s += L"最后一";
            else s += Fmt(L"%d", r.nthWeekday);
            s += Fmt(L"个周%s", WeekdayShortCN(r.byDay[0]));
        } else if (r.byMonthDay > 0) {
            s += Fmt(L" · %d 号", r.byMonthDay);
        }
    }
    if (r.freq == RF_YEARLY && r.byMonth > 0) s += Fmt(L" · %d 月", r.byMonth);
    if (r.count > 0) s += Fmt(L" · 共 %d 次", r.count);
    else if (r.hasUntil) s += Fmt(L" · 至 %s", ISODate(r.until).c_str());
    if (!r.exDates.empty()) s += Fmt(L" · 例外 %d 次", (int)r.exDates.size());
    return s;
}