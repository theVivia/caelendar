// cleanup.h - 批量清理：按日期范围 / 标签 / 颜色 / 类型 / 关键词 / 状态筛选删除
#pragma once
#include "model.h"
#include <set>
#include <string>
#include <vector>

enum CleanupTypeMode { CT_ALL = 0, CT_SINGLE = 1, CT_RECUR = 2 };
enum CleanupStatusMode { CS_ALL = 0, CS_DONE = 1, CS_UNDONE = 2 };
// delMode: 0 = 整个系列；1 = 仅删除日期范围内的实例
struct CleanupFilter {
    bool hasFrom = false, hasTo = false;
    DateTime from{}, to{};
    std::wstring keyword;
    std::set<std::string> tagIds;
    std::set<std::wstring> colors;   // 大写 hex；"#NONE" 表示“无自定义颜色”
    int typeMode = CT_ALL;
    int statusMode = CS_ALL;
    bool onlyAllDay = false;
    bool onlyWithAlarm = false;
    int delMode = 0;
};

struct CleanupStats {
    int series = 0;        // 匹配的日程（系列）数
    int occurrences = 0;   // 范围内会删除的实例数（单次按 1 计）
};

bool CleanupDateInRange(const DateTime& d, const CleanupFilter& f);
bool CleanupSeriesMatches(const Event& e, const CleanupFilter& f);
CleanupStats CleanupPreview(const Store& st, const CleanupFilter& f);
// 调用前请自行 Snapshot()；返回删除的数量（系列数或实例数）
int CleanupApply(Store& st, const CleanupFilter& f);
// 快速操作（调用前请自行 Snapshot()）
int CleanupDeleteAll(Store& st);
int CleanupDeleteDone(Store& st);
int CleanupDeleteImported(Store& st);
