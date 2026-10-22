// smart.cpp - 智能整理实现
#include "smart.h"
#include "util.h"
#include <algorithm>
#include <map>
#include <set>
#include <cmath>

namespace {

const std::vector<std::wstring>& kCourseColors = PaletteHex();

unsigned FnvHash(const std::wstring& s) {
    unsigned h = 2166136261u;
    for (wchar_t c : s) { h ^= (unsigned)c; h *= 16777619u; }
    return h;
}

std::wstring NormalizeTitle(const std::wstring& s) {
    std::wstring t = Trim(s);
    // 去掉首尾空格、常见后缀空格差异；全角/半角括号不强制统一
    return t;
}

struct Item {
    std::string id;
    std::wstring title;
    DateTime start, end;
    int wd = 0;
    int startMin = 0;
    int durMin = 0;
    bool allDay = false;
    int serial = 0;
};

int MedianGap(std::vector<int>& gaps) {
    if (gaps.empty()) return 0;
    std::sort(gaps.begin(), gaps.end());
    return gaps[gaps.size() / 2];
}

} // namespace

bool LooksLikeCourse(const std::wstring& title) {
    static const wchar_t* kws[] = {
        L"高等数学", L"大学英语", L"大学物理", L"程序设计", L"思想道德", L"心理健康",
        L"近现代史", L"工程学", L"工程导论", L"职业生涯", L"军事理论", L"军训",
        L"大学化学", L"线性代数", L"概率论", L"数据结构", L"操作系统", L"计算机网络",
        L"电路", L"模拟电子", L"数字电子", L"自动控制", L"电机", L"单片机",
        L"马克思主义", L"毛泽东", L"习近平", L"形势与政策", L"大学体育", L"体育（",
        L"英语（", L"数学（", L"物理（", L"实验（", L"（实验）", L"导论", L"概论",
        L"课", L"讲座", L"训练",
    };
    for (auto* kw : kws) {
        if (title.find(kw) != std::wstring::npos) return true;
    }
    return false;
}

std::wstring SmartCourseColor(const std::wstring& title) {
    unsigned h = FnvHash(title);
    return kCourseColors[h % kCourseColors.size()];
}

SmartResult SmartOrganize(Store& st, const SmartOptions& opt) {
    SmartResult result;
    std::string courseTagId = st.EnsureTag(L"课程", L"#6EA8FF");
    std::set<std::string> idFilter;
    for (auto& id : opt.onlyIds) idFilter.insert(id);
    bool restrictIds = !idFilter.empty();

    // 1) 收集候选：单次日程（未打标签的优先）
    std::map<std::wstring, std::vector<Item>> groups;
    std::map<std::string, const Event*> evById;
    int scanned = 0;
    for (auto& e : st.events) {
        if (e.IsRecurring()) continue;
        if (restrictIds && !idFilter.count(e.id)) continue;
        if (opt.onlyUntagged && !e.tags.empty()) continue;
        scanned++;
        evById[e.id] = &e;
        Item it;
        it.id = e.id;
        it.title = NormalizeTitle(e.title);
        it.start = e.start;
        it.end = e.end;
        it.wd = DayOfWeek(e.start.y, e.start.mo, e.start.d);
        it.startMin = e.start.h * 60 + e.start.mi;
        it.durMin = std::max(0, MinutesBetween(e.start, e.end));
        it.allDay = e.allDay;
        it.serial = DaySerial(e.start.y, e.start.mo, e.start.d);
        std::wstring key = it.title + L"|" + Fmt(L"%d|%d|%d|%d", it.wd, it.startMin, it.durMin, it.allDay ? 1 : 0);
        groups[key].push_back(it);
    }
    result.scanned = scanned;

    // 2) 先给课程类单次日程打标签 / 配色（无论是否合并）
    std::map<std::string, std::pair<bool, std::wstring>> patches;   // id -> (addCourseTag, color)
    for (auto& kv : groups) {
        auto& items = kv.second;
        for (auto& it : items) {
            const Event* e = evById[it.id];
            bool course = LooksLikeCourse(it.title);
            bool needTag = opt.tagCourses && course && std::find(e->tags.begin(), e->tags.end(), courseTagId) == e->tags.end();
            bool needColor = opt.autoColor && e->color.empty() && course;
            if (needTag || needColor) {
                patches[it.id] = { needTag, needColor ? SmartCourseColor(it.title) : L"" };
                if (needTag) result.courseTagged++;
                if (needColor) result.colored++;
            }
        }
    }

    // 3) 检测每周规律并合并为重复系列
    std::set<std::string> removedIds;
    std::vector<Event> newSeries;
    std::vector<std::wstring> groupNotes;
    for (auto& kv : groups) {
        auto& items = kv.second;
        if (!opt.mergeWeekly || items.size() < 3) continue;
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.serial < b.serial; });
        // 相邻间隔（天）
        std::vector<int> gaps;
        for (size_t i = 1; i < items.size(); ++i) gaps.push_back(items[i].serial - items[i - 1].serial);
        int med = MedianGap(gaps);
        if (med < 7) continue;
        int weekSteps = (int)std::lround(med / 7.0);
        if (weekSteps < 1 || weekSteps > 4) continue;
        int expect = weekSteps * 7;
        int matches = 0;
        for (int g : gaps) if (std::abs(g - expect) <= 2) ++matches;
        if ((double)matches / gaps.size() < 0.7) continue;   // 70% 以上符合每周规律

        const Event* first = evById[items.front().id];
        if (!first) continue;
        Event series = *first;
        series.id = NewId("ev");
        series.uid.clear();
        series.overrides.clear();
        series.recur = Recur();
        series.recur.freq = RF_WEEKLY;
        series.recur.interval = weekSteps;
        series.recur.byDay = { items.front().wd };
        series.recur.hasUntil = true;
        series.recur.until = items.back().start;
        series.recur.count = 0;
        // 缺勤周 → 例外日（保持与原始数据完全一致）
        std::set<int> present;
        for (auto& it : items) present.insert(it.serial);
        DateTime cursor = first->start;
        int guard = 0;
        while (guard++ < 400) {
            int serial = DaySerial(cursor.y, cursor.mo, cursor.d);
            if (serial > items.back().serial) break;
            if (!present.count(serial)) series.recur.exDates.push_back(cursor);
            cursor = AddDaysDT(cursor, expect);
        }
        // 课程标签 / 配色
        if (opt.tagCourses && LooksLikeCourse(series.title) &&
            std::find(series.tags.begin(), series.tags.end(), courseTagId) == series.tags.end()) {
            series.tags.push_back(courseTagId);
            result.courseTagged++;
        }
        if (opt.autoColor && series.color.empty() && LooksLikeCourse(series.title)) {
            series.color = SmartCourseColor(series.title);
            result.colored++;
        }
        // 合并后没有单独补丁了：从 patches 移除这些 id 的重复计数与补丁
        for (auto& it : items) {
            auto pit = patches.find(it.id);
            if (pit != patches.end()) {
                if (pit->second.first) result.courseTagged--;
                if (!pit->second.second.empty()) result.colored--;
                patches.erase(pit);
            }
            removedIds.insert(it.id);
        }
        result.seriesCreated++;
        result.eventsMerged += (int)items.size();
        groupNotes.push_back(Fmt(L"%s ×%d", series.title.c_str(), (int)items.size()));
        newSeries.push_back(series);
    }

    if (removedIds.empty() && patches.empty()) {
        result.summary = Fmt(L"扫描 %d 条：没有发现可整理的重复规律", result.scanned);
        return result;
    }

    // 4) 应用修改
    std::vector<Event> kept;
    kept.reserve(st.events.size());
    for (auto& e : st.events) {
        if (removedIds.count(e.id)) continue;
        auto pit = patches.find(e.id);
        if (pit != patches.end()) {
            if (pit->second.first) e.tags.push_back(courseTagId);
            if (!pit->second.second.empty()) e.color = pit->second.second;
        }
        kept.push_back(e);
    }
    for (auto& s : newSeries) kept.push_back(s);
    st.events.swap(kept);
    st.Touch();

    std::wstring note;
    if (!groupNotes.empty()) {
        note = L"；合并示例：" + groupNotes[0];
        if (groupNotes.size() > 1) note += Fmt(L" 等 %d 组", (int)groupNotes.size());
    }
    result.summary = Fmt(L"扫描 %d 条 → 新建 %d 个重复系列（并入 %d 条），课程标签 %d 条，自动配色 %d 条%s",
                         result.scanned, result.seriesCreated, result.eventsMerged,
                         result.courseTagged, result.colored, note.c_str());
    return result;
}
