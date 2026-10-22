// util.h - 基础工具：字符串、日期时间、文件
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

// ---------- 字符串 ----------
std::wstring Utf8ToWide(const std::string& s);
std::string  WideToUtf8(const std::wstring& s);
std::wstring Fmt(const wchar_t* fmt, ...);
std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep);
std::vector<std::wstring> SplitAny(const std::wstring& s, const std::wstring& seps);
std::wstring Join(const std::vector<std::wstring>& v, const std::wstring& sep);
std::wstring Trim(const std::wstring& s);
std::wstring LTrim(const std::wstring& s);
std::wstring RTrim(const std::wstring& s);
std::wstring Lower(const std::wstring& s);
bool StartsWith(const std::wstring& s, const std::wstring& p);
bool EndsWith(const std::wstring& s, const std::wstring& p);
bool Contains(const std::wstring& s, const std::wstring& sub);
std::wstring ReplaceAll(std::wstring s, const std::wstring& from, const std::wstring& to);
bool ToInt(const std::wstring& s, int& out);
std::wstring Pad2(int v);

// ---------- 日期时间（本地墙钟） ----------
struct Date { int y = 1970, m = 1, d = 1; };
struct DateTime { int y = 1970, mo = 1, d = 1, h = 0, mi = 0; };

bool IsLeapYear(int y);
int  DaysInMonth(int y, int m);
int  DayOfWeek(int y, int m, int d);   // 0=周日 .. 6=周六
int  DaySerial(int y, int m, int d);   // 距 1970-01-01 的天数（可负）
Date FromSerial(int serial);
DateTime MakeDT(int y, int mo, int d, int h = 0, int mi = 0);
DateTime NowDT();
DateTime Today();
DateTime AddDaysDT(const DateTime& t, int days);
DateTime AddMinutesDT(const DateTime& t, int minutes);
DateTime AddMonthsDT(const DateTime& t, int months);  // 月末夹取
DateTime AddYearsDT(const DateTime& t, int years);
int  MinutesBetween(const DateTime& a, const DateTime& b);
int  CompareDT(const DateTime& a, const DateTime& b);
bool SameDay(const DateTime& a, const DateTime& b);
bool IsDateOnly(const DateTime& t);   // h==0 && mi==0
DateTime StartOfDay(const DateTime& t);
DateTime EndOfDayExclusive(const DateTime& t);
DateTime StartOfWeek(const DateTime& t, int weekStart);  // weekStart 1=周一
int  WeekOfYear(const DateTime& t);

std::wstring ISODT(const DateTime& t);      // 2026-10-17T08:00
std::wstring ISODate(const DateTime& t);    // 2026-10-17
constexpr int MIN_CALENDAR_YEAR = 1901;
constexpr int MAX_CALENDAR_YEAR = 2100;
bool IsSupportedDate(const DateTime& t);
DateTime ClampDateToSupported(const DateTime& t);
std::wstring TimeStr(const DateTime& t);    // 08:00
std::wstring EventTimeRangeText(const DateTime& s, const DateTime& e, bool allDay);
std::wstring DateTimeHuman(const DateTime& t);  // 10月17日 周六 08:00
DateTime ParseISODT(const std::wstring& s, bool* ok = nullptr);
DateTime ParseISODate(const std::wstring& s, bool* ok = nullptr);
// 人类可读时间差：日 / 时 / 分可省略结构（215 分钟 -> "3 小时 35 分钟后"）
std::wstring HumanFutureCN(const DateTime& t, const DateTime& now);
std::wstring HumanFutureShortCN(const DateTime& t, const DateTime& now);
std::wstring HumanRemainCN(const DateTime& end, const DateTime& now);
std::wstring HumanPastCN(const DateTime& t, const DateTime& now);
const wchar_t* WeekdayCN(int wd);            // 周日..周六
const wchar_t* WeekdayShortCN(int wd);       // 日..六

// ---------- 文件 ----------
std::string  ReadFileBytes(const std::wstring& path, bool* ok = nullptr);
bool         WriteFileBytesAtomic(const std::wstring& path, const std::string& data);
bool         WriteFileBytes(const std::wstring& path, const std::string& data);
std::wstring GetExeDir();
std::wstring GetDataDir();          // exeDir\data
std::wstring GetCalendarPath();     // exeDir\data\calendar.json
std::wstring PathJoin(const std::wstring& a, const std::wstring& b);
bool         EnsureDir(const std::wstring& dir);
bool         FileExists(const std::wstring& path);
std::vector<std::wstring> ListFiles(const std::wstring& dir, const std::wstring& ext);
bool         CopyFileSimple(const std::wstring& src, const std::wstring& dst);

void DebugLog(const wchar_t* fmt, ...);
// 共享调色板（编辑器 / 标签 / 批量改色 / 清理面板统一使用）
const std::vector<std::wstring>& PaletteHex();
const std::vector<std::wstring>& PaletteNames();
// 颜色转换
std::wstring RgbToHex(int r, int g, int b);
bool HexToRgb(const std::wstring& hex, int& r, int& g, int& b);
void RgbToCmyk(int r, int g, int b, int& c, int& m, int& y, int& k);
void CmykToRgb(int c, int m, int y, int k, int& r, int& g, int& b);

// ---------- 其他 ----------
std::string NewId(const char* prefix);
int  RandInt(int lo, int hi);
