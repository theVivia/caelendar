// util.cpp
#include "util.h"
#include <cstdarg>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <random>
#include <shlwapi.h>

// ---------- 字符串 ----------
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string a(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &a[0], n, nullptr, nullptr);
    return a;
}
std::wstring Fmt(const wchar_t* fmt, ...) {
    wchar_t buf[4096];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    return buf;
}
std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> out; std::wstring cur;
    for (wchar_t c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur); return out;
}
std::vector<std::wstring> SplitAny(const std::wstring& s, const std::wstring& seps) {
    std::vector<std::wstring> out; std::wstring cur;
    for (wchar_t c : s) {
        if (seps.find(c) != std::wstring::npos) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur); return out;
}
std::wstring Join(const std::vector<std::wstring>& v, const std::wstring& sep) {
    std::wstring out;
    for (size_t i = 0; i < v.size(); ++i) { if (i) out += sep; out += v[i]; }
    return out;
}
std::wstring LTrim(const std::wstring& s) {
    size_t i = 0; while (i < s.size() && iswspace(s[i])) ++i; return s.substr(i);
}
std::wstring RTrim(const std::wstring& s) {
    size_t i = s.size(); while (i > 0 && iswspace(s[i - 1])) --i; return s.substr(0, i);
}
std::wstring Trim(const std::wstring& s) { return RTrim(LTrim(s)); }
std::wstring Lower(const std::wstring& s) {
    std::wstring o = s;
    for (auto& c : o) c = (wchar_t)towlower(c);
    return o;
}
bool StartsWith(const std::wstring& s, const std::wstring& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
bool EndsWith(const std::wstring& s, const std::wstring& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
bool Contains(const std::wstring& s, const std::wstring& sub) {
    return s.find(sub) != std::wstring::npos;
}
std::wstring ReplaceAll(std::wstring s, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
    return s;
}
bool ToInt(const std::wstring& s, int& out) {
    if (s.empty()) return false;
    wchar_t* end = nullptr;
    long v = wcstol(s.c_str(), &end, 10);
    if (end == s.c_str()) return false;
    while (end && *end && iswspace(*end)) ++end;
    if (end && *end) return false;
    out = (int)v; return true;
}
std::wstring Pad2(int v) { return Fmt(L"%02d", v); }

// ---------- 日期 ----------
bool IsLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }
int DaysInMonth(int y, int m) {
    static const int dm[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (m < 1 || m > 12) return 30;
    if (m == 2 && IsLeapYear(y)) return 29;
    return dm[m - 1];
}
int DaySerial(int y, int m, int d) {
    // Howard Hinnant days_from_civil
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2u) / 5u + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int)doe - 719468;
}
Date FromSerial(int z) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = (int)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    return Date{ y + (m <= 2 ? 1 : 0), (int)m, (int)d };
}
int DayOfWeek(int y, int m, int d) {
    int s = DaySerial(y, m, d);
    int w = (s + 4) % 7;   // 1970-01-01 周四
    if (w < 0) w += 7;
    return w;   // 0=周日
}
DateTime MakeDT(int y, int mo, int d, int h, int mi) { return DateTime{ y,mo,d,h,mi }; }
DateTime NowDT() {
    SYSTEMTIME st; GetLocalTime(&st);
    return DateTime{ st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute };
}
DateTime Today() { auto n = NowDT(); return DateTime{ n.y,n.mo,n.d,0,0 }; }
DateTime AddDaysDT(const DateTime& t, int days) {
    Date d = FromSerial(DaySerial(t.y, t.mo, t.d) + days);
    return DateTime{ d.y, d.m, d.d, t.h, t.mi };
}
DateTime AddMinutesDT(const DateTime& t, int minutes) {
    int serial = DaySerial(t.y, t.mo, t.d);
    int total = t.h * 60 + t.mi + minutes;
    while (total < 0) { total += 1440; serial--; }
    while (total >= 1440) { total -= 1440; serial++; }
    Date d = FromSerial(serial);
    return DateTime{ d.y, d.m, d.d, total / 60, total % 60 };
}
DateTime AddMonthsDT(const DateTime& t, int months) {
    int idx = (t.y * 12 + (t.mo - 1)) + months;
    int y = idx / 12, m = idx % 12 + 1;
    if (m <= 0) { m += 12; y--; }
    int d = std::min(t.d, DaysInMonth(y, m));
    return DateTime{ y, m, d, t.h, t.mi };
}
DateTime AddYearsDT(const DateTime& t, int years) { return AddMonthsDT(t, years * 12); }
int MinutesBetween(const DateTime& a, const DateTime& b) {
    int sa = DaySerial(a.y, a.mo, a.d), sb = DaySerial(b.y, b.mo, b.d);
    return (sb - sa) * 1440 + (b.h * 60 + b.mi) - (a.h * 60 + a.mi);
}
int CompareDT(const DateTime& a, const DateTime& b) {
    if (a.y != b.y) return a.y < b.y ? -1 : 1;
    if (a.mo != b.mo) return a.mo < b.mo ? -1 : 1;
    if (a.d != b.d) return a.d < b.d ? -1 : 1;
    if (a.h != b.h) return a.h < b.h ? -1 : 1;
    if (a.mi != b.mi) return a.mi < b.mi ? -1 : 1;
    return 0;
}
bool SameDay(const DateTime& a, const DateTime& b) { return a.y == b.y && a.mo == b.mo && a.d == b.d; }
bool IsDateOnly(const DateTime& t) { return t.h == 0 && t.mi == 0; }
DateTime StartOfDay(const DateTime& t) { return DateTime{ t.y,t.mo,t.d,0,0 }; }
DateTime EndOfDayExclusive(const DateTime& t) { return AddDaysDT(StartOfDay(t), 1); }
DateTime StartOfWeek(const DateTime& t, int weekStart) {
    int wd = DayOfWeek(t.y, t.mo, t.d);   // 0=周日
    int delta = (wd - weekStart + 7) % 7;
    return StartOfDay(AddDaysDT(t, -delta));
}
int WeekOfYear(const DateTime& t) {
    // ISO 周数
    DateTime jan4 = DateTime{ t.y, 1, 4, 0, 0 };
    int wd = DayOfWeek(jan4.y, jan4.mo, jan4.d);
    DateTime week1 = AddDaysDT(jan4, -((wd + 6) % 7));   // 周一
    int days = DaySerial(t.y, t.mo, t.d) - DaySerial(week1.y, week1.mo, week1.d);
    return days / 7 + 1;
}
std::wstring ISODT(const DateTime& t) { return Fmt(L"%04d-%02d-%02dT%02d:%02d", t.y, t.mo, t.d, t.h, t.mi); }
bool IsSupportedDate(const DateTime& t) {
    return t.y >= MIN_CALENDAR_YEAR && t.y <= MAX_CALENDAR_YEAR;
}
DateTime ClampDateToSupported(const DateTime& t) {
    if (t.y < MIN_CALENDAR_YEAR) return DateTime{ MIN_CALENDAR_YEAR, 1, 1, 0, 0 };
    if (t.y > MAX_CALENDAR_YEAR) return DateTime{ MAX_CALENDAR_YEAR, 12, 31, 23, 59 };
    return t;
}

std::wstring ISODate(const DateTime& t) { return Fmt(L"%04d-%02d-%02d", t.y, t.mo, t.d); }
std::wstring TimeStr(const DateTime& t) { return Fmt(L"%02d:%02d", t.h, t.mi); }
std::wstring EventTimeRangeText(const DateTime& s, const DateTime& e, bool allDay) {
    if (allDay) {
        int n = DaySerial(e.y, e.mo, e.d) - DaySerial(s.y, s.mo, s.d);
        if (n <= 1) return L"全天";
        return Fmt(L"全天(+%d)", n - 1);
    }
    if (CompareDT(s, e) == 0) return TimeStr(s);   // 时间点（结束=开始）
    int sd = DaySerial(s.y, s.mo, s.d), ed = DaySerial(e.y, e.mo, e.d);
    bool endMidnight = (e.h == 0 && e.mi == 0);
    int n = ed - sd;
    if (endMidnight && n > 0) --n;
    std::wstring out = TimeStr(s) + L"–";
    if (endMidnight && ed > sd) out += L"24:00";
    else out += TimeStr(e);
    if (n <= 0) return out;
    if (n <= 2) return out + Fmt(L"(+%d)", n);
    return TimeStr(s) + L"–" + Fmt(L"%d/%d/%d %s", e.y, e.mo, e.d, TimeStr(e).c_str());
}
std::wstring DateTimeHuman(const DateTime& t) {
    return Fmt(L"%d月%d日 %s %02d:%02d", t.mo, t.d, WeekdayCN(DayOfWeek(t.y, t.mo, t.d)), t.h, t.mi);
}
DateTime ParseISODT(const std::wstring& s, bool* ok) {
    DateTime t{};
    int y, mo, d, h = 0, mi = 0;
    int n = swscanf_s(s.c_str(), L"%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi);
    if (n < 3) {
        n = swscanf_s(s.c_str(), L"%d/%d/%d %d:%d", &y, &mo, &d, &h, &mi);
    }
    bool good = (n >= 3 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31 && y > 1900 && y < 2200);
    if (ok) *ok = good;
    if (!good) return t;
    t.y = y; t.mo = mo; t.d = d; t.h = (n >= 4 ? h : 0); t.mi = (n >= 5 ? mi : 0);
    return t;
}
DateTime ParseISODate(const std::wstring& s, bool* ok) {
    auto t = ParseISODT(s, ok);
    t.h = 0; t.mi = 0;
    return t;
}
const wchar_t* WeekdayCN(int wd) {
    static const wchar_t* w[7] = { L"周日", L"周一", L"周二", L"周三", L"周四", L"周五", L"周六" };
    return w[(wd % 7 + 7) % 7];
}
const wchar_t* WeekdayShortCN(int wd) {
    static const wchar_t* w[7] = { L"日", L"一", L"二", L"三", L"四", L"五", L"六" };
    return w[(wd % 7 + 7) % 7];
}

// ---------- 文件 ----------
std::string ReadFileBytes(const std::wstring& path, bool* ok) {
    std::string out;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { if (ok) *ok = false; return out; }
    LARGE_INTEGER li{};
    if (GetFileSizeEx(h, &li)) {
        out.resize((size_t)li.QuadPart);
        DWORD read = 0;
        if (!out.empty()) ReadFile(h, &out[0], (DWORD)out.size(), &read, nullptr);
        out.resize(read);
    }
    CloseHandle(h);
    if (ok) *ok = true;
    return out;
}
bool WriteFileBytes(const std::wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = data.empty() || (WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr) && written == data.size());
    FlushFileBuffers(h);
    CloseHandle(h);
    return ok;
}
bool WriteFileBytesAtomic(const std::wstring& path, const std::string& data) {
    std::wstring tmp = path + L".tmp";
    if (!WriteFileBytes(tmp, data)) return false;
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}
std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, _countof(buf));
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}
std::wstring GetDataDir() { return PathJoin(GetExeDir(), L"data"); }
std::wstring GetCalendarPath() { return PathJoin(GetDataDir(), L"calendar.json"); }
std::wstring PathJoin(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    wchar_t last = a.back();
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}
bool EnsureDir(const std::wstring& dir) {
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return true;
    return CreateDirectoryW(dir.c_str(), nullptr) != 0;
}
bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}
std::vector<std::wstring> ListFiles(const std::wstring& dir, const std::wstring& ext) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(PathJoin(dir, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring name = fd.cFileName;
        if (ext.empty() || EndsWith(Lower(name), Lower(ext))) out.push_back(PathJoin(dir, name));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}
bool CopyFileSimple(const std::wstring& src, const std::wstring& dst) {
    return CopyFileW(src.c_str(), dst.c_str(), FALSE) != 0;
}
void DebugLog(const wchar_t* fmt, ...) {
    wchar_t buf[4096];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    EnsureDir(GetDataDir());
    std::wstring path = PathJoin(GetDataDir(), L"debug.log");
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st; GetLocalTime(&st);
    std::wstring line = Fmt(L"[%04d-%02d-%02d %02d:%02d:%02d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond) + buf + L"\r\n";
    std::string u8 = WideToUtf8(line);
    DWORD w = 0; SetFilePointer(h, 0, nullptr, FILE_END);
    WriteFile(h, u8.data(), (DWORD)u8.size(), &w, nullptr);
    CloseHandle(h);
}
std::string NewId(const char* prefix) {
    static std::mt19937_64 rng(std::random_device{}());
    char buf[64];
    snprintf(buf, sizeof(buf), "%s_%08llx%04x", prefix, (unsigned long long)(rng() & 0xffffffffULL), (unsigned)(rng() & 0xffff));
    return buf;
}
int RandInt(int lo, int hi) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// ---------- 共享调色板 ----------
const std::vector<std::wstring>& PaletteHex() {
    static const std::vector<std::wstring> p = {
        L"#6EA8FF", L"#8E7BFF", L"#FFB454", L"#FF6B7A", L"#4FD1C5", L"#F9A8D4",
        L"#C084FC", L"#A3E635", L"#34D399", L"#38BDF8", L"#FB923C", L"#FACC15",
        L"#60A5FA", L"#E879F9", L"#F87171", L"#2DD4BF", L"#FCD34D", L"#94A3B8",
    };
    return p;
}
const std::vector<std::wstring>& PaletteNames() {
    static const std::vector<std::wstring> n = {
        L"蓝", L"紫", L"橙", L"红", L"青", L"粉",
        L"淡紫", L"青柠", L"绿", L"天蓝", L"橘", L"黄",
        L"亮蓝", L"洋红", L"珊瑚", L"薄荷", L"金", L"灰",
    };
    return n;
}
// ---------- 颜色转换 ----------
std::wstring RgbToHex(int r, int g, int b) {
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    return Fmt(L"#%02X%02X%02X", r, g, b);
}
bool HexToRgb(const std::wstring& hex, int& r, int& g, int& b) {
    std::wstring s = hex;
    if (!s.empty() && s[0] == L'#') s.erase(0, 1);
    if (s.size() != 6) return false;
    auto hv = [](wchar_t ch) -> int {
        if (ch >= L'0' && ch <= L'9') return ch - L'0';
        if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
        if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
        return -1;
    };
    int v[6];
    for (int i = 0; i < 6; ++i) { v[i] = hv(s[i]); if (v[i] < 0) return false; }
    r = v[0] * 16 + v[1];
    g = v[2] * 16 + v[3];
    b = v[4] * 16 + v[5];
    return true;
}
void RgbToCmyk(int r, int g, int b, int& c, int& m, int& y, int& k) {
    float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    float kf = 1.0f - std::max(rf, std::max(gf, bf));
    if (kf >= 0.999f) { c = m = y = 0; k = 100; return; }
    c = (int)std::lround((1.0f - rf - kf) / (1.0f - kf) * 100.0f);
    m = (int)std::lround((1.0f - gf - kf) / (1.0f - kf) * 100.0f);
    y = (int)std::lround((1.0f - bf - kf) / (1.0f - kf) * 100.0f);
    k = (int)std::lround(kf * 100.0f);
}
void CmykToRgb(int c, int m, int y, int k, int& r, int& g, int& b) {
    float cf = c / 100.0f, mf = m / 100.0f, yf = y / 100.0f, kf = k / 100.0f;
    r = (int)std::lround(255.0f * (1.0f - cf) * (1.0f - kf));
    g = (int)std::lround(255.0f * (1.0f - mf) * (1.0f - kf));
    b = (int)std::lround(255.0f * (1.0f - yf) * (1.0f - kf));
}
// ---------- 人类可读时间差（日 / 时 / 分，省略为零的中间单位） ----------
std::wstring HumanFutureCN(const DateTime& t, const DateTime& now) {
    int ds = DaySerial(t.y, t.mo, t.d) - DaySerial(now.y, now.mo, now.d);
    if (ds < 0) return HumanPastCN(t, now);
    if (ds == 0) {
        int mins = (t.h * 60 + t.mi) - (now.h * 60 + now.mi);
        if (mins <= 0) return L"马上";
        if (mins < 60) return Fmt(L"%d 分钟后", mins);
        int h = mins / 60, m = mins % 60;
        if (m == 0) return Fmt(L"%d 小时后", h);
        return Fmt(L"%d 小时 %d 分钟后", h, m);
    }
    if (ds == 1) return Fmt(L"明天 %02d:%02d", t.h, t.mi);
    if (ds == 2) return Fmt(L"后天 %02d:%02d", t.h, t.mi);
    if (t.y == now.y) return Fmt(L"%d月%d日 %02d:%02d", t.mo, t.d, t.h, t.mi);
    return Fmt(L"%d年%d月%d日 %02d:%02d", t.y, t.mo, t.d, t.h, t.mi);
}

std::wstring HumanFutureShortCN(const DateTime& t, const DateTime& now) {
    int ds = DaySerial(t.y, t.mo, t.d) - DaySerial(now.y, now.mo, now.d);
    if (ds < 0) return HumanPastCN(t, now);
    if (ds == 0) {
        int mins = (t.h * 60 + t.mi) - (now.h * 60 + now.mi);
        if (mins <= 0) return L"马上";
        if (mins < 60) return Fmt(L"%d 分后", mins);
        return Fmt(L"%d 小时后", mins / 60);
    }
    if (ds == 1) return L"明天";
    if (ds == 2) return L"后天";
    if (ds <= 6) {
        int wd = DayOfWeek(t.y, t.mo, t.d);
        return Fmt(L"%s", WeekdayCN(wd));
    }
    if (t.y == now.y) return Fmt(L"%d月%d日", t.mo, t.d);
    return Fmt(L"%d年%d月%d日", t.y, t.mo, t.d);
}

std::wstring HumanRemainCN(const DateTime& end, const DateTime& now) {
    int mins = MinutesBetween(now, end);
    if (mins <= 0) return L"即将结束";
    int days = mins / (24 * 60), h = (mins % (24 * 60)) / 60, m = mins % 60;
    if (days > 0) {
        if (h == 0) return Fmt(L"%d 天", days);
        return Fmt(L"%d 天 %d 小时", days, h);
    }
    if (h > 0) {
        if (m == 0) return Fmt(L"%d 小时", h);
        return Fmt(L"%d 小时 %d 分钟", h, m);
    }
    return Fmt(L"%d 分钟", mins);
}

std::wstring HumanPastCN(const DateTime& t, const DateTime& now) {
    int ds = DaySerial(now.y, now.mo, now.d) - DaySerial(t.y, t.mo, t.d);
    if (ds == 0) {
        int mins = (now.h * 60 + now.mi) - (t.h * 60 + t.mi);
        if (mins <= 0) return L"刚刚";
        if (mins < 60) return Fmt(L"%d 分钟前", mins);
        int h = mins / 60, m = mins % 60;
        if (m == 0) return Fmt(L"%d 小时前", h);
        return Fmt(L"%d 小时 %d 分钟前", h, m);
    }
    if (ds == 1) return Fmt(L"昨天 %02d:%02d", t.h, t.mi);
    if (ds == 2) return Fmt(L"前天 %02d:%02d", t.h, t.mi);
    if (t.y == now.y) return Fmt(L"%d月%d日 %02d:%02d", t.mo, t.d, t.h, t.mi);
    return Fmt(L"%d年%d月%d日 %02d:%02d", t.y, t.mo, t.d, t.h, t.mi);
}
