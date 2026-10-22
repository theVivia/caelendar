// quick.h - 自然语言快速添加 / 批量文本解析
#pragma once
#include "model.h"
#include <string>
#include <vector>
#include <map>

struct QuickResult {
    Event ev;
    bool ok = false;
    std::wstring error;
    std::wstring summary;
    std::map<std::string, std::wstring> tagNames;   // 新标签 id -> 名称
};

// 解析一行自然语言："明天 15:00-16:00 高数作业 #作业 !!" / "周三 8点 早八 #课程"
QuickResult ParseQuick(const std::wstring& text, const DateTime& baseDate, const Store* store = nullptr);

struct BulkParseResult {
    std::vector<Event> events;
    std::vector<std::wstring> errors;
    std::map<std::string, std::wstring> tagNames;
};
BulkParseResult ParseBulk(const std::wstring& text, const DateTime& baseDate, const Store* store = nullptr);
