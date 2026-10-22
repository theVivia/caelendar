// printplan.cpp - 周计划 / 课程表打印与打印版 HTML 导出
#include "app.h"
#include <commdlg.h>
#include <algorithm>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

std::wstring Esc(const std::wstring& s) {
    std::wstring o;
    for (wchar_t c : s) {
        if (c == L'&') o += L"&amp;";
        else if (c == L'<') o += L"&lt;";
        else if (c == L'>') o += L"&gt;";
        else if (c == L'"') o += L"&quot;";
        else o += c;
    }
    return o;
}

DateTime WeekStart(const DateTime& cursor) {
    int wd = DayOfWeek(cursor.y, cursor.mo, cursor.d);
    int offset = app.store.settings.weekStart == 1 ? ((wd + 6) % 7) : wd;
    return AddDaysDT(StartOfDay(cursor), -offset);
}

std::vector<Occurrence> CollectWeek(const DateTime& weekStart) {
    std::vector<Occurrence> out;
    DateTime from = StartOfDay(weekStart);
    auto occs = ExpandOccurrences(app.store, from, AddDaysDT(from, 7));
    for (auto& o : occs) {
        if (o.ev.status == ST_CANCELLED) continue;
        out.push_back(o);
    }
    std::sort(out.begin(), out.end(), [](const Occurrence& a, const Occurrence& b) {
        if (!(a.start.y == b.start.y && a.start.mo == b.start.mo && a.start.d == b.start.d))
            return DaySerial(a.start.y, a.start.mo, a.start.d) < DaySerial(b.start.y, b.start.mo, b.start.d);
        if (a.ev.allDay != b.ev.allDay) return a.ev.allDay;
        return CompareDT(a.start, b.start) < 0;
    });
    return out;
}

bool IsCourse(const Occurrence& o) {
    for (auto& t : o.ev.tags) if (t == "course") return true;
    return false;
}

std::wstring TimeText(const Occurrence& o) {
    return FormatTimeRange(o);
}

std::wstring DayLabel(const DateTime& d) {
    return Fmt(L"%d月%d日 %s", d.mo, d.d, WeekdayCN(DayOfWeek(d.y, d.mo, d.d)));
}

std::wstring PlanTitle(const DateTime& weekStart) {
    return Fmt(L"星历 · 周计划  %d-%02d-%02d ~ %d-%02d-%02d", weekStart.y, weekStart.mo, weekStart.d,
               AddDaysDT(weekStart, 6).y, AddDaysDT(weekStart, 6).mo, AddDaysDT(weekStart, 6).d);
}

std::wstring BuildHtml(const DateTime& weekStart) {
    auto occs = CollectWeek(weekStart);
    auto course = [&]() {
        std::vector<Occurrence> v;
        for (auto& o : occs) if (IsCourse(o)) v.push_back(o);
        return v;
    }();
    std::wstring html;
    html += L"<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">";
    html += L"<title>" + Esc(PlanTitle(weekStart)) + L"</title><style>";
    html += L"body{font-family:'Microsoft YaHei','Noto Sans SC',sans-serif;color:#1a1a1a;background:#fff;margin:24px;}";
    html += L"h1{font-size:22px;margin:0 0 4px;}h2{font-size:16px;margin:22px 0 8px;}";
    html += L".sub{color:#666;font-size:12px;margin-bottom:16px}";
    html += L".week{display:flex;gap:8px;align-items:stretch}.day{flex:1;border:1px solid #d8d8d8;border-radius:6px;padding:6px;min-height:150px}";
    html += L".day h3{font-size:13px;margin:0 0 6px;border-bottom:1px solid #e5e5e5;padding-bottom:4px}";
    html += L".ev{font-size:11px;margin:4px 0;line-height:1.35}.ev b{font-weight:600}.loc{color:#666;font-size:10px}";
    html += L".done{color:#888;text-decoration:line-through}";
    html += L"table{border-collapse:collapse;width:100%;font-size:12px}td,th{border:1px solid #d8d8d8;padding:5px 6px;vertical-align:top}";
    html += L"th{background:#f5f5f5;text-align:left}";
    html += L"@media print{body{margin:10mm}.day{min-height:35mm}@page{size:A4 landscape;margin:10mm}}";
    html += L"</style></head><body>";
    html += L"<h1>星历 · 周计划</h1><div class=\"sub\">" + Esc(PlanTitle(weekStart)) +
            L"　共 " + Fmt(L"%d", (int)occs.size()) + L" 项日程，其中课程 " + Fmt(L"%d", (int)course.size()) + L" 项</div>";
    html += L"<div class=\"week\">";
    for (int i = 0; i < 7; ++i) {
        DateTime day = AddDaysDT(weekStart, i);
        html += L"<div class=\"day\"><h3>" + Esc(DayLabel(day)) + L"</h3>";
        for (auto& o : occs) {
            if (DaySerial(o.start.y, o.start.mo, o.start.d) != DaySerial(day.y, day.mo, day.d)) continue;
            html += L"<div class=\"ev" + std::wstring(o.ev.done ? L" done" : L"") + L"\"><b>" +
                    Esc(TimeText(o)) + L"</b> " + Esc(o.ev.title);
            if (!o.ev.location.empty()) html += L"<div class=\"loc\">" + Esc(o.ev.location) + L"</div>";
            html += L"</div>";
        }
        html += L"</div>";
    }
    html += L"</div>";
    // 课程表：按开始时间分行的 7 列网格
    html += L"<h2>课程表</h2><table><tr><th>时间</th>";
    for (int i = 0; i < 7; ++i) html += L"<th>" + Esc(WeekdayCN(DayOfWeek(AddDaysDT(weekStart, i).y,
                                                                          AddDaysDT(weekStart, i).mo,
                                                                          AddDaysDT(weekStart, i).d))) + L"</th>";
    html += L"</tr>";
    std::vector<std::wstring> starts;
    for (auto& o : course) {
        std::wstring s = o.ev.allDay ? L"全天" : TimeStr(o.start);
        if (std::find(starts.begin(), starts.end(), s) == starts.end()) starts.push_back(s);
    }
    std::sort(starts.begin(), starts.end());
    for (auto& st : starts) {
        html += L"<tr><td>" + Esc(st) + L"</td>";
        for (int i = 0; i < 7; ++i) {
            DateTime day = AddDaysDT(weekStart, i);
            html += L"<td>";
            for (auto& o : course) {
                if (DaySerial(o.start.y, o.start.mo, o.start.d) != DaySerial(day.y, day.mo, day.d)) continue;
                std::wstring s = o.ev.allDay ? L"全天" : TimeStr(o.start);
                if (s != st) continue;
                html += Esc(o.ev.title);
                if (!o.ev.location.empty()) html += L"<div class=\"loc\">" + Esc(o.ev.location) + L"</div>";
            }
            html += L"</td>";
        }
        html += L"</tr>";
    }
    html += L"</table><div class=\"sub\" style=\"margin-top:14px\">导出自 CAELENDAR 星历。浏览器打印时选择“另存为 PDF”即可得到 PDF。</div>";
    html += L"</body></html>";
    return html;
}

// ---------------- GDI 打印 ----------------
void DrawTextGdi(HDC dc, const std::wstring& s, int x, int y, int size, bool bold, COLORREF color) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(size, GetDeviceCaps(dc, LOGPIXELSY), 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
    HFONT font = CreateFontIndirectW(&lf);
    HFONT old = (HFONT)SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, x, y, s.c_str(), (int)s.size());
    SelectObject(dc, old);
    DeleteObject(font);
}

void DrawLineGdi(HDC dc, int x1, int y1, int x2, int y2, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HPEN old = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

bool PrintGdi(const DateTime& weekStart) {
    PRINTDLGW pd{};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = g_hwnd;
    pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
    if (!PrintDlgW(&pd) || !pd.hDC) return false;
    HDC dc = pd.hDC;
    auto occs = CollectWeek(weekStart);
    DOCINFOW di{};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"星历周计划";
    if (StartDocW(dc, &di) <= 0) { DeleteDC(dc); return false; }
    int pageW = GetDeviceCaps(dc, HORZRES), pageH = GetDeviceCaps(dc, VERTRES);
    int margin = pageW / 20;
    int y = margin;
    StartPage(dc);
    DrawTextGdi(dc, L"星历 · 周计划", margin, y, 22, true, RGB(20, 20, 20));
    y += MulDiv(26, GetDeviceCaps(dc, LOGPIXELSY), 72);
    DrawTextGdi(dc, PlanTitle(weekStart), margin, y, 10, false, RGB(90, 90, 90));
    y += MulDiv(18, GetDeviceCaps(dc, LOGPIXELSY), 72);
    int lineY = y;
    DrawLineGdi(dc, margin, lineY, pageW - margin, lineY, RGB(190, 190, 190));
    y += MulDiv(10, GetDeviceCaps(dc, LOGPIXELSY), 72);
    int rowH = MulDiv(10, GetDeviceCaps(dc, LOGPIXELSY), 72);
    int dayW = (pageW - margin * 2);
    for (int i = 0; i < 7; ++i) {
        DateTime day = AddDaysDT(weekStart, i);
        DrawTextGdi(dc, DayLabel(day), margin, y, 12, true, RGB(20, 20, 20));
        y += rowH + MulDiv(3, GetDeviceCaps(dc, LOGPIXELSY), 72);
        int shown = 0;
        for (auto& o : occs) {
            if (DaySerial(o.start.y, o.start.mo, o.start.d) != DaySerial(day.y, day.mo, day.d)) continue;
            if (shown >= 8) {
                DrawTextGdi(dc, L"…（更多日程请在星历中查看）", margin + dayW / 10, y, 8, false, RGB(120, 120, 120));
                y += rowH;
                break;
            }
            std::wstring line = TimeText(o) + L"  " + o.ev.title;
            if (!o.ev.location.empty()) line += L"  @" + o.ev.location;
            DrawTextGdi(dc, line, margin + dayW / 20, y, 9.5f, false,
                        o.ev.done ? RGB(130, 130, 130) : RGB(35, 35, 35));
            y += rowH;
            ++shown;
        }
        DrawLineGdi(dc, margin, y + 2, pageW - margin, y + 2, RGB(225, 225, 225));
        y += MulDiv(7, GetDeviceCaps(dc, LOGPIXELSY), 72);
    }
    EndPage(dc);
    // 第二页：课程表
    StartPage(dc);
    y = margin;
    DrawTextGdi(dc, L"星历 · 课程表", margin, y, 22, true, RGB(20, 20, 20));
    y += MulDiv(26, GetDeviceCaps(dc, LOGPIXELSY), 72);
    DrawTextGdi(dc, PlanTitle(weekStart), margin, y, 10, false, RGB(90, 90, 90));
    y += MulDiv(22, GetDeviceCaps(dc, LOGPIXELSY), 72);
    int colW = (pageW - margin * 2) / 8;
    for (int i = 0; i < 8; ++i) {
        DrawTextGdi(dc, i == 0 ? L"时间" : WeekdayCN(DayOfWeek(AddDaysDT(weekStart, i - 1).y,
                                                              AddDaysDT(weekStart, i - 1).mo,
                                                              AddDaysDT(weekStart, i - 1).d)),
                    margin + i * colW + 3, y, 11, true, RGB(20, 20, 20));
    }
    y += MulDiv(18, GetDeviceCaps(dc, LOGPIXELSY), 72);
    DrawLineGdi(dc, margin, y, pageW - margin, y, RGB(150, 150, 150));
    y += MulDiv(6, GetDeviceCaps(dc, LOGPIXELSY), 72);
    std::vector<std::wstring> starts;
    for (auto& o : occs) {
        if (!IsCourse(o)) continue;
        std::wstring s = o.ev.allDay ? L"全天" : TimeStr(o.start);
        if (std::find(starts.begin(), starts.end(), s) == starts.end()) starts.push_back(s);
    }
    std::sort(starts.begin(), starts.end());
    int cellLine = MulDiv(9, GetDeviceCaps(dc, LOGPIXELSY), 72);
    for (auto& st : starts) {
        DrawTextGdi(dc, st, margin + 3, y, 9, false, RGB(70, 70, 70));
        for (int i = 0; i < 7; ++i) {
            DateTime day = AddDaysDT(weekStart, i);
            int ty = y;
            for (auto& o : occs) {
                if (!IsCourse(o)) continue;
                if (DaySerial(o.start.y, o.start.mo, o.start.d) != DaySerial(day.y, day.mo, day.d)) continue;
                std::wstring s = o.ev.allDay ? L"全天" : TimeStr(o.start);
                if (s != st) continue;
                DrawTextGdi(dc, o.ev.title, margin + (i + 1) * colW + 3, ty, 8.5f, true, RGB(30, 30, 30));
                ty += cellLine;
                if (!o.ev.location.empty()) {
                    DrawTextGdi(dc, o.ev.location, margin + (i + 1) * colW + 3, ty, 7.5f, false, RGB(110, 110, 110));
                    ty += cellLine;
                }
            }
        }
        y += MulDiv(24, GetDeviceCaps(dc, LOGPIXELSY), 72);
        DrawLineGdi(dc, margin, y - MulDiv(3, GetDeviceCaps(dc, LOGPIXELSY), 72),
                    pageW - margin, y - MulDiv(3, GetDeviceCaps(dc, LOGPIXELSY), 72), RGB(225, 225, 225));
    }
    EndPage(dc);
    EndDoc(dc);
    DeleteDC(dc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
    return true;
}

}  // namespace

bool ExportWeeklyPlanHtml(std::wstring& err, std::wstring* outPath) {
    DateTime ws = WeekStart(app.cursor);
    std::wstring dir = PathJoin(GetDataDir(), L"export");
    EnsureDir(dir);
    std::wstring path = PathJoin(dir, Fmt(L"星历-周计划-%04d%02d%02d.html", ws.y, ws.mo, ws.d));
    std::wstring html = BuildHtml(ws);
    if (!WriteFileBytesAtomic(path, WideToUtf8(html))) { err = L"写入 HTML 失败"; return false; }
    if (outPath) *outPath = path;
    DebugLog(L"周计划导出: %s", path.c_str());
    return true;
}

static int PngEncoderClsid(CLSID* clsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* info = (ImageCodecInfo*)malloc(size);
    if (!info) return -1;
    GetImageEncoders(num, size, info);
    int found = -1;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(info[i].MimeType, L"image/png") == 0) {
            *clsid = info[i].Clsid;
            found = (int)i;
            break;
        }
    }
    free(info);
    return found;
}

bool ExportWeeklyPlanPng(std::wstring& err, std::wstring* outPath) {
    DateTime ws = WeekStart(app.cursor);
    auto occs = CollectWeek(ws);
    int courses = 0;
    for (auto& o : occs) if (IsCourse(o)) ++courses;

    const int Wd = 1600, Ht = 1180;
    Bitmap bmp(Wd, Ht, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    SolidBrush white(Color(255, 255, 255, 255));
    g.FillRectangle(&white, 0, 0, Wd, Ht);

    FontFamily ff(L"Microsoft YaHei");
    Font fTitle(&ff, 30, FontStyleBold, UnitPixel);
    Font fHead(&ff, 19, FontStyleBold, UnitPixel);
    Font fBody(&ff, 15, FontStyleRegular, UnitPixel);
    Font fSmall(&ff, 13, FontStyleRegular, UnitPixel);
    SolidBrush ink(Color(255, 28, 30, 38));
    SolidBrush dim(Color(255, 110, 115, 125));
    SolidBrush accent(Color(255, 160, 112, 30));
    Pen border(Color(255, 210, 212, 218), 1.2f);
    Pen line(Color(255, 232, 233, 238), 1.0f);

    int margin = 40;
    g.DrawString(PlanTitle(ws).c_str(), -1, &fTitle, PointF((REAL)margin, (REAL)36), &ink);
    std::wstring sub = Fmt(L"共 %d 项日程 · 课程 %d 门 · 每周课程表 / 周计划", (int)occs.size(), courses);
    g.DrawString(sub.c_str(), -1, &fBody, PointF((REAL)margin, (REAL)92), &dim);

    int gap = 12;
    int colW = (Wd - margin * 2 - gap * 6) / 7;
    int top = 150;
    int colH = Ht - top - 80;
    for (int i = 0; i < 7; ++i) {
        DateTime day = AddDaysDT(ws, i);
        int x = margin + i * (colW + gap);
        RectF col((REAL)x, (REAL)top, (REAL)colW, (REAL)colH);
        SolidBrush colBg(Color(255, 250, 250, 251));
        g.FillRectangle(&colBg, col);
        g.DrawRectangle(&border, col);
        std::wstring head = Fmt(L"%d/%d %s", day.mo, day.d, WeekdayCN(DayOfWeek(day.y, day.mo, day.d)));
        g.DrawString(head.c_str(), -1, &fHead, PointF((REAL)(x + 12), (REAL)(top + 10)), &ink);
        g.DrawLine(&line, (REAL)(x + 10), (REAL)(top + 44), (REAL)(x + colW - 10), (REAL)(top + 44));

        REAL ey = (REAL)(top + 54);
        int drawn = 0, total = 0;
        for (auto& o : occs) {
            if (DaySerial(o.start.y, o.start.mo, o.start.d) != DaySerial(day.y, day.mo, day.d)) continue;
            ++total;
            if (drawn >= 10) continue;
            std::wstring tm = o.ev.allDay ? L"全天" : (o.isRecurring ? TimeStr(o.start) : TimeText(o));
            if (CompareDT(o.ev.start, o.ev.end) == 0) tm = TimeStr(o.ev.start);
            std::wstring title = o.ev.title;
            if (title.size() > 18) title = title.substr(0, 17) + L"…";
            g.DrawString(tm.c_str(), -1, &fSmall, PointF((REAL)(x + 10), ey), &accent);
            g.DrawString(title.c_str(), -1, &fBody, PointF((REAL)(x + 10), ey + 18), &ink);
            ey += 44;
            if (!o.ev.location.empty()) {
                std::wstring loc = o.ev.location;
                if (loc.size() > 20) loc = loc.substr(0, 19) + L"…";
                g.DrawString(loc.c_str(), -1, &fSmall, PointF((REAL)(x + 10), ey - 6), &dim);
                ey += 8;
            }
            ++drawn;
        }
        if (total == 0) {
            g.DrawString(L"（无日程）", -1, &fSmall, PointF((REAL)(x + 10), ey), &dim);
        } else if (total > drawn) {
            g.DrawString(Fmt(L"+%d 项…", total - drawn).c_str(), -1, &fSmall, PointF((REAL)(x + 10), ey), &dim);
        }
    }
    g.DrawString(L"由 CAELENDAR 星历生成 · 图片可直接分享；PDF 可用「打印本周」或「导出打印版 HTML」后另存", -1,
                 &fSmall, PointF((REAL)margin, (REAL)(Ht - 44)), &dim);

    CLSID pngClsid;
    if (PngEncoderClsid(&pngClsid) < 0) { err = L"找不到 PNG 编码器"; return false; }
    std::wstring dir = PathJoin(GetDataDir(), L"export");
    EnsureDir(dir);
    std::wstring path = PathJoin(dir, Fmt(L"星历-周计划-%04d%02d%02d.png", ws.y, ws.mo, ws.d));
    if (bmp.Save(path.c_str(), &pngClsid, nullptr) != Ok) {
        err = L"保存 PNG 失败";
        return false;
    }
    if (outPath) *outPath = path;
    DebugLog(L"周计划图片导出: %s", path.c_str());
    return true;
}

bool PrintPlanProbe(std::wstring& err) {
    DateTime ws = WeekStart(app.cursor);
    auto occs = CollectWeek(ws);
    int courses = 0;
    for (auto& o : occs) if (IsCourse(o)) ++courses;
    std::wstring path;
    if (!ExportWeeklyPlanHtml(err, &path)) return false;
    bool ok = false;
    std::string html = ReadFileBytes(path, &ok);
    bool hasWeek = html.find("周计划") != std::string::npos;
    bool hasTable = html.find("课程表") != std::string::npos;
    DebugLog(L"probe-print: events=%d courses=%d html=%d size=%d week=%d table=%d",
             (int)occs.size(), courses, ok ? 1 : 0, (int)html.size(), hasWeek ? 1 : 0, hasTable ? 1 : 0);
    err.clear();
    return ok && hasWeek && hasTable;
}

void OpenPrintPanel() {
    app.overlay = OV_PRINT;
    UIInvalidate();
}

void DrawPrintPanelOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(680), ph = S(560);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    DateTime ws = WeekStart(app.cursor);
    auto occs = CollectWeek(ws);
    int courses = 0;
    for (auto& o : occs) if (IsCourse(o)) ++courses;
    Text(g, L"打印 / 导出周计划", S(17), true, th.text, RectF(x + S(24), y + S(16), S(360), S(26)));
    Text(g, PlanTitle(ws), S(9.5f), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    RectF info(x + S(24), y + S(80), pw - S(48), S(170));
    FillRoundRect(g, info, S(10), WithAlpha(th.bg, 170));
    StrokeRoundRect(g, info, S(10), th.borderSoft, S(1));
    Text(g, Fmt(L"本周 %d 项日程 · %d 门课程", (int)occs.size(), courses), S(14), true, th.accent,
         RectF(info.X + S(16), info.Y + S(12), info.Width - S(32), S(24)));
    Text(g, L"包含周计划（7 天列表）和课程表（7 列网格）两页。", S(10), false, th.dim,
         RectF(info.X + S(16), info.Y + S(42), info.Width - S(32), S(18)));
    float ly = info.Y + S(70);
    int shown = 0;
    for (auto& o : occs) {
        if (shown >= 5) break;
        Text(g, Fmt(L"· %s  %s", TimeText(o).c_str(), o.ev.title.c_str()), S(9.5f), false, th.dim,
             RectF(info.X + S(16), ly, info.Width - S(32), S(18)), ALIGN_LEFT, false, true);
        ly += S(19);
        ++shown;
    }
    if (occs.empty())
        Text(g, L"这一周还没有日程；打印出来会是空白周计划。", S(10), false, th.dim2,
             RectF(info.X + S(16), ly, info.Width - S(32), S(18)));

    float by = y + S(272);
    UI::Btn(g, RectF(x + S(24), by, S(150), S(38)), CMD_IMPORT_OK, L"打印本周…", 0, ICO_FOLDER, []() {
        if (!PrintGdi(WeekStart(app.cursor))) ShowToast(L"打印已取消或失败");
        else ShowToast(L"已发送到打印机");
    });
    UI::Btn(g, RectF(x + S(180), by, S(170), S(38)), CMD_IMPORT_OK + 1, L"导出打印版 HTML", 1, ICO_COPY, []() {
        std::wstring err, path;
        if (!ExportWeeklyPlanHtml(err, &path)) { ShowToast(err.empty() ? L"导出失败" : err); return; }
        ShowToast(L"已导出：" + path, 5000);
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + S(356), by, S(130), S(38)), CMD_IMPORT_OK + 3, L"导出图片…", 1, ICO_CAMERA, []() {
        std::wstring err, path;
        if (!ExportWeeklyPlanPng(err, &path)) { ShowToast(err.empty() ? L"导出图片失败" : err); return; }
        ShowToast(L"已导出图片：" + path, 5000);
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + S(492), by, S(120), S(38)), CMD_IMPORT_OK + 2, L"打开目录", 1, ICO_FOLDER, []() {
        EnsureDir(PathJoin(GetDataDir(), L"export"));
        ShellExecuteW(nullptr, L"open", PathJoin(GetDataDir(), L"export").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    });

    Text(g, L"打印提示", S(10), false, th.dim2, RectF(x + S(24), y + S(330), S(120), S(18)));
    Text(g, L"· 在系统打印对话框里选择「Microsoft Print to PDF」，保存文件就是 PDF。",
         S(9), false, th.dim, RectF(x + S(24), y + S(352), pw - S(48), S(16)), ALIGN_LEFT, false, true);
    Text(g, L"· 或「导出打印版 HTML」，用浏览器打开后 Ctrl+P，目标打印机选「另存为 PDF」。",
         S(9), false, th.dim, RectF(x + S(24), y + S(372), pw - S(48), S(16)), ALIGN_LEFT, false, true);
    Text(g, L"· 页面使用系统字体，中文无需额外字体包；A4 横向，页边距自动。",
         S(9), false, th.dim, RectF(x + S(24), y + S(392), pw - S(48), S(16)), ALIGN_LEFT, false, true);
    Text(g, L"PDF 由 Windows / 浏览器生成，星历本身不打包 PDF 库。", S(8.5f), false, th.dim2,
         RectF(x + S(24), y + S(418), pw - S(48), S(16)), ALIGN_LEFT, false, true);

    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
