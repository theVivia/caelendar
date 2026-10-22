// lunar.cpp - 农历、公历/农历节日
#include "app.h"
#include <string>

#include "lunar_table.inc"

struct SolarFestival { int m, d; const wchar_t* name; const wchar_t* cat; };
static const SolarFestival kSolar[] = {
    // 国家法定 / 国家纪念
    {1, 1, L"元旦", L"国家"}, {3, 12, L"植树节", L"国家"}, {4, 15, L"国家安全教育日", L"国家"},
    {5, 1, L"劳动节", L"国家"}, {5, 4, L"青年节", L"国家"}, {5, 12, L"防灾减灾日", L"国家"},
    {6, 1, L"儿童节", L"国际"}, {7, 1, L"建党节", L"国家"}, {8, 1, L"建军节", L"国家"},
    {9, 3, L"抗战胜利纪念日", L"纪念"}, {9, 10, L"教师节", L"国家"},
    {9, 18, L"九一八事变纪念日", L"纪念"}, {9, 30, L"烈士纪念日", L"纪念"},
    {10, 1, L"国庆节", L"国家"}, {10, 10, L"辛亥革命纪念日", L"纪念"},
    {12, 2, L"交通安全日", L"国家"}, {12, 4, L"宪法日", L"国家"},
    {12, 13, L"国家公祭日", L"纪念"}, {12, 20, L"澳门回归纪念日", L"纪念"},
    // 国际 / 常见洋节
    {2, 14, L"情人节", L"国际"}, {3, 8, L"妇女节", L"国际"}, {4, 1, L"愚人节", L"国际"},
    {4, 22, L"世界地球日", L"国际"}, {4, 23, L"世界读书日", L"国际"},
    {5, 12, L"护士节", L"国际"}, {6, 5, L"世界环境日", L"国际"},
    {10, 31, L"万圣夜", L"国际"}, {11, 8, L"记者节", L"国家"},
    {12, 24, L"平安夜", L"国际"}, {12, 25, L"圣诞", L"国际"},
};


struct LunarDate {
    bool valid = false;
    int year = 0;       // 农历年
    int month = 0;      // 农历月 1..12
    int day = 0;        // 农历日 1..30
    bool leap = false;  // 闰月
    int monthDays = 30;
};

static const int kLunarYears = 200;   // 1901..2100

const wchar_t* LunarMonthCN(int m, bool leap) {
    static const wchar_t* names[] = { L"正月", L"二月", L"三月", L"四月", L"五月", L"六月",
                                      L"七月", L"八月", L"九月", L"十月", L"冬月", L"腊月" };
    if (m < 1 || m > 12) return L"";
    static std::wstring buf;
    buf = leap ? L"闰" : L"";
    buf += names[m - 1];
    return buf.c_str();
}

const wchar_t* LunarDayCN(int d) {
    static const wchar_t* days[] = {
        L"初一", L"初二", L"初三", L"初四", L"初五", L"初六", L"初七", L"初八", L"初九", L"初十",
        L"十一", L"十二", L"十三", L"十四", L"十五", L"十六", L"十七", L"十八", L"十九", L"二十",
        L"廿一", L"廿二", L"廿三", L"廿四", L"廿五", L"廿六", L"廿七", L"廿八", L"廿九", L"三十" };
    if (d < 1 || d > 30) return L"";
    return days[d - 1];
}

static LunarDate SolarToLunar(const DateTime& dt) {
    LunarDate out;
    int32_t serial = DaySerial(dt.y, dt.mo, dt.d);
    if (serial < kLunarNewYear[0] || serial > kLunarNewYear[kLunarYears - 1] + 400) return out;
    int lo = 0, hi = kLunarYears - 1, idx = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (kLunarNewYear[mid] <= serial) { idx = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    if (idx < 0) return out;
    int32_t start = kLunarNewYear[idx];
    int leapActual = kLunarLeap[idx];
    uint16_t bits = kLunarMonthBits[idx];
    int positions = leapActual > 0 ? 13 : 12;
    int leapPos = leapActual > 0 ? leapActual + 1 : 0;
    for (int pos = 1; pos <= positions; ++pos) {
        int len = 29 + ((bits >> (pos - 1)) & 1);
        if (serial < start + len) {
            out.valid = true;
            out.year = 1901 + idx;
            out.leap = (leapPos > 0 && pos == leapPos);
            if (leapActual == 0) out.month = pos;
            else if (pos == leapPos) out.month = leapActual;
            else if (pos > leapPos) out.month = pos - 1;
            else out.month = pos;
            out.day = serial - start + 1;
            out.monthDays = len;
            return out;
        }
        start += len;
    }
    return out;
}

std::wstring LunarText(const DateTime& d) {
    LunarDate l = SolarToLunar(d);
    if (!l.valid) return L"";
    if (l.day == 1) return LunarMonthCN(l.month, l.leap);
    return LunarDayCN(l.day);
}

std::wstring LunarFullText(const DateTime& d) {
    LunarDate l = SolarToLunar(d);
    if (!l.valid) return L"";
    return std::wstring(L"农历") + LunarMonthCN(l.month, l.leap) + LunarDayCN(l.day);
}

static void PushHoliday(std::vector<HolidayInfo>& out, const std::wstring& name, const wchar_t* cat) {
    for (auto& h : out) if (h.name == name) return;
    out.push_back(HolidayInfo{ name, cat });
}

// 每月第 n 个周几（周日=0）
static int NthWeekday(int y, int m, int wd, int n) {
    int first = DayOfWeek(y, m, 1);
    int day = 1 + ((wd - first + 7) % 7) + (n - 1) * 7;
    return day <= DaysInMonth(y, m) ? day : 0;
}

std::vector<HolidayInfo> HolidaysOn(const DateTime& d) {
    std::vector<HolidayInfo> out;
    // 1) 农历传统节日
    LunarDate l = SolarToLunar(d);
    if (l.valid && !l.leap) {
        if (l.month == 1 && l.day == 1) PushHoliday(out, L"春节", L"传统");
        else if (l.month == 1 && l.day == 15) PushHoliday(out, L"元宵节", L"传统");
        else if (l.month == 2 && l.day == 2) PushHoliday(out, L"龙抬头", L"传统");
        else if (l.month == 3 && l.day == 3) PushHoliday(out, L"上巳节", L"传统");
        else if (l.month == 5 && l.day == 5) PushHoliday(out, L"端午节", L"传统");
        else if (l.month == 7 && l.day == 7) PushHoliday(out, L"七夕", L"传统");
        else if (l.month == 7 && l.day == 15) PushHoliday(out, L"中元节", L"传统");
        else if (l.month == 8 && l.day == 15) PushHoliday(out, L"中秋节", L"传统");
        else if (l.month == 9 && l.day == 9) PushHoliday(out, L"重阳节", L"传统");
        else if (l.month == 10 && l.day == 1) PushHoliday(out, L"寒衣节", L"传统");
        else if (l.month == 10 && l.day == 15) PushHoliday(out, L"下元节", L"传统");
        else if (l.month == 12 && l.day == 8) PushHoliday(out, L"腊八节", L"传统");
        else if (l.month == 12 && l.day == 23) PushHoliday(out, L"小年", L"传统");
    }
    // 除夕：次日是正月初一
    if (l.valid) {
        LunarDate ln = SolarToLunar(AddDaysDT(d, 1));
        if (ln.valid && ln.month == 1 && ln.day == 1) PushHoliday(out, L"除夕", L"传统");
    }
    // 2) 节气型节日（清明）
    std::wstring term = SolarTermNameOn(d);
    if (term == L"清明") PushHoliday(out, L"清明节", L"传统");
    // 3) 固定公历节日
    for (auto& f : kSolar) {
        if (d.mo == f.m && d.d == f.d) PushHoliday(out, f.name, f.cat);
    }
    // 4) 按星期变化的节日
    if (d.mo == 5 && d.d == NthWeekday(d.y, 5, 0, 2)) PushHoliday(out, L"母亲节", L"国际");
    if (d.mo == 5 && d.d == NthWeekday(d.y, 5, 0, 3)) PushHoliday(out, L"全国助残日", L"国家");
    if (d.mo == 6 && d.d == NthWeekday(d.y, 6, 0, 3)) PushHoliday(out, L"父亲节", L"国际");
    if (d.mo == 11 && d.d == NthWeekday(d.y, 11, 4, 4)) PushHoliday(out, L"感恩节", L"国际");
    return out;
}

std::wstring LunarFestival(const DateTime& d) {
    auto h = HolidaysOn(d);
    return h.empty() ? L"" : h[0].name;
}

bool LunarDateOf(const DateTime& d, int& lunarYear, int& month, int& day, bool& leap) {
    LunarDate l = SolarToLunar(d);
    if (!l.valid) return false;
    lunarYear = l.year;
    month = l.month;
    day = l.day;
    leap = l.leap;
    return true;
}

int LunarYearOf(const DateTime& d) {
    LunarDate l = SolarToLunar(d);
    return l.valid ? l.year : 0;
}

bool LunarToSolarDate(int lunarYear, int month, int day, bool leap, Date& out) {
    int idx = lunarYear - 1901;
    if (idx < 0 || idx >= kLunarYears || month < 1 || month > 12 || day < 1 || day > 30) return false;
    int leapActual = kLunarLeap[idx];
    int positions = leapActual > 0 ? 13 : 12;
    int target = 0;
    if (leap) {
        if (leapActual != month) return false;
        target = leapActual + 1;
    } else {
        target = (leapActual > 0 && month >= leapActual) ? month + 1 : month;
    }
    if (target < 1 || target > positions) return false;
    int32_t start = kLunarNewYear[idx];
    uint16_t bits = kLunarMonthBits[idx];
    for (int pos = 1; pos < target; ++pos) start += 29 + ((bits >> (pos - 1)) & 1);
    int len = 29 + ((bits >> (target - 1)) & 1);
    int useDay = std::min(day, len);   // 小月三十按廿九处理
    int32_t serial = start + useDay - 1;
    out = FromSerial(serial);
    return true;
}