// recur.h - 重复规则展开
#pragma once
#include "model.h"
#include <vector>
#include <string>

// 生成 begins 列表（含 from..to 范围内的所有重复开始时间；单次事件也可能被返回）
std::vector<DateTime> RecurStartList(const Event& e, const DateTime& from, const DateTime& to);

// 展开为可渲染的实例（应用 EXDATE、覆盖实例、移入范围的覆盖）
std::vector<Occurrence> ExpandEventOccurrences(const Event& e, const DateTime& from, const DateTime& to);
std::vector<Occurrence> ExpandOccurrences(const Store& st, const DateTime& from, const DateTime& to);
bool IsRecurringStart(const Event& e, const DateTime& t);
bool FindRecurStartNear(const Event& e, const DateTime& t, DateTime& out);

// 即将触发的提醒
struct AlarmDue {
    std::string seriesId;
    std::string evId;
    DateTime occurrenceStart;
    DateTime occStart;
    DateTime occEnd;
    DateTime fireAt;
    std::wstring title;
    std::wstring body;
    int alarmIndex = 0;
    std::wstring key;   // seriesId + | + ISO(start) + | + idx
};
std::vector<AlarmDue> UpcomingAlarms(const Store& st, const DateTime& now, int withinMinutes);

// 生成规则的可读描述
std::wstring RecurDescription(const Event& e);