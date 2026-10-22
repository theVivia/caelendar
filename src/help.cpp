// help.cpp - F1 功能地图 + 快捷键（新功能统一入口）
#include "app.h"
#include <functional>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

struct HelpItem {
    const wchar_t* key;
    const wchar_t* name;
    const wchar_t* desc;
    std::function<void()> fn;
};

std::vector<HelpItem> BuildHelpItems() {
    return {
        { L"Ctrl+K", L"命令面板", L"搜索命令/日程；/ 打开完整斜杠指令", []() { OpenCommandPalette(); } },
        { L"F2", L"学习统计", L"周 / 月 / 年复盘，DDL 完成率", []() { OpenStats(1, app.cursor); } },
        { L"F3", L"DDL 看板", L"作业按课程 / 截止分组，勾完成", []() {
            app.overlay = OV_NONE;
            app.view = VIEW_AGENDA;
            app.agendaBoard = true;
            app.selected.clear();
            InvalidateOccs();
            UIInvalidate();
        } },
        { L"F4", L"考试倒计时", L"最近考试与剩余天数", []() { OpenExamPanel(); } },
        { L"F5", L"番茄钟", L"专注计时，记录到学习统计", []() { OpenFocusPanel(); } },
        { L"F6", L"习惯打卡", L"早起 / 背单词 / 运动每周目标", []() { OpenHabitsPanel(); } },
        { L"F7", L"打印周计划", L"系统打印 / 导出 PDF 或 HTML", []() { OpenPrintPanel(); } },
        { L"F8", L"课表档案", L"多学期 / 本科 + 辅修课表切换", []() { OpenProfilesPanel(); } },
        { L"F9", L"提醒中心", L"错过提醒集中处理 / 专注模式", []() { OpenReminderCenter(); } },
        { L"Ctrl+Shift+T", L"主题工坊", L"自定义配色 / 圆角 / JSON 导入导出", []() { OpenThemeStudio(); } },
        { L"Ctrl+Shift+S", L"SQLite 存储", L"迁移 / 回退 / 数据量", []() { OpenDbManager(); } },
        { L"Ctrl+Shift+M", L"换机迁移", L"设备 ID / 日志 / 加密迁移包", []() { OpenSyncCenter(); } },
        { L"Ctrl+I", L"智能导入", L"教务 HTML / CSV / 剪贴板 / ICS / OCR", []() { OpenSmartImportFromFile(); } },
        { L"", L"ICS 订阅", L"只读订阅，不覆盖手动修改", []() { OpenSubscriptions(); } },
        { L"", L"设置", L"主题 / 学期 / 挂件 / 提醒 / 数据", []() { OpenSettings(); } },
        { L"", L"忙闲分享", L"外网 30 天忙闲页 / 预约留言 / Funnel", []() { OpenSharePanel(); } },
        { L"", L"斜杠指令", L"输入 / 打开完整指令面板（47 条）", []() { OpenSlashPalette(); } },
        { L"", L"命令面板", L"Ctrl+K 浏览全部功能（可滚动）", []() { OpenCommandPalette(); } },
        { L"", L"年视图", L"全年总览 / 每月日程量与考试标记", []() {
            app.overlay = OV_NONE;
            app.view = VIEW_YEAR;
            InvalidateOccs();
            UIInvalidate();
        } },
        { L"F1", L"功能地图", L"本页：所有新功能入口和快捷键", []() {} },
    };
}

}  // namespace

int HelpEntryCount() { return (int)BuildHelpItems().size(); }

void OpenHelp() {
    app.overlay = OV_HELP;
    UIInvalidate();
}

void DrawHelpOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(920), ph = S(660);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(16), 8, 26);
    FillRoundRect(g, panel, S(16), th.card);
    StrokeRoundRect(g, panel, S(16), th.border, S(1));
    Icon(g, ICO_STAR, RectF(x + S(24), y + S(26), S(20), S(20)), th.accent, 1.5f);
    {
        float lh = S(34), lw = lh * (739.0f / 326.0f);
        Bitmap* brand = BrandLogo();
        if (brand) g.DrawImage(brand, RectF(x + S(48), y + S(18), lw, lh));
        else Text(g, L"CAELENDAR", S(20), true, th.text, RectF(x + S(54), y + S(18), S(300), S(30)));
    }
    Text(g, Fmt(L"版本 %s · %s · 功能地图 / 快捷键（F1 随时打开）", kAppVersion, kAppBuildDate),
         S(10), false, th.dim2, RectF(x + S(24), y + S(60), pw - S(48), S(18)));

    auto items = BuildHelpItems();
    float hy = y + S(92);
    Text(g, L"功能地图（点击任意一项打开）", S(12), true, th.accent, RectF(x + S(24), hy, S(400), S(20)));
    hy += S(28);
    int perCol = 10;
    float colW = (pw - S(48) - S(16)) / 2.0f;
    float rowH = S(34);
    for (int i = 0; i < (int)items.size() && i < perCol * 2; ++i) {
        int col = i / perCol, row = i % perCol;
        RectF r(x + S(24) + col * (colW + S(16)), hy + row * rowH, colW, rowH - S(4));
        bool hov = IsHovered(CMD_HELP_ITEM_BASE + i);
        FillRoundRect(g, r, S(8), WithAlpha(hov ? th.accent : th.panel2, hov ? 26 : 150));
        StrokeRoundRect(g, r, S(8), hov ? WithAlpha(th.accent, 140) : th.borderSoft, S(1));
        if (items[i].key[0]) {
            Text(g, items[i].key, S(8.5f), true, th.accent2, RectF(r.X + S(10), r.Y + S(4), S(74), S(14)));
        }
        Text(g, items[i].name, S(11), true, th.text, RectF(r.X + S(86), r.Y + S(3), r.Width - S(96), S(18)), ALIGN_LEFT, true);
        Text(g, items[i].desc, S(8.5f), false, th.dim2, RectF(r.X + S(86), r.Y + S(19), r.Width - S(96), S(14)), ALIGN_LEFT, true);
        AddHit(r.X, r.Y, r.Width, r.Height, CMD_HELP_ITEM_BASE + i, [items, i]() { items[i].fn(); });
    }

    float sy = hy + perCol * rowH + S(4);
    Text(g, L"快捷键", S(12), true, th.accent, RectF(x + S(24), sy, S(200), S(20)));
    sy += S(24);
    auto line = [&](const std::wstring& t) {
        Text(g, t, S(9.5f), false, th.dim, RectF(x + S(28), sy, pw - S(56), S(18)), ALIGN_LEFT, true);
        sy += S(20);
    };
    line(L"Ctrl+N 新建 · Ctrl+F 搜索 · / 斜杠指令 · Ctrl+G 跳日期 · Ctrl+Z 撤销 · Ctrl+Y 重做 · Ctrl+S 保存 · Ctrl+K 命令面板（含忙闲分享）");
    line(L"T 今天 · M 月 · W 周 · D 日 · Y 年 · A 日程 · ← → 前后切换 · Delete 删除所选 · Esc 关闭");
    line(L"F1 功能地图 · F2 统计 · F3 看板 · F4 考试 · F5 番茄 · F6 习惯 · F7 打印 · F8 档案 · F9 提醒");
    line(L"右键日期 / 周·日空白 = 当天详情 · 双击空白新建 · Shift+点击多选 · 关闭窗口默认隐藏到托盘");
    sy += S(6);
    Text(g, L"数据位置：程序目录\\data\\calendar.json（可整体备份）", S(9), false, th.dim2,
         RectF(x + S(28), sy, pw - S(56), S(16)), ALIGN_LEFT, true);
    sy += S(20);
    Text(g, Fmt(L"数据校验：%s · 版本快照保留最近 10 个", app.dataHealth.c_str()), S(9), false, th.dim2,
         RectF(x + S(28), sy, pw - S(56), S(16)), ALIGN_LEFT, true);

    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(110) - S(10) - S(90) - S(10), y + ph - S(54), S(90), S(34)),
            CMD_ABOUT_VERSIONS, L"版本历史", 1, -1, []() { OpenVersionsOverlay(); });
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(110) - S(10), y + ph - S(54), S(110), S(34)),
            CMD_ABOUT_RESTART, L"重启星历", 1, -1, []() { RestartApp(); });
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(54), S(90), S(34)), CMD_ABOUT_CLOSE, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
