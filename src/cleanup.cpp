// cleanup.cpp - 批量清理核心逻辑 + 清理面板
#include "cleanup.h"
#include "app.h"
#include "recur.h"
#include <algorithm>
#include <vector>
#include <string>

using namespace Gdiplus;
using namespace gfx;

// ---------------- 匹配逻辑 ----------------
static std::wstring UpperHex(std::wstring s) {
    for (auto& c : s) {
        if (c >= L'a' && c <= L'f') c = (wchar_t)(c - L'a' + L'A');
    }
    return s;
}
static bool HasKeyword(const Event& e, const std::wstring& kw) {
    if (kw.empty()) return true;
    std::wstring lower;
    auto lowerOf = [](const std::wstring& src) {
        std::wstring out = src;
        for (auto& c : out) {
            if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
        }
        return out;
    };
    std::wstring k = lowerOf(kw);
    return lowerOf(e.title).find(k) != std::wstring::npos ||
           lowerOf(e.desc).find(k) != std::wstring::npos ||
           lowerOf(e.location).find(k) != std::wstring::npos ||
           lowerOf(e.source).find(k) != std::wstring::npos;
}

bool CleanupDateInRange(const DateTime& d, const CleanupFilter& f) {
    if (!f.hasFrom && !f.hasTo) return true;
    DateTime day = StartOfDay(d);
    if (f.hasFrom && CompareDT(day, StartOfDay(f.from)) < 0) return false;
    if (f.hasTo && CompareDT(day, StartOfDay(f.to)) > 0) return false;
    return true;
}

bool CleanupSeriesMatches(const Event& e, const CleanupFilter& f) {
    if (!HasKeyword(e, f.keyword)) return false;
    if (!f.tagIds.empty()) {
        bool hit = false;
        for (auto& t : e.tags) if (f.tagIds.count(t)) { hit = true; break; }
        if (!hit) return false;
    }
    if (!f.colors.empty()) {
        std::wstring c = UpperHex(e.color);
        bool hit = false;
        if (c.empty()) hit = f.colors.count(L"#NONE") > 0;
        else hit = f.colors.count(c) > 0;
        if (!hit) return false;
    }
    if (f.typeMode == CT_SINGLE && e.IsRecurring()) return false;
    if (f.typeMode == CT_RECUR && !e.IsRecurring()) return false;
    if (f.statusMode == CS_DONE && !e.done) return false;
    if (f.statusMode == CS_UNDONE && e.done) return false;
    if (f.onlyAllDay && !e.allDay) return false;
    if (f.onlyWithAlarm && e.alarms.empty()) return false;
    return true;
}

static int CountOccurrencesInRange(const Store& st, const Event& e, const CleanupFilter& f) {
    if (!e.IsRecurring()) return CleanupDateInRange(e.start, f) ? 1 : 0;
    DateTime from = f.hasFrom ? StartOfDay(f.from) : StartOfDay(e.start);
    DateTime to = f.hasTo ? AddDaysDT(StartOfDay(f.to), 1) : AddDaysDT(StartOfDay(e.start), 1);
    if (CompareDT(to, from) <= 0) return 0;
    auto occs = ExpandOccurrences(const_cast<Store&>(st), from, to);
    int n = 0;
    for (auto& o : occs) {
        if (o.seriesId != e.id) continue;
        if (CleanupDateInRange(o.origStart, f)) ++n;
    }
    return n;
}

CleanupStats CleanupPreview(const Store& st, const CleanupFilter& f) {
    CleanupStats s;
    for (auto& e : st.events) {
        if (!CleanupSeriesMatches(e, f)) continue;
        if (f.hasFrom || f.hasTo) {
            int n = CountOccurrencesInRange(st, e, f);
            if (n == 0) continue;
            s.series++;
            s.occurrences += n;
        } else {
            s.series++;
            s.occurrences += e.IsRecurring() ? 2 : 1;   // 无范围时给个保守提示
        }
    }
    return s;
}

// ---------------- 应用删除 ----------------
int CleanupApply(Store& st, const CleanupFilter& f) {
    int removed = 0;
    if (f.delMode == 0) {
        std::vector<Event> kept;
        for (auto& e : st.events) {
            bool match = CleanupSeriesMatches(e, f) &&
                         (!(f.hasFrom || f.hasTo) || CountOccurrencesInRange(st, e, f) > 0);
            if (match) { removed++; continue; }
            kept.push_back(e);
        }
        if (removed > 0) {
            st.events.swap(kept);
            st.Touch();
        }
        return removed;
    }
    // 仅删除范围内的实例
    std::vector<std::pair<std::string, DateTime>> occTargets;
    std::vector<std::string> singleTargets;
    for (auto& e : st.events) {
        if (!CleanupSeriesMatches(e, f)) continue;
        if (!e.IsRecurring()) {
            if (CleanupDateInRange(e.start, f)) singleTargets.push_back(e.id);
            continue;
        }
        DateTime from = f.hasFrom ? StartOfDay(f.from) : StartOfDay(e.start);
        DateTime to = f.hasTo ? AddDaysDT(StartOfDay(f.to), 1) : AddDaysDT(StartOfDay(e.start), 400);
        auto occs = ExpandOccurrences(st, from, to);
        for (auto& o : occs) {
            if (o.seriesId != e.id) continue;
            if (!CleanupDateInRange(o.origStart, f)) continue;
            occTargets.push_back({ o.seriesId, o.origStart });
        }
    }
    for (auto& id : singleTargets) {
        if (st.DeleteEvent(id)) removed++;
    }
    for (auto& t : occTargets) {
        if (st.DeleteOccurrence(t.first, t.second, 0)) removed++;
    }
    if (removed > 0) st.Touch();
    return removed;
}

int CleanupDeleteAll(Store& st) {
    int n = (int)st.events.size();
    st.events.clear();
    st.Touch();
    return n;
}

int CleanupDeleteDone(Store& st) {
    std::vector<Event> kept;
    int n = 0;
    for (auto& e : st.events) {
        if (e.done) { n++; continue; }
        kept.push_back(e);
    }
    if (n > 0) {
        st.events.swap(kept);
        st.Touch();
    }
    return n;
}

int CleanupDeleteImported(Store& st) {
    std::vector<Event> kept;
    int n = 0;
    for (auto& e : st.events) {
        if (e.source.find(L"ICS") != std::wstring::npos) { n++; continue; }
        kept.push_back(e);
    }
    if (n > 0) {
        st.events.swap(kept);
        st.Touch();
    }
    return n;
}
