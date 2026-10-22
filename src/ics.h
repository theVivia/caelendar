// ics.h - ICS 导入导出
#pragma once
#include "model.h"
#include <string>
#include <map>
#include <vector>

struct IcsParseResult {
    std::vector<Event> events;
    std::vector<std::wstring> warnings;
    std::wstring calendarName;
    std::map<std::string, std::wstring> tagNames;   // 标签 id -> 名称
    bool hasRange = false;
    DateTime minDate, maxDate;
};

// 解析 ICS 文本（暂存，不改动 Store）
IcsParseResult ParseIcs(const std::string& utf8);

struct IcsImportResult {
    int added = 0, updated = 0, skipped = 0;
    std::vector<std::string> addedIds;
    std::vector<std::string> updatedIds;
    std::vector<std::wstring> warnings;
};

// mode: 0=按 UID 合并（有则更新） 1=全部追加 2=跳过重复
IcsImportResult ImportIcs(Store& st, const std::string& utf8, int mode, const std::wstring& sourceName);

// 导出（ids 为空 = 全部）
std::string ExportIcs(const Store& st, const std::vector<std::string>& ids = {}, const std::wstring& calName = L"CAELENDAR");
