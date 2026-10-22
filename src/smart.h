// smart.h - 智能整理：课程识别 / 自动配色 / 规律重复合并
#pragma once
#include "model.h"
#include <string>
#include <vector>

struct SmartOptions {
    bool tagCourses = true;     // 给识别为课程的日程打「课程」标签
    bool autoColor = true;      // 没有颜色的课程自动配色
    bool mergeWeekly = true;    // 把有规律的每周重复日程合并成重复系列
    bool onlyUntagged = true;   // 只处理没有标签的日程（避免动用户手写数据）
    std::vector<std::string> onlyIds;   // 非空时只处理这些 id
};

struct SmartResult {
    int scanned = 0;
    int courseTagged = 0;
    int colored = 0;
    int seriesCreated = 0;
    int eventsMerged = 0;
    std::wstring summary;
};

// 标题是否像一门课
bool LooksLikeCourse(const std::wstring& title);
// 按课程名生成稳定颜色（来自内置暗色友好调色板）
std::wstring SmartCourseColor(const std::wstring& title);
// 执行智能整理（会修改 Store；调用前请自行 Snapshot 以便撤销）
SmartResult SmartOrganize(Store& st, const SmartOptions& opt);
