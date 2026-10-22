// grades.cpp - 成绩与 GPA（4.0 制，学分加权）
#include "app.h"
#include "json.h"
#include <algorithm>
#include <cstdlib>

using namespace Gdiplus;
using namespace gfx;

namespace {

struct GradeRec {
    std::wstring course;
    double credit = 0;
    double score = 0;
};

std::vector<GradeRec> g_grades;
bool g_gradesLoaded = false;

std::wstring GradesPath() { return PathJoin(GetDataDir(), L"grades.json"); }

void SaveGrades() {
    JValue arr = JValue::Arr();
    for (auto& g : g_grades) {
        JValue o = JValue::Obj();
        o.Set(L"course") = JValue(g.course);
        o.Set(L"credit") = JValue(g.credit);
        o.Set(L"score") = JValue(g.score);
        arr.Push(o);
    }
    WriteFileBytesAtomic(GradesPath(), JsonDump(arr, true));
}

double ScoreToGpa(double score) {
    if (score >= 90) return 4.0;
    if (score >= 85) return 3.7;
    if (score >= 82) return 3.3;
    if (score >= 78) return 3.0;
    if (score >= 75) return 2.7;
    if (score >= 72) return 2.3;
    if (score >= 68) return 2.0;
    if (score >= 64) return 1.5;
    if (score >= 60) return 1.0;
    return 0.0;
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

}  // namespace

void LoadGrades() {
    if (g_gradesLoaded) return;
    g_gradesLoaded = true;
    bool ok = false;
    std::string raw = ReadFileBytes(GradesPath(), &ok);
    if (!ok) return;
    std::wstring err;
    JValue root = JsonParse(raw, &err);
    if (!root.IsArr()) return;
    for (auto& item : root.arr) {
        if (!item.IsObj()) continue;
        GradeRec g;
        g.course = item.Get(L"course").AsStr();
        g.credit = item.Get(L"credit").AsNum(0);
        g.score = item.Get(L"score").AsNum(0);
        if (!g.course.empty() && g.credit > 0) g_grades.push_back(g);
    }
}

double GradesGpa() {
    double credits = 0, points = 0;
    for (auto& g : g_grades) {
        credits += g.credit;
        points += g.credit * ScoreToGpa(g.score);
    }
    return credits > 0 ? points / credits : 0.0;
}

double GradesCredits() {
    double c = 0;
    for (auto& g : g_grades) c += g.credit;
    return c;
}

int GradesCount() { return (int)g_grades.size(); }

void OpenGrades() {
    LoadGrades();
    app.gradeCourse.clear();
    app.gradeCredit.clear();
    app.gradeScore.clear();
    app.overlay = OV_GRADES;
    UIInvalidate();
}

void DrawGradesOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(720), ph = S(560);
    pw = std::min(pw, W - S(32));
    ph = std::min(ph, H - S(24));
    float x = (W - pw) / 2, y = (H - ph) / 2;
    UI::Dim(g, W, H, 170);
    Shadow(g, RectF(x, y, pw, ph), S(18), 8, 26);
    FillRoundRect(g, RectF(x, y, pw, ph), S(18), th.card);
    StrokeRoundRect(g, RectF(x, y, pw, ph), S(18), th.border, S(1));
    Txt(g, L"成绩 / GPA", S(17), true, th.text, RectF(x + S(22), y + S(16), S(240), S(26)));
    Txt(g, Fmt(L"GPA %.2f · 总学分 %.1f · %d 门课", GradesGpa(), GradesCredits(), GradesCount()),
        S(10), true, th.accent, RectF(x + S(22), y + S(46), pw - S(120), S(18)));
    Txt(g, L"4.0 制：90+=4.0 · 85=3.7 · 82=3.3 · 78=3.0 · 75=2.7 · 72=2.3 · 68=2.0 · 64=1.5 · 60=1.0", S(8.5f),
        false, th.dim2, RectF(x + S(22), y + S(66), pw - S(44), S(14)), ALIGN_LEFT, false, true);
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_GRADES_CLOSE, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    // 列表
    float listY = y + S(92);
    int shown = std::min((int)g_grades.size(), 8);
    if (g_grades.empty()) {
        Txt(g, L"还没有成绩记录，在下面输入课程、学分和成绩后点「添加」。", S(10), false, th.dim2,
            RectF(x + S(22), listY + S(10), pw - S(44), S(20)));
    }
    for (int i = 0; i < shown; ++i) {
        auto& rec = g_grades[i];
        RectF row(x + S(22), listY + i * S(34), pw - S(44), S(30));
        FillRoundRect(g, row, S(7), WithAlpha(th.panel2, 160));
        Txt(g, rec.course, S(10), false, th.text, RectF(row.X + S(10), row.Y, row.Width - S(300), row.Height), ALIGN_VCENTER, false, true);
        Txt(g, Fmt(L"学分 %.1f", rec.credit), S(9), false, th.dim, RectF(RX(row) - S(280), row.Y, S(80), row.Height), ALIGN_RIGHT | ALIGN_VCENTER);
        Txt(g, Fmt(L"成绩 %.0f", rec.score), S(9), false, th.dim, RectF(RX(row) - S(196), row.Y, S(80), row.Height), ALIGN_RIGHT | ALIGN_VCENTER);
        Txt(g, Fmt(L"绩点 %.2f", ScoreToGpa(rec.score)), S(9), true, th.accent, RectF(RX(row) - S(112), row.Y, S(80), row.Height), ALIGN_RIGHT | ALIGN_VCENTER);
        AddHit(RX(row) - S(28), row.Y + S(4), S(22), S(22), CMD_GRADES_DEL_BASE + i, [i]() {
            if (i >= 0 && i < (int)g_grades.size()) {
                g_grades.erase(g_grades.begin() + i);
                SaveGrades();
                ShowToast(L"已删除成绩");
                UIInvalidate();
            }
        });
        Txt(g, L"×", S(12), false, WithAlpha(th.danger, 210), RectF(RX(row) - S(28), row.Y, S(22), row.Height), ALIGN_CENTER | ALIGN_VCENTER);
    }

    // 输入行
    float inputY = y + ph - S(96);
    Txt(g, L"添加成绩", S(10), true, th.accent, RectF(x + S(22), inputY - S(18), S(160), S(16)));
    RectF bCourse(x + S(22), inputY, S(220), S(34));
    RectF bCredit(x + S(250), inputY, S(90), S(34));
    RectF bScore(x + S(348), inputY, S(90), S(34));
    auto editBox = [&](int id, RectF box, std::wstring* buf, const wchar_t* hint) {
        FillRoundRect(g, box, S(8), th.panel2);
        StrokeRoundRect(g, box, S(8), th.borderSoft, S(1));
        UIEdit(id, RectF(box.X + S(10), box.Y + S(8), box.Width - S(20), S(20)), buf, false, S(11));
        if (buf->empty()) Txt(g, hint, S(9), false, WithAlpha(th.dim2, 160), RectF(box.X + S(10), box.Y, box.Width - S(20), box.Height), ALIGN_VCENTER);
    };
    editBox(9802, bCourse, &app.gradeCourse, L"课程名");
    editBox(9803, bCredit, &app.gradeCredit, L"学分");
    editBox(9804, bScore, &app.gradeScore, L"成绩");
    UI::Btn(g, RectF(x + S(448), inputY, S(104), S(34)), CMD_GRADES_ADD, L"添加", 0, ICO_CHECK, []() {
        LoadGrades();
        std::wstring course = app.gradeCourse;
        while (!course.empty() && (course.front() == L' ' || course.front() == L'　')) course.erase(course.begin());
        while (!course.empty() && (course.back() == L' ' || course.back() == L'　')) course.pop_back();
        if (course.empty()) { ShowToast(L"先填课程名"); return; }
        double credit = std::wcstod(app.gradeCredit.c_str(), nullptr);
        double score = std::wcstod(app.gradeScore.c_str(), nullptr);
        if (credit <= 0 || credit > 30) { ShowToast(L"学分要填 0–30 之间的数字"); return; }
        if (score < 0 || score > 100) { ShowToast(L"成绩要填 0–100 之间"); return; }
        GradeRec r;
        r.course = course;
        r.credit = credit;
        r.score = score;
        g_grades.push_back(r);
        SaveGrades();
        app.gradeCourse.clear();
        app.gradeCredit.clear();
        app.gradeScore.clear();
        ShowToast(L"已添加成绩");
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + pw - S(130), inputY, S(104), S(34)), CMD_GRADES_CLOSE, L"关闭", 1, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}

std::string GradesToJsonArray() {
    JValue arr = JValue::Arr();
    for (auto& g : g_grades) {
        JValue o = JValue::Obj();
        o.Set(L"course") = JValue(g.course);
        o.Set(L"credit") = JValue(g.credit);
        o.Set(L"score") = JValue(g.score);
        arr.Push(o);
    }
    return JsonDump(arr);
}

std::string GradesAttachToStoreJson(const std::string& storeJson) {
    std::wstring err;
    JValue root = JsonParse(storeJson, &err);
    if (!root.IsObj()) return storeJson;
    root.Set(L"grades") = JsonParse(GradesToJsonArray(), &err);
    return JsonDump(root);
}

void GradesApplyFromStoreJson(const std::string& storeJson) {
    std::wstring err;
    JValue root = JsonParse(storeJson, &err);
    if (!root.IsObj()) return;
    const JValue& grades = root.Get(L"grades");
    if (!grades.IsArr()) return;
    g_grades.clear();
    for (auto& item : grades.arr) {
        if (!item.IsObj()) continue;
        GradeRec g;
        g.course = item.Get(L"course").AsStr();
        g.credit = item.Get(L"credit").AsNum(0);
        g.score = item.Get(L"score").AsNum(0);
        if (!g.course.empty() && g.credit > 0) g_grades.push_back(g);
    }
    SaveGrades();
}

bool GradesProbeAdd(const std::wstring& course, double credit, double score) {
    LoadGrades();
    GradeRec r;
    r.course = course;
    r.credit = credit;
    r.score = score;
    g_grades.push_back(r);
    SaveGrades();
    return true;
}
