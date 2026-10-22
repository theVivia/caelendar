// solarterm.cpp - 二十四节气（太阳视黄经 15° 间隔，北京时间）
#include "app.h"
#include <cmath>
#include <vector>
#include <map>
#include <algorithm>

namespace {

const wchar_t* kTermNames[24] = {
    L"小寒", L"大寒", L"立春", L"雨水", L"惊蛰", L"春分",
    L"清明", L"谷雨", L"立夏", L"小满", L"芒种", L"夏至",
    L"小暑", L"大暑", L"立秋", L"处暑", L"白露", L"秋分",
    L"寒露", L"霜降", L"立冬", L"小雪", L"大雪", L"冬至"
};
// 近似公历日期（只用于找根，最终日期由太阳黄经求出）
const int kApproxMonth[24] = { 1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12 };
const int kApproxDay[24] = { 5,20,4,19,5,20,4,20,5,21,5,21,7,23,7,23,7,23,8,23,7,22,7,22 };
// 太阳视黄经目标（小寒 285° … 冬至 270°）
const double kTargetLongitude[24] = {
    285, 300, 315, 330, 345, 0, 15, 30, 45, 60, 75, 90,
    105, 120, 135, 150, 165, 180, 195, 210, 225, 240, 255, 270
};

double Normalize360(double x) {
    x = std::fmod(x, 360.0);
    if (x < 0) x += 360.0;
    return x;
}

// 两个黄经之差，落在 (-180, 180]
double AngleDiff(double a, double b) {
    double d = std::fmod(a - b + 540.0, 360.0) - 180.0;
    return d;
}

// 儒略日（0h UT），Gregorian
double JulianDayNumber(int y, int m, int d) {
    if (m <= 2) { y -= 1; m += 12; }
    int a = y / 100;
    int b = 2 - a + a / 4;
    return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + d + b - 1524.5;
}

// 北京时间某天某小时对应的 UT 儒略日
double JulianForLocal(int y, int m, int d, double hour) {
    return JulianDayNumber(y, m, d) + (hour - 8.0) / 24.0;
}

// 太阳视黄经（Meeus 低精度，误差约 0.01°，对“日期”足够）
double SolarLongitude(double jd) {
    double T = (jd - 2451545.0) / 36525.0;
    double L0 = 280.46646 + 36000.76983 * T + 0.0003032 * T * T;
    double M = 357.52911 + 35999.05029 * T - 0.0001537 * T * T;
    double Mr = M * 3.14159265358979323846 / 180.0;
    double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * std::sin(Mr) +
               (0.019993 - 0.000101 * T) * std::sin(2 * Mr) +
               0.000289 * std::sin(3 * Mr);
    double trueLong = L0 + C;
    double omega = 125.04 - 1934.136 * T;
    double apparent = trueLong - 0.00569 - 0.00478 * std::sin(omega * 3.14159265358979323846 / 180.0);
    return Normalize360(apparent);
}

DateTime FindTerm(int year, int idx) {
    DateTime base = MakeDT(year, kApproxMonth[idx], kApproxDay[idx]);
    double prevDiff = 0;
    int prevOff = 0;
    bool havePrev = false;
    for (int off = -4; off <= 4; ++off) {
        DateTime day = AddDaysDT(base, off);
        double jd = JulianForLocal(day.y, day.mo, day.d, 0.0);   // 当天 00:00 北京时间
        double diff = AngleDiff(SolarLongitude(jd), kTargetLongitude[idx]);
        if (havePrev && prevDiff <= 0 && diff >= 0 && diff - prevDiff < 100) {
            double frac = -prevDiff / (diff - prevDiff);           // 0..1 天
            DateTime res = AddDaysDT(base, prevOff);
            res = AddMinutesDT(res, (int)std::lround(frac * 1440.0));
            return res;
        }
        prevDiff = diff;
        prevOff = off;
        havePrev = true;
    }
    return base;   // 兜底（正常不会走到）
}

std::map<int, std::vector<DateTime>> g_cache;

}  // namespace

std::vector<DateTime> SolarTermDatesOfYear(int year) {
    auto it = g_cache.find(year);
    if (it != g_cache.end()) return it->second;
    std::vector<DateTime> out;
    out.reserve(24);
    for (int i = 0; i < 24; ++i) out.push_back(FindTerm(year, i));
    g_cache[year] = out;
    return out;
}

std::wstring SolarTermNameOn(const DateTime& d) {
    if (d.y < 1901 || d.y > 2100) return L"";
    auto terms = SolarTermDatesOfYear(d.y);
    for (size_t i = 0; i < terms.size(); ++i) {
        if (terms[i].y == d.y && terms[i].mo == d.mo && terms[i].d == d.d)
            return kTermNames[i];
    }
    return L"";
}
