// quick.cpp - 自然语言快速添加 / 批量文本解析
#include "quick.h"
#include "app.h"
#include "util.h"
#include <vector>
#include <algorithm>
#include <cwchar>

namespace {

static int WeekdayChar(wchar_t c) {
    switch (c) {
    case L'日': case L'天': return 0;
    case L'一': return 1;
    case L'二': return 2;
    case L'三': return 3;
    case L'四': return 4;
    case L'五': return 5;
    case L'六': return 6;
    default: return -1;
    }
}

static bool FindAndErase(std::wstring& work, const std::wstring& token) {
    size_t p = work.find(token);
    if (p == std::wstring::npos) return false;
    work.erase(p, token.size());
    return true;
}

// 从 work 中移除 "15:00-16:00" / "8点到9点40" / "下午3点" 等
static bool ExtractTimeRange(std::wstring& work, DateTime& startDT, DateTime& endDT, bool& hasEnd) {
    hasEnd = false;
    // 先试范围：时间 A 到/~/-
    std::vector<std::wstring> seps = { L"到", L"~", L"～", L"-", L"至" };
    for (auto& sep : seps) {
        size_t p = work.find(sep);
        if (p == std::wstring::npos || p == 0) continue;
        // 左右各自尝试取时间
        std::wstring left = work.substr(0, p);
        std::wstring right = work.substr(p + sep.size());
        // left 取最后一个空格之后的部分
        size_t sp = left.find_last_of(L" \t");
        std::wstring lt = (sp == std::wstring::npos) ? left : left.substr(sp + 1);
        size_t sp2 = right.find_first_of(L" \t");
        std::wstring rt = (sp2 == std::wstring::npos) ? right : right.substr(0, sp2);
        int h1 = 0, m1 = 0, h2 = 0, m2 = 0;
        if (TimeInputParse(lt, h1, m1) && TimeInputParse(rt, h2, m2)) {
            // 继承上下午
            if (Contains(lt, L"下午") || Contains(lt, L"晚上") || Contains(lt, L"傍晚")) {
                if (h2 < 12) h2 += 12;
            }
            startDT.h = h1; startDT.mi = m1;
            endDT.h = h2; endDT.mi = m2;
            if (h2 < h1 || (h2 == h1 && m2 <= m1)) hasEnd = false;
            else hasEnd = true;
            // 删除范围文本
            std::wstring eraseFrom = (sp == std::wstring::npos) ? std::wstring() : work.substr(0, sp + 1);
            size_t eraseStart = eraseFrom.size();
            size_t eraseEnd = p + sep.size() + ((sp2 == std::wstring::npos) ? right.size() : sp2);
            work.erase(eraseStart, eraseEnd - eraseStart);
            return true;
        }
    }
    // 单个时间：在整个文本里找 HH:MM / N点 / 上午N点 等
    std::vector<std::wstring> candidates;
    // H:MM / HH:MM 形式
    for (size_t i = 0; i + 3 < work.size(); ++i) {
        bool twoDigit = (i + 4 < work.size() && iswdigit(work[i]) && iswdigit(work[i + 1]) && work[i + 2] == L':' &&
                         iswdigit(work[i + 3]) && iswdigit(work[i + 4]));
        bool oneDigit = (iswdigit(work[i]) && work[i + 1] == L':' && iswdigit(work[i + 2]) && iswdigit(work[i + 3]) &&
                         (i == 0 || !iswdigit(work[i - 1])));
        if (twoDigit || oneDigit) {
            size_t len = twoDigit ? 5 : 4;
            // 向前包含修饰词
            size_t st = i;
            std::wstring prefix = work.substr(0, i);
            static const wchar_t* mods[] = { L"上午", L"下午", L"早上", L"晚上", L"凌晨", L"中午" };
            for (auto* mod : mods) {
                size_t mp = prefix.rfind(mod);
                if (mp != std::wstring::npos && i - mp <= 3) { st = mp; break; }
            }
            candidates.push_back(work.substr(st, i + len - st));
            break;
        }
    }
    if (candidates.empty()) {
        // N点 / N点半
        size_t dot = work.find(L'点');
        if (dot != std::wstring::npos && dot > 0) {
            // 找到点前面的数字
            size_t st = dot;
            while (st > 0 && (iswdigit(work[st - 1]) || work[st - 1] == L':')) --st;
            // 向前带修饰词
            static const wchar_t* mods[] = { L"上午", L"下午", L"早上", L"晚上", L"凌晨", L"中午" };
            for (auto* mod : mods) {
                size_t mp = work.rfind(mod, st);
                if (mp != std::wstring::npos && st - mp <= 3) { st = mp; break; }
            }
            size_t en = dot + 1;
            if (en < work.size() && work[en] == L'半') ++en;
            else {
                size_t q = en;
                while (q < work.size() && iswdigit(work[q])) ++q;
                en = q;
            }
            candidates.push_back(work.substr(st, en - st));
        }
    }
    if (candidates.empty()) return false;
    std::wstring token = candidates[0];
    int h = 0, m = 0;
    if (!TimeInputParse(token, h, m)) return false;
    startDT.h = h; startDT.mi = m;
    FindAndErase(work, token);
    return true;
}

static bool ExtractDate(std::wstring& work, const DateTime& base, DateTime& outDate) {
    DateTime today = (base.y != 0) ? base : Today();
    today.h = 0; today.mi = 0;
    // 明确的日期
    struct Found { size_t pos; size_t len; DateTime dt; };
    std::vector<Found> found;
    // YYYY-MM-DD / YYYY/MM/DD
    for (size_t i = 0; i + 10 <= work.size(); ++i) {
        int y, mo, d;
        wchar_t sep1 = work[i + 4], sep2 = work[i + 7];
        if (!iswdigit(work[i])) continue;
        if (swscanf_s(work.substr(i, 10).c_str(), L"%d-%d-%d", &y, &mo, &d) == 3 && sep1 == L'-' && sep2 == L'-') {
            found.push_back(Found{ i, 10, MakeDT(y, mo, d) });
            break;
        }
        if (swscanf_s(work.substr(i, 10).c_str(), L"%d/%d/%d", &y, &mo, &d) == 3 && sep1 == L'/' && sep2 == L'/') {
            found.push_back(Found{ i, 10, MakeDT(y, mo, d) });
            break;
        }
    }
    if (found.empty()) {
        // M月D日
        size_t p = work.find(L'月');
        if (p != std::wstring::npos) {
            size_t st = p;
            while (st > 0 && (iswdigit(work[st - 1]) || work[st - 1] == L'/')) --st;
            size_t en = p + 1;
            while (en < work.size() && (iswdigit(work[en]))) ++en;
            if (en < work.size() && (work[en] == L'日' || work[en] == L'号')) ++en;
            if (en > p + 1) {
                int mo = 0, d = 0;
                std::wstring sub = work.substr(st, en - st);
                if (swscanf_s(sub.c_str(), L"%d月%d", &mo, &d) == 2 || swscanf_s(sub.c_str(), L"%d", &mo) == 1) {
                    if (d == 0) d = 1;
                    if (mo >= 1 && mo <= 12 && d >= 1 && d <= 31)
                        found.push_back(Found{ st, en - st, MakeDT(today.y, mo, d) });
                }
            }
        }
    }
    if (found.empty()) {
        // 10/20
        for (size_t i = 0; i + 5 <= work.size(); ++i) {
            int a = 0, b = 0;
            if (swscanf_s(work.substr(i, 5).c_str(), L"%d/%d", &a, &b) == 2 && a >= 1 && a <= 12 && b >= 1 && b <= 31) {
                found.push_back(Found{ i, 5, MakeDT(today.y, a, b) });
                break;
            }
        }
    }
    if (found.empty()) {
        struct KW { const wchar_t* s; int delta; };
        static const KW kws[] = {
            { L"大后天", 3 }, { L"后天", 2 }, { L"明天", 1 }, { L"今天", 0 }, { L"今晚", 0 },
            { L"明早", 1 }, { L"明晚", 1 }, { L"昨天", -1 },
        };
        for (auto& kw : kws) {
            size_t p = work.find(kw.s);
            if (p != std::wstring::npos) {
                DateTime d = AddDaysDT(today, kw.delta);
                std::wstring k = kw.s;
                // 今晚/明晚默认晚上，稍后时间解析处理
                work.erase(p, k.size());
                outDate = d;
                return true;
            }
        }
    }
    if (!found.empty()) {
        auto& f = found[0];
        outDate = f.dt;
        work.erase(f.pos, f.len);
        return true;
    }
    // 周X
    size_t p = work.find(L'周');
    if (p == std::wstring::npos) p = work.find(L"星期");
    if (p == std::wstring::npos) p = work.find(L"礼拜");
    if (p != std::wstring::npos) {
        size_t cs = p + (work[p] == L'周' ? 1 : 2);
        if (cs < work.size()) {
            int wd = WeekdayChar(work[cs]);
            if (wd >= 0) {
                bool nextWeek = p > 0 && work[p - 1] == L'下';
                int cur = DayOfWeek(today.y, today.mo, today.d);
                int diff = (wd - cur + 7) % 7;
                if (nextWeek) diff += 7;
                outDate = AddDaysDT(today, diff);
                size_t start = nextWeek ? p - 1 : p;
                work.erase(start, cs + 1 - start);
                return true;
            }
        }
    }
    return false;
}

static int CnDigit(wchar_t c) {
    switch (c) {
    case L'一': return 1;
    case L'二': return 2;
    case L'两': return 2;
    case L'三': return 3;
    case L'四': return 4;
    case L'五': return 5;
    case L'六': return 6;
    case L'七': return 7;
    case L'八': return 8;
    case L'九': return 9;
    case L'十': return 10;
    default: return 0;
    }
}
static int ReadSmallNumber(const std::wstring& work, size_t& i) {
    size_t st = i;
    while (i < work.size() && iswdigit(work[i])) ++i;
    if (i > st) return _wtoi(work.substr(st, i - st).c_str());
    int d = i < work.size() ? CnDigit(work[i]) : 0;
    if (d > 0) ++i;
    return d;
}
static bool IsDaySep(wchar_t c) {
    return c == L'、' || c == L'，' || c == L',' || c == L' ' || c == L'和' || c == L'及' || c == L'与';
}
static void ExtractRecurrence(std::wstring& work, Event& ev, bool& hasRecur) {
    hasRecur = false;
    struct RF { const wchar_t* key; int freq; };
    static const RF rfs[] = {
        { L"每天", RF_DAILY }, { L"每日", RF_DAILY }, { L"每周", RF_WEEKLY }, { L"每星期", RF_WEEKLY },
        { L"每礼拜", RF_WEEKLY }, { L"每个月", RF_MONTHLY }, { L"每月", RF_MONTHLY }, { L"每年", RF_YEARLY },
    };
    for (auto& r : rfs) {
        size_t p = work.find(r.key);
        if (p == std::wstring::npos) continue;
        size_t after = p + wcslen(r.key);
        ev.recur.freq = r.freq;
        hasRecur = true;
        if (r.freq == RF_WEEKLY) {
            // 每周一三五 / 每周一、三、五
            while (after < work.size()) {
                while (after < work.size() && IsDaySep(work[after])) ++after;
                int wd = (after < work.size()) ? WeekdayChar(work[after]) : -1;
                if (wd < 0) break;
                ev.recur.byDay.push_back(wd);
                ++after;
            }
            work.erase(p, after - p);
        } else if (r.freq == RF_MONTHLY) {
            // 每月5号 / 每月十五日
            size_t i = after;
            while (i < work.size() && (work[i] == L' ' || work[i] == L'的')) ++i;
            size_t numStart = i;
            int d = ReadSmallNumber(work, i);
            if (d > 0 && i < work.size() && (work[i] == L'号' || work[i] == L'日')) {
                ev.recur.byMonthDay = std::min(31, d);
                work.erase(p, i + 1 - p);
            } else {
                work.erase(p, wcslen(r.key));
            }
        } else if (r.freq == RF_YEARLY) {
            // 每年10月8日 / 每年十月八日（仅支持阿拉伯数字）
            size_t i = after;
            while (i < work.size() && (work[i] == L' ' || work[i] == L'的')) ++i;
            int mo = 0, dd = 0;
            size_t st = i;
            while (i < work.size() && iswdigit(work[i])) ++i;
            if (i > st) mo = _wtoi(work.substr(st, i - st).c_str());
            if (mo > 0 && i < work.size() && work[i] == L'月') {
                ++i;
                size_t ds = i;
                while (i < work.size() && iswdigit(work[i])) ++i;
                if (i > ds && i < work.size() && (work[i] == L'日' || work[i] == L'号')) {
                    dd = _wtoi(work.substr(ds, i - ds).c_str());
                    ++i;
                }
                if (dd > 0) {
                    ev.recur.byMonth = std::min(12, mo);
                    ev.recur.byMonthDay = std::min(31, dd);
                    work.erase(p, i - p);
                } else {
                    work.erase(p, wcslen(r.key));
                }
            } else {
                work.erase(p, wcslen(r.key));
            }
        } else {
            work.erase(p, wcslen(r.key));
        }
        return;
    }
    // 每 N 天/周（阿拉伯或中文数字）
    size_t mp = work.find(L"每");
    if (mp != std::wstring::npos) {
        size_t i = mp + 1;
        int n = ReadSmallNumber(work, i);
        if (n > 0 && i < work.size()) {
            if (work[i] == L'天' || work[i] == L'日') { ev.recur.freq = RF_DAILY; ev.recur.interval = std::max(1, n); work.erase(mp, i + 1 - mp); hasRecur = true; return; }
            if (work[i] == L'周' || work[i] == L'星期') { ev.recur.freq = RF_WEEKLY; ev.recur.interval = std::max(1, n); work.erase(mp, i + 1 - mp); hasRecur = true; return; }
            if (work[i] == L'个' && i + 2 < work.size() && work[i + 1] == L'月') { ev.recur.freq = RF_MONTHLY; ev.recur.interval = std::max(1, n); work.erase(mp, i + 2 - mp); hasRecur = true; return; }
        }
    }
}
static bool ExtractReminder(std::wstring& work, Event& ev) {
    size_t p = work.find(L"提前");
    if (p == std::wstring::npos) p = work.find(L"提醒");
    if (p == std::wstring::npos) return false;
    size_t i = p + 2;
    while (i < work.size() && (work[i] == L' ' || work[i] == L':' || work[i] == L'：')) ++i;
    int n = 0;
    if (i + 1 < work.size() && work[i] == L'半' && work[i + 1] == L'小') { n = 30; i += 2; }
    else if (i + 1 < work.size() && work[i] == L'一' && work[i + 1] == L'小') { n = 60; i += 2; }
    else {
        int d = ReadSmallNumber(work, i);
        if (d <= 0) return false;
        n = d;
        if (i < work.size() && (work[i] == L'小' || work[i] == L'时' || work[i] == L'个')) {
            n *= 60;
            if (i < work.size() && work[i] == L'个') ++i;
            if (i < work.size() && work[i] == L'小') ++i;
        }
    }
    while (i < work.size()) {
        wchar_t c = work[i];
        if (c == L'钟' || c == L'分' || c == L'时' || c == L'小' || c == L'前' || c == L'之' || c == L'个' || c == L'的') ++i;
        else break;
    }
    if (n <= 0 || n > 24 * 60) return false;
    ev.alarms.push_back(Alarm{ -n, true, L"" });
    work.erase(p, i - p);
    return true;
}
static bool ExtractLocation(std::wstring& work, Event& ev) {
    // 1) @xxx
    size_t at = work.find(L'@');
    if (at != std::wstring::npos) {
        size_t j = at + 1;
        std::wstring loc;
        while (j < work.size() && !iswspace(work[j])) loc += work[j++];
        if (!loc.empty()) {
            ev.location = loc;
            work.erase(at, j - at);
            return true;
        }
    }
    // 2) “地点 xxx / 教室 xxx”
    static const wchar_t* keys[] = { L"地点", L"教室" };
    for (auto* k : keys) {
        size_t p = work.find(k);
        if (p == std::wstring::npos) continue;
        size_t i = p + wcslen(k);
        while (i < work.size() && (work[i] == L' ' || work[i] == L':' || work[i] == L'：')) ++i;
        size_t st = i;
        while (i < work.size() && !iswspace(work[i]) && work[i] != L',' && work[i] != L'，' &&
               work[i] != L'。' && work[i] != L';' && work[i] != L'；') ++i;
        if (i > st) {
            ev.location = work.substr(st, i - st);
            work.erase(p, i - p);
            return true;
        }
    }
    // 3) “在 xxx / 于 xxx”：只接受“在 ”空格后、或“在A306”字母数字开头，避免吃掉普通句子
    for (auto* k : { L"在", L"于" }) {
        size_t p = work.find(k);
        if (p == std::wstring::npos) continue;
        size_t i = p + 1;
        bool spaced = (i < work.size() && iswspace(work[i]));
        while (i < work.size() && iswspace(work[i])) ++i;
        if (i >= work.size()) continue;
        bool asciiStart = iswalnum(work[i]) || work[i] == L'@';
        if (!spaced && !asciiStart) continue;
        size_t st = i;
        while (i < work.size() && !iswspace(work[i]) && work[i] != L',' && work[i] != L'，' && work[i] != L'。') ++i;
        if (i - st > 24) continue;
        std::wstring loc = work.substr(st, i - st);
        if (loc.empty()) continue;
        // 排除“在 9点”这类时间
        if (iswdigit(loc[0]) && (loc.size() == 1 || loc[1] == L':' || loc[1] == L'点' || loc[1] == L'时')) continue;
        ev.location = loc;
        work.erase(p, i - p);
        return true;
    }
    return false;
}

} // namespace

QuickResult ParseQuick(const std::wstring& text, const DateTime& baseDate, const Store* store) {
    QuickResult r;
    std::wstring work = Trim(text);
    if (work.empty()) { r.error = L"内容为空"; return r; }
    Event ev;
    ev.source = L"快速添加";
    bool allDay = false;
    if (FindAndErase(work, L"全天")) allDay = true;
    // 重复
    bool hasRecur = false;
    ExtractRecurrence(work, ev, hasRecur);
    // 提醒（提前 10 分钟 / 提前半小时 / 提前1小时）
    ExtractReminder(work, ev);
    // “标签 xxx” 转成 #xxx，交给下面的 # 标签流程
    {
        size_t p = work.find(L"标签");
        if (p != std::wstring::npos) {
            size_t i = p + 2;
            while (i < work.size() && (work[i] == L' ' || work[i] == L':' || work[i] == L'：')) ++i;
            size_t st = i;
            while (i < work.size() && !iswspace(work[i]) && work[i] != L',' && work[i] != L'，' && work[i] != L'#') ++i;
            if (i > st) work.replace(p, i - p, L"#" + work.substr(st, i - st));
        }
    }
    // 日期
    DateTime day = baseDate;
    if (day.y == 0) day = Today();
    bool hasDate = ExtractDate(work, day, day);
    if (hasDate && !IsSupportedDate(day)) { r.error = L"日期范围仅支持 1901–2100 年"; return r; }
    // 时间
    DateTime startDT = day;
    DateTime endDT = day;
    bool hasEnd = false;
    bool hasTime = ExtractTimeRange(work, startDT, endDT, hasEnd);
    // 标签
    for (size_t i = 0; i < work.size(); ++i) {
        if (work[i] != L'#' && work[i] != L'＃') continue;
        size_t j = i + 1;
        std::wstring name;
        while (j < work.size() && !iswspace(work[j]) && work[j] != L'#' && work[j] != L'＃' && work[j] != L'!') name += work[j++];
        i = j - 1;
        if (name.empty()) continue;
        std::string id = TagIdForName(name);
        bool resolved = false;
        if (store) {
            for (auto& t : store->tags) {
                if (t.name == name) { id = t.id; resolved = true; break; }
            }
        }
        ev.tags.push_back(id);
        if (!resolved) r.tagNames[id] = name;
    }
    // 去除标签文本
    {
        std::wstring cleaned;
        bool inTag = false;
        for (wchar_t c : work) {
            if (c == L'#' || c == L'＃') { inTag = true; continue; }
            if (inTag) {
                if (iswspace(c) || c == L'!' || c == L'！') { inTag = false; cleaned += c; }
                continue;
            }
            cleaned += c;
        }
        work = cleaned;
    }
    // 优先级
    int prio = PRIO_NONE;
    size_t bang = work.find(L"!!");
    if (bang != std::wstring::npos) { prio = PRIO_HIGH; work.erase(bang, 2); }
    else {
        size_t b2 = work.find(L'!');
        if (b2 != std::wstring::npos) { prio = PRIO_MED; work.erase(b2, 1); }
        size_t b3 = work.find(L'！');
        if (b3 != std::wstring::npos) { prio = PRIO_HIGH; work.erase(b3, 1); }
    }
    ev.priority = prio;
    // 地点：@xxx / 地点 xxx / 教室 xxx / 在 xxx
    ExtractLocation(work, ev);
    // 标题
    std::wstring title = Trim(work);
    // 去掉首尾多余分隔符
    while (!title.empty() && (title.front() == L' ' || title.front() == L',' || title.front() == L'，' ||
                              title.front() == L'-' || title.front() == L'：' || title.front() == L':'))
        title.erase(title.begin());
    while (!title.empty() && (title.back() == L' ' || title.back() == L',' || title.back() == L'，' ||
                              title.back() == L'-' || title.back() == L'：' || title.back() == L':' || title.back() == L'!' || title.back() == L'！'))
        title.pop_back();
    if (title.empty()) { r.error = L"没找到标题，试试「明天 9:00 交作业 #作业」"; return r; }
    ev.title = title;
    // 时间组装
    if (allDay) {
        ev.allDay = true;
        ev.start = MakeDT(day.y, day.mo, day.d, 0, 0);
        ev.end = AddDaysDT(ev.start, 1);
    } else {
        Date endDate = FromSerial(DaySerial(day.y, day.mo, day.d) + (hasTime ? 0 : 0));
        (void)endDate;
        ev.start = MakeDT(day.y, day.mo, day.d, hasTime ? startDT.h : 9, hasTime ? startDT.mi : 0);
        if (hasTime && hasEnd) {
            DateTime e = MakeDT(day.y, day.mo, day.d, endDT.h, endDT.mi);
            if (CompareDT(e, ev.start) <= 0) e = AddDaysDT(e, 1);
            ev.end = e;
        } else if (hasTime) {
            ev.end = AddMinutesDT(ev.start, 60);
        } else {
            ev.end = AddMinutesDT(ev.start, 60);
        }
        ev.allDay = false;
    }
    if (!hasDate && !hasTime && !hasRecur) {
        // 时间词都没有：默认今天 9:00（但允许纯标题创建）
    }
    // 重复的 until 不设，保持无限
    if (hasRecur) {
        if (ev.recur.byDay.empty() && ev.recur.freq == RF_WEEKLY)
            ev.recur.byDay.push_back(DayOfWeek(ev.start.y, ev.start.mo, ev.start.d));
    }
    // 摘要
    std::wstring when = ev.allDay ? (ISODate(ev.start) + L" 全天")
                                  : (DateTimeHuman(ev.start) + L" – " + TimeStr(ev.end));
    std::wstring summary = when;
    if (hasRecur) {
        const wchar_t* f = ev.recur.freq == RF_DAILY ? L"每天" : ev.recur.freq == RF_WEEKLY ? L"每周" :
                           ev.recur.freq == RF_MONTHLY ? L"每月" : L"每年";
        summary += L" · " + std::wstring(f);
        if (!ev.recur.byDay.empty() && ev.recur.freq == RF_WEEKLY) {
            summary += L" ";
            for (int wd : ev.recur.byDay) summary += WeekdayShortCN(wd);
        }
    }
    if (!ev.tags.empty()) {
        summary += L" · 标签 ";
        for (auto& t : ev.tags) {
            auto it = r.tagNames.find(t);
            summary += L"#" + (it != r.tagNames.end() ? it->second : (store ? store->TagName(t) : Utf8ToWide(t))) + L" ";
        }
    }
    if (!ev.location.empty()) summary += L" · 地点 " + ev.location;
    if (!ev.alarms.empty()) summary += Fmt(L" · 提前 %d 分钟提醒", -ev.alarms[0].offsetMin);
    if (ev.priority == PRIO_HIGH) summary += L"· 高优先级";
    else if (ev.priority == PRIO_MED) summary += L"· 中优先级";
    r.summary = summary;
    r.ev = ev;
    r.ok = true;
    return r;
}

BulkParseResult ParseBulk(const std::wstring& text, const DateTime& baseDate, const Store* store) {
    BulkParseResult r;
    auto lines = Split(text, L'\n');
    int lineNo = 0;
    for (auto& line : lines) {
        ++lineNo;
        std::wstring l = Trim(line);
        if (l.empty() || StartsWith(l, L"//")) continue;
        QuickResult q = ParseQuick(l, baseDate, store);
        if (q.ok) {
            r.events.push_back(q.ev);
            for (auto& kv : q.tagNames) r.tagNames[kv.first] = kv.second;
        } else {
            r.errors.push_back(Fmt(L"第 %d 行：%s", lineNo, q.error.c_str()));
        }
    }
    return r;
}
