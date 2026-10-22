// dialogs.cpp - 遮罩层框架、菜单、确认框、快速添加、批量添加、导入、标签编辑、关于
#include "app.h"
#include <commdlg.h>
#include "ics.h"
#include "smart.h"
#include "quick.h"
#include "json.h"
#include <algorithm>
#include <vector>
#include <string>

using namespace Gdiplus;
using namespace gfx;

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}
static int DrawWrappedText(Graphics& g, const std::wstring& text, float px, Color c, RectF r, int maxLines) {
    std::vector<std::wstring> lines;
    std::wstring cur;
    for (wchar_t ch : text) {
        if (ch == L'\n') {
            lines.push_back(cur);
            cur.clear();
            if ((int)lines.size() >= maxLines) break;
            continue;
        }
        std::wstring next = cur + ch;
        SizeF sz;
        MeasureText(g, next, px, false, sz);
        if (sz.Width > r.Width && !cur.empty()) {
            lines.push_back(cur);
            cur = std::wstring(1, ch);
            if ((int)lines.size() >= maxLines) break;
        } else {
            cur = next;
        }
    }
    if ((int)lines.size() < maxLines && !cur.empty()) lines.push_back(cur);
    int drawn = 0;
    for (auto& ln : lines) {
        if (drawn >= maxLines) break;
        Txt(g, ln, px, false, c, RectF(r.X, r.Y + drawn * S(18), r.Width, S(16)), ALIGN_LEFT, false, false);
        ++drawn;
    }
    return drawn;
}
static void EnsureTags(const std::map<std::string, std::wstring>& names) {
    for (auto& kv : names) if (!app.store.FindTag(kv.first)) app.store.EnsureTag(kv.second);
}
static std::wstring QuickTagText(const QuickResult& r) {
    std::wstring s;
    for (auto& t : r.ev.tags) {
        auto it = r.tagNames.find(t);
        std::wstring name = (it != r.tagNames.end()) ? it->second : app.store.TagName(t);
        s += L"#" + name + L" ";
    }
    return s;
}

static void PanelFrame(Graphics& g, float W, float H, float pw, float ph, float& px, float& py, float radius = 16) {
    const Theme& th = TH();
    pw = std::min(pw, W - S(40));
    ph = std::min(ph, H - S(30));
    px = (W - pw) / 2;
    py = (H - ph) / 2;
    RectF r(px, py, pw, ph);
    Shadow(g, r, radius, 8, 26);
    FillRoundRect(g, r, radius, th.card);
    StrokeRoundRect(g, r, radius, WithAlpha(th.border, 255), S(1));
    FillVGradient(g, RectF(px, py, pw, S(60)), WithAlpha(th.accent, 10), WithAlpha(th.accent, 0));
}

// ---------------- 菜单 ----------------
void OpenMenu(float x, float y, std::vector<MenuItemDef> items, OverlayKind returnOverlay) {
    app.menu.items = std::move(items);
    app.menu.open = true;
    app.menu.returnOverlay = returnOverlay;
    app.overlay = OV_MENU;
    if (x == 0 && y == 0) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(g_hwnd, &pt);
        x = (float)pt.x;
        y = (float)pt.y;
    }
    app.menu.x = x;
    app.menu.y = y;
    UIInvalidate();
}

void DrawMenuOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    if (app.menu.items.empty()) return;
    float itemH = S(32), padY = S(6), padX = S(6);
    float width = S(190);
    for (auto& it : app.menu.items) {
        SizeF sz; MeasureText(g, it.label, S(11.5f), false, sz);
        width = std::max(width, sz.Width + S(64));
    }
    float height = app.menu.items.size() * itemH + padY * 2;
    float x = app.menu.x, y = app.menu.y;
    if (x + width > W - S(8)) x = W - width - S(8);
    if (y + height > H - S(8)) y = H - height - S(8);
    if (x < S(8)) x = S(8);
    if (y < S(8)) y = S(8);
    RectF box(x, y, width, height);
    Shadow(g, box, S(10), 7, 40);
    FillRoundRect(g, box, S(10), th.card);
    StrokeRoundRect(g, box, S(10), WithAlpha(th.border, 255), S(1));
    for (size_t i = 0; i < app.menu.items.size(); ++i) {
        MenuItemDef& it = app.menu.items[i];
        RectF row(x + padX, y + padY + i * itemH, width - padX * 2, itemH - S(2));
        int cmd = CMD_MENU_ITEM_BASE + (int)i;
        bool hover = IsHovered(cmd) && !it.disabled;
        if (hover) FillRoundRect(g, row, S(7), WithAlpha(th.cardHover, 230));
        float labelX = row.X + S(34);
        if (!it.color.empty()) {
            FillCircle(g, row.X + S(16), row.Y + row.Height / 2, S(5.5f), Hex(it.color));
            StrokeCircle(g, row.X + S(16), row.Y + row.Height / 2, S(5.5f), WithAlpha(th.text, 60), S(1));
        } else if (it.icon >= 0) {
            Icon(g, it.icon, RectF(row.X + S(10), row.Y + (row.Height - S(14)) / 2, S(14), S(14)),
                 it.disabled ? WithAlpha(th.dim2, 120) : (it.danger ? th.danger : th.dim), 1.5f);
        }
        Color fg = it.disabled ? WithAlpha(th.dim2, 110) : (it.danger ? th.danger : (hover ? th.text : th.dim));
        Txt(g, it.label, S(11.5f), false, fg, RectF(labelX, row.Y, row.Width - S(44), row.Height), ALIGN_VCENTER);
        if (!it.disabled) {
            auto fn = it.fn;
            AddHit(row.X, row.Y, row.Width, row.Height, cmd, [fn]() {
                if (fn) fn();
                if (app.overlay == OV_MENU) {
                    app.overlay = app.menu.returnOverlay;
                    app.menu.open = false;
                }
                UIInvalidate();
            });
        }
    }
}

// ---------------- 确认框 ----------------
void OpenConfirm(const std::wstring& title, const std::wstring& body, std::function<void()> onOk,
                 const std::wstring& okText, const std::wstring& cancelText) {
    app.confirm.open = true;
    app.confirm.title = title;
    app.confirm.body = body;
    app.confirm.okText = okText.empty() ? L"确定" : okText;
    app.confirm.cancelText = cancelText.empty() ? L"取消" : cancelText;
    app.confirm.onOk = std::move(onOk);
    app.overlay = OV_CONFIRM;
    UIInvalidate();
}

void DrawConfirmOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(430), ph = S(252);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, app.confirm.title, S(16), true, th.text, RectF(x + S(24), y + S(20), pw - S(48), S(26)));
    DrawWrappedText(g, app.confirm.body, S(11.5f), th.dim,
                    RectF(x + S(24), y + S(56), pw - S(48), S(110)), 6);
    UI::Btn(g, RectF(x + pw - S(112) - S(24), y + ph - S(52), S(112), S(34)), CMD_CONFIRM_CANCEL, app.confirm.cancelText, 1, -1, []() {
        app.overlay = OV_NONE;
        app.confirm.open = false;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(112) * 2 - S(34), y + ph - S(52), S(112), S(34)), CMD_CONFIRM_OK, app.confirm.okText, 3, -1, []() {
        auto fn = app.confirm.onOk;
        app.overlay = OV_NONE;
        app.confirm.open = false;
        app.confirm.onOk = nullptr;
        if (fn) fn();
        UIInvalidate();
    });
}

// ---------------- 快速添加 ----------------
void OpenQuickAdd() {
    app.quick.open = true;
    app.quick.text.clear();
    app.quick.previews.clear();
    app.overlay = OV_QUICKADD;
    UIInvalidate();
}

void DrawQuickAddOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(620), ph = S(330);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, L"快速添加", S(16), true, th.text, RectF(x + S(24), y + S(18), pw - S(60), S(26)));
    Txt(g, L"用一句话描述：明天 15:00-16:00 高数作业 #作业 地点 A306 提前10分钟 !!", S(10), false, th.dim2,
        RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    RectF box(x + S(24), y + S(70), pw - S(48), S(42));
    FillRoundRect(g, box, S(9), th.panel2);
    StrokeRoundRect(g, box, S(9), WithAlpha(th.accent, 100), S(1));
    UIEdit(9201, RectF(box.X + S(10), box.Y + S(9), box.Width - S(20), S(24)), &app.quick.text, false, S(13.5f));
    // 解析预览
    QuickResult r = ParseQuick(app.quick.text, app.lastClickDay.y ? app.lastClickDay : Today(), &app.store);
    RectF pv(x + S(24), y + S(126), pw - S(48), S(92));
    FillRoundRect(g, pv, S(10), WithAlpha(th.bg, 170));
    StrokeRoundRect(g, pv, S(10), th.borderSoft, S(1));
    if (app.quick.text.empty()) {
        Txt(g, L"支持：日期时间、#标签、地点 A306 / 在 图书馆、提前10分钟、每周一三五、每月5号、!! 高优先级、全天",
            S(10), false, th.dim2, RectF(pv.X + S(14), pv.Y + S(14), pv.Width - S(28), S(40)), ALIGN_LEFT, false, true);
        Txt(g, L"也可以按 Enter 直接创建。", S(10), false, th.dim2, RectF(pv.X + S(14), pv.Y + S(52), pv.Width - S(28), S(20)));
    } else if (!r.ok) {
        Txt(g, L"没看懂：" + r.error, S(11), false, th.danger, RectF(pv.X + S(14), pv.Y + S(16), pv.Width - S(28), S(24)));
        Txt(g, L"提示：至少要有一个标题，例如「明天 9:00 交作业 #作业」。", S(9.5f), false, th.dim2,
            RectF(pv.X + S(14), pv.Y + S(44), pv.Width - S(28), S(20)));
    } else {
        Txt(g, r.ev.title, S(13.5f), true, th.text, RectF(pv.X + S(14), pv.Y + S(10), pv.Width - S(28), S(22)));
        std::wstring when = r.ev.allDay ? (ISODate(r.ev.start) + L" 全天") : (DateTimeHuman(r.ev.start) + L" – " + TimeStr(r.ev.end));
        Txt(g, when, S(10.5f), false, th.accent, RectF(pv.X + S(14), pv.Y + S(34), pv.Width - S(28), S(18)));
        Txt(g, QuickTagText(r) + (r.ev.priority == PRIO_HIGH ? L"高优先级" : L""), S(9.5f), false, th.dim2,
            RectF(pv.X + S(14), pv.Y + S(54), pv.Width - S(28), S(18)));
        Txt(g, r.summary, S(9), false, WithAlpha(th.dim2, 190), RectF(pv.X + S(14), pv.Y + S(70), pv.Width - S(28), S(16)));
    }
    // 按钮
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(54), S(90), S(34)), CMD_QA_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        app.quick.open = false;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) * 2 - S(8), y + ph - S(54), S(90), S(34)), CMD_QA_DETAIL, L"编辑详情", 1, -1, []() {
        QuickResult r2 = ParseQuick(app.quick.text, app.lastClickDay.y ? app.lastClickDay : Today(), &app.store);
        if (!r2.ok) { ShowToast(L"还没解析出日程"); return; }
        EnsureTags(r2.tagNames);
        app.overlay = OV_EDITOR;
        app.quick.open = false;
        app.editor.isNew = true;
        app.editor.seriesId.clear();
        app.editor.seriesRecurring = false;
        app.editor.scope = 2;
        app.editor.OpenFromEvent(r2.ev);
        app.editor.title = r2.ev.title;
        UIInvalidate();
    });
    bool canSave = r.ok;
    UI::Btn(g, RectF(x + pw - S(24) - S(90) * 3 - S(16), y + ph - S(54), S(90), S(34)), CMD_QA_OK, L"直接创建", canSave ? 0 : 1, -1, [r]() {
        if (!r.ok) { ShowToast(L"先输入能看懂的描述"); return; }
        auto createIt = [r]() {
            app.store.Snapshot();
            EnsureTags(r.tagNames);
            app.store.AddEvent(r.ev);
            app.overlay = OV_NONE;
            app.quick.open = false;
            InvalidateOccs();
            DoSaveStore();
            ShowToast(L"已创建");
            UIInvalidate();
        };
        auto conflicts = FindEventConflicts(r.ev);
        if (!conflicts.empty()) {
            std::wstring desc = L"与「" + conflicts[0] + L"」";
            if (conflicts.size() > 1) desc += Fmt(L" 等 %d 条日程", (int)conflicts.size());
            desc += L"时间重叠，仍要创建吗？";
            OpenConfirm(L"时间冲突", desc, createIt, L"仍然创建", L"取消");
            return;
        }
        createIt();
    });
    if (!canSave) {
        // 屏蔽保存按钮（AddHit 已注册，用空动作防止误触）——简单起见：按钮变灰但可点，点击提示
    }
    if (GetFocus() != GetDlgItem(g_hwnd, 9201) && app.quick.text.empty()) {
        // 进入时聚焦
        UISetEditFocus(9201, false);
    }
}

// ---------------- 批量添加 ----------------
static std::wstring BulkPresetPath() { return PathJoin(GetDataDir(), L"bulk-presets.json"); }
static int BulkParseHM(const std::wstring& s, int defMin) {
    int h = -1, m = -1;
    if (swscanf_s(s.c_str(), L"%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59)
        return h * 60 + m;
    return defMin;
}
static std::wstring BulkFmtHM(int mins) {
    mins = std::max(0, std::min(1439, mins));
    return Fmt(L"%02d:%02d", mins / 60, mins % 60);
}
static void BulkLoadTarget() {
    if (app.bulk.perEditSerial == 0) {
        app.bulk.editLocation = app.bulk.location;
        app.bulk.editStart = app.bulk.startText;
        app.bulk.editEnd = app.bulk.endText;
        return;
    }
    auto it = app.bulk.dayOverrides.find(app.bulk.perEditSerial);
    if (it == app.bulk.dayOverrides.end()) {
        app.bulk.editLocation = app.bulk.location;
        app.bulk.editStart = app.bulk.startText;
        app.bulk.editEnd = app.bulk.endText;
    } else {
        app.bulk.editStart = BulkFmtHM(it->second.startMin >= 0 ? it->second.startMin
                                                               : BulkParseHM(app.bulk.startText, 18 * 60));
        app.bulk.editEnd = BulkFmtHM(it->second.endMin >= 0 ? it->second.endMin
                                                             : BulkParseHM(app.bulk.endText, 20 * 60));
        app.bulk.editLocation = it->second.location.empty() ? app.bulk.location : it->second.location;
    }
}
static void BulkApplyTarget() {
    if (app.bulk.perEditSerial == 0) {
        app.bulk.location = app.bulk.editLocation;
        app.bulk.startText = app.bulk.editStart;
        app.bulk.endText = app.bulk.editEnd;
    } else {
        BulkDayOverride ov;
        ov.startMin = BulkParseHM(app.bulk.editStart, 18 * 60);
        ov.endMin = BulkParseHM(app.bulk.editEnd, 20 * 60);
        ov.location = app.bulk.editLocation;
        app.bulk.dayOverrides[app.bulk.perEditSerial] = ov;
    }
}
static bool BulkDayHasOverride(int serial) {
    return app.bulk.dayOverrides.find(serial) != app.bulk.dayOverrides.end();
}

static void LoadBulkPresets() {
    app.bulk.presets.clear();
    bool ok = false;
    std::string u8 = ReadFileBytes(BulkPresetPath(), &ok);
    if (ok && !u8.empty()) {
        JValue root = JsonParse(u8);
        if (root.IsArr()) {
            for (auto& it : root.arr) {
                if (!it.IsObj()) continue;
                BulkPreset p;
                p.name = it.Has(L"name") ? it.Get(L"name").AsStr() : L"";
                p.title = it.Has(L"title") ? it.Get(L"title").AsStr() : L"";
                p.location = it.Has(L"location") ? it.Get(L"location").AsStr() : L"";
                p.startMin = it.Has(L"start") ? it.Get(L"start").AsInt(18 * 60) : 18 * 60;
                p.endMin = it.Has(L"end") ? it.Get(L"end").AsInt(20 * 60) : 20 * 60;
                p.colorIdx = it.Has(L"colorIdx") ? it.Get(L"colorIdx").AsInt(-1) : -1;
                p.customColor = it.Has(L"customColor") ? it.Get(L"customColor").AsStr() : L"";
                if (it.Has(L"tags") && it.Get(L"tags").IsArr())
                    for (auto& t : it.Get(L"tags").arr) p.tags.push_back(WideToUtf8(t.AsStr()));
                if (!p.name.empty()) app.bulk.presets.push_back(p);
            }
        }
    }
    if (app.bulk.presets.empty()) {
        BulkPreset p;
        p.name = L"晚自习";
        p.title = L"晚自习";
        p.startMin = 18 * 60;
        p.endMin = 20 * 60;
        app.bulk.presets.push_back(p);
        BulkPreset q;
        q.name = L"实验课";
        q.title = L"实验课";
        q.startMin = 14 * 60;
        q.endMin = 15 * 60 + 40;
        app.bulk.presets.push_back(q);
    }
}
static void SaveBulkPresets() {
    JValue root = JValue::Arr();
    for (auto& p : app.bulk.presets) {
        JValue o = JValue::Obj();
        o.Set(L"name") = JValue(p.name);
        o.Set(L"title") = JValue(p.title);
        o.Set(L"location") = JValue(p.location);
        o.Set(L"start") = JValue(p.startMin);
        o.Set(L"end") = JValue(p.endMin);
        o.Set(L"colorIdx") = JValue(p.colorIdx);
        if (!p.customColor.empty()) o.Set(L"customColor") = JValue(p.customColor);
        JValue tags = JValue::Arr();
        for (auto& t : p.tags) tags.Push(JValue(Utf8ToWide(t)));
        o.Set(L"tags") = tags;
        root.Push(o);
    }
    WriteFileBytesAtomic(BulkPresetPath(), JsonDump(root, true));
}

void OpenBulkAdd() {
    app.bulk.open = true;
    app.bulk.recurMode = 0;
    app.bulk.repeatCount = 12;
    app.bulk.text = L"2026-10-20 08:00-09:40 高等数学 #课程\n2026-10-21 23:59 英语作文截止 #作业\n周三 14:00 实验报告 #实验";
    // 日历多选模式
    app.bulk.calMode = true;
    app.bulk.calY = app.now.y;
    app.bulk.calM = app.now.mo;
    app.bulk.days.clear();
    app.bulk.title = L"晚自习";
    app.bulk.location.clear();
    app.bulk.startText = L"18:00";
    app.bulk.endText = L"20:00";
    app.bulk.colorIdx = -1;
    app.bulk.customColor.clear();
    app.bulk.tags.clear();
    app.bulk.presetName.clear();
    app.bulk.perEditSerial = 0;
    app.bulk.dayOverrides.clear();
    app.bulk.editLocation.clear();
    app.bulk.editStart = L"18:00";
    app.bulk.editEnd = L"20:00";
    LoadBulkPresets();
    app.overlay = OV_BULKADD;
    UIInvalidate();
}

static void DrawBulkAddTextOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(700), ph = S(620);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, L"批量添加 · 文本模式", S(16), true, th.text, RectF(x + S(24), y + S(18), pw - S(200), S(26)));
    UI::Chip(g, RectF(x + pw - S(142), y + S(16), S(110), S(26)), CMD_BULK_MODE_CAL, L"日历模式", false, th.accent, []() {
        app.bulk.calMode = true;
        UIInvalidate();
    }, 9.5f);
    Txt(g, L"每行一条：日期/星期 + 时间 + 标题 + #标签 + !!（高优先级）。可选重复规则与颜色，创建后仍可编辑。",
        S(9.5f), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)));
    RectF box(x + S(24), y + S(70), pw - S(48), S(236));
    FillRoundRect(g, box, S(10), th.panel2);
    StrokeRoundRect(g, box, S(10), th.borderSoft, S(1));
    UIEdit(9301, RectF(box.X + S(10), box.Y + S(10), box.Width - S(20), box.Height - S(20)), &app.bulk.text, true, S(12));

    BulkParseResult br = ParseBulk(app.bulk.text, Today(), &app.store);
    // 重复规则
    float oy = y + S(318);
    Txt(g, L"重复", S(10), false, th.dim2, RectF(x + S(24), oy, S(60), S(16)));
    const wchar_t* recNames[5] = { L"不重复", L"每天", L"每周", L"每月", L"每年" };
    for (int i = 0; i < 5; ++i) {
        UI::Chip(g, RectF(x + S(24) + i * S(94), oy + S(18), S(86), S(28)),
                 CMD_BULK_REPEAT_BASE + i, recNames[i], app.bulk.recurMode == i, th.accent, [i]() {
                     app.bulk.recurMode = i;
                     UIInvalidate();
                 }, 10.0f);
    }
    float cy = oy + S(56);
    if (app.bulk.recurMode > 0) {
        UI::Stepper(g, RectF(x + S(24), cy, S(190), S(28)), CMD_BULK_COUNT_DEC, CMD_BULK_COUNT_INC,
                    Fmt(L"重复次数 %d", app.bulk.repeatCount), []() {
                        app.bulk.repeatCount = std::max(1, app.bulk.repeatCount - 1);
                        UIInvalidate();
                    }, []() {
                        app.bulk.repeatCount = std::min(200, app.bulk.repeatCount + 1);
                        UIInvalidate();
                    }, 9.5f);
        cy += S(40);
    } else {
        cy += S(6);
    }
    // 颜色
    Txt(g, L"颜色", S(10), false, th.dim2, RectF(x + S(24), cy, S(60), S(16)));
    float swx = x + S(24);
    UI::Chip(g, RectF(swx, cy + S(18), S(56), S(26)), CMD_BULK_COLOR_BASE, L"自动",
             app.bulk.colorIdx < 0 && app.bulk.customColor.empty(), th.accent, []() {
                 app.bulk.colorIdx = -1;
                 app.bulk.customColor.clear();
                 UIInvalidate();
             }, 9.5f);
    swx += S(64);
    const auto& colors = PaletteHex();
    for (int i = 0; i < (int)colors.size(); ++i) {
        RectF sw(swx + i * S(24), cy + S(20), S(20), S(20));
        FillRoundRect(g, sw, S(5), Hex(colors[i]));
        if (app.bulk.colorIdx == i) StrokeRoundRect(g, sw, S(5), th.text, S(2));
        AddHit(sw.X, sw.Y, sw.Width, sw.Height, CMD_BULK_COLOR_BASE + 1 + i, [i]() {
            app.bulk.colorIdx = i;
            app.bulk.customColor.clear();
            UIInvalidate();
        });
    }
    {
        float cux = swx + (int)colors.size() * S(24) + S(4);
        RectF cu(cux, cy + S(20), S(30), S(20));
        FillRoundRect(g, cu, S(5), app.bulk.customColor.empty() ? WithAlpha(th.panel2, 200) : Hex(app.bulk.customColor));
        StrokeRoundRect(g, cu, S(5), WithAlpha(th.text, 90), S(1));
        Txt(g, L"…", S(11), true, WithAlpha(th.text, 220), cu, ALIGN_CENTER | ALIGN_VCENTER);
        AddHit(cu.X, cu.Y, cu.Width, cu.Height, CMD_BULK_COLOR_CUSTOM, []() {
            OpenColorPicker(L"批量添加颜色", app.bulk.customColor.empty() ? L"#6EA8FF" : app.bulk.customColor,
                            [](const std::wstring& hex) {
                                app.bulk.colorIdx = -1;
                                app.bulk.customColor = hex;
                                UIInvalidate();
                            });
        });
    }
    // 预览
    Txt(g, Fmt(L"将创建 %d 条", (int)br.events.size()), S(11), true, th.accent, RectF(x + S(24), y + ph - S(106), S(160), S(20)));
    if (!br.errors.empty()) {
        Txt(g, Fmt(L"%d 行无法解析：%s", (int)br.errors.size(), br.errors[0].c_str()), S(9.5f), false, th.danger,
            RectF(x + S(170), y + ph - S(106), pw - S(200), S(20)));
    }
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_BULK_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        app.bulk.open = false;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) * 2 - S(8), y + ph - S(52), S(110), S(34)), CMD_BULK_OK,
            Fmt(L"创建 %d 条", (int)br.events.size()), 0, -1, [br]() {
        if (br.events.empty()) { ShowToast(L"没有可创建的行"); return; }
        app.store.Snapshot();
        EnsureTags(br.tagNames);
        auto dupKey = [](const Event& e) {
            return e.title + L"|" + ISODT(e.start) + L"|" + ISODT(e.end) + L"|" + (e.allDay ? L"1" : L"0");
        };
        std::set<std::wstring> seen;
        for (auto& old : app.store.events) seen.insert(dupKey(old));
        const auto& pal = PaletteHex();
        int added = 0, skipped = 0;
        for (auto& src : br.events) {
            Event e = src;
            if (!app.bulk.customColor.empty()) e.color = app.bulk.customColor;
            else if (app.bulk.colorIdx >= 0 && app.bulk.colorIdx < (int)pal.size()) e.color = pal[app.bulk.colorIdx];
            if (app.bulk.recurMode > 0) {
                e.recur = Recur();
                e.recur.freq = app.bulk.recurMode;
                e.recur.interval = 1;
                if (e.recur.freq == RF_WEEKLY) e.recur.byDay = { DayOfWeek(e.start.y, e.start.mo, e.start.d) };
                if (e.recur.freq == RF_MONTHLY) e.recur.byMonthDay = e.start.d;
                if (e.recur.freq == RF_YEARLY) e.recur.byMonth = e.start.mo;
                e.recur.count = std::max(1, app.bulk.repeatCount);
            }
            if (!seen.insert(dupKey(e)).second) { ++skipped; continue; }
            app.store.AddEvent(e);
            ++added;
        }
        app.overlay = OV_NONE;
        app.bulk.open = false;
        InvalidateOccs();
        DoSaveStore();
        ShowToast(skipped > 0 ? Fmt(L"已创建 %d 条，跳过 %d 条完全重复", added, skipped)
                              : Fmt(L"已创建 %d 条日程", added));
        UIInvalidate();
    });
}

void DrawBulkAddOverlay(Graphics& g, float W, float H) {
    if (!app.bulk.calMode) { DrawBulkAddTextOverlay(g, W, H); return; }
    gfx::FontPartScope fontPart(gfx::FP_DIALOG);
    const Theme& th = TH();
    float pw = S(950), ph = S(660);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, L"批量添加 · 日历多选", S(16), true, th.text, RectF(x + S(24), y + S(16), S(460), S(26)));
    Txt(g, L"在左边年历点选日期（可跨月），填好公共字段后一次创建；地点、时间、标签、颜色一起写入。",
        S(9.5f), false, th.dim2, RectF(x + S(24), y + S(44), pw - S(200), S(18)));
    UI::Chip(g, RectF(x + pw - S(172), y + S(16), S(100), S(26)), CMD_BULK_MODE_TEXT, L"文本模式", false, th.accent, []() {
        app.bulk.calMode = false;
        UIInvalidate();
    }, 9.5f);

    // ---- 左侧年历 ----
    float lx = x + S(24), ly = y + S(86), lw = S(430);
    {
        UI::Btn(g, RectF(lx, ly, S(30), S(26)), CMD_BULK_CAL_PREV, L"<", 1, -1, []() {
            DateTime m = AddMonthsDT(MakeDT(app.bulk.calY, app.bulk.calM, 1), -1);
            app.bulk.calY = m.y; app.bulk.calM = m.mo;
            UIInvalidate();
        });
        UI::Btn(g, RectF(lx + S(36), ly, S(30), S(26)), CMD_BULK_CAL_NEXT, L">", 1, -1, []() {
            DateTime m = AddMonthsDT(MakeDT(app.bulk.calY, app.bulk.calM, 1), 1);
            app.bulk.calY = m.y; app.bulk.calM = m.mo;
            UIInvalidate();
        });
        Txt(g, Fmt(L"%d年%d月", app.bulk.calY, app.bulk.calM), S(13), true, th.text,
            RectF(lx + S(76), ly, S(170), S(26)), ALIGN_VCENTER);
        UI::Chip(g, RectF(lx + S(252), ly, S(62), S(26)), CMD_BULK_CAL_TODAY, L"今天", false, th.accent, []() {
            app.bulk.calY = app.now.y; app.bulk.calM = app.now.mo;
            UIInvalidate();
        }, 9.5f);
        DateTime first = MakeDT(app.bulk.calY, app.bulk.calM, 1);
        DateTime gridStart = StartOfWeek(first, app.store.settings.weekStart);
        int ws = app.store.settings.weekStart;
        float gh = ly + S(34);
        for (int i = 0; i < 7; ++i) {
            int wd = (ws + i) % 7;
            Txt(g, WeekdayCN(wd), S(10), false, (wd == 0 || wd == 6) ? WithAlpha(th.accent2, 220) : th.dim,
                RectF(lx + i * (lw / 7.0f), gh, lw / 7.0f, S(18)), ALIGN_CENTER | ALIGN_VCENTER);
        }
        float cw = lw / 7.0f, ch = S(44);
        int startSerial = DaySerial(gridStart.y, gridStart.mo, gridStart.d);
        for (int r = 0; r < 6; ++r) {
            for (int c = 0; c < 7; ++c) {
                int idx = r * 7 + c;
                int serial = startSerial + idx;
                Date d = FromSerial(serial);
                DateTime day = MakeDT(d.y, d.m, d.d);
                RectF cell(lx + c * cw + S(2), gh + S(20) + r * ch, cw - S(4), ch - S(3));
                bool sel = app.bulk.days.count(serial) > 0;
                bool inMonth = (d.m == app.bulk.calM && d.y == app.bulk.calY);
                bool isToday = SameDay(day, app.now);
                if (sel) FillRoundRect(g, cell, S(7), WithAlpha(th.accent, 55));
                else if (isToday) FillRoundRect(g, cell, S(7), WithAlpha(th.accent, 18));
                if (sel) StrokeRoundRect(g, cell, S(7), th.accent, S(1.5f));
                else if (isToday) StrokeRoundRect(g, cell, S(7), WithAlpha(th.accent, 150), S(1));
                Color dc = inMonth ? (sel ? th.accent : th.text) : th.dim2;
                Txt(g, Fmt(L"%d", d.d), S(12), sel, dc, RectF(cell.X, cell.Y + S(3), cell.Width, S(16)), ALIGN_CENTER);
                if (sel) Txt(g, L"✓", S(8), false, th.accent, RectF(cell.X, cell.Y + S(21), cell.Width, S(12)), ALIGN_CENTER);
                AddHit(cell.X, cell.Y, cell.Width, cell.Height, CMD_BULK_CAL_DAY_BASE + idx, [serial]() {
                    if (app.bulk.days.count(serial)) app.bulk.days.erase(serial);
                    else app.bulk.days.insert(serial);
                    UIInvalidate();
                });
            }
        }
    }

    // ---- 右侧字段 ----
    float rx = x + S(486), ry = y + S(86), rw = pw - S(510);
    auto label = [&](const wchar_t* s, float yy) {
        Txt(g, s, S(10), false, th.dim, RectF(rx, yy, S(56), S(18)), ALIGN_VCENTER);
    };
    label(L"标题", ry);
    UIEdit(9311, RectF(rx + S(56), ry, rw - S(56), S(28)), &app.bulk.title, false, S(12));
    ry += S(38);
    label(L"地点", ry);
    UIEdit(9312, RectF(rx + S(56), ry, rw - S(56), S(28)), &app.bulk.editLocation, false, S(12));
    ry += S(38);
    label(L"时间", ry);
    UIEdit(9313, RectF(rx + S(56), ry, S(86), S(28)), &app.bulk.editStart, false, S(12));
    Txt(g, L"–", S(12), false, th.dim, RectF(rx + S(146), ry, S(20), S(28)), ALIGN_CENTER | ALIGN_VCENTER);
    UIEdit(9314, RectF(rx + S(166), ry, S(86), S(28)), &app.bulk.editEnd, false, S(12));
    UI::Btn(g, RectF(rx + S(266), ry, S(104), S(28)),
            CMD_BULK_SAVE_PRESET + 10, app.bulk.perEditSerial ? L"应用到该天" : L"应用到公共", 1, -1, []() {
                BulkApplyTarget();
                ShowToast(app.bulk.perEditSerial ? L"已保存为该天单独设置" : L"已更新公共字段");
                UIInvalidate();
            });
    ry += S(40);

    label(L"颜色", ry);
    {
        float swx = rx + S(56);
        UI::Chip(g, RectF(swx, ry, S(52), S(26)), CMD_BULK_COLOR_BASE, L"自动",
                 app.bulk.colorIdx < 0 && app.bulk.customColor.empty(), th.accent, []() {
                     app.bulk.colorIdx = -1;
                     app.bulk.customColor.clear();
                     UIInvalidate();
                 }, 9.5f);
        swx += S(58);
        const auto& pal = PaletteHex();
        int n = (int)std::min<size_t>(pal.size(), 15);
        for (int i = 0; i < n; ++i) {
            RectF sw(swx + i * S(24), ry + S(3), S(20), S(20));
            FillRoundRect(g, sw, S(5), Hex(pal[i]));
            if (app.bulk.colorIdx == i) StrokeRoundRect(g, sw, S(5), th.text, S(2));
            AddHit(sw.X, sw.Y, sw.Width, sw.Height, CMD_BULK_COLOR_BASE + 1 + i, [i]() {
                app.bulk.colorIdx = i;
                app.bulk.customColor.clear();
                UIInvalidate();
            });
        }
        float cux = swx + n * S(24) + S(2);
        RectF cu(cux, ry + S(3), S(30), S(20));
        FillRoundRect(g, cu, S(5), app.bulk.customColor.empty() ? WithAlpha(th.panel2, 200) : Hex(app.bulk.customColor));
        StrokeRoundRect(g, cu, S(5), WithAlpha(th.text, 90), S(1));
        Txt(g, L"…", S(11), true, WithAlpha(th.text, 220), cu, ALIGN_CENTER | ALIGN_VCENTER);
        AddHit(cu.X, cu.Y, cu.Width, cu.Height, CMD_BULK_COLOR_CUSTOM, []() {
            OpenColorPicker(L"批量添加颜色", app.bulk.customColor.empty() ? L"#6EA8FF" : app.bulk.customColor,
                            [](const std::wstring& hex) {
                                app.bulk.colorIdx = -1;
                                app.bulk.customColor = hex;
                                UIInvalidate();
                            });
        });
    }
    ry += S(46);

    label(L"标签", ry);
    {
        float tx = rx + S(56), ty = ry;
        int shown = 0;
        for (int i = 0; i < (int)app.store.tags.size(); ++i) {
            const Tag& tg = app.store.tags[i];
            if (ty > ry + S(44)) break;
            SizeF sz;
            MeasureText(g, tg.name, S(9.5f), false, sz);
            float w = sz.Width + S(22);
            if (tx + w > rx + rw) { tx = rx + S(56); ty += S(26); }
            if (ty > ry + S(44)) break;
            bool act = app.bulk.tags.count(tg.id) > 0;
            UI::Chip(g, RectF(tx, ty, w, S(22)), CMD_BULK_TAG_BASE + i, tg.name, act, Hex(tg.color),
                     [id = tg.id]() {
                         if (app.bulk.tags.count(id)) app.bulk.tags.erase(id);
                         else app.bulk.tags.insert(id);
                         UIInvalidate();
                     }, 9.5f);
            tx += w + S(4);
            ++shown;
        }
        ry += S(44) + S(26);
    }

    label(L"预设", ry);
    {
        UIEdit(9315, RectF(rx + S(56), ry - S(2), S(146), S(26)), &app.bulk.presetName, false, S(11));
        UI::Btn(g, RectF(rx + S(210), ry - S(2), S(84), S(26)), CMD_BULK_SAVE_PRESET, L"保存预设", 1, -1, []() {
            if (app.bulk.presetName.empty()) { ShowToast(L"先给预设起个名字"); return; }
            BulkPreset p;
            p.name = app.bulk.presetName;
            p.title = app.bulk.title;
            p.location = app.bulk.location;
            p.startMin = BulkParseHM(app.bulk.startText, 18 * 60);
            p.endMin = BulkParseHM(app.bulk.endText, 20 * 60);
            p.colorIdx = app.bulk.colorIdx;
            p.customColor = app.bulk.customColor;
            for (auto& t : app.bulk.tags) p.tags.push_back(t);
            bool replaced = false;
            for (auto& old : app.bulk.presets) if (old.name == p.name) { old = p; replaced = true; break; }
            if (!replaced) app.bulk.presets.push_back(p);
            SaveBulkPresets();
            ShowToast(L"预设已保存");
            UIInvalidate();
        });
        float py = ry + S(32);
        for (int i = 0; i < (int)app.bulk.presets.size() && i < 6; ++i) {
            const BulkPreset& p = app.bulk.presets[i];
            float bx = rx + S(56) + (i % 2) * S(168);
            float byy = py + (i / 2) * S(28);
            UI::Chip(g, RectF(bx, byy, S(140), S(24)), CMD_BULK_PRESET_APPLY_BASE + i, p.name, false, th.accent2, [i]() {
                if (i >= (int)app.bulk.presets.size()) return;
                const BulkPreset pp = app.bulk.presets[i];
                app.bulk.title = pp.title;
                app.bulk.location = pp.location;
                app.bulk.startText = BulkFmtHM(pp.startMin);
                app.bulk.endText = BulkFmtHM(pp.endMin);
                if (app.bulk.perEditSerial == 0) BulkLoadTarget();
                app.bulk.colorIdx = pp.colorIdx;
                app.bulk.customColor = pp.customColor;
                app.bulk.tags.clear();
                for (auto& t : pp.tags) app.bulk.tags.insert(t);
                UIInvalidate();
            }, 9.0f);
            AddHit(bx + S(142), byy, S(18), S(24), CMD_BULK_PRESET_DEL_BASE + i, [i]() {
                if (i < (int)app.bulk.presets.size()) {
                    app.bulk.presets.erase(app.bulk.presets.begin() + i);
                    SaveBulkPresets();
                    UIInvalidate();
                }
            });
            Txt(g, L"×", S(11), false, th.dim, RectF(bx + S(142), byy, S(18), S(24)), ALIGN_CENTER | ALIGN_VCENTER);
        }
    }

    // ---- 逐日调整 ----
    {
        float dy = y + S(430);
        Txt(g, L"逐日调整：点下面日期切换目标，改右侧时间/地点后点「应用修改」", S(9), false, th.dim2,
            RectF(lx, dy, lw, S(16)));
        std::vector<int> ds(app.bulk.days.begin(), app.bulk.days.end());
        std::sort(ds.begin(), ds.end());
        int maxChips = std::min<int>((int)ds.size(), 12);
        for (int i = 0; i < maxChips; ++i) {
            int serial = ds[i];
            Date d = FromSerial(serial);
            bool cur = (app.bulk.perEditSerial == serial);
            bool hasOv = BulkDayHasOverride(serial);
            std::wstring labelTxt = Fmt(L"%d/%d%s", d.m, d.d, hasOv ? L"*" : L"");
            float bx = lx + (i % 6) * S(70);
            float byy = dy + S(20) + (i / 6) * S(26);
            UI::Chip(g, RectF(bx, byy, S(64), S(22)), CMD_BULK_TARGET_BASE + i, labelTxt, cur, th.accent, [serial]() {
                if (app.bulk.perEditSerial == serial) app.bulk.perEditSerial = 0;
                else app.bulk.perEditSerial = serial;
                BulkLoadTarget();
                UIInvalidate();
            }, 9.0f);
        }
        float ty = dy + S(20) + S(56);
        if (app.bulk.perEditSerial) {
            Date d = FromSerial(app.bulk.perEditSerial);
            Txt(g, Fmt(L"当前目标：%d月%d日（单独）", d.m, d.d), S(9.5f), true, th.accent,
                RectF(lx, ty, S(220), S(18)), ALIGN_VCENTER);
            UI::Btn(g, RectF(lx + S(226), ty, S(110), S(22)), CMD_BULK_SAVE_PRESET + 11, L"恢复继承公共", 1, -1, []() {
                app.bulk.dayOverrides.erase(app.bulk.perEditSerial);
                app.bulk.perEditSerial = 0;
                BulkLoadTarget();
                ShowToast(L"已恢复继承公共");
                UIInvalidate();
            });
        } else {
            Txt(g, L"当前目标：公共字段（所有日期默认使用）", S(9.5f), true, th.dim,
                RectF(lx, ty, S(320), S(18)), ALIGN_VCENTER);
        }
    }

    // ---- 底部 ----
    float by = y + ph - S(66);
    {
        std::vector<int> ds(app.bulk.days.begin(), app.bulk.days.end());
        std::wstring sum;
        for (int i = 0; i < (int)ds.size() && i < 8; ++i) {
            Date d = FromSerial(ds[i]);
            if (i) sum += L"、";
            sum += Fmt(L"%d/%d", d.m, d.d);
        }
        if (ds.size() > 8) sum += Fmt(L" 等 %d 天", (int)ds.size());
        Txt(g, ds.empty() ? L"还没选日期：在左边年历点日期" : Fmt(L"已选 %d 天：%s", (int)ds.size(), sum.c_str()),
            S(10.5f), true, ds.empty() ? th.dim2 : th.accent, RectF(x + S(24), by, pw - S(470), S(24)), ALIGN_VCENTER);
        UI::Btn(g, RectF(x + S(24), by + S(30), S(110), S(30)), CMD_BULK_CLEAR_DAYS, L"清空日期", 1, -1, []() {
            app.bulk.days.clear();
            UIInvalidate();
        });
    }
    UI::Btn(g, RectF(x + pw - S(24) - S(90), by + S(26), S(90), S(34)), CMD_BULK_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        app.bulk.open = false;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(130) - S(8), by + S(26), S(130), S(34)), CMD_BULK_OK,
            Fmt(L"创建 %d 条", (int)app.bulk.days.size()), 0, -1, []() {
        if (app.bulk.days.empty()) { ShowToast(L"先在年历里选日期"); return; }
        if (app.bulk.title.empty()) { ShowToast(L"标题不能为空"); return; }
        BulkApplyTarget();
        int smPub = BulkParseHM(app.bulk.startText, 18 * 60);
        int emPub = BulkParseHM(app.bulk.endText, 20 * 60);
        app.store.Snapshot();
        auto dupKey = [](const Event& e) {
            return e.title + L"|" + ISODT(e.start) + L"|" + ISODT(e.end) + L"|" + (e.allDay ? L"1" : L"0");
        };
        std::set<std::wstring> seen;
        for (auto& old : app.store.events) seen.insert(dupKey(old));
        const auto& pal = PaletteHex();
        int added = 0, skipped = 0;
        for (int serial : app.bulk.days) {
            int sm = smPub, em = emPub;
            std::wstring loc = app.bulk.location;
            auto oit = app.bulk.dayOverrides.find(serial);
            if (oit != app.bulk.dayOverrides.end()) {
                if (oit->second.startMin >= 0) sm = oit->second.startMin;
                if (oit->second.endMin >= 0) em = oit->second.endMin;
                if (!oit->second.location.empty()) loc = oit->second.location;
            }
            Date d = FromSerial(serial);
            DateTime sd = MakeDT(d.y, d.m, d.d);
            DateTime ed = sd;
            if (em <= sm) ed = AddDaysDT(ed, 1);
            DateTime s = sd;
            s.h = sm / 60; s.mi = sm % 60;
            DateTime e = ed;
            e.h = em / 60; e.mi = em % 60;
            Event ev;
            ev.title = app.bulk.title;
            ev.location = loc;
            ev.start = s;
            ev.end = e;
            for (auto& t : app.bulk.tags) ev.tags.push_back(t);
            if (!app.bulk.customColor.empty()) ev.color = app.bulk.customColor;
            else if (app.bulk.colorIdx >= 0 && app.bulk.colorIdx < (int)pal.size()) ev.color = pal[app.bulk.colorIdx];
            if (!seen.insert(dupKey(ev)).second) { ++skipped; continue; }
            app.store.AddEvent(ev);
            ++added;
        }
        app.overlay = OV_NONE;
        app.bulk.open = false;
        InvalidateOccs();
        DoSaveStore();
        ShowToast(skipped > 0 ? Fmt(L"已创建 %d 条，跳过 %d 条重复", added, skipped)
                              : Fmt(L"已创建 %d 条日程", added));
        UIInvalidate();
    });
}

// ---------------- 日程模板 ----------------
static std::wstring EventTemplatePath() { return PathJoin(GetDataDir(), L"event-templates.json"); }

static void LoadEventTemplates(std::vector<EventTemplate>& out) {
    out.clear();
    bool ok = false;
    std::string u8 = ReadFileBytes(EventTemplatePath(), &ok);
    if (ok && !u8.empty()) {
        JValue root = JsonParse(u8);
        if (root.IsArr()) {
            for (auto& it : root.arr) {
                if (!it.IsObj()) continue;
                EventTemplate t;
                t.name = it.Has(L"name") ? it.Get(L"name").AsStr() : L"";
                t.title = it.Has(L"title") ? it.Get(L"title").AsStr() : t.name;
                t.location = it.Has(L"location") ? it.Get(L"location").AsStr() : L"";
                t.startMin = it.Has(L"start") ? it.Get(L"start").AsInt(18 * 60) : 18 * 60;
                t.endMin = it.Has(L"end") ? it.Get(L"end").AsInt(20 * 60) : 20 * 60;
                t.color = it.Has(L"color") ? it.Get(L"color").AsStr() : L"";
                if (it.Has(L"tags") && it.Get(L"tags").IsArr())
                    for (auto& tg : it.Get(L"tags").arr) t.tags.push_back(WideToUtf8(tg.AsStr()));
                if (!t.name.empty()) out.push_back(t);
            }
        }
    }
    if (out.empty()) {
        EventTemplate t;
        t.name = L"早八"; t.title = L"早八"; t.startMin = 8 * 60; t.endMin = 9 * 60 + 40;
        out.push_back(t);
        t.name = L"晚自习"; t.title = L"晚自习"; t.startMin = 18 * 60; t.endMin = 20 * 60;
        out.push_back(t);
        t.name = L"实验课"; t.title = L"实验课"; t.startMin = 14 * 60; t.endMin = 15 * 60 + 40;
        out.push_back(t);
        t.name = L"小组会议"; t.title = L"小组会议"; t.startMin = 20 * 60; t.endMin = 21 * 60;
        out.push_back(t);
    }
}
static void SaveEventTemplates(const std::vector<EventTemplate>& list) {
    JValue root = JValue::Arr();
    for (auto& t : list) {
        JValue o = JValue::Obj();
        o.Set(L"name") = JValue(t.name);
        o.Set(L"title") = JValue(t.title);
        o.Set(L"location") = JValue(t.location);
        o.Set(L"start") = JValue(t.startMin);
        o.Set(L"end") = JValue(t.endMin);
        if (!t.color.empty()) o.Set(L"color") = JValue(t.color);
        JValue tags = JValue::Arr();
        for (auto& tg : t.tags) tags.Push(JValue(Utf8ToWide(tg)));
        o.Set(L"tags") = tags;
        root.Push(o);
    }
    WriteFileBytesAtomic(EventTemplatePath(), JsonDump(root, true));
}

void OpenTemplateNewMenu() {
    std::vector<EventTemplate> tpls;
    LoadEventTemplates(tpls);
    std::vector<MenuItemDef> items;
    for (auto& t : tpls) {
        std::wstring label = t.name + L"   " + Fmt(L"%02d:%02d–%02d:%02d", t.startMin / 60, t.startMin % 60,
                                                    t.endMin / 60, t.endMin % 60);
        EventTemplate copy = t;
        items.push_back({ label, ICO_CLOCK, false, false, [copy]() {
            DateTime day = app.cursor.y ? StartOfDay(app.cursor) : Today();
            OpenEditorNew(day, copy.startMin / 60, copy.startMin % 60);
            app.editor.title = copy.title;
            app.editor.location = copy.location;
            app.editor.time1 = BulkFmtHM(copy.startMin);
            app.editor.time2 = BulkFmtHM(copy.endMin);
            app.editor.color = copy.color;
            app.editor.tags.clear();
            for (auto& tg : copy.tags) app.editor.tags.insert(tg);
            app.editor.alarms = copy.alarms;
            UIInvalidate();
        } });
    }
    OpenMenu(0, 0, items);
}

void SaveEditorAsTemplate() {
    Event out;
    std::wstring err;
    if (!app.editor.Collect(out, err) || out.title.empty()) {
        ShowToast(L"标题为空，无法保存模板");
        return;
    }
    std::vector<EventTemplate> tpls;
    LoadEventTemplates(tpls);
    EventTemplate t;
    t.name = out.title;
    t.title = out.title;
    t.location = out.location;
    t.startMin = out.start.h * 60 + out.start.mi;
    t.endMin = out.end.h * 60 + out.end.mi;
    t.color = out.color;
    t.tags = out.tags;
    t.alarms = out.alarms;
    bool replaced = false;
    for (auto& x : tpls) {
        if (x.name == t.name) { x = t; replaced = true; break; }
    }
    if (!replaced) {
        tpls.insert(tpls.begin(), t);
        if ((int)tpls.size() > 20) tpls.resize(20);
    }
    SaveEventTemplates(tpls);
    ShowToast(Fmt(L"已保存模板「%s」", t.name.c_str()));
    UIInvalidate();
}

void OpenLocationPresetMenu() {
    std::vector<std::wstring> locs;
    for (int i = (int)app.store.events.size() - 1; i >= 0 && (int)locs.size() < 12; --i) {
        const Event& e = app.store.events[i];
        if (e.location.empty()) continue;
        if (std::find(locs.begin(), locs.end(), e.location) == locs.end()) locs.push_back(e.location);
    }
    if (locs.empty()) {
        ShowToast(L"还没有用过的地点");
        return;
    }
    std::vector<MenuItemDef> items;
    for (auto& l : locs) {
        items.push_back({ l, ICO_MAP, false, false, [l]() {
            app.editor.location = l;
            UIInvalidate();
        } });
    }
    OpenMenu(0, 0, items, OV_EDITOR);
}

// ---------------- 导入 ICS ----------------
void DrawImportOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(600), ph = S(540);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, L"导入 ICS 日历", S(16), true, th.text, RectF(x + S(24), y + S(18), pw - S(60), S(26)));
    Txt(g, app.import.file, S(9.5f), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    auto& pr = app.import.parsed;
    RectF info(x + S(24), y + S(74), pw - S(48), S(168));
    FillRoundRect(g, info, S(10), WithAlpha(th.bg, 170));
    StrokeRoundRect(g, info, S(10), th.borderSoft, S(1));
    Txt(g, Fmt(L"解析到 %d 个日程", (int)pr.events.size()), S(14), true, th.accent, RectF(info.X + S(14), info.Y + S(12), S(240), S(22)));
    if (pr.hasRange) {
        Txt(g, Fmt(L"日期范围：%s → %s", ISODate(pr.minDate).c_str(), ISODate(pr.maxDate).c_str()),
            S(10), false, th.dim, RectF(info.X + S(14), info.Y + S(40), info.Width - S(28), S(18)));
    }
    if (!pr.calendarName.empty()) {
        Txt(g, L"日历名称：" + pr.calendarName, S(10), false, th.dim, RectF(info.X + S(14), info.Y + S(60), info.Width - S(28), S(18)));
    }
    int warnN = (int)pr.warnings.size();
    Txt(g, warnN ? Fmt(L"%d 条提示", warnN) : L"没有发现问题", S(10), false, warnN ? th.warn : th.success,
        RectF(info.X + S(14), info.Y + S(80), info.Width - S(28), S(18)));
    float wy = info.Y + S(104);
    for (int i = 0; i < warnN && i < 3; ++i) {
        Txt(g, pr.warnings[i], S(9), false, th.dim2, RectF(info.X + S(14), wy, info.Width - S(28), S(16)), ALIGN_LEFT, false, true);
        wy += S(17);
    }
    Txt(g, L"合并方式", S(10.5f), false, th.dim2, RectF(x + S(24), y + S(256), S(120), S(18)));
    const wchar_t* modes[3] = { L"按 UID 合并（推荐）", L"全部追加", L"跳过重复" };
    for (int i = 0; i < 3; ++i) {
        RectF r(x + S(24) + i * S(180), y + S(278), S(172), S(32));
        UI::Chip(g, r, CMD_IMPORT_MODE_BASE + i, modes[i], app.import.mode == i, th.accent, [i]() {
            app.import.mode = i;
            UIInvalidate();
        }, 10.5f);
    }
    Txt(g, L"提示：合并模式会更新同 UID 的已有日程；导入不会删除本地已有日程。", S(9), false, th.dim2,
        RectF(x + S(24), y + S(320), pw - S(48), S(16)));
    RectF smartR(x + S(24), y + S(340), pw - S(48), S(42));
    {
        bool on = app.import.smart;
        RectF cb(smartR.X, smartR.Y + S(2), S(18), S(18));
        FillRoundRect(g, cb, S(5), on ? th.accent : WithAlpha(th.panel2, 200));
        StrokeRoundRect(g, cb, S(5), on ? th.accent : th.border, S(1));
        if (on) Icon(g, ICO_CHECK, RectF(cb.X + S(3), cb.Y + S(3), S(12), S(12)), th.onAccent, 2.0f);
        Txt(g, L"智能整理：自动识别课程并打「课程」标签、自动配色", S(9.5f), false, on ? th.text : th.dim,
            RectF(smartR.X + S(26), smartR.Y - S(1), smartR.Width - S(26), S(18)), ALIGN_LEFT, false, false);
        Txt(g, L"把每周重复的课合并为可编辑的重复系列（可撤销）", S(9.5f), false, on ? WithAlpha(th.text, 220) : th.dim2,
            RectF(smartR.X + S(26), smartR.Y + S(18), smartR.Width - S(26), S(18)), ALIGN_LEFT, false, false);
        AddHit(smartR.X, smartR.Y, smartR.Width, smartR.Height, CMD_IMPORT_SMART, []() {
            app.import.smart = !app.import.smart;
            UIInvalidate();
        });
    }
    Txt(g, L"导入后逐门课配色：下一步可单独修改每一门", S(9), false, WithAlpha(th.dim2, 200),
        RectF(x + S(24), y + S(386), pw - S(48), S(16)));
    Txt(g, L"导入后统一颜色", S(10.5f), false, th.dim2, RectF(x + S(24), y + S(404), S(160), S(18)));
    {
        const auto& colors = PaletteHex();
        float swx = x + S(24);
        UI::Chip(g, RectF(swx, y + S(424), S(52), S(24)), CMD_IMPORT_COLOR_BASE, L"自动",
                 app.import.colorIdx < 0 && app.import.colorCustom.empty(), th.accent, []() {
                     app.import.colorIdx = -1;
                     app.import.colorCustom.clear();
                     UIInvalidate();
                 }, 9.5f);
        swx += S(60);
        for (int i = 0; i < (int)colors.size(); ++i) {
            RectF sw(swx + i * S(24), y + S(426), S(20), S(20));
            FillRoundRect(g, sw, S(5), Hex(colors[i]));
            if (app.import.colorIdx == i) StrokeRoundRect(g, sw, S(5), th.text, S(2));
            AddHit(sw.X, sw.Y, sw.Width, sw.Height, CMD_IMPORT_COLOR_BASE + 1 + i, [i]() {
                app.import.colorIdx = i;
                app.import.colorCustom.clear();
                UIInvalidate();
            });
        }
        float cux = swx + colors.size() * S(24) + S(4);
        RectF cu(cux, y + S(426), S(28), S(20));
        FillRoundRect(g, cu, S(5), app.import.colorCustom.empty() ? WithAlpha(th.panel2, 200) : Hex(app.import.colorCustom));
        StrokeRoundRect(g, cu, S(5), WithAlpha(th.text, 90), S(1));
        Txt(g, L"…", S(11), true, WithAlpha(th.text, 220), cu, ALIGN_CENTER | ALIGN_VCENTER);
        AddHit(cu.X, cu.Y, cu.Width, cu.Height, CMD_IMPORT_COLOR_BASE + 100, []() {
            OpenColorPicker(L"导入统一颜色", app.import.colorCustom.empty() ? L"#6EA8FF" : app.import.colorCustom,
                            [](const std::wstring& hex) {
                                app.import.colorIdx = -1;
                                app.import.colorCustom = hex;
                                UIInvalidate();
                            });
        });
        Txt(g, L"选颜色会覆盖智能自动配色；点“…”自定义 HEX/RGB/CMYK", S(8.5f), false, WithAlpha(th.dim2, 190),
            RectF(x + S(24), y + S(452), pw - S(48), S(16)));
    }
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(54), S(90), S(34)), CMD_IMPORT_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        app.import.open = false;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) * 2 - S(8), y + ph - S(54), S(110), S(34)), CMD_IMPORT_OK, L"开始导入", 0, -1, []() {
        if (app.import.parsed.events.empty()) { ShowToast(L"没有可导入的日程"); return; }
        bool ok = false;
        std::string data = ReadFileBytes(app.import.file, &ok);
        if (!ok) { ShowToast(L"文件读取失败"); return; }
        app.store.Snapshot();
        IcsImportResult r = ImportIcs(app.store, data, app.import.mode, L"ICS 导入");
        std::vector<std::string> ids = r.addedIds;
        ids.insert(ids.end(), r.updatedIds.begin(), r.updatedIds.end());
        std::wstring smartNote;
        if (app.import.smart && !ids.empty()) {
            SmartOptions so;
            so.onlyIds = ids;
            so.onlyUntagged = true;
            SmartResult sr = SmartOrganize(app.store, so);
            smartNote = L" · " + sr.summary;
        }
        if (!app.import.colorCustom.empty() && !ids.empty()) {
            for (auto& id : ids) {
                Event* e = app.store.Find(id);
                if (e) { e->color = app.import.colorCustom; app.store.Touch(); }
            }
            smartNote += L" · 已统一自定义颜色";
        } else if (app.import.colorIdx >= 0 && !ids.empty()) {
            const auto& pal = PaletteHex();
            if (app.import.colorIdx < (int)pal.size()) {
                for (auto& id : ids) {
                    Event* e = app.store.Find(id);
                    if (e) { e->color = pal[app.import.colorIdx]; app.store.Touch(); }
                }
                smartNote += L" · 已统一颜色";
            }
        }
        InvalidateOccs();
        DoSaveStore();
        app.import.open = false;
        if (!ids.empty()) {
            app.seriesColor.open = true;
            app.seriesColor.scroll = 0;
            app.overlay = OV_SERIESCOLOR;
        } else {
            app.overlay = OV_NONE;
        }
        ShowToast(Fmt(L"导入完成：新增 %d，更新 %d，跳过 %d", r.added, r.updated, r.skipped) + smartNote, 6000);
        UIInvalidate();
    });
}

static void PickTagIconFile() {
    wchar_t file[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"图片 (*.png;*.jpg;*.jpeg;*.bmp;*.ico)\0*.png;*.jpg;*.jpeg;*.bmp;*.ico\0所有文件 (*.*)\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    std::wstring dir = PathJoin(GetDataDir(), L"icons");
    CreateDirectoryW(dir.c_str(), nullptr);
    const wchar_t* ext = wcsrchr(file, L'.');
    std::wstring extStr = ext ? ext : L".png";
    std::wstring name = Fmt(L"tagicon_%u%s", (unsigned)GetTickCount(), extStr.c_str());
    std::wstring dest = PathJoin(dir, name);
    if (!CopyFileW(file, dest.c_str(), FALSE)) {
        ShowToast(L"复制图标失败");
        return;
    }
    app.tagEdit.customIcon = L"icons/" + name;
    ShowToast(L"已导入自定义图标");
    UIInvalidate();
}

// ---------------- 标签编辑 ----------------
void DrawTagEditOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(600), ph = S(580);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, app.tagEdit.isNew ? L"新建标签" : L"编辑标签", S(16), true, th.text, RectF(x + S(24), y + S(18), pw - S(60), S(26)));
    Txt(g, L"名称", S(10), false, th.dim2, RectF(x + S(24), y + S(56), S(80), S(16)));
    RectF nameBox(x + S(24), y + S(74), pw - S(48), S(38));
    FillRoundRect(g, nameBox, S(8), th.panel2);
    StrokeRoundRect(g, nameBox, S(8), th.borderSoft, S(1));
    UIEdit(9501, RectF(nameBox.X + S(10), nameBox.Y + S(8), nameBox.Width - S(20), S(22)), &app.tagEdit.name, false, S(13));
    Txt(g, L"颜色", S(10), false, th.dim2, RectF(x + S(24), y + S(124), S(80), S(16)));
    const auto& colors = PaletteHex();
    for (int i = 0; i < (int)colors.size(); ++i) {
        float bx = x + S(24) + (i % 6) * S(54);
        float by = y + S(144) + (i / 6) * S(40);
        RectF sw(bx, by, S(28), S(28));
        bool active = (app.tagEdit.color == colors[i]);
        FillRoundRect(g, sw, S(7), Hex(colors[i]));
        if (active) StrokeRoundRect(g, sw, S(7), th.text, S(2));
        std::wstring c = colors[i];
        AddHit(sw.X, sw.Y, sw.Width, sw.Height, CMD_TAGEDIT_COLOR_BASE + i, [c]() {
            app.tagEdit.color = c;
            UIInvalidate();
        });
    }
    {
        RectF cu(x + pw - S(90), y + S(120), S(66), S(24));
        FillRoundRect(g, cu, S(6), WithAlpha(th.panel2, 200));
        StrokeRoundRect(g, cu, S(6), WithAlpha(th.text, 80), S(1));
        Txt(g, L"自定义…", S(9), false, th.dim, cu, ALIGN_CENTER | ALIGN_VCENTER);
        AddHit(cu.X, cu.Y, cu.Width, cu.Height, CMD_TAGEDIT_COLOR_CUSTOM, []() {
            OpenColorPicker(L"标签颜色", app.tagEdit.color, [](const std::wstring& hex) {
                app.tagEdit.color = hex;
                UIInvalidate();
            });
        });
    }
    // 图标选择
    Txt(g, L"图标", S(10), false, th.dim2, RectF(x + S(24), y + S(292), S(80), S(16)));
    const auto& icons = TagIconChoices();
    int cols = 10;
    float cell = S(38), gap = S(5);
    float gridW = cols * cell + (cols - 1) * gap;
    float gx = x + (pw - gridW) / 2;
    for (int i = 0; i < (int)icons.size(); ++i) {
        float bx = gx + (i % cols) * (cell + gap);
        float by = y + S(314) + (i / cols) * (cell + gap);
        RectF box(bx, by, cell, cell);
        bool active = (app.tagEdit.icon == icons[i]);
        if (active) {
            FillRoundRect(g, box, S(8), WithAlpha(th.accent, 44));
            StrokeRoundRect(g, box, S(8), WithAlpha(th.accent, 170), S(1.5f));
        } else if (IsHovered(CMD_TAGEDIT_ICON_BASE + i)) {
            FillRoundRect(g, box, S(8), WithAlpha(th.cardHover, 200));
        }
        Color col = active ? Hex(app.tagEdit.color) : th.dim;
        Icon(g, IconFromName(icons[i]), RectF(bx + S(10), by + S(10), cell - S(20), cell - S(20)), col, 1.5f);
        std::wstring key = icons[i];
        AddHit(bx, by, cell, cell, CMD_TAGEDIT_ICON_BASE + i, [key]() {
            app.tagEdit.icon = key;
            UIInvalidate();
        });
    }
    {
        float ry2 = y + S(492);
        // 计入 DDL 看板
        RectF cb(x + S(24), ry2, S(18), S(18));
        FillRoundRect(g, cb, S(5), app.tagEdit.task ? th.accent : WithAlpha(th.panel2, 200));
        StrokeRoundRect(g, cb, S(5), app.tagEdit.task ? th.accent : th.border, S(1));
        if (app.tagEdit.task) Icon(g, ICO_CHECK, RectF(cb.X + S(3), cb.Y + S(3), S(12), S(12)), th.onAccent, 2.0f);
        Txt(g, L"计入 DDL / 作业看板", S(10), false, app.tagEdit.task ? th.text : th.dim,
            RectF(cb.X + S(26), ry2 - S(1), S(210), S(20)), ALIGN_VCENTER);
        AddHit(cb.X, cb.Y, S(240), S(22), CMD_TAGEDIT_TASK, []() {
            app.tagEdit.task = !app.tagEdit.task;
            UIInvalidate();
        });
        // 自定义图标
        if (!app.tagEdit.customIcon.empty()) {
            Tag tmp;
            tmp.id = app.tagEdit.editingId;
            tmp.name = app.tagEdit.name;
            tmp.color = app.tagEdit.color;
            tmp.icon = app.tagEdit.icon;
            tmp.customIcon = app.tagEdit.customIcon;
            DrawTagIcon(g, tmp, RectF(x + S(340), ry2 - S(4), S(24), S(24)), th.dim, 1.5f);
        }
        UI::Btn(g, RectF(x + S(372), ry2 - S(6), S(140), S(28)), CMD_TAGEDIT_ICON_IMPORT,
                app.tagEdit.customIcon.empty() ? L"导入自定义图标…" : L"更换图标…", 1, ICO_FOLDER, []() {
                    PickTagIconFile();
                });
        if (!app.tagEdit.customIcon.empty()) {
            UI::Btn(g, RectF(x + S(520), ry2 - S(6), S(52), S(28)), CMD_TAGEDIT_ICON_CLEAR, L"清除", 1, -1, []() {
                app.tagEdit.customIcon.clear();
                UIInvalidate();
            });
        }
    }
    if (!app.tagEdit.error.empty()) {
        Txt(g, app.tagEdit.error, S(10), false, th.danger, RectF(x + S(24), y + ph - S(84), pw - S(200), S(18)));
    }
    UI::Btn(g, RectF(x + pw - S(24) - S(88), y + ph - S(52), S(88), S(34)), CMD_TAGEDIT_CANCEL, L"取消", 1, -1, []() {
        app.overlay = OV_NONE;
        app.tagEdit.open = false;
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(24) - S(88) * 2 - S(8), y + ph - S(52), S(88), S(34)), CMD_TAGEDIT_OK, L"保存", 0, -1, []() {
        std::wstring name = Trim(app.tagEdit.name);
        if (name.empty()) { app.tagEdit.error = L"名称不能为空"; UIInvalidate(); return; }
        app.store.Snapshot();
        if (app.tagEdit.isNew) {
            std::string nid = app.store.AddTag(name, app.tagEdit.color, app.tagEdit.icon);
            Tag* t = app.store.FindTag(nid);
            if (t) {
                t->task = app.tagEdit.task;
                t->customIcon = app.tagEdit.customIcon;
            }
        } else {
            Tag* t = app.store.FindTag(app.tagEdit.editingId);
            if (t) {
                t->name = name;
                t->color = app.tagEdit.color;
                t->icon = app.tagEdit.icon;
                t->task = app.tagEdit.task;
                t->customIcon = app.tagEdit.customIcon;
            }
        }
        app.store.Touch();
        DoSaveStore();
        app.overlay = OV_SETTINGS;   // 回到设置
        app.tagEdit.open = false;
        UIInvalidate();
    });
}

// ---------------- 机器人（Telegram） ----------------
void OpenBotPanel() {
    app.overlay = OV_BOT;
    UIInvalidate();
}

void OpenHotkeys() {
    app.overlay = OV_HOTKEYS;
    app.hotkeysCapturing = false;
    app.hotkeyCaptureIdx = -1;
    app.hotkeyError.clear();
    UIInvalidate();
}

void DrawHotkeysOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(640), ph = S(470);
    pw = std::min(pw, W - S(32));
    ph = std::min(ph, H - S(24));
    float x = (W - pw) / 2, y = (H - ph) / 2;
    Shadow(g, RectF(x, y, pw, ph), S(18), 8, 26);
    FillRoundRect(g, RectF(x, y, pw, ph), S(18), th.card);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(18), th.border, S(1));
    Txt(g, L"快捷键设置", S(17), true, th.text, RectF(x + S(22), y + S(16), S(240), S(26)));
    Txt(g, L"点一行后按下新组合键（至少 Ctrl 或 Alt）；Esc 取消", S(9), false, th.dim2,
        RectF(x + S(22), y + S(44), pw - S(44), S(16)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_HOTKEYS_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        app.hotkeysCapturing = false;
        app.hotkeyCaptureIdx = -1;
        UIInvalidate();
    });
    Settings& st = app.store.settings;
    const wchar_t* names[8] = { L"月视图", L"周视图", L"日视图", L"年视图", L"日程列表", L"回到今天", L"搜索", L"新建日程" };
    const std::wstring* keys[8] = { &st.hotkeyMonth, &st.hotkeyWeek, &st.hotkeyDay, &st.hotkeyYear,
                                    &st.hotkeyAgenda, &st.hotkeyToday, &st.hotkeySearch, &st.hotkeyNew };
    float rowY = y + S(72);
    for (int i = 0; i < 8; ++i) {
        bool capturing = app.hotkeysCapturing && app.hotkeyCaptureIdx == i;
        RectF row(x + S(20), rowY + i * S(38), pw - S(40), S(32));
        FillRoundRect(g, row, S(8), capturing ? WithAlpha(th.accent, 26) : WithAlpha(th.panel2, 150));
        StrokeRoundRect(g, row, S(8), capturing ? WithAlpha(th.accent, 150) : th.borderSoft, S(1));
        Txt(g, names[i], S(10.5f), false, capturing ? th.accent : th.text, RectF(row.X + S(12), row.Y, S(200), row.Height), ALIGN_VCENTER);
        std::wstring key = capturing ? (app.hotkeyError.empty() ? L"按下新按键…" : app.hotkeyError) : *keys[i];
        Txt(g, key, S(10), true, capturing ? (app.hotkeyError.empty() ? th.accent : th.danger) : th.dim,
            RectF(row.X + S(240), row.Y, row.Width - S(252), row.Height), ALIGN_RIGHT | ALIGN_VCENTER, false, true);
        int idx = i;
        AddHit(row.X, row.Y, row.Width, row.Height, CMD_HOTKEYS_ROW_BASE + i, [idx]() {
            app.hotkeysCapturing = true;
            app.hotkeyCaptureIdx = idx;
            app.hotkeyError.clear();
            UIInvalidate();
        });
    }
    UI::Btn(g, RectF(x + S(20), y + ph - S(48), S(120), S(32)), CMD_HOTKEYS_RESET, L"恢复默认", 1, -1, []() {
        Settings& st = app.store.settings;
        st.hotkeyMonth = L"M"; st.hotkeyWeek = L"W"; st.hotkeyDay = L"D"; st.hotkeyYear = L"Y";
        st.hotkeyAgenda = L"A"; st.hotkeyToday = L"T"; st.hotkeySearch = L"Ctrl+F"; st.hotkeyNew = L"Ctrl+N";
        app.needSave = true;
        DoSaveStore();
        ShowToast(L"快捷键已恢复默认");
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(110), y + ph - S(48), S(90), S(32)), CMD_HOTKEYS_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        app.hotkeysCapturing = false;
        app.hotkeyCaptureIdx = -1;
        UIInvalidate();
    });
}

void DrawBotOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(640), ph = S(520);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, L"机器人（Telegram）", S(16), true, th.text, RectF(x + S(24), y + S(18), pw - S(60), S(26)));
    Txt(g, L"1. 在 Telegram 找 @BotFather，/newbot 创建机器人，复制 token；2. 填到下面点保存；3. 给机器人发 /start 完成绑定。",
        S(9.5f), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(40)), ALIGN_LEFT, false, true);
    Txt(g, L"权限说明：机器人只响应绑定的这个聊天；新增日程会直接写入本机数据。",
        S(9), false, WithAlpha(th.dim2, 200), RectF(x + S(24), y + S(88), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    float ry = y + S(120);
    Txt(g, L"Bot Token", S(10), false, th.dim2, RectF(x + S(24), ry, S(90), S(28)), ALIGN_VCENTER);
    UIEdit(9340, RectF(x + S(112), ry, pw - S(136), S(28)), &app.bot.token, false, S(11));
    ry += S(44);
    UI::Btn(g, RectF(x + S(112), ry, S(140), S(30)), CMD_BOT_TOGGLE, app.bot.enabled ? L"停用机器人" : L"保存并启用", 1, ICO_CHECK, []() {
        BotSaveToken(app.bot.token);
        BotSetEnabled(!app.bot.enabled);
        UIInvalidate();
    });
    Txt(g, app.bot.enabled ? (app.bot.chatId ? L"已绑定聊天 ID：" + Fmt(L"%lld", app.bot.chatId) : L"等待 /start 绑定") : L"未启用",
        S(9.5f), false, app.bot.enabled ? th.success : th.dim2, RectF(x + S(262), ry, pw - S(300), S(30)), ALIGN_VCENTER, false, true);
    UI::Btn(g, RectF(x + S(332), ry, S(150), S(32)), CMD_ACC_MERGE, L"双向同步（合并）", 2, ICO_REFRESH, []() {
        if (!app.syncUi.loggedIn) { app.syncUi.error = L"请先登录"; UIInvalidate(); return; }
        std::string remoteJson;
        std::wstring err;
        if (!SyncPullStore(app.syncUi.server, app.syncUi.token, remoteJson, err)) {
            app.syncUi.error = err;
            UIInvalidate();
            return;
        }
        GradesApplyFromStoreJson(remoteJson);
        Store remote;
        if (!remote.FromJsonStr(remoteJson, &err)) {
            app.syncUi.error = L"服务器数据损坏：" + err;
            UIInvalidate();
            return;
        }
        MergeStats st;
        Store merged = MergeStores(app.store, remote, st);
        EnsureDir(PathJoin(GetDataDir(), L"backup"));
        std::wstring bak = PathJoin(GetDataDir(), L"backup/manual-before-merge-" + ISODate(app.now) + L"-" +
                                                  Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
        CopyFileSimple(GetCalendarPath(), bak);
        app.store = std::move(merged);
        InvalidateOccs();
        DoSaveStore();
        if (!SyncPushStore(app.syncUi.server, app.syncUi.token,
                           GradesAttachToStoreJson(app.store.ToJsonStr()), err)) {
            app.syncUi.error = L"合并完成但上传失败：" + err;
        } else {
            app.syncUi.status = Fmt(L"合并完成：采用服务器 %d、本地保留 %d、新增 %d、删除 %d；已上传",
                                    st.updated, st.kept, st.added, st.deleted);
            app.syncUi.error.clear();
        }
        UIInvalidate();
    });
    ry += S(48);
    Txt(g, L"状态：" + app.bot.status, S(10), false, app.bot.status.rfind(L"错误", 0) == 0 ? th.danger : th.dim,
        RectF(x + S(24), ry, pw - S(48), S(20)), ALIGN_LEFT, false, true);
    ry += S(28);
    Txt(g, L"命令：/today 今日 · /week 未来 7 天 · /add 明天 9:00 开会 #会议 · /help",
        S(9), false, WithAlpha(th.dim2, 200), RectF(x + S(24), ry, pw - S(48), S(18)), ALIGN_LEFT, false, true);
    Txt(g, L"注意：token 只保存在本机 data\\bot.json，不会上传；机器人需要能访问 api.telegram.org。",
        S(8.5f), false, WithAlpha(th.dim2, 180), RectF(x + S(24), y + ph - S(92), pw - S(48), S(32)), ALIGN_LEFT, false, true);
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(54), S(90), S(34)), CMD_BOT_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}

// ---------------- 账号同步 ----------------
void OpenAccountSync() {
    if (app.syncUi.server.empty()) app.syncUi.server = L"http://127.0.0.1:17890";
    app.syncUi.error.clear();
    app.overlay = OV_ACCOUNT;
    UIInvalidate();
}

void ApplyPulledStore(const std::string& storeJson) {
    GradesApplyFromStoreJson(storeJson);
    Store tmp;
    std::wstring err;
    if (!tmp.FromJsonStr(storeJson, &err)) {
        app.syncUi.error = L"服务器数据损坏：" + err;
        UIInvalidate();
        return;
    }
    EnsureDir(PathJoin(GetDataDir(), L"backup"));
    std::wstring bak = PathJoin(GetDataDir(), L"backup/manual-before-pull-" + ISODate(app.now) + L"-" +
                                              Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
    CopyFileSimple(GetCalendarPath(), bak);
    int n = (int)tmp.events.size();
    app.store = std::move(tmp);
    InvalidateOccs();
    DoSaveStore();
    app.syncUi.status = Fmt(L"已从服务器拉取 %d 条日程（拉取前已备份）", n);
    app.syncUi.error.clear();
    UIInvalidate();
}

void DrawAccountSyncOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(640), ph = S(560);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Txt(g, L"账号同步", S(16), true, th.text, RectF(x + S(24), y + S(18), pw - S(60), S(26)));
    Txt(g, L"先把一端星历的「分享」服务开启（默认端口 17890），另一端填服务器地址登录后上传/拉取。",
        S(9.5f), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    Txt(g, L"上传/拉取都会与服务器合并（按事件 updated 取较新，两边独有事件都保留），不会整份覆盖。",
        S(9.5f), false, WithAlpha(th.dim2, 200), RectF(x + S(24), y + S(64), pw - S(48), S(18)), ALIGN_LEFT, false, true);

    float ry = y + S(104);
    Txt(g, L"服务器", S(10), false, th.dim2, RectF(x + S(24), ry, S(70), S(24)), ALIGN_VCENTER);
    UIEdit(9330, RectF(x + S(96), ry, pw - S(120), S(28)), &app.syncUi.server, false, S(11));
    ry += S(40);
    Txt(g, L"账号", S(10), false, th.dim2, RectF(x + S(24), ry, S(70), S(24)), ALIGN_VCENTER);
    UIEdit(9331, RectF(x + S(96), ry, S(220), S(28)), &app.syncUi.user, false, S(11));
    Txt(g, L"密码", S(10), false, th.dim2, RectF(x + S(330), ry, S(46), S(24)), ALIGN_VCENTER);
    UIEdit(9332, RectF(x + S(380), ry, S(236), S(28)), &app.syncUi.password, false, S(11));
    ry += S(44);
    UI::Btn(g, RectF(x + S(96), ry, S(110), S(30)), CMD_ACC_REGISTER, L"注册", 1, ICO_USER, []() {
        std::wstring err;
        if (SyncAccountRegister(app.syncUi.server, app.syncUi.user, app.syncUi.password, err)) {
            app.syncUi.status = L"注册成功，已登录";
            app.syncUi.error.clear();
        } else {
            app.syncUi.error = err;
        }
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + S(214), ry, S(110), S(30)), CMD_ACC_LOGIN, L"登录", 1, -1, []() {
        std::wstring err;
        if (SyncAccountLogin(app.syncUi.server, app.syncUi.user, app.syncUi.password, err)) {
            app.syncUi.status = L"登录成功";
            app.syncUi.error.clear();
        } else {
            app.syncUi.error = err;
        }
        UIInvalidate();
    });
    Txt(g, app.syncUi.loggedIn ? (L"已登录：" + app.syncUi.user) : L"未登录", S(9.5f), false,
        app.syncUi.loggedIn ? th.success : th.dim2, RectF(x + S(340), ry, pw - S(380), S(30)), ALIGN_VCENTER);
    ry += S(48);
    Txt(g, L"数据", S(10), false, th.dim2, RectF(x + S(24), ry, S(70), S(24)), ALIGN_VCENTER);
    UI::Btn(g, RectF(x + S(96), ry, S(112), S(32)), CMD_ACC_PUSH, L"上传", 1, ICO_COPY, []() {
        if (!app.syncUi.loggedIn) { app.syncUi.error = L"请先登录"; UIInvalidate(); return; }
        DoSaveStore();
        std::wstring err;
        // 先拉服务器数据与本地合并，再上传合并结果：避免用不完整的本地数据覆盖服务器
        std::string remoteJson;
        if (SyncPullStore(app.syncUi.server, app.syncUi.token, remoteJson, err)) {
            GradesApplyFromStoreJson(remoteJson);
            Store remote;
            if (remote.FromJsonStr(remoteJson, &err)) {
                MergeStats st;
                Store merged = MergeStores(app.store, remote, st);
                EnsureDir(PathJoin(GetDataDir(), L"backup"));
                std::wstring bak = PathJoin(GetDataDir(), L"backup/manual-before-upload-merge-" +
                                            ISODate(app.now) + L"-" + Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
                CopyFileSimple(GetCalendarPath(), bak);
                app.store = std::move(merged);
                InvalidateOccs();
                DoSaveStore();
                if (SyncPushStore(app.syncUi.server, app.syncUi.token,
                                  GradesAttachToStoreJson(app.store.ToJsonStr()), err)) {
                    app.syncUi.status = Fmt(L"已合并上传：服务器 %d + 本地 %d → %d 条日程、%d 门成绩",
                                            (int)remote.events.size(), (int)app.store.events.size(),
                                            (int)app.store.events.size(), GradesCount());
                    app.syncUi.error.clear();
                } else {
                    app.syncUi.error = L"合并完成但上传失败：" + err;
                }
                UIInvalidate();
                return;
            }
        }
        // 服务器无数据/拉取失败：直接上传本地（不会删除服务器数据，因为服务器是空的或不可达）
        app.syncUi.error.clear();
        if (SyncPushStore(app.syncUi.server, app.syncUi.token,
                          GradesAttachToStoreJson(app.store.ToJsonStr()), err)) {
            app.syncUi.status = Fmt(L"已上传 %d 条日程、%d 门成绩（服务器暂无可合并数据）",
                                    (int)app.store.events.size(), GradesCount());
            app.syncUi.error.clear();
        } else {
            app.syncUi.error = err;
        }
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + S(214), ry, S(112), S(32)), CMD_ACC_PULL, L"拉取", 1, ICO_REFRESH, []() {
        if (!app.syncUi.loggedIn) { app.syncUi.error = L"请先登录"; UIInvalidate(); return; }
        std::string store;
        std::wstring err;
        if (!SyncPullStore(app.syncUi.server, app.syncUi.token, store, err)) {
            app.syncUi.error = err;
            UIInvalidate();
            return;
        }
        OpenConfirm(L"从服务器拉取", L"将用服务器上的数据覆盖本地日程。\n拉取前会自动备份当前数据到 data\\backup。",
                    [store]() { ApplyPulledStore(store); }, L"拉取并覆盖", L"取消");
    });
    ry += S(48);
    if (!app.syncUi.status.empty())
        Txt(g, app.syncUi.status, S(9.5f), false, th.success, RectF(x + S(24), ry, pw - S(48), S(18)), ALIGN_LEFT, false, true);
    if (!app.syncUi.error.empty())
        Txt(g, app.syncUi.error, S(9.5f), false, th.danger, RectF(x + S(24), ry + S(20), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    Txt(g, L"提示：分享服务地址通常是 http://本机IP:17890；跨网可用 Tailscale。合并会先备份本地数据（backup/manual-before-merge-*.json）。",
        S(8.5f), false, WithAlpha(th.dim2, 200), RectF(x + S(24), y + ph - S(92), pw - S(48), S(32)), ALIGN_LEFT, false, true);

    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(54), S(90), S(34)), CMD_ACC_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}

// ---------------- 关于 ----------------
void DrawAboutOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(660), ph = S(520);
    float x, y;
    PanelFrame(g, W, H, pw, ph, x, y, S(16));
    Icon(g, ICO_STAR, RectF(x + S(24), y + S(26), S(20), S(20)), th.accent, 1.5f);
    {
        float lh = S(34), lw = lh * (739.0f / 326.0f);
        Bitmap* brand = BrandLogo();
        if (brand) g.DrawImage(brand, RectF(x + S(24) + S(24), y + S(18), lw, lh));
        else Txt(g, L"CAELENDAR", S(20), true, th.text, RectF(x + S(54), y + S(18), S(300), S(30)));
    }
    Txt(g, Fmt(L"版本 %s · %s · 单文件 Win32/GDI+ · 静态链接 · 无需安装", kAppVersion, kAppBuildDate),
        S(10), false, th.dim2, RectF(x + S(24), y + S(58), S(600), S(18)));
    float ty = y + S(86);
    auto line = [&](const std::wstring& s) {
        Txt(g, s, S(11), false, th.dim, RectF(x + S(28), ty, pw - S(56), S(20)));
        ty += S(23);
    };
    line(L"· 年/月/周/日/日程 五视图，双击空白新建，右键编辑，Shift+点击多选批量操作");
    line(L"· 重复日程：每天/每周/每月/每年，可只改一次、此后所有或整个系列");
    line(L"· 标签可选颜色+图标（40 个常用图标），优先级/状态/完成勾选/地点/链接/备注/多条提醒");
    line(L"· 导入/导出 .ics，兼容 Google 日历、Outlook、手机日历");
    line(L"· 桌面挂件：常驻一角、置顶、可拖动；日程提醒 + 喝水提醒");
    line(L"· 8 套暗色主题；学期周/单双周；农历/节气/节日；喝水打卡");
    line(L"· 字体与字号可按区域自定义；关闭窗口隐藏到托盘后台运行；可开机自启动");
    line(L"");
    Txt(g, L"快捷键", S(12), true, th.accent, RectF(x + S(28), ty, S(200), S(20)));
    ty += S(24);
    line(L"Ctrl+N 新建 · Ctrl+F 搜索 · / 斜杠指令 · Ctrl+G 跳日期 · Ctrl+Z 撤销 · Ctrl+Y 重做 · Ctrl+S 保存");
    line(L"T 今天 · M 月 · W 周 · D 日 · Y 年 · A 日程 · ← → 前后切换 · F1 本页");
    line(L"关闭窗口 = 隐藏到托盘继续提醒（设置可改为直接退出）· 托盘右键 = 退出");
    line(L"数据位置：程序目录\\data\\calendar.json（可整体备份）");
    line(Fmt(L"数据校验：%s · 版本快照保留最近 10 个", app.dataHealth.c_str()));
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(110) - S(10) - S(90) - S(10), y + ph - S(54), S(90), S(34)), CMD_ABOUT_VERSIONS, L"版本历史", 1, -1, []() { OpenVersionsOverlay(); });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(110) - S(10), y + ph - S(54), S(110), S(34)), CMD_ABOUT_RESTART, L"重启星历", 1, -1, []() { RestartApp(); });
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(54), S(90), S(34)), CMD_ABOUT_CLOSE, L"知道了", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}

// ---------------- 分发 ----------------
void DrawOverlays(Graphics& g, float W, float H) {
    gfx::SetFontPart(app.overlay == OV_EDITOR ? gfx::FP_EDITOR : gfx::FP_DIALOG);
    switch (app.overlay) {
    case OV_MENU: DrawMenuOverlay(g, W, H); break;
    case OV_EDITOR: DrawEditorOverlay(g, W, H); break;
    case OV_QUICKADD: DrawQuickAddOverlay(g, W, H); break;
    case OV_BULKADD: DrawBulkAddOverlay(g, W, H); break;
    case OV_SETTINGS: DrawSettingsOverlay(g, W, H); break;
    case OV_HOTKEYS: DrawHotkeysOverlay(g, W, H); break;
    case OV_GRADES: DrawGradesOverlay(g, W, H); break;
    case OV_HEALTH: DrawHealthOverlay(g, W, H); break;
    case OV_IMPORT: DrawSmartImportOverlay(g, W, H); break;
    case OV_CONFIRM: DrawConfirmOverlay(g, W, H); break;
    case OV_TAGEDIT: DrawTagEditOverlay(g, W, H); break;
    case OV_FONTS: DrawFontOverlay(g, W, H); break;
    case OV_WIDGET: DrawWidgetSettingsOverlay(g, W, H); break;
    case OV_CLEANUP: DrawCleanupOverlay(g, W, H); break;
    case OV_COLORPICK: DrawColorPickOverlay(g, W, H); break;
    case OV_SERIESCOLOR: DrawSeriesColorsOverlay(g, W, H); break;
    case OV_RESCHEDULE: DrawRescheduleOverlay(g, W, H); break;
    case OV_DATEJUMP: DrawDateJumpOverlay(g, W, H); break;
    case OV_DAYDETAIL: DrawDayDetailOverlay(g, W, H); break;
    case OV_STATS: DrawStatsOverlay(g, W, H); break;
    case OV_VERSIONS: DrawVersionsOverlay(g, W, H); break;
    case OV_BACKUP: DrawBackupOverlay(g, W, H); break;
    case OV_REMINDERS: DrawReminderCenterOverlay(g, W, H); break;
    case OV_THEMESTUDIO: DrawThemeStudioOverlay(g, W, H); break;
    case OV_CMDK: DrawCommandPaletteOverlay(g, W, H); break;
    case OV_OCR: DrawOcrResultOverlay(g, W, H); break;
    case OV_SUBS: DrawSubscriptionsOverlay(g, W, H); break;
    case OV_DB: DrawDbManagerOverlay(g, W, H); break;
    case OV_SYNC: DrawSyncCenterOverlay(g, W, H); break;
    case OV_ACCOUNT: DrawAccountSyncOverlay(g, W, H); break;
    case OV_BOT: DrawBotOverlay(g, W, H); break;
    case OV_EXAMS: DrawExamPanelOverlay(g, W, H); break;
    case OV_FOCUS: DrawFocusPanelOverlay(g, W, H); break;
    case OV_HABITS: DrawHabitsPanelOverlay(g, W, H); break;
    case OV_PRINT: DrawPrintPanelOverlay(g, W, H); break;
    case OV_PROFILES: DrawProfilesPanelOverlay(g, W, H); break;
    case OV_ABOUT: DrawAboutOverlay(g, W, H); break;
    case OV_HELP: DrawHelpOverlay(g, W, H); break;
    case OV_SLASH: DrawSlashOverlay(g, W, H); break;
    case OV_SHARE: DrawShareOverlay(g, W, H); break;
    default: break;
    }
}

void CloseOverlay() {
    app.overlay = OV_NONE;
    app.menu.open = false;
    app.confirm.open = false;
    app.quick.open = false;
    app.bulk.open = false;
    app.import.open = false;
    app.tagEdit.open = false;
    app.settingsOpen = false;
    UIInvalidate();
    if (g_hwnd) SetFocus(g_hwnd);
}

bool HandleOverlayKey(WPARAM key) {
    if (app.overlay == OV_NONE) return false;
    if (key == VK_ESCAPE) {
        if (app.overlay == OV_MENU) {
            app.overlay = app.menu.returnOverlay;
            app.menu.open = false;
            UIInvalidate();
            return true;
        }
        if (app.overlay == OV_FONTS && app.fontPickerOpen) {
            app.fontPickerOpen = false;
            UIInvalidate();
            return true;
        }
        if (app.overlay == OV_EDITOR) app.editor.open = false;
        CloseOverlay();
        return true;
    }
    if (key == VK_RETURN && app.overlay == OV_QUICKADD) {
        QuickResult r = ParseQuick(app.quick.text, app.lastClickDay.y ? app.lastClickDay : Today(), &app.store);
        if (r.ok) {
            app.store.Snapshot();
            app.store.AddEvent(r.ev);
            CloseOverlay();
            InvalidateOccs();
            DoSaveStore();
            ShowToast(L"已创建");
        } else {
            ShowToast(L"没看懂这句话");
        }
        return true;
    }
    if (key == VK_RETURN && app.overlay == OV_EDITOR) {
        // 交给编辑器自己的保存逻辑（通过模拟点击保存按钮的命中）
        UIInvalidate();
        // 简单方式：调用保存函数（在 editor.cpp 中实现，这里通过保存按钮回调不可达，改为直接触发）
        extern void EditorSave();
        EditorSave();
        return true;
    }
    return false;
}

void OpenSettings() {
    app.settingsOpen = true;
    app.overlay = OV_SETTINGS;
    UIInvalidate();
}