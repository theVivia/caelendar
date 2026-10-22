// import2.cpp - 智能导入：教务 HTML / CSV / TSV / 粘贴文本 / ICS
#include "app.h"
#include "quick.h"
#include "smart.h"
#include <commdlg.h>
#include <cwctype>
#include <algorithm>
#include <cstdlib>

using namespace Gdiplus;
using namespace gfx;

namespace {

std::wstring Trim2(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}
std::wstring Lower2(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}
bool Has2(const std::wstring& hay, const std::wstring& needle) {
    return Lower2(hay).find(Lower2(needle)) != std::wstring::npos;
}
std::wstring ReplaceAll2(std::wstring s, const std::wstring& from, const std::wstring& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != std::wstring::npos) { s.replace(p, from.size(), to); p += to.size(); }
    return s;
}
std::wstring StripTags(const std::wstring& html) {
    std::wstring out;
    bool inTag = false;
    for (wchar_t c : html) {
        if (c == L'<') { inTag = true; continue; }
        if (c == L'>') { inTag = false; continue; }
        if (!inTag) out += c;
    }
    return out;
}
std::wstring DecodeEntities(std::wstring s) {
    s = ReplaceAll2(s, L"&nbsp;", L" ");
    s = ReplaceAll2(s, L"&amp;", L"&");
    s = ReplaceAll2(s, L"&lt;", L"<");
    s = ReplaceAll2(s, L"&gt;", L">");
    s = ReplaceAll2(s, L"&quot;", L"\"");
    s = ReplaceAll2(s, L"&#39;", L"'");
    s = ReplaceAll2(s, L"&emsp;", L" ");
    s = ReplaceAll2(s, L"&ensp;", L" ");
    return s;
}
std::wstring CellText(const std::wstring& html) {
    std::wstring s = ReplaceAll2(html, L"<br>", L"\n");
    s = ReplaceAll2(s, L"<br/>", L"\n");
    s = ReplaceAll2(s, L"<br />", L"\n");
    return Trim2(DecodeEntities(StripTags(s)));
}

bool IsValidUtf8(const std::string& s) {
    int need = 0;
    for (unsigned char c : s) {
        if (need == 0) {
            if ((c & 0x80) == 0) continue;
            else if ((c & 0xE0) == 0xC0) need = 1;
            else if ((c & 0xF0) == 0xE0) need = 2;
            else if ((c & 0xF8) == 0xF0) need = 3;
            else return false;
        } else {
            if ((c & 0xC0) != 0x80) return false;
            --need;
        }
    }
    return need == 0;
}
std::wstring DecodeBytes(const std::string& raw) {
    if (raw.empty()) return L"";
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        return Utf8ToWide(raw.substr(3));
    if (IsValidUtf8(raw)) return Utf8ToWide(raw);
    for (UINT cp : { (UINT)936, (UINT)CP_ACP }) {
        int n = MultiByteToWideChar(cp, 0, raw.data(), (int)raw.size(), nullptr, 0);
        if (n > 0) {
            std::wstring w(n, 0);
            MultiByteToWideChar(cp, 0, raw.data(), (int)raw.size(), &w[0], n);
            return w;
        }
    }
    return Utf8ToWide(raw);
}

std::vector<std::vector<std::wstring>> HtmlRows(const std::wstring& html) {
    std::vector<std::vector<std::wstring>> rows;
    std::wstring low = Lower2(html);
    size_t p = 0;
    while (true) {
        size_t tr = low.find(L"<tr", p);
        if (tr == std::wstring::npos) break;
        size_t trEnd = low.find(L'<', tr + 3);
        if (trEnd == std::wstring::npos) break;
        size_t close = low.find(L"</tr>", trEnd);
        if (close == std::wstring::npos) close = html.size();
        std::wstring body = html.substr(trEnd + 1, close - trEnd - 1);
        std::wstring bodyLow = Lower2(body);
        std::vector<std::wstring> cells;
        size_t q = 0;
        while (true) {
            size_t td = bodyLow.find(L"<td", q);
            size_t th = bodyLow.find(L"<th", q);
            size_t start = std::min(td == std::wstring::npos ? bodyLow.size() : td,
                                    th == std::wstring::npos ? bodyLow.size() : th);
            if (start >= bodyLow.size()) break;
            bool isTd = (td != std::wstring::npos && start == td);
            size_t tagEnd = bodyLow.find(L'>', start);
            if (tagEnd == std::wstring::npos) break;
            size_t cellEnd = isTd ? bodyLow.find(L"</td>", tagEnd) : bodyLow.find(L"</th>", tagEnd);
            if (cellEnd == std::wstring::npos) cellEnd = body.size();
            cells.push_back(CellText(body.substr(tagEnd + 1, cellEnd - tagEnd - 1)));
            q = cellEnd + 4;
        }
        if (!cells.empty()) rows.push_back(cells);
        p = close + 5;
    }
    return rows;
}

std::vector<std::wstring> SplitDelimited(const std::wstring& line, wchar_t delim) {
    std::vector<std::wstring> cells;
    std::wstring cur;
    bool quote = false;
    for (wchar_t c : line) {
        if (c == L'"') { quote = !quote; continue; }
        if (c == delim && !quote) { cells.push_back(Trim2(cur)); cur.clear(); }
        else cur += c;
    }
    cells.push_back(Trim2(cur));
    return cells;
}

std::vector<std::vector<std::wstring>> TextRows(const std::wstring& text) {
    std::wstring t = ReplaceAll2(text, L"\r\n", L"\n");
    t = ReplaceAll2(t, L"\r", L"\n");
    std::vector<std::wstring> lines;
    std::wstring cur;
    for (wchar_t c : t) {
        if (c == L'\n') { lines.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) lines.push_back(cur);
    if (lines.empty()) return {};
    int tabs = (int)std::count(lines[0].begin(), lines[0].end(), L'\t');
    int commas = (int)std::count(lines[0].begin(), lines[0].end(), L',');
    int pipes = (int)std::count(lines[0].begin(), lines[0].end(), L'|');
    if (tabs == 0 && commas == 0 && pipes == 0) return {};
    wchar_t delim = tabs >= commas && tabs >= pipes ? L'\t' : (commas >= pipes ? L',' : L'|');
    std::vector<std::vector<std::wstring>> rows;
    for (auto& l : lines) {
        if (Trim2(l).empty()) continue;
        auto cells = SplitDelimited(l, delim);
        bool any = false;
        for (auto& c : cells) if (!c.empty()) any = true;
        if (any) rows.push_back(cells);
    }
    return rows;
}

int WeekdayIndex(const std::wstring& s) {
    static const wchar_t* names[7][3] = {
        { L"周一", L"星期一", L"礼拜一" }, { L"周二", L"星期二", L"礼拜二" }, { L"周三", L"星期三", L"礼拜三" },
        { L"周四", L"星期四", L"礼拜四" }, { L"周五", L"星期五", L"礼拜五" }, { L"周六", L"星期六", L"礼拜六" },
        { L"周日", L"星期日", L"礼拜日" }
    };
    for (int i = 0; i < 7; ++i)
        for (auto* n : names[i]) if (s.find(n) != std::wstring::npos) return (i + 1) % 7;  // 0=Sun
    static const wchar_t* en[7] = { L"mon", L"tue", L"wed", L"thu", L"fri", L"sat", L"sun" };
    std::wstring low = Lower2(s);
    for (int i = 0; i < 7; ++i) if (low.find(en[i]) != std::wstring::npos) return (i + 1) % 7;
    return -1;
}

bool ParseTimeToken(const std::wstring& raw, int& h, int& m) {
    std::wstring s = raw;
    s = ReplaceAll2(s, L"：", L":");
    size_t p = s.find(L':');
    if (p == std::wstring::npos) return false;
    std::wstring hs = s.substr(0, p), ms = s.substr(p + 1);
    while (!hs.empty() && !iswdigit(hs.front())) hs.erase(hs.begin());
    while (!hs.empty() && !iswdigit(hs.back())) hs.pop_back();
    while (!ms.empty() && !iswdigit(ms.front())) ms.erase(ms.begin());
    while (!ms.empty() && !iswdigit(ms.back())) ms.pop_back();
    if (hs.empty() || ms.empty()) return false;
    h = std::max(0, std::min(23, _wtoi(hs.c_str())));
    m = std::max(0, std::min(59, _wtoi(ms.c_str())));
    return true;
}

bool TimeRange(const std::wstring& raw, int& h1, int& m1, int& h2, int& m2) {
    std::wstring s = ReplaceAll2(raw, L"：", L":");
    s = ReplaceAll2(s, L"~", L"-");
    s = ReplaceAll2(s, L"–", L"-");
    s = ReplaceAll2(s, L"—", L"-");
    s = ReplaceAll2(s, L"至", L"-");
    size_t dash = s.find(L'-');
    if (dash == std::wstring::npos) {
        if (!ParseTimeToken(s, h1, m1)) return false;
        h2 = h1; m2 = m1;
        return true;
    }
    if (!ParseTimeToken(s.substr(0, dash), h1, m1)) return false;
    if (!ParseTimeToken(s.substr(dash + 1), h2, m2)) return false;
    return true;
}

// 从 "2026-10-20 08:00" 这类字符串里抽出时间
bool ExtractClock(const std::wstring& s, int& h1, int& m1, int& h2, int& m2) {
    std::wstring t = ReplaceAll2(s, L"：", L":");
    for (size_t i = 0; i + 1 < t.size(); ++i) {
        if (iswdigit(t[i]) && t[i + 1] == L':') return TimeRange(t.substr(i), h1, m1, h2, m2);
    }
    return false;
}

// 节次 → 时间（常见大学作息；识别不了就用行号兜底）
bool PeriodRange(const std::wstring& raw, int& h1, int& m1, int& h2, int& m2) {
    std::wstring s = Lower2(raw);
    if (s.find(L"节") == std::wstring::npos && s.find(L"课") == std::wstring::npos) return false;
    int n1 = -1, n2 = -1;
    const wchar_t* p = s.c_str();
    while (*p) {
        if (iswdigit(*p)) {
            int v = 0;
            while (iswdigit(*p)) { v = v * 10 + (*p - L'0'); ++p; }
            if (n1 < 0) n1 = v; else if (n2 < 0) n2 = v;
        } else ++p;
    }
    if (n1 < 0) return false;
    if (n2 < 0) n2 = n1;
    static const int starts[12][2] = { {8,0},{8,55},{10,0},{10,55},{14,0},{14,55},
                                       {16,0},{16,55},{19,0},{19,55},{20,50},{21,40} };
    int i1 = std::max(1, std::min(12, n1)) - 1;
    int i2 = std::max(1, std::min(12, n2)) - 1;
    h1 = starts[i1][0]; m1 = starts[i1][1];
    if (i2 + 1 < 12) { h2 = starts[i2 + 1][0]; m2 = starts[i2 + 1][1]; }
    else { h2 = 22; m2 = 30; }
    if (h2 < h1) std::swap(h1, h2);
    return true;
}

void RowTime(const std::vector<std::wstring>& row, int rowIndex, int& h1, int& m1, int& h2, int& m2) {
    std::wstring head = row.empty() ? L"" : row[0];
    if (TimeRange(head, h1, m1, h2, m2)) return;
    if (PeriodRange(head, h1, m1, h2, m2)) return;
    static const int defaults[8][4] = { {8,0,9,40},{10,0,11,40},{14,0,15,40},{16,0,17,40},
                                         {19,0,20,40},{20,50,22,30},{8,0,9,40},{10,0,11,40} };
    int k = std::max(0, std::min(7, rowIndex));
    h1 = defaults[k][0]; m1 = defaults[k][1]; h2 = defaults[k][2]; m2 = defaults[k][3];
}

std::vector<std::wstring> CellLines(const std::wstring& cell) {
    std::wstring s = cell;
    for (const wchar_t* sep : { L"\r\n", L"\n", L"<br>", L"；", L";", L"|" })
        s = ReplaceAll2(s, sep, L"\n");
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == L'\n') { std::wstring t = Trim2(cur); if (!t.empty()) out.push_back(t); cur.clear(); }
        else cur += c;
    }
    std::wstring t = Trim2(cur);
    if (!t.empty()) out.push_back(t);
    return out;
}

bool LocationLike(const std::wstring& s) {
    return Has2(s, L"教") || Has2(s, L"楼") || Has2(s, L"室") || Has2(s, L"号") || Has2(s, L"机房") ||
           Has2(s, L"馆") || Has2(s, L"校区") || Has2(s, L"实验");
}
bool TeacherLike(const std::wstring& s) {
    return Has2(s, L"老师") || Has2(s, L"教师") || Has2(s, L"教授") || Has2(s, L"讲师") || Has2(s, L"博士");
}

void MakeCourseEvent(std::vector<Event>& out, std::vector<std::wstring>& warnings, const std::wstring& cell,
                     int wd, int h1, int m1, int h2, int m2, const std::wstring& fallbackTitle) {
    auto lines = CellLines(cell);
    if (lines.empty()) return;
    // 一行单元格塞多门课：用 "、"/"；" 已在 CellLines 分开；这里只取第一门
    std::wstring title = lines[0];
    if (title.empty()) title = fallbackTitle;
    if (title.empty() || title.size() > 40) { warnings.push_back(Fmt(L"跳过疑似无效单元格：%s", title.c_str())); return; }
    std::wstring location, teacher;
    std::vector<std::wstring> desc;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (location.empty() && LocationLike(lines[i])) location = lines[i];
        else if (teacher.empty() && TeacherLike(lines[i])) teacher = lines[i];
        else desc.push_back(lines[i]);
    }
    DateTime semStart = app.store.settings.semester.enabled ? app.store.settings.semester.start : Today();
    int weeks = app.store.settings.semester.enabled ? std::max(1, app.store.settings.semester.weeks) : 18;
    DateTime first = StartOfDay(semStart);
    int guard = 0;
    while (DayOfWeek(first.y, first.mo, first.d) != wd && guard++ < 8) first = AddDaysDT(first, 1);
    Event e;
    e.title = title;
    e.location = location;
    if (!teacher.empty()) e.desc = teacher;
    if (!desc.empty()) {
        std::wstring d;
        for (auto& x : desc) { if (!d.empty()) d += L" / "; d += x; }
        e.desc = e.desc.empty() ? d : (e.desc + L" / " + d);
    }
    e.start = DateTime{ first.y, first.mo, first.d, h1, m1 };
    e.end = DateTime{ first.y, first.mo, first.d, h2, m2 };
    if (CompareDT(e.end, e.start) <= 0) e.end = AddMinutesDT(e.start, 90);
    e.recur.freq = RF_WEEKLY;
    e.recur.interval = 1;
    e.recur.byDay = { wd };
    e.recur.wkst = 1;
    e.recur.hasUntil = true;
    e.recur.until = AddDaysDT(DateTime{ semStart.y, semStart.mo, semStart.d, 23, 59 }, weeks * 7);
    e.tags = { "course" };
    e.source = L"课表导入";
    e.allDay = false;
    // 去重（同一门课被合并单元格重复）
    for (auto& old : out) {
        if (old.title == e.title && old.start.h == e.start.h && old.start.mi == e.start.mi &&
            !old.recur.byDay.empty() && !e.recur.byDay.empty() && old.recur.byDay[0] == e.recur.byDay[0]) return;
    }
    out.push_back(e);
}

bool LooksLikeTable(std::vector<std::vector<std::wstring>>& rows) {
    if (rows.size() < 2) return false;
    int wdCount = 0;
    for (auto& cell : rows[0]) if (WeekdayIndex(cell) >= 0) ++wdCount;
    return wdCount >= 2;
}

}  // namespace

std::wstring ReadClipboardText() {
    if (!OpenClipboard(g_hwnd)) return L"";
    std::wstring out;
    // 优先 HTML 格式（来自浏览器/教务系统表格），其次纯文本
    UINT fmtHtml = RegisterClipboardFormatW(L"HTML Format");
    if (fmtHtml && IsClipboardFormatAvailable(fmtHtml)) {
        HANDLE h = GetClipboardData(fmtHtml);
        if (h) {
            const char* p = (const char*)GlobalLock(h);
            if (p) {
                size_t len = GlobalSize(h);
                std::string s(p, len);
                size_t start = s.find("StartHTML:");
                if (start != std::string::npos) {
                    size_t offPos = start + 10;
                    long off = atol(s.c_str() + offPos);
                    if (off > 0 && (size_t)off < s.size()) {
                        std::string html = s.substr(off);
                        out = DecodeBytes(html);
                    }
                }
                GlobalUnlock(h);
            }
        }
    }
    if (out.empty() && IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h) {
            const wchar_t* p = (const wchar_t*)GlobalLock(h);
            if (p) out = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

TableImportResult ParseScheduleText(const std::wstring& text) {
    TableImportResult r;
    std::wstring t = text;
    t = ReplaceAll2(t, L"\r\n", L"\n");
    if (Trim2(t).empty()) { r.summary = L"内容为空"; return r; }
    std::vector<std::vector<std::wstring>> rows;
    bool html = Has2(t, L"<table") || Has2(t, L"<tr");
    if (html) rows = HtmlRows(t);
    if (rows.empty()) rows = TextRows(t);
    if (LooksLikeTable(rows)) {
        auto& header = rows[0];
        // 找到星期列；若第 0 列也是星期，则没有时间列
        int wdCols[7] = { -1, -1, -1, -1, -1, -1, -1 };
        for (int c = 0; c < (int)header.size(); ++c) {
            int wd = WeekdayIndex(header[c]);
            if (wd >= 0) wdCols[wd] = c;
        }
        bool hasTimeCol = true;
        for (int wd = 0; wd < 7; ++wd) if (wdCols[wd] == 0) hasTimeCol = false;
        for (int ri = 1; ri < (int)rows.size(); ++ri) {
            auto& row = rows[ri];
            int h1 = 8, m1 = 0, h2 = 9, m2 = 40;
            if (hasTimeCol) RowTime(row, ri - 1, h1, m1, h2, m2);
            bool timeOk = true;
            if (hasTimeCol && !row.empty()) {
                timeOk = false;
                std::wstring head = row[0];
                int a, b, c, d;
                if (TimeRange(head, a, b, c, d)) timeOk = true;
                else if (PeriodRange(head, a, b, c, d)) timeOk = true;
            }
            for (int wd = 0; wd < 7; ++wd) {
                int col = wdCols[wd];
                if (col < 0 || col >= (int)row.size()) continue;
                if (Trim2(row[col]).empty()) continue;
                if (!timeOk) {
                    r.warnings.push_back(Fmt(L"第 %d 行没识别出时间，已使用默认节次时间", ri + 1));
                    RowTime(row, ri - 1, h1, m1, h2, m2);
                }
                MakeCourseEvent(r.events, r.warnings, row[col], wd, h1, m1, h2, m2, L"课程");
            }
        }
        if (!r.events.empty()) {
            r.ok = true;
            r.table = true;
            int weeks = app.store.settings.semester.enabled ? std::max(1, app.store.settings.semester.weeks) : 18;
            r.summary = Fmt(L"识别为课表：%d 门课 · 每周重复 %d 周", (int)r.events.size(), weeks);
            return r;
        }
        r.warnings.push_back(L"识别到表格，但没找到可导入的课程单元格");
    }
    // CSV/文本日程表：表头含 标题/开始/日期
    if (!rows.empty()) {
        auto& header = rows[0];
        int cTitle = -1, cStart = -1, cEnd = -1, cLoc = -1, cDesc = -1;
        for (int c = 0; c < (int)header.size(); ++c) {
            std::wstring h = Lower2(header[c]);
            if (cTitle < 0 && (h.find(L"标题") != std::wstring::npos || h.find(L"名称") != std::wstring::npos ||
                               h.find(L"title") != std::wstring::npos || h.find(L"课程") != std::wstring::npos)) cTitle = c;
            if (cStart < 0 && (h.find(L"开始") != std::wstring::npos || h.find(L"start") != std::wstring::npos ||
                               h.find(L"日期") != std::wstring::npos || h.find(L"时间") != std::wstring::npos)) cStart = c;
            if (cEnd < 0 && (h.find(L"结束") != std::wstring::npos || h.find(L"end") != std::wstring::npos)) cEnd = c;
            if (cLoc < 0 && (h.find(L"地点") != std::wstring::npos || h.find(L"教室") != std::wstring::npos ||
                             h.find(L"location") != std::wstring::npos)) cLoc = c;
            if (cDesc < 0 && (h.find(L"备注") != std::wstring::npos || h.find(L"说明") != std::wstring::npos)) cDesc = c;
        }
        if (cTitle >= 0 && cStart >= 0) {
            for (int ri = 1; ri < (int)rows.size(); ++ri) {
                auto& row = rows[ri];
                if (cTitle >= (int)row.size() || Trim2(row[cTitle]).empty()) continue;
                Event e;
                e.title = Trim2(row[cTitle]);
                std::wstring startS = cStart < (int)row.size() ? row[cStart] : L"";
                DateTime d;
                if (!DateInputParse(startS, Today(), d)) {
                    r.warnings.push_back(Fmt(L"第 %d 行的日期无法识别：%s", ri + 1, startS.c_str()));
                    continue;
                }
                int h1 = 0, m1 = 0, h2 = 1, m2 = 0;
                if (!ExtractClock(startS, h1, m1, h2, m2)) { h1 = 9; m1 = 0; h2 = 10; m2 = 0; }
                e.start = DateTime{ d.y, d.mo, d.d, h1, m1 };
                e.end = DateTime{ d.y, d.mo, d.d, h2, m2 };
                if (cEnd >= 0 && cEnd < (int)row.size()) {
                    std::wstring es = row[cEnd];
                    DateTime ed;
                    int eh1, em1, eh2, em2;
                    if (DateInputParse(es, d, ed)) {
                        if (ExtractClock(es, eh1, em1, eh2, em2)) e.end = DateTime{ ed.y, ed.mo, ed.d, eh1 };
                        else e.end = DateTime{ ed.y, ed.mo, ed.d, e.end.h, e.end.mi };
                    }
                }
                if (CompareDT(e.end, e.start) <= 0) e.end = AddMinutesDT(e.start, 90);
                if (cLoc >= 0 && cLoc < (int)row.size()) e.location = row[cLoc];
                if (cDesc >= 0 && cDesc < (int)row.size()) e.desc = row[cDesc];
                e.source = L"导入";
                r.events.push_back(e);
            }
            if (!r.events.empty()) {
                r.ok = true;
                r.table = false;
                r.summary = Fmt(L"识别为日程表：%d 条", (int)r.events.size());
                return r;
            }
        }
    }
    // 兜底：一行一条自然语言
    BulkParseResult br = ParseBulk(t, Today(), &app.store);
    if (!br.events.empty()) {
        r.events = std::move(br.events);
        r.warnings = br.errors;
        r.ok = true;
        r.table = false;
        r.summary = Fmt(L"识别为多行文本：%d 条", (int)r.events.size());
        return r;
    }
    if (r.warnings.empty()) r.warnings.push_back(L"没有识别出课表或日程，试试复制教务系统课表表格，或上传 CSV/HTML");
    r.summary = L"无法识别";
    return r;
}

static int CountConflicts(const std::vector<Event>& events) {
    int n = 0;
    for (auto& e : events) {
        for (auto& old : app.store.events) {
            if (old.title != e.title) continue;
            bool sameStart = CompareDT(old.start, e.start) == 0 && old.start.h == e.start.h && old.start.mi == e.start.mi;
            bool sameWeekly = old.recur.freq == RF_WEEKLY && e.recur.freq == RF_WEEKLY &&
                              !old.recur.byDay.empty() && !e.recur.byDay.empty() &&
                              old.recur.byDay[0] == e.recur.byDay[0] && old.start.h == e.start.h && old.start.mi == e.start.mi;
            if (sameStart || sameWeekly) { ++n; break; }
        }
    }
    return n;
}

void LoadSmartImport(const std::wstring& file, const std::wstring& text, bool fromClipboard, const std::wstring& label) {
    app.import.open = true;
    app.import.file = file;
    app.import.label = label;
    app.import.tableMode = false;
    app.import.tableEvents.clear();
    app.import.tableWarnings.clear();
    app.import.summary.clear();
    app.import.conflicts = 0;
    app.import.parsed = IcsParseResult{};
    app.import.error.clear();
    std::wstring body = text;
    if (!file.empty()) {
        bool ok = false;
        std::string raw = ReadFileBytes(file, &ok);
        if (!ok) { app.import.error = L"文件读取失败"; app.overlay = OV_IMPORT; UIInvalidate(); return; }
        body = DecodeBytes(raw);
    }
    if (body.empty()) {
        app.import.error = L"没有可导入的内容";
        app.overlay = OV_IMPORT;
        UIInvalidate();
        return;
    }
    if (Has2(body, L"BEGIN:VCALENDAR")) {
        app.import.parsed = ParseIcs(WideToUtf8(body));
        app.import.summary = Fmt(L"ICS 日历：%d 个日程", (int)app.import.parsed.events.size());
        app.import.tableMode = false;
        if (app.import.parsed.events.empty()) app.import.error = L"ICS 里没有解析到日程";
    } else {
        TableImportResult tr = ParseScheduleText(body);
        app.import.tableMode = true;
        app.import.tableEvents = tr.events;
        app.import.tableWarnings = tr.warnings;
        app.import.summary = tr.summary;
        app.import.conflicts = CountConflicts(tr.events);
        if (!tr.ok) app.import.error = tr.summary;
    }
    if (fromClipboard) app.import.mode = 0;
    app.overlay = OV_IMPORT;
    DebugLog(L"智能导入: %s table=%d events=%d conflicts=%d",
             app.import.summary.c_str(), app.import.tableMode ? 1 : 0,
             (int)(app.import.tableMode ? app.import.tableEvents.size() : app.import.parsed.events.size()),
             app.import.conflicts);
    UIInvalidate();
}

void OpenSmartImportFromFile() {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"课表/日历 (*.ics;*.csv;*.txt;*.html;*.htm)\0*.ics;*.csv;*.txt;*.html;*.htm\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    std::wstring path = file;
    size_t p = path.find_last_of(L"\\/");
    std::wstring name = p == std::wstring::npos ? path : path.substr(p + 1);
    LoadSmartImport(path, L"", false, name);
}

namespace {
void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}
bool TableConflict(const Event& e) {
    for (auto& old : app.store.events) {
        if (old.title != e.title) continue;
        if (CompareDT(old.start, e.start) == 0 && old.start.h == e.start.h && old.start.mi == e.start.mi) return true;
        if (old.recur.freq == RF_WEEKLY && e.recur.freq == RF_WEEKLY && !old.recur.byDay.empty() && !e.recur.byDay.empty() &&
            old.recur.byDay[0] == e.recur.byDay[0] && old.start.h == e.start.h && old.start.mi == e.start.mi) return true;
    }
    return false;
}
std::wstring SampleWhen(const Event& e) {
    static const wchar_t* wd[7] = { L"周日", L"周一", L"周二", L"周三", L"周四", L"周五", L"周六" };
    int d = DayOfWeek(e.start.y, e.start.mo, e.start.d);
    return Fmt(L"%s %02d:%02d-%02d:%02d", wd[d], e.start.h, e.start.mi, e.end.h, e.end.mi);
}
}  // namespace

void OpenOcrFromImageFile() {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"图片 (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    std::wstring path = file;
    size_t p = path.find_last_of(L"\\\\/");
    std::wstring name = p == std::wstring::npos ? path : path.substr(p + 1);
    ShowToast(L"正在 OCR 识别…", 1200);
    std::wstring err;
    if (WindowsOcrImage(path, L"zh-Hans-CN", app.import.ocrText, err)) {
        app.import.ocrSource = L"OCR · " + name;
        app.overlay = OV_NONE;
        OpenOcrResult();
    } else {
        ShowToast(err.empty() ? L"OCR 失败" : err, 5000);
    }
}

void DrawSmartImportOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(600);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));

    Txt(g, L"智能导入", S(17), true, th.text, RectF(x + S(24), y + S(16), S(240), S(28)));
    Txt(g, L"教务课表 HTML / CSV / 粘贴文本 / ICS · 导入前预览，绝不删除或静默覆盖已有日程", S(9), false, th.dim2,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        app.import.open = false;
        UIInvalidate();
    });

    bool table = app.import.tableMode;
    int count = table ? (int)app.import.tableEvents.size() : (int)app.import.parsed.events.size();
    std::wstring label = app.import.label.empty() ? L"（未选择来源）" : app.import.label;
    Txt(g, L"来源：" + label, S(9.5f), false, th.dim, RectF(x + S(24), y + S(72), pw - S(48), S(18)), ALIGN_LEFT, false, true);

    RectF pv(x + S(24), y + S(96), pw - S(48), S(194));
    FillRoundRect(g, pv, S(10), WithAlpha(th.bg, 170));
    StrokeRoundRect(g, pv, S(10), th.borderSoft, S(1));
    Txt(g, app.import.summary.empty() ? (app.import.error.empty() ? L"等待导入内容…" : app.import.error) : app.import.summary,
        S(13.5f), true, count > 0 ? th.accent : (app.import.error.empty() ? th.dim : th.danger),
        RectF(pv.X + S(14), pv.Y + S(12), pv.Width - S(28), S(24)));
    if (count > 0) {
        Txt(g, Fmt(L"%d 条待导入 · %d 条可能与已有日程重复", count, app.import.conflicts), S(10), false,
            app.import.conflicts ? th.warn : th.dim, RectF(pv.X + S(14), pv.Y + S(40), pv.Width - S(28), S(18)));
        float sy = pv.Y + S(66);
        if (table) {
            int n = 0;
            for (auto& e : app.import.tableEvents) {
                if (n >= 3) break;
                std::wstring title = e.title;
                Txt(g, Fmt(L"· %s", title.c_str()), S(10), true, th.text,
                    RectF(pv.X + S(14), sy, pv.Width - S(150), S(18)), ALIGN_LEFT, false, true);
                Txt(g, SampleWhen(e), S(9), false, th.dim2,
                    RectF(RX(pv) - S(138), sy, S(124), S(18)), ALIGN_RIGHT);
                sy += S(20);
                ++n;
            }
        } else {
            if (app.import.parsed.hasRange)
                Txt(g, Fmt(L"日期范围：%s → %s", ISODate(app.import.parsed.minDate).c_str(), ISODate(app.import.parsed.maxDate).c_str()),
                    S(9.5f), false, th.dim, RectF(pv.X + S(14), sy, pv.Width - S(28), S(18)));
            if (!app.import.parsed.calendarName.empty())
                Txt(g, L"日历：" + app.import.parsed.calendarName, S(9.5f), false, th.dim, RectF(pv.X + S(14), sy, pv.Width - S(28), S(18)));
            sy += S(24);
            int n = 0;
            for (auto& e : app.import.parsed.events) {
                if (n >= 3) break;
                Txt(g, Fmt(L"· %s · %s", e.title.c_str(), DateTimeHuman(e.start).c_str()), S(9.5f), false, th.dim,
                    RectF(pv.X + S(14), sy, pv.Width - S(28), S(18)), ALIGN_LEFT, false, true);
                sy += S(20);
                ++n;
            }
        }
    }
    if (!app.import.error.empty() && count > 0)
        Txt(g, app.import.error, S(9), false, th.danger, RectF(pv.X + S(14), BY(pv) - S(40), pv.Width - S(28), S(18)));
    Txt(g, L"PDF 课表：先截图或另存为图片，再点「截图 OCR」识别", S(9), false, WithAlpha(th.dim2, 210),
        RectF(pv.X + S(14), BY(pv) - S(22), pv.Width - S(28), S(18)));

    // 来源按钮
    float by = y + S(304);
    UI::Btn(g, RectF(x + S(24), by, S(130), S(32)), CMD_IMPORT_CANCEL, L"选择文件…", 1, ICO_FOLDER, []() {
        app.overlay = OV_NONE;
        app.import.open = false;
        OpenSmartImportFromFile();
    });
    UI::Btn(g, RectF(x + S(164), by, S(150), S(32)), CMD_IMPORT_SMART, L"从剪贴板粘贴", 1, ICO_COPY, []() {
        std::wstring t = ReadClipboardText();
        if (Trim(t).empty()) { ShowToast(L"剪贴板里没有文本或表格"); return; }
        LoadSmartImport(L"", t, true, L"剪贴板");
    });
    UI::Btn(g, RectF(x + S(324), by, S(110), S(32)), CMD_IMPORT_SMART, L"截图 OCR…", 1, ICO_SEARCH, []() {
        OpenOcrFromImageFile();
    });
    Txt(g, L"支持 Excel 粘贴、网页表格、CSV、.ics", S(8.5f), false, th.dim2,
        RectF(x + S(444), by, pw - S(468), S(32)), ALIGN_LEFT, false, true);

    // 合并模式
    Txt(g, L"重复处理", S(10.5f), false, th.dim2, RectF(x + S(24), y + S(352), S(120), S(18)));
    if (table) {
        UI::Chip(g, RectF(x + S(24), y + S(374), S(180), S(30)), CMD_IMPORT_MODE_BASE + 0, L"跳过重复（推荐）",
                 app.import.mode == 0, th.accent, []() { app.import.mode = 0; UIInvalidate(); }, 10.5f);
        UI::Chip(g, RectF(x + S(214), y + S(374), S(150), S(30)), CMD_IMPORT_MODE_BASE + 1, L"全部追加",
                 app.import.mode != 0, th.accent, []() { app.import.mode = 1; UIInvalidate(); }, 10.5f);
        Txt(g, L"重复判定：同标题 + 同一星期/时间；导入只新增，不修改已有日程。", S(9), false, th.dim2,
            RectF(x + S(24), y + S(412), pw - S(48), S(18)));
    } else {
        const wchar_t* modes[3] = { L"按 UID 合并（推荐）", L"全部追加", L"跳过重复" };
        for (int i = 0; i < 3; ++i)
            UI::Chip(g, RectF(x + S(24) + i * S(200), y + S(374), S(190), S(30)), CMD_IMPORT_MODE_BASE + i, modes[i],
                     app.import.mode == i, th.accent, [i]() { app.import.mode = i; UIInvalidate(); }, 10.5f);
        Txt(g, L"合并只更新同 UID 的已有日程；任何模式都不会删除本地日程。", S(9), false, th.dim2,
            RectF(x + S(24), y + S(412), pw - S(48), S(18)));
    }

    // 智能整理
    RectF sm(x + S(24), y + S(444), pw - S(48), S(24));
    {
        bool on = app.import.smart;
        RectF cb(sm.X, sm.Y + S(2), S(18), S(18));
        FillRoundRect(g, cb, S(5), on ? th.accent : WithAlpha(th.panel2, 200));
        StrokeRoundRect(g, cb, S(5), on ? th.accent : th.border, S(1));
        if (on) Icon(g, ICO_CHECK, RectF(cb.X + S(3), cb.Y + S(3), S(12), S(12)), th.onAccent, 2.0f);
        Txt(g, L"智能整理：自动打「课程」标签、识别课程配色、合并重复（导入后可撤销）", S(9.5f), false,
            on ? th.text : th.dim, RectF(sm.X + S(26), sm.Y, sm.Width - S(26), S(22)));
        AddHit(sm.X, sm.Y, sm.Width, sm.Height, CMD_IMPORT_SMART, []() {
            app.import.smart = !app.import.smart;
            UIInvalidate();
        });
    }

    // 底部
    float fy = y + ph - S(54);
    UI::Btn(g, RectF(x + S(24), fy, S(118), S(34)), CMD_IMPORT_CANCEL, L"订阅管理…", 1, ICO_REFRESH, []() {
        app.overlay = OV_NONE;
        app.import.open = false;
        OpenSubscriptions();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90), fy, S(90), S(34)), CMD_IMPORT_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        app.import.open = false;
        UIInvalidate();
    });
    bool can = count > 0;
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(10) - S(120), fy, S(120), S(34)), CMD_IMPORT_OK,
            L"导入预览中的日程", can ? 0 : 1, -1, []() {
        bool table = app.import.tableMode;
        int n = table ? (int)app.import.tableEvents.size() : (int)app.import.parsed.events.size();
        if (n == 0) { ShowToast(L"没有可导入的日程"); return; }
        app.store.Snapshot();
        std::vector<std::string> ids;
        int added = 0, updated = 0, skipped = 0;
        if (table) {
            for (auto& e : app.import.tableEvents) {
                if (app.import.mode == 0 && TableConflict(e)) { ++skipped; continue; }
                std::string id = app.store.AddEvent(e);
                ids.push_back(id);
                ++added;
            }
        } else {
            std::string data = WideToUtf8(app.import.rawBody);
            IcsImportResult r = ImportIcs(app.store, data, app.import.mode, L"ICS 导入");
            added = r.added;
            updated = r.updated;
            skipped = r.skipped;
            ids = r.addedIds;
            ids.insert(ids.end(), r.updatedIds.begin(), r.updatedIds.end());
        }
        std::wstring note;
        if (app.import.smart && !ids.empty()) {
            SmartOptions so;
            so.onlyIds = ids;
            so.onlyUntagged = false;
            SmartResult sr = SmartOrganize(app.store, so);
            if (!sr.summary.empty()) note = L" · " + sr.summary;
        }
        app.store.Touch();
        InvalidateOccs();
        DoSaveStore();
        app.import.open = false;
        app.overlay = OV_NONE;
        ShowToast(Fmt(L"导入完成：新增 %d，更新 %d，跳过 %d", added, updated, skipped) + note, 6000);
        UIInvalidate();
    });
}