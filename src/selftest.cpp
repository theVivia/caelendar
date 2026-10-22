// selftest.cpp - 自测（--selftest）：不依赖 GUI，结果写 data/selftest.log
#include "util.h"
#include "json.h"
#include "model.h"
#include "recur.h"
#include "ics.h"
#include "quick.h"
#include "smart.h"
#include "cleanup.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

static int g_pass = 0, g_fail = 0;
static std::wstring g_log;

static void Log(const std::wstring& s) { g_log += s + L"\r\n"; }

static void Check(bool cond, const std::wstring& name, const std::wstring& detail = L"") {
    if (cond) {
        ++g_pass;
        Log(L"[PASS] " + name);
    } else {
        ++g_fail;
        Log(L"[FAIL] " + name + (detail.empty() ? L"" : L"  ==> " + detail));
    }
}

static std::wstring DTListStr(const std::vector<DateTime>& v) {
    std::wstring s;
    for (size_t i = 0; i < v.size(); ++i) { if (i) s += L","; s += ISODT(v[i]); }
    return s;
}

static bool SameList(const std::vector<DateTime>& a, const std::vector<DateTime>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (CompareDT(a[i], b[i]) != 0) return false;
    return true;
}

static bool PastUntilT(const Event& e, const DateTime& t) {
    if (!e.recur.hasUntil) return false;
    DateTime u = e.recur.until;
    if (u.h == 0 && u.mi == 0) u = AddMinutesDT(u, 24 * 60 - 1);
    return CompareDT(t, u) > 0;
}
static int NthWd(int y, int m, int weekday, int n) {
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

// 独立实现：逐日扫描（与 recur.cpp 的周期跳跃算法不同）
static std::vector<DateTime> BruteForceStarts(const Event& e, const DateTime& from, const DateTime& to) {
    std::vector<DateTime> out;
    const Recur& r = e.recur;
    if (r.freq == RF_NONE) {
        DateTime en = e.DisplayEnd();
        if (CompareDT(e.start, to) < 0 && CompareDT(en, from) > 0) out.push_back(e.start);
        return out;
    }
    int produced = 0;
    DateTime day = StartOfDay(e.start);
    DateTime toDay = StartOfDay(to);
    for (int i = 0; i < 40000; ++i, day = AddDaysDT(day, 1)) {
        if (CompareDT(day, toDay) > 0) break;
        int diff = DaySerial(day.y, day.mo, day.d) - DaySerial(e.start.y, e.start.mo, e.start.d);
        if (diff < 0) continue;
        bool match = false;
        if (r.freq == RF_DAILY) {
            match = (diff % std::max(1, r.interval) == 0);
        } else if (r.freq == RF_WEEKLY) {
            std::vector<int> days = r.byDay;
            if (days.empty()) days.push_back(DayOfWeek(e.start.y, e.start.mo, e.start.d));
            bool dayOk = std::find(days.begin(), days.end(), DayOfWeek(day.y, day.mo, day.d)) != days.end();
            DateTime w0 = StartOfWeek(e.start, r.wkst);
            int weekDiff = (DaySerial(day.y, day.mo, day.d) - DaySerial(w0.y, w0.mo, w0.d)) / 7;
            match = dayOk && weekDiff >= 0 && (weekDiff % std::max(1, r.interval) == 0);
        } else if (r.freq == RF_MONTHLY) {
            int monthDiff = (day.y * 12 + day.mo) - (e.start.y * 12 + e.start.mo);
            if (monthDiff < 0 || monthDiff % std::max(1, r.interval) != 0) continue;
            if (r.nthWeekday != 0 && !r.byDay.empty()) {
                int target = NthWd(day.y, day.mo, r.byDay[0], r.nthWeekday);
                match = (day.d == target);
            } else {
                int baseDay = r.byMonthDay > 0 ? r.byMonthDay : e.start.d;
                if (baseDay < 0) baseDay = DaysInMonth(day.y, day.mo) + 1 + baseDay;
                match = (day.d == baseDay);
            }
        } else if (r.freq == RF_YEARLY) {
            int yearDiff = day.y - e.start.y;
            if (yearDiff < 0 || yearDiff % std::max(1, r.interval) != 0) continue;
            int month = r.byMonth > 0 ? r.byMonth : e.start.mo;
            if (day.mo != month) continue;
            if (r.nthWeekday != 0 && !r.byDay.empty()) {
                int target = NthWd(day.y, day.mo, r.byDay[0], r.nthWeekday);
                match = (day.d == target);
            } else {
                int baseDay = r.byMonthDay > 0 ? r.byMonthDay : e.start.d;
                if (baseDay < 0) baseDay = DaysInMonth(day.y, month) + 1 + baseDay;
                match = (day.d == baseDay);
            }
        }
        if (!match) continue;
        DateTime t = MakeDT(day.y, day.mo, day.d, e.start.h, e.start.mi);
        if (CompareDT(t, e.start) < 0) continue;
        if (r.count > 0 && produced >= r.count) break;
        ++produced;
        if (PastUntilT(e, t)) break;
        if (CompareDT(t, from) >= 0 && CompareDT(t, to) <= 0) out.push_back(t);
    }
    return out;
}

// ---------------- 日期 ----------------
static void TestDate() {
    Log(L"## 日期时间");
    Check(DaysInMonth(2026, 2) == 28, L"2026-02 有 28 天");
    Check(DaysInMonth(2024, 2) == 29, L"2024-02 有 29 天");
    Check(DaysInMonth(2000, 2) == 29, L"2000-02 有 29 天");
    Check(DaysInMonth(1900, 2) == 28, L"1900-02 有 28 天（世纪年负控）");
    Check(DayOfWeek(2026, 10, 17) == 6, L"2026-10-17 是周六");
    Check(DayOfWeek(2026, 10, 1) == 4, L"2026-10-01 是周四");
    Check(DayOfWeek(1970, 1, 1) == 4, L"1970-01-01 是周四（epoch）");
    int s = DaySerial(2026, 10, 17);
    Date d = FromSerial(s);
    Check(d.y == 2026 && d.m == 10 && d.d == 17, L"DaySerial <-> FromSerial 往返");
    Check(DaySerial(2000, 3, 1) - DaySerial(2000, 2, 28) == 2, L"2000-02-29 存在");
    DateTime t = AddMonthsDT(MakeDT(2026, 1, 31), 1);
    Check(t.y == 2026 && t.mo == 2 && t.d == 28, L"1/31 + 1月 = 2/28（月末夹取）");
    t = AddMonthsDT(MakeDT(2024, 1, 31), 1);
    Check(t.d == 29, L"2024 1/31 + 1月 = 2/29");
    t = AddMinutesDT(MakeDT(2026, 10, 17, 23, 50), 20);
    Check(t.d == 18 && t.h == 0 && t.mi == 10, L"跨日加分钟");
    {
        DateTime tn = MakeDT(2026, 10, 18, 10, 0);
        Check(HumanFutureCN(MakeDT(2026, 10, 18, 10, 45), tn) == L"45 分钟后", L"时间差：45 分钟");
        Check(HumanFutureCN(MakeDT(2026, 10, 18, 12, 30), tn) == L"2 小时 30 分钟后", L"时间差：2 小时 30 分");
        Check(HumanFutureCN(MakeDT(2026, 10, 19, 9, 0), tn) == L"明天 09:00", L"时间差：明天");
        Check(HumanFutureCN(MakeDT(2026, 10, 20, 9, 0), tn) == L"后天 09:00", L"时间差：后天");
        Check(HumanFutureCN(MakeDT(2026, 10, 25, 9, 0), tn) == L"10月25日 09:00", L"时间差：跨日日期");
        Check(HumanRemainCN(MakeDT(2026, 10, 18, 12, 0), MakeDT(2026, 10, 18, 8, 45)) == L"3 小时 15 分钟", L"时间差：剩余");
        Check(HumanPastCN(MakeDT(2026, 10, 18, 9, 30), tn) == L"30 分钟前", L"时间差：过去");
    }
    t = AddMinutesDT(MakeDT(2026, 10, 17, 0, 10), -20);
    Check(t.d == 16 && t.h == 23 && t.mi == 50, L"跨日减分钟");
    Check(WeekOfYear(MakeDT(2026, 10, 17)) == 42, L"ISO 周数：2026-10-17 是第 42 周");
    Check(WeekOfYear(MakeDT(2026, 1, 1)) == 1, L"ISO 周数：2026-01-01 是第 1 周");
    Check(SameDay(MakeDT(2026, 10, 17, 1, 2), MakeDT(2026, 10, 17, 23, 59)), L"SameDay");
    auto p = ParseISODT(L"2026-10-17T08:30");
    Check(p.y == 2026 && p.mo == 10 && p.d == 17 && p.h == 8 && p.mi == 30, L"ParseISODT");
    bool ok = true;
    ParseISODT(L"hello", &ok);
    Check(!ok, L"ParseISODT 坏输入负控");
}

// ---------------- JSON ----------------
static void TestJson() {
    Log(L"## JSON");
    std::string src = u8"{\"a\":1,\"b\":[true,false,null,\"x\\u4e2d\\n\"],\"c\":{\"k\":-2.5}}";
    std::wstring err;
    JValue v = JsonParse(src, &err);
    Check(v.IsObj(), L"JSON 解析对象", err);
    Check(v.Get(L"a").AsInt() == 1, L"JSON 整数");
    Check(v.Get(L"b").Size() == 4, L"JSON 数组长度");
    Check(v.Get(L"b").arr[3].AsStr() == L"x中\n", L"JSON 转义与 \\u 解码");
    Check(v.Get(L"c").Get(L"k").AsNum() == -2.5, L"JSON 负数");
    // 往返
    std::string dumped = JsonDump(v, true);
    std::wstring err2;
    JValue v2 = JsonParse(dumped, &err2);
    Check(v2.Get(L"b").arr[3].AsStr() == L"x中\n", L"JSON 往返保持内容", err2);
    // 负控
    std::wstring e1;
    JValue bad = JsonParse("{\"a\":}", &e1);
    Check(bad.IsNull() && !e1.empty(), L"JSON 坏输入返回 null + 错误");
    std::wstring e2;
    JsonParse("{} garbage", &e2);
    Check(!e2.empty(), L"JSON 尾部垃圾被拒绝");
    // 深层嵌套
    std::string deep = "{\"a\":{\"b\":{\"c\":[1,2,{\"d\":\"ok\"}]}}}";
    std::wstring e3;
    JValue dv = JsonParse(deep, &e3);
    Check(dv.Get(L"a").Get(L"b").Get(L"c").arr[2].Get(L"d").AsStr() == L"ok", L"JSON 深层嵌套", e3);
}

// ---------------- 重复规则 ----------------
static void TestRecur() {
    Log(L"## 重复规则（与独立逐日扫描实现交叉校验）");
    struct Case { const wchar_t* name; Event e; DateTime from, to; };
    std::vector<Case> cases;
    {
        Event e; e.start = MakeDT(2026, 10, 5, 8, 0); e.end = MakeDT(2026, 10, 5, 9, 40);
        e.recur.freq = RF_WEEKLY; e.recur.byDay = { 1, 3, 5 }; e.recur.until = MakeDT(2026, 11, 30); e.recur.hasUntil = true;
        cases.push_back({ L"每周一三五 至 11/30", e, MakeDT(2026, 10, 1), MakeDT(2026, 12, 31) });
    }
    {
        Event e; e.start = MakeDT(2026, 9, 1, 10, 0); e.end = MakeDT(2026, 9, 1, 11, 0);
        e.recur.freq = RF_WEEKLY; e.recur.interval = 2; e.recur.byDay = { 2, 4 };
        cases.push_back({ L"隔周周二周四（无终点）", e, MakeDT(2026, 10, 1), MakeDT(2026, 12, 31) });
    }
    {
        Event e; e.start = MakeDT(2026, 1, 7, 19, 0); e.end = MakeDT(2026, 1, 7, 20, 0);
        e.recur.freq = RF_WEEKLY; e.recur.count = 10;
        cases.push_back({ L"每周（默认开始日）共 10 次", e, MakeDT(2026, 1, 1), MakeDT(2026, 4, 1) });
    }
    {
        Event e; e.start = MakeDT(2026, 1, 30, 8, 0); e.end = MakeDT(2026, 1, 30, 9, 0);
        e.recur.freq = RF_DAILY; e.recur.interval = 3; e.recur.count = 30;
        cases.push_back({ L"每 3 天共 30 次", e, MakeDT(2026, 1, 1), MakeDT(2026, 5, 1) });
    }
    {
        Event e; e.start = MakeDT(2026, 1, 31, 9, 0); e.end = MakeDT(2026, 1, 31, 10, 0);
        e.recur.freq = RF_MONTHLY; e.recur.byMonthDay = 31;
        cases.push_back({ L"每月 31 号（小月跳过）", e, MakeDT(2026, 1, 1), MakeDT(2026, 12, 31) });
    }
    {
        Event e; e.start = MakeDT(2026, 1, 13, 14, 0); e.end = MakeDT(2026, 1, 13, 15, 0);
        e.recur.freq = RF_MONTHLY; e.recur.nthWeekday = 2; e.recur.byDay = { 2 };
        cases.push_back({ L"每月第 2 个周二", e, MakeDT(2026, 1, 1), MakeDT(2026, 8, 1) });
    }
    {
        Event e; e.start = MakeDT(2026, 1, 15, 12, 0); e.end = MakeDT(2026, 1, 15, 13, 0);
        e.recur.freq = RF_MONTHLY; e.recur.interval = 2; e.recur.count = 5;
        cases.push_back({ L"隔月 15 号共 5 次", e, MakeDT(2026, 1, 1), MakeDT(2027, 1, 1) });
    }
    {
        Event e; e.start = MakeDT(2026, 3, 1, 8, 0); e.end = MakeDT(2026, 3, 1, 9, 0);
        e.recur.freq = RF_YEARLY; e.recur.byMonth = 3; e.recur.byMonthDay = 1; e.recur.count = 3;
        cases.push_back({ L"每年 3 月 1 日共 3 次", e, MakeDT(2026, 1, 1), MakeDT(2030, 1, 1) });
    }
    {
        Event e; e.start = MakeDT(2024, 2, 29, 9, 0); e.end = MakeDT(2024, 2, 29, 10, 0);
        e.recur.freq = RF_YEARLY; e.recur.count = 3;
        cases.push_back({ L"每 2/29（非闰年跳过）", e, MakeDT(2024, 1, 1), MakeDT(2036, 1, 1) });
    }
    {
        Event e; e.start = MakeDT(2020, 1, 6, 10, 0); e.end = MakeDT(2020, 1, 6, 11, 0);
        e.recur.freq = RF_WEEKLY; e.recur.byDay = { 1 };
        cases.push_back({ L"很久以前开始的周会（远距离范围）", e, MakeDT(2026, 10, 1), MakeDT(2026, 12, 31) });
    }
    for (auto& c : cases) {
        auto fast = RecurStartList(c.e, c.from, c.to);
        auto slow = BruteForceStarts(c.e, c.from, c.to);
        Check(SameList(fast, slow), std::wstring(L"交叉校验: ") + c.name,
              L"引擎=" + DTListStr(fast) + L" 暴力=" + DTListStr(slow));
    }
    // 单次事件在范围内
    {
        Event e; e.start = MakeDT(2026, 10, 17, 15, 0); e.end = MakeDT(2026, 10, 17, 17, 0);
        auto v = RecurStartList(e, MakeDT(2026, 10, 1), MakeDT(2026, 10, 31));
        Check(v.size() == 1, L"单次事件展开 1 条");
        auto v2 = RecurStartList(e, MakeDT(2026, 11, 1), MakeDT(2026, 11, 30));
        Check(v2.empty(), L"单次事件范围外为 0（负控）");
    }
}

// ---------------- 覆盖/例外/拆分 ----------------
static void TestOccurrenceOps() {
    Log(L"## 重复实例：修改/删除/拆分");
    Store st;
    st.tags = DefaultTags();
    st.settings.seededSample = true;
    Event e;
    e.title = L"高数课";
    e.start = MakeDT(2026, 10, 5, 8, 0);    // 周一
    e.end = MakeDT(2026, 10, 5, 9, 40);
    e.recur.freq = RF_WEEKLY;
    e.recur.byDay = { 1 };
    e.recur.until = MakeDT(2026, 12, 28);
    e.recur.hasUntil = true;
    std::string id = st.AddEvent(e);
    auto oct = ExpandOccurrences(st, MakeDT(2026, 10, 1), MakeDT(2026, 11, 1));
    Check(oct.size() == 4, L"系列 10 月有 4 次", Fmt(L"实际 %d", (int)oct.size()));

    // 仅改这一次：换到 10/12 10:00，并改标题
    Event eff = *st.Find(id);
    eff.title = L"高数课（补课）";
    eff.start = MakeDT(2026, 10, 12, 10, 0);
    eff.end = MakeDT(2026, 10, 12, 11, 40);
    st.UpdateOccurrence(id, MakeDT(2026, 10, 12, 8, 0), eff, 0);
    auto after = ExpandOccurrences(st, MakeDT(2026, 10, 1), MakeDT(2026, 11, 1));
    Check(after.size() == 4, L"覆盖后仍有 4 次", Fmt(L"实际 %d", (int)after.size()));
    bool foundMoved = false, foundOld = false;
    for (auto& o : after) {
        if (o.ev.title == L"高数课（补课）" && o.start.h == 10 && o.start.d == 12) foundMoved = true;
        if (o.start.d == 12 && o.start.h == 8) foundOld = true;
    }
    Check(foundMoved, L"覆盖实例出现在新时间 10/12 10:00");
    Check(!foundOld, L"原 10/12 08:00 被抑制（负控）");

    // 仅删这一次
    st.DeleteOccurrence(id, MakeDT(2026, 10, 19, 8, 0), 0);
    after = ExpandOccurrences(st, MakeDT(2026, 10, 1), MakeDT(2026, 11, 1));
    Check(after.size() == 3, L"删除单次后剩 3 次", Fmt(L"实际 %d", (int)after.size()));

    // 此后所有：11/2 起改为 10:00 并改名
    Event eff2 = *st.Find(id);
    eff2.title = L"高数课（新时间）";
    eff2.start = MakeDT(2026, 11, 2, 10, 0);
    eff2.end = MakeDT(2026, 11, 2, 11, 40);
    st.UpdateOccurrence(id, MakeDT(2026, 11, 2, 8, 0), eff2, 1);
    auto nov = ExpandOccurrences(st, MakeDT(2026, 11, 1), MakeDT(2026, 12, 1));
    int newCnt = 0, oldCnt = 0;
    for (auto& o : nov) {
        if (o.ev.title == L"高数课（新时间）" && o.start.h == 10) ++newCnt;
        else if (o.ev.title == L"高数课" && o.start.h == 8) ++oldCnt;
    }
    Check(newCnt == 5, L"拆分后新系列 11 月 5 次", Fmt(L"实际 %d", newCnt));
    Check(oldCnt == 0, L"旧系列 11 月不再出现", Fmt(L"实际 %d", oldCnt));
    auto oct2 = ExpandOccurrences(st, MakeDT(2026, 10, 1), MakeDT(2026, 11, 1));
    Check(oct2.size() == 3, L"拆分后旧系列 10 月仍 3 次", Fmt(L"实际 %d", (int)oct2.size()));
}

// ---------------- Store ----------------
static void TestStore() {
    Log(L"## 存储 / 撤销 / 标签 / 喝水");
    Store st;
    st.tags = DefaultTags();
    st.settings.seededSample = true;
    Event e;
    e.title = L"测试事件";
    e.start = MakeDT(2026, 10, 17, 9, 0);
    e.end = MakeDT(2026, 10, 17, 10, 0);
    std::string id = st.AddEvent(e);
    Check(st.events.size() == 1 && !id.empty(), L"添加事件");
    st.Snapshot();
    Event* p = st.Find(id);
    p->title = L"改名后";
    st.Touch();
    // 注意：不在自测里调用 Save()，避免污染真实数据
    Check(st.Undo(), L"撤销可用");
    const Event* q = st.Find(id);
    Check(q && q->title == L"测试事件", L"撤销恢复旧标题", q ? q->title : L"<空>");
    Check(st.Redo(), L"重做可用");
    q = st.Find(id);
    Check(q && q->title == L"改名后", L"重做恢复新标题", q ? q->title : L"<空>");

    // 序列化往返
    std::string js = st.ToJsonStr();
    Store st2;
    std::wstring err;
    bool ok = st2.FromJsonStr(js, &err);
    Check(ok, L"Store JSON 往返解析", err);
    Check(st2.events.size() == st.events.size(), L"Store 往返事件数一致");
    Check(!st2.events.empty() && st2.events[0].title == L"改名后", L"Store 往返标题一致");

    // 标签
    std::string tid = st.EnsureTag(L"自定义标签", L"#123456");
    Check(st.FindTag(tid) != nullptr && st.TagName(tid) == L"自定义标签", L"新建标签");
    std::string tid2 = st.EnsureTag(L"自定义标签");
    Check(tid == tid2, L"同名标签去重");
    st.events[0].tags.push_back(tid);
    st.DeleteTag(tid);
    Check(st.FindTag(tid) == nullptr && !st.events[0].HasTag(tid), L"删除标签并从事件移除");

    // 喝水
    int before = st.WaterToday();
    st.AddWaterCup();
    Check(st.WaterToday() == before + 1, L"喝水 +1 杯");
    st.SetWater(ISODate(Today()), 0);
    Check(st.WaterToday() == 0, L"喝水可以清零（按错了能改）");
    st.SetWater(ISODate(Today()), -5);
    Check(st.WaterToday() == 0, L"喝水设为负数自动夹到 0（负控）");
    st.SetWater(ISODate(Today()), 3);
    Check(st.WaterToday() == 3, L"喝水可以直接设为指定杯数");
    // 批量清理：范围筛选 / 只删范围内实例 / 标签 / 颜色 / 快速操作
    {
        Store cl;
        cl.tags = DefaultTags();
        Event ea;
        ea.title = L"高数作业";
        ea.start = MakeDT(2026, 10, 10, 20, 0);
        ea.end = MakeDT(2026, 10, 10, 21, 0);
        ea.tags.push_back("homework");
        ea.color = L"#5EA8FF";
        ea.done = true;
        std::string idA = cl.AddEvent(ea);
        Event eb;
        eb.title = L"期中考试";
        eb.start = MakeDT(2026, 10, 12, 9, 0);
        eb.end = MakeDT(2026, 10, 12, 11, 0);
        eb.tags.push_back("exam");
        std::string idB = cl.AddEvent(eb);
        Event ec;
        ec.title = L"每周例会";
        ec.start = MakeDT(2026, 10, 5, 8, 0);
        ec.end = MakeDT(2026, 10, 5, 9, 0);
        ec.recur.freq = RF_WEEKLY;
        ec.recur.interval = 1;
        ec.recur.byDay = { 1 };
        ec.recur.hasUntil = true;
        ec.recur.until = MakeDT(2026, 10, 19, 8, 0);
        std::string idC = cl.AddEvent(ec);
        Event ed;
        ed.title = L"买牛奶";
        ed.start = MakeDT(2026, 11, 20, 19, 0);
        ed.end = MakeDT(2026, 11, 20, 19, 20);
        cl.AddEvent(ed);

        CleanupFilter f;
        f.hasFrom = true; f.from = MakeDT(2026, 10, 1);
        f.hasTo = true; f.to = MakeDT(2026, 10, 31);
        f.delMode = 1;
        CleanupStats s = CleanupPreview(cl, f);
        Check(s.series == 3 && s.occurrences == 5, L"清理预览：10 月范围 3 个日程 / 5 个实例",
              Fmt(L"%d/%d", s.series, s.occurrences));
        int removed = CleanupApply(cl, f);
        Event* series = cl.Find(idC);
        Check(removed == 5 && (int)cl.events.size() == 2, L"按范围删实例：删 5 次，系列本身保留",
              Fmt(L"删除=%d 剩余=%d", removed, (int)cl.events.size()));
        Check(series && series->recur.exDates.size() == 3, L"仅删范围内实例：系列写入 3 个例外日",
              Fmt(L"%d", series ? (int)series->recur.exDates.size() : 0));
        Check(cl.Find(idA) == nullptr && cl.Find(idB) == nullptr, L"范围内的单次日程被删除");
        Check(cl.Find(idC) != nullptr, L"范围外的重复系列没有被整体删除（负控）");

        // 标签 / 颜色 / 关键词 / 快速操作
        Store cl2;
        cl2.tags = DefaultTags();
        Event t1; t1.title = L"课程A"; t1.start = MakeDT(2026, 10, 1, 8, 0); t1.end = MakeDT(2026, 10, 1, 9, 0);
        t1.tags.push_back("course"); t1.color = L"#5EA8FF";
        cl2.AddEvent(t1);
        Event t2; t2.title = L"课程B"; t2.start = MakeDT(2026, 10, 2, 8, 0); t2.end = MakeDT(2026, 10, 2, 9, 0);
        t2.tags.push_back("course");
        cl2.AddEvent(t2);
        CleanupFilter tf;
        tf.tagIds.insert("course");
        Check(CleanupPreview(cl2, tf).series == 2, L"按标签筛选命中 2 个日程");
        CleanupFilter cf;
        cf.colors.insert(L"#5EA8FF");
        Check(CleanupPreview(cl2, cf).series == 1, L"按颜色筛选只命中 1 个日程");
        CleanupFilter nf;
        nf.keyword = L"不存在的关键词";
        int n0 = CleanupApply(cl2, nf);
        Check(n0 == 0 && (int)cl2.events.size() == 2, L"无匹配时删除 0 条（负控）");
        Store cl3;
        cl3.tags = DefaultTags();
        for (int i = 0; i < 3; ++i) {
            Event e;
            e.title = Fmt(L"任务%d", i);
            e.start = MakeDT(2026, 10, 10 + i, 9, 0);
            e.end = MakeDT(2026, 10, 10 + i, 10, 0);
            e.done = (i != 0);
            cl3.AddEvent(e);
        }
        int dn = CleanupDeleteDone(cl3);
        Check(dn == 2, L"快速删除已完成：删 2 条", Fmt(L"%d", dn));
        Check(CleanupDeleteAll(cl3) == 1 && cl3.events.empty(), L"快速清空全部");
    }
    // 只完成某一次：重复系列不改整个系列
    Event rec;
    rec.title = L"高等数学";
    rec.start = MakeDT(2026, 9, 7, 8, 0);
    rec.end = MakeDT(2026, 9, 7, 9, 40);
    rec.recur.freq = RF_WEEKLY;
    rec.recur.interval = 1;
    rec.recur.byDay = { 1 };
    rec.recur.hasUntil = true;
    rec.recur.until = MakeDT(2026, 10, 5, 8, 0);
    std::string rid = st.AddEvent(rec);
    DateTime targetOcc = MakeDT(2026, 9, 14, 8, 0);
    bool doneOk = st.SetOccurrenceDone(rid, targetOcc, targetOcc, MakeDT(2026, 9, 14, 9, 40), true);
    Check(doneOk, L"按次完成接口返回成功");
    const Event* back = st.Find(rid);
    Check(back && back->overrides.size() == 1 && back->overrides[0].done, L"重复日程只完成本次（生成例外覆盖）");
    auto occsDone = ExpandOccurrences(st, MakeDT(2026, 9, 7, 0, 0), MakeDT(2026, 9, 22, 0, 0));
    int doneCnt = 0, notDoneCnt = 0;
    for (auto& o : occsDone) {
        if (SameDay(o.start, MakeDT(2026, 9, 14, 0, 0)) && o.ev.done) ++doneCnt;
        if (SameDay(o.start, MakeDT(2026, 9, 21, 0, 0)) && !o.ev.done) ++notDoneCnt;
    }
    Check(doneCnt == 1 && notDoneCnt == 1, L"只影响目标那一次，后续仍未完成（负控）");
    bool undoneOk = st.SetOccurrenceDone(rid, targetOcc, targetOcc, MakeDT(2026, 9, 14, 9, 40), false);
    auto occsUndo = ExpandOccurrences(st, MakeDT(2026, 9, 7, 0, 0), MakeDT(2026, 9, 22, 0, 0));
    int doneAfterUndo = 0;
    for (auto& o : occsUndo) if (SameDay(o.start, MakeDT(2026, 9, 14, 0, 0)) && o.ev.done) ++doneAfterUndo;
    Check(undoneOk && doneAfterUndo == 0, L"按次完成可以撤销回未完成（右键菜单修复）");
    // 调休：把一次重复课迁到另一天，其他次不受影响
    auto occsMove = ExpandOccurrences(st, MakeDT(2026, 9, 20, 0, 0), MakeDT(2026, 9, 30, 0, 0));
    const Occurrence* moveOcc = nullptr;
    for (auto& o : occsMove) if (SameDay(o.start, MakeDT(2026, 9, 21, 0, 0))) moveOcc = &o;
    bool movedOk = false;
    if (moveOcc) {
        movedOk = st.RescheduleOccurrence(rid, moveOcc->origStart, moveOcc->ev, moveOcc->start, moveOcc->end, Date{ 2026, 9, 23 });
    }
    auto occsAfterMove = ExpandOccurrences(st, MakeDT(2026, 9, 20, 0, 0), MakeDT(2026, 10, 1, 0, 0));
    bool on23 = false, on21 = false, on28 = false;
    for (auto& o : occsAfterMove) {
        if (SameDay(o.start, MakeDT(2026, 9, 23, 0, 0))) on23 = true;
        if (SameDay(o.start, MakeDT(2026, 9, 21, 0, 0))) on21 = true;
        if (SameDay(o.start, MakeDT(2026, 9, 28, 0, 0))) on28 = true;
    }
    Check(movedOk && on23 && !on21 && on28, L"调休：单次迁移到 9/23，原 9/21 消失、后续仍在（负控）",
          Fmt(L"moved=%d on23=%d on21=%d on28=%d", movedOk ? 1 : 0, on23 ? 1 : 0, on21 ? 1 : 0, on28 ? 1 : 0));
    Event singleMove; singleMove.title = L"单次日程"; singleMove.start = MakeDT(2026, 9, 1, 14, 0); singleMove.end = MakeDT(2026, 9, 1, 15, 0);
    std::string smid = st.AddEvent(singleMove);
    bool smMoved = st.RescheduleOccurrence(smid, singleMove.start, singleMove, singleMove.start, singleMove.end, Date{ 2026, 9, 5 });
    const Event* sm = st.Find(smid);
    Check(smMoved && sm && sm->start.d == 5 && sm->start.h == 14 && sm->end.d == 5, L"单次日程调休保留时间与时长");
    // 挂件设置 JSON 往返
    st.settings.widget.mode = L"list";
    st.settings.widget.skin = L"neon";
    st.settings.widget.scale = 130;
    st.settings.widget.showDate = false;
    st.settings.widget.showToday = false;
    Store stw;
    std::wstring errw;
    Check(stw.FromJsonStr(st.ToJsonStr(), &errw), L"挂件设置 JSON 往返解析", errw);
    Check(stw.settings.widget.mode == L"list" && stw.settings.widget.skin == L"neon" &&
          stw.settings.widget.scale == 130 && !stw.settings.widget.showDate && !stw.settings.widget.showToday,
          L"挂件布局 / 皮肤 / 大小 / 显示项保留");

    // 坏 JSON 恢复
    Store st3;
    std::wstring err3;
    Check(!st3.FromJsonStr("{bad json", &err3) && !err3.empty(), L"坏 JSON 被拒绝（负控）");

    // 字体 / 后台设置往返
    st.settings.fontUI = L"Microsoft YaHei UI";
    st.settings.fontNum = L"Consolas";
    st.settings.fontScale = 115;
    st.settings.fontParts[0].size = 120;
    st.settings.fontParts[2].family = L"KaiTi";
    st.settings.closeToTray = false;
    st.settings.autoStart = true;
    std::string js2 = st.ToJsonStr();
    Store st4;
    std::wstring err4;
    Check(st4.FromJsonStr(js2, &err4), L"字体设置 JSON 往返解析", err4);
    Check(st4.settings.fontUI == L"Microsoft YaHei UI" && st4.settings.fontNum == L"Consolas" &&
          st4.settings.fontScale == 115, L"字体族与全局字号保留");
    Check(st4.settings.fontParts[0].size == 120 && st4.settings.fontParts[2].family == L"KaiTi",
          L"分区字号 / 分区字体保留");
    Check(!st4.settings.closeToTray && st4.settings.autoStart, L"托盘 / 开机自启设置保留");
}

// ---------------- 渲染层数据 ----------------
static void TestExpand() {
    Log(L"## 渲染层展开");
    Store st;
    st.tags = DefaultTags();
    st.settings.seededSample = true;
    Event allday;
    allday.title = L"跨天活动";
    allday.allDay = true;
    allday.start = MakeDT(2026, 10, 20, 0, 0);
    allday.end = MakeDT(2026, 10, 22, 0, 0);   // 排他：20、21 两天
    st.AddEvent(allday);
    auto occ = ExpandOccurrences(st, MakeDT(2026, 10, 19), MakeDT(2026, 10, 24));
    Check(occ.size() == 1, L"全天跨天事件展开 1 次");
    if (!occ.empty()) Check(occ[0].ev.allDay && occ[0].start.d == 20, L"全天事件开始日正确");

    Event timed;
    timed.title = L"会议";
    timed.start = MakeDT(2026, 10, 20, 23, 0);
    timed.end = MakeDT(2026, 10, 21, 1, 0);
    st.AddEvent(timed);
    auto occ2 = ExpandOccurrences(st, MakeDT(2026, 10, 21), MakeDT(2026, 10, 22));
    bool found = false;
    for (auto& o : occ2) if (o.ev.title == L"会议") found = true;
    Check(found, L"跨夜事件在次日范围内可见");
    auto occ3 = ExpandOccurrences(st, MakeDT(2026, 9, 1), MakeDT(2026, 10, 19));
    bool foundEarly = false;
    for (auto& o : occ3) if (o.ev.title == L"会议") foundEarly = true;
    Check(!foundEarly, L"范围外不返回（负控）");

    // 提醒
    Event rem;
    rem.title = L"提醒测试";
    rem.start = AddMinutesDT(NowDT(), 30);
    rem.end = AddMinutesDT(rem.start, 60);
    rem.alarms.push_back(Alarm{ -10, true, L"" });
    st.AddEvent(rem);
    auto dues = UpcomingAlarms(st, NowDT(), 120);
    bool alarmFound = false;
    for (auto& a : dues) if (a.title == L"提醒测试") alarmFound = true;
    Check(alarmFound, L"提前 10 分钟提醒被计算出来");
    // 负控：2 小时后的事件不应出现
    Event farEv;
    farEv.title = L"很久以后";
    farEv.start = AddMinutesDT(NowDT(), 600);
    farEv.end = AddMinutesDT(farEv.start, 60);
    farEv.alarms.push_back(Alarm{ 0, true, L"" });
    st.AddEvent(farEv);
    auto dues2 = UpcomingAlarms(st, NowDT(), 120);
    bool farEvFound = false;
    for (auto& a : dues2) if (a.title == L"很久以后") farEvFound = true;
    Check(!farEvFound, L"窗口外提醒不出现（负控）");
}

static void TestIcsDedupe() {
    Log(L"## ICS 批量导入去重（完全重复跳过）");
    const std::string ics =
        "BEGIN:VCALENDAR\nVERSION:2.0\n"
        "BEGIN:VEVENT\nUID:dup-a\nDTSTART:20261102T080000\nDTEND:20261102T094000\nSUMMARY:高等数学\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:dup-b\nDTSTART:20261102T080000\nDTEND:20261102T094000\nSUMMARY:高等数学\nEND:VEVENT\n"
        "End:VCALENDAR\n";
    Store st;
    st.tags = DefaultTags();
    IcsImportResult r1 = ImportIcs(st, ics, 2, L"测试导入");
    Check(r1.added == 1 && r1.skipped == 1, L"同批次内完全重复的日程只导入 1 条",
          Fmt(L"added=%d skipped=%d", r1.added, r1.skipped));
    IcsImportResult r2 = ImportIcs(st, ics, 2, L"测试导入");
    Check(r2.added == 0 && r2.skipped == 2, L"再次导入同一批：全部跳过，不产生重复",
          Fmt(L"added=%d skipped=%d", r2.added, r2.skipped));
    Check((int)st.events.size() == 1, L"库中始终只有 1 条，无重复");
}
static void TestColor() {
    Log(L"## 颜色转换（HEX / RGB / CMYK）");
    int r = 0, g = 0, b = 0;
    Check(HexToRgb(L"#6EA8FF", r, g, b) && r == 110 && g == 168 && b == 255, L"HEX → RGB：\"#6EA8FF\" = 110,168,255");
    Check(RgbToHex(110, 168, 255) == L"#6EA8FF", L"RGB → HEX 往返");
    Check(RgbToHex(0, 0, 0) == L"#000000" && RgbToHex(255, 255, 255) == L"#FFFFFF", L"黑/白边界");
    Check(!HexToRgb(L"#GGGGGG", r, g, b), L"坏 HEX 被拒绝（负控）");
    int c = 0, m = 0, y = 0, k = 0;
    RgbToCmyk(110, 168, 255, c, m, y, k);
    int rr = 0, gg = 0, bb = 0;
    CmykToRgb(c, m, y, k, rr, gg, bb);
    Check(std::abs(rr - 110) <= 3 && std::abs(gg - 168) <= 3 && std::abs(bb - 255) <= 3,
          L"RGB → CMYK → RGB 误差 ≤3", Fmt(L"%d,%d,%d", rr, gg, bb));
}
static void TestSmart() {
    Log(L"## 智能整理：课程识别 / 自动配色 / 合并重复");
    Check(LooksLikeCourse(L"高等数学A（上）"), L"识别「高等数学A（上）」为课程");
    Check(!LooksLikeCourse(L"买牛奶"), L"「买牛奶」不被误判为课程（负控）");
    std::wstring c1 = SmartCourseColor(L"高等数学A（上）");
    std::wstring c2 = SmartCourseColor(L"高等数学A（上）");
    Check(c1 == c2 && c1.size() == 7 && c1[0] == L'#', L"课程颜色稳定且为 #RRGGBB", c1);

    Store st;
    st.tags = DefaultTags();
    DateTime base = MakeDT(2026, 9, 7, 8, 0);   // 周一
    int weeks[5] = { 0, 1, 2, 4, 5 };           // 第 3 周缺勤 → 例外日
    for (int i = 0; i < 5; ++i) {
        Event e;
        e.title = L"高等数学A（上）";
        e.start = AddDaysDT(base, weeks[i] * 7);
        e.end = AddMinutesDT(e.start, 100);
        st.AddEvent(e);
    }
    for (int i = 0; i < 3; ++i) {
        Event e;
        e.title = L"大学英语（一）";
        e.start = AddDaysDT(MakeDT(2026, 9, 8, 10, 0), i * 7);
        e.end = AddMinutesDT(e.start, 100);
        st.AddEvent(e);
    }
    for (int i = 0; i < 3; ++i) {
        Event e;
        e.title = L"买牛奶";
        e.start = AddDaysDT(MakeDT(2026, 9, 9, 19, 0), i * 7);
        e.end = AddMinutesDT(e.start, 20);
        st.AddEvent(e);
    }
    int before = (int)st.events.size();
    SmartOptions opt;
    SmartResult sr = SmartOrganize(st, opt);
    Check(sr.seriesCreated == 3, L"识别出 3 组每周规律并合并", Fmt(L"%d", sr.seriesCreated));
    Check((int)st.events.size() == 3 && before == 11, L"事件从 11 条合并为 3 个系列",
          Fmt(L"%d -> %d", before, (int)st.events.size()));
    Check(sr.eventsMerged == 11, L"共并入 11 条事件", Fmt(L"%d", sr.eventsMerged));
    const Event* math = nullptr;
    for (auto& e : st.events) if (e.title == L"高等数学A（上）") math = &e;
    Check(math != nullptr, L"合并后存在高等数学系列");
    if (math) {
        Check(math->recur.freq == RF_WEEKLY && math->recur.byDay.size() == 1 && math->recur.byDay[0] == 1,
              L"系列为每周一");
        Check(!math->color.empty(), L"系列自动配色", math->color);
        bool hasCourse = false;
        for (auto& t : math->tags) if (t == "course") hasCourse = true;
        Check(hasCourse, L"系列自动打「课程」标签");
        Check(math->recur.exDates.size() == 1, L"缺勤周记为例外日", Fmt(L"%d", (int)math->recur.exDates.size()));
        Check(math->recur.hasUntil, L"系列有结束日期");
    }
    const Event* milk = nullptr;
    for (auto& e : st.events) if (e.title == L"买牛奶") milk = &e;
    if (milk) Check(milk->tags.empty(), L"非课程不加课程标签（负控）");

    // 单条课程类事件：只打标签/配色，不合并
    Store st2;
    st2.tags = DefaultTags();
    Event one;
    one.title = L"大学物理";
    one.start = MakeDT(2026, 10, 1, 8, 0);
    one.end = MakeDT(2026, 10, 1, 9, 40);
    st2.AddEvent(one);
    SmartResult sr2 = SmartOrganize(st2, opt);
    Check(sr2.seriesCreated == 0 && (int)st2.events.size() == 1, L"重复不足 3 次不合并（负控）");
    Check(!st2.events[0].tags.empty() && !st2.events[0].color.empty(), L"单条课程仍自动打标签并配色");
}
std::wstring LunarText(const DateTime& d);
std::wstring LunarFullText(const DateTime& d);
std::wstring LunarFestival(const DateTime& d);
struct HolidayInfo {
    std::wstring name;
    std::wstring category;
};
std::vector<HolidayInfo> HolidaysOn(const DateTime& d);
bool LunarDateOf(const DateTime& d, int& lunarYear, int& month, int& day, bool& leap);
bool LunarToSolarDate(int lunarYear, int month, int day, bool leap, Date& out);
int LunarYearOf(const DateTime& d);
std::wstring SolarTermNameOn(const DateTime& d);
std::vector<DateTime> SolarTermDatesOfYear(int year);

static void TestLunar() {
    Log(L"## 农历 / 节日");
    Check(LunarText(MakeDT(2026, 2, 17)) == L"正月", L"2026-02-17 是农历正月初一（春节）");
    Check(LunarFestival(MakeDT(2026, 2, 17)) == L"春节", L"春节识别");
    Check(LunarText(MakeDT(2026, 10, 17)) == L"初八", L"2026-10-17 是农历九月初八");
    Check(LunarFullText(MakeDT(2026, 10, 17)) == L"农历九月初八", L"完整农历文本");
    Check(LunarText(MakeDT(2023, 3, 22)) == L"闰二月", L"2023-03-22 是闰二月初一");
    Check(LunarFestival(MakeDT(2025, 10, 6)) == L"中秋节", L"2025-10-06 中秋（农历八月十五）");
    Check(LunarFestival(MakeDT(2026, 6, 19)) == L"端午节", L"端午节识别");
    Check(LunarFestival(MakeDT(2024, 2, 9)) == L"除夕", L"除夕识别（次日正月初一）");
    Check(LunarText(MakeDT(1900, 1, 1)).empty(), L"表范围外返回空（负控）");
    int ly = 0, lm = 0, ld = 0; bool leap = false;
    Check(LunarDateOf(MakeDT(2026, 10, 17), ly, lm, ld, leap) && ly == 2026 && lm == 9 && ld == 8 && !leap,
          L"农历反解：2026-10-17 = 2026 年九月八日");
    Date solar{};
    Check(LunarToSolarDate(2024, 1, 1, false, solar) && solar.y == 2024 && solar.m == 2 && solar.d == 10,
          L"农历转公历：2024 正月初一 = 2024-02-10");
    Date solar2{};
    Check(LunarToSolarDate(2005, 8, 15, false, solar2) && solar2.y == 2005 && solar2.m == 9 && solar2.d == 18,
          L"农历转公历：2005 八月十五 = 2005-09-18");
    Date solarLeap{};
    Check(LunarToSolarDate(2023, 2, 1, true, solarLeap) && solarLeap.y == 2023 && solarLeap.m == 3 && solarLeap.d == 22,
          L"农历闰月：2023 闰二月初一 = 2023-03-22");
    Store healthy;
    Event he; he.title = L"正常日程"; he.start = MakeDT(2026, 10, 18, 9, 0); he.end = MakeDT(2026, 10, 18, 10, 0);
    healthy.AddEvent(he);
    Check(ValidateStoreData(healthy) == L"通过", L"数据校验：正常数据通过");
    Store broken;
    Event be; be.title = L"坏日程"; be.start = MakeDT(2026, 10, 18, 10, 0); be.end = MakeDT(2026, 10, 18, 9, 0);
    broken.AddEvent(be);
    Check(ValidateStoreData(broken) != L"通过", L"数据校验：结束早于开始能发现");
    Store lst;
    Event lb; lb.title = L"农历生日"; lb.start = MakeDT(2005, 10, 10); lb.end = MakeDT(2005, 10, 10);
    lb.recur.freq = RF_YEARLY; lb.recur.interval = 1; lb.recur.lunar = true;
    lst.AddEvent(lb);
    auto lunarOcc = ExpandOccurrences(lst, MakeDT(2026, 10, 1), MakeDT(2026, 10, 31));
    bool onLunar = false;
    for (auto& o : lunarOcc) if (SameDay(o.start, MakeDT(2026, 10, 17))) onLunar = true;
    Check(onLunar, L"每年农历重复：2026 年农历九月八日落在 2026-10-17");
    Check(SolarTermNameOn(MakeDT(2026, 1, 5)) == L"小寒", L"节气：2026-01-05 = 小寒");
    Check(SolarTermNameOn(MakeDT(2026, 3, 20)) == L"春分", L"节气：2026-03-20 = 春分");
    Check(SolarTermNameOn(MakeDT(2026, 10, 23)) == L"霜降", L"节气：2026-10-23 = 霜降");
    Check(SolarTermDatesOfYear(2026).size() == 24, L"节气：每年 24 个");
    auto h101 = HolidaysOn(MakeDT(2026, 10, 1));
    Check(!h101.empty() && h101[0].name == L"国庆节", L"节日：2026-10-01 国庆节");
    auto h405 = HolidaysOn(MakeDT(2026, 4, 5));
    bool qingming = false;
    for (auto& h : h405) if (h.name == L"清明节") qingming = true;
    Check(qingming && SolarTermNameOn(MakeDT(2026, 4, 5)) == L"清明", L"节日/节气重叠：清明同时是节气与清明节");
    auto h512 = HolidaysOn(MakeDT(2026, 5, 12));
    Check(h512.size() >= 2, L"同一天多个节日：5-12 防灾减灾日+护士节");
    auto h510 = HolidaysOn(MakeDT(2026, 5, 10));
    bool mother = false;
    for (auto& h : h510) if (h.name == L"母亲节") mother = true;
    Check(mother, L"变动节日：2026-05-10 母亲节");
    auto h930 = HolidaysOn(MakeDT(2026, 9, 30));
    bool martyr = false;
    for (auto& h : h930) if (h.name == L"烈士纪念日") martyr = true;
    Check(martyr, L"国家纪念日：2026-09-30 烈士纪念日");
}

#include "selftest_tests.inc"

int RunSelfTests() {
    g_pass = g_fail = 0;
    g_log.clear();
    EnsureDir(GetDataDir());
    auto flush = [&]() { WriteFileBytesAtomic(PathJoin(GetDataDir(), L"selftest.log"), WideToUtf8(g_log)); };
    Log(L"# 星历自测 " + Store::NowStamp());
    flush();
    TestDate(); flush();
    TestJson(); flush();
    TestColor(); flush();
    TestRecur(); flush();
    TestOccurrenceOps(); flush();
    TestStore(); flush();
    TestExpand(); flush();
    TestIcs(); flush();
    TestQuick(); flush();
    TestLunar(); flush();
    TestSmart(); flush();
    TestIcsDedupe(); flush();
    Log(L"");
    Log(Fmt(L"## 结果：%d 通过 / %d 失败", g_pass, g_fail));
    std::string utf8 = WideToUtf8(g_log);
    WriteFileBytesAtomic(PathJoin(GetDataDir(), L"selftest.log"), utf8);
    OutputDebugStringW(g_log.c_str());
    return g_fail;
}