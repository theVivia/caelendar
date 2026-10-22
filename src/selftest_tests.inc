// selftest_tests.inc - ICS / 快速添加 详细测试（被 selftest.cpp include）
static void TestIcs() {
    Log(L"## ICS 导入导出");
    std::string ics =
        u8"BEGIN:VCALENDAR\r\n"
        u8"VERSION:2.0\r\n"
        u8"PRODID:-//Test//Test 1.0//EN\r\n"
        u8"X-WR-CALNAME:测试日历\r\n"
        u8"BEGIN:VTIMEZONE\r\n"
        u8"TZID:Asia/Shanghai\r\n"
        u8"BEGIN:STANDARD\r\n"
        u8"DTSTART:19700101T000000\r\n"
        u8"TZOFFSETFROM:+0800\r\n"
        u8"TZOFFSETTO:+0800\r\n"
        u8"END:STANDARD\r\n"
        u8"END:VTIMEZONE\r\n"
        u8"BEGIN:VEVENT\r\n"
        u8"UID:evt-1@test\r\n"
        u8"DTSTAMP:20261001T000000Z\r\n"
        u8"SUMMARY:期中复习\r\n"
        u8"DTSTART;TZID=Asia/Shanghai:20261020T190000\r\n"
        u8"DTEND;TZID=Asia/Shanghai:20261020T203000\r\n"
        u8"CATEGORIES:课程,复习\r\n"
        u8"PRIORITY:1\r\n"
        u8"BEGIN:VALARM\r\n"
        u8"ACTION:DISPLAY\r\n"
        u8"TRIGGER:-PT15M\r\n"
        u8"END:VALARM\r\n"
        u8"END:VEVENT\r\n"
        u8"BEGIN:VEVENT\r\n"
        u8"UID:evt-2@test\r\n"
        u8"SUMMARY:UTC 会议\r\n"
        u8"DTSTART:20261021T010000Z\r\n"
        u8"DTEND:20261021T020000Z\r\n"
        u8"END:VEVENT\r\n"
        u8"BEGIN:VEVENT\r\n"
        u8"UID:evt-3@test\r\n"
        u8"SUMMARY:生日\r\n"
        u8"DTSTART;VALUE=DATE:20261025\r\n"
        u8"DTEND;VALUE=DATE:20261026\r\n"
        u8"END:VEVENT\r\n"
        u8"BEGIN:VEVENT\r\n"
        u8"UID:evt-4@test\r\n"
        u8"SUMMARY:周会\r\n"
        u8"DTSTART;TZID=Asia/Shanghai:20261005T100000\r\n"
        u8"DTEND;TZID=Asia/Shanghai:20261005T110000\r\n"
        u8"RRULE:FREQ=WEEKLY;BYDAY=MO;UNTIL=20261228T020000Z;INTERVAL=1\r\n"
        u8"EXDATE;TZID=Asia/Shanghai:20261019T100000\r\n"
        u8"END:VEVENT\r\n"
        u8"BEGIN:VEVENT\r\n"
        u8"UID:evt-4@test\r\n"
        u8"RECURRENCE-ID;TZID=Asia/Shanghai:20261012T100000\r\n"
        u8"DTSTART;TZID=Asia/Shanghai:20261012T150000\r\n"
        u8"DTEND;TZID=Asia/Shanghai:20261012T160000\r\n"
        u8"SUMMARY:周会（改期）\r\n"
        u8"END:VEVENT\r\n"
        u8"BEGIN:VEVENT\r\n"
        u8"UID:evt-5@test\r\n"
        u8"DTSTART;VALUE=DATE:20261101\r\n"
        u8"SUMMARY:折行测试\r\n"
        u8" 继续内容\r\n"
        u8"DESCRIPTION:会议\\, 重要\\;带分号\r\n"
        u8"END:VEVENT\r\n"
        u8"END:VCALENDAR\r\n";
    IcsParseResult pr = ParseIcs(ics);
    Check((int)pr.events.size() == 5, L"解析出 5 个系列", Fmt(L"实际 %d", (int)pr.events.size()));
    Check(pr.calendarName == L"测试日历", L"读取 X-WR-CALNAME", pr.calendarName);
    auto findEv = [&](const wchar_t* title) -> const Event* {
        for (auto& e : pr.events) if (e.title == title) return &e;
        return nullptr;
    };
    const Event* ev1 = findEv(L"期中复习");
    Check(ev1 != nullptr, L"找到 期中复习");
    if (ev1) {
        Check(ev1->start.y == 2026 && ev1->start.mo == 10 && ev1->start.d == 20 && ev1->start.h == 19 && ev1->start.mi == 0,
              L"TZID 本地时间 19:00", ISODT(ev1->start));
        Check(ev1->end.h == 20 && ev1->end.mi == 30, L"结束时间 20:30", ISODT(ev1->end));
        Check(ev1->priority == PRIO_HIGH, L"PRIORITY:1 → 高优先级");
        Check(ev1->alarms.size() == 1 && ev1->alarms[0].offsetMin == -15, L"VALARM -PT15M → 提前 15 分钟");
        Check(ev1->tags.size() == 2, L"CATEGORIES → 2 个标签", Fmt(L"%d", (int)ev1->tags.size()));
        Check(pr.tagNames.size() >= 2, L"标签名映射齐全");
    }
    const Event* ev2 = findEv(L"UTC 会议");
    Check(ev2 != nullptr, L"找到 UTC 会议");
    if (ev2) Check(ev2->start.h == 9, L"UTC 01:00 → 本地 09:00（本机 UTC+8）", ISODT(ev2->start));
    const Event* ev3 = findEv(L"生日");
    Check(ev3 && ev3->allDay && ev3->start.d == 25 && ev3->end.d == 26, L"全天事件与排他结束日期");
    const Event* ev4 = findEv(L"周会");
    Check(ev4 != nullptr, L"找到 周会");
    if (ev4) {
        Check(ev4->recur.freq == RF_WEEKLY && ev4->recur.byDay.size() == 1 && ev4->recur.byDay[0] == 1, L"RRULE 每周一");
        Check(ev4->recur.hasUntil, L"UNTIL 已解析");
        Check(ev4->recur.exDates.size() == 1, L"EXDATE 已解析", Fmt(L"%d", (int)ev4->recur.exDates.size()));
        Check(ev4->overrides.size() == 1, L"RECURRENCE-ID 覆盖实例已挂载", Fmt(L"%d", (int)ev4->overrides.size()));
        if (!ev4->overrides.empty())
            Check(ev4->overrides[0].title == L"周会（改期）" && ev4->overrides[0].start.h == 15, L"覆盖实例改为 15:00");
    }
    const Event* ev5 = findEv(L"折行测试继续内容");
    Check(ev5 != nullptr, L"折行 SUMMARY 正确拼接");
    if (ev5) Check(ev5->desc == L"会议, 重要;带分号", L"转义字符 \\, \\; 还原", ev5->desc);
    IcsParseResult bad = ParseIcs(u8"BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:无开始时间\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
    Check(bad.events.empty() && !bad.warnings.empty(), L"缺少 DTSTART 的事件被拒绝并给出警告（负控）");
    IcsParseResult empty = ParseIcs("");
    Check(empty.events.empty(), L"空文件不崩溃（负控）");
    Store st;
    st.tags = DefaultTags();
    IcsImportResult ir = ImportIcs(st, ics, 0, L"测试导入");
    Check(ir.added == 5, L"首次导入新增 5 条", Fmt(L"added=%d", ir.added));
    IcsImportResult ir2 = ImportIcs(st, ics, 0, L"测试导入");
    Check(ir2.added == 0 && ir2.updated == 5, L"再次导入按 UID 合并（0 新增 / 5 更新）",
          Fmt(L"added=%d updated=%d", ir2.added, ir2.updated));
    Check((int)st.events.size() == 5, L"合并后事件数仍为 5");
    IcsImportResult ir3 = ImportIcs(st, ics, 2, L"测试导入");
    Check(ir3.skipped == 5 && ir3.added == 0, L"跳过重复模式全部跳过", Fmt(L"skipped=%d", ir3.skipped));
    IcsImportResult ir4 = ImportIcs(st, ics, 1, L"测试导入");
    Check(ir4.added == 5 && (int)st.events.size() == 10, L"全部追加模式新增 5 条");
    std::string exported = ExportIcs(st);
    WriteFileBytesAtomic(PathJoin(GetDataDir(), L"export-test.ics"), exported);
    Check(Contains(Utf8ToWide(exported), L"BEGIN:VCALENDAR") && Contains(Utf8ToWide(exported), L"BEGIN:VEVENT"),
          L"导出包含 VCALENDAR/VEVENT");
    IcsParseResult pr2 = ParseIcs(exported);
    for (auto& e : pr2.events) Log(L"  [往返事件] " + e.title + L"  start=" + ISODT(e.start) + L" uid=" + e.uid);
    for (auto& w : pr2.warnings) Log(L"  [往返警告] " + w);
    Check((int)pr2.events.size() == (int)st.events.size(), L"导出后再解析事件数一致",
          Fmt(L"导出 %d / 解析 %d", (int)st.events.size(), (int)pr2.events.size()));
    auto findTitle = [&](const wchar_t* title) {
        for (auto& e : pr2.events) if (e.title == title) return true;
        return false;
    };
    Check(findTitle(L"期中复习") && findTitle(L"生日") && findTitle(L"周会"), L"往返后关键事件仍在");
    bool overrideKept = false, exdateKept = false, recurKept = false, tagsKept = false;
    for (auto& e : pr2.events) {
        if (e.title == L"周会") {
            if (!e.overrides.empty()) overrideKept = true;
            if (!e.recur.exDates.empty()) exdateKept = true;
            if (e.recur.freq == RF_WEEKLY) recurKept = true;
            if (e.tags.size() >= 1) tagsKept = true;
        }
    }
    Check(overrideKept, L"往返后覆盖实例保留");
    Check(exdateKept, L"往返后 EXDATE 保留");
    Check(recurKept, L"往返后 RRULE 保留");
    bool tagRoundTrip = false;
    for (auto& e : pr2.events) if (e.title == L"期中复习" && e.tags.size() >= 2) tagRoundTrip = true;
    Check(tagRoundTrip, L"往返后标签保留（期中复习 2 个标签）");
}

static void TestQuick() {
    Log(L"## 快速添加解析");
    Store st;
    st.tags = DefaultTags();
    DateTime base = MakeDT(2026, 10, 17, 0, 0);
    QuickResult r = ParseQuick(L"明天 15:00-16:00 高数作业 #作业 !!", base, &st);
    Check(r.ok, L"解析「明天 15:00-16:00 高数作业 #作业 !!」", r.error);
    if (r.ok) {
        Check(r.ev.title == L"高数作业", L"标题提取", r.ev.title);
        Check(r.ev.start.d == 18 && r.ev.start.h == 15 && r.ev.start.mi == 0, L"明天 15:00", ISODT(r.ev.start));
        Check(r.ev.end.h == 16 && r.ev.end.mi == 0, L"结束 16:00", ISODT(r.ev.end));
        Check(r.ev.tags.size() == 1 && r.ev.tags[0] == "homework", L"#作业 → 内置标签 id");
        Check(r.ev.priority == PRIO_HIGH, L"!! → 高优先级");
    }
    QuickResult r2 = ParseQuick(L"周三 8点 早八 #课程", base, &st);
    Check(r2.ok && r2.ev.start.h == 8 && r2.ev.title == L"早八" &&
          DayOfWeek(r2.ev.start.y, r2.ev.start.mo, r2.ev.start.d) == 3,
          L"周三 8点 → 星期三 08:00", r2.ok ? ISODT(r2.ev.start) : r2.error);
    QuickResult r3 = ParseQuick(L"每天 22:30 复盘", base, &st);
    Check(r3.ok && r3.ev.recur.freq == RF_DAILY && r3.ev.start.h == 22 && r3.ev.start.mi == 30,
          L"每天 22:30 → 每日重复", r3.ok ? ISODT(r3.ev.start) : r3.error);
    QuickResult r4 = ParseQuick(L"2026-12-31 23:59 跨年", base, &st);
    Check(r4.ok && r4.ev.start.y == 2026 && r4.ev.start.mo == 12 && r4.ev.start.d == 31 && r4.ev.start.h == 23,
          L"明确日期 2026-12-31 23:59", r4.ok ? ISODT(r4.ev.start) : r4.error);
    Check(r4.ok && r4.ev.end.d == 1 && r4.ev.end.mo == 1 && r4.ev.end.y == 2027, L"跨天结束时间自动顺延");
    QuickResult r5 = ParseQuick(L"周五下午3点半 开会", base, &st);
    Check(r5.ok && r5.ev.start.h == 15 && r5.ev.start.mi == 30 && r5.ev.title == L"开会",
          L"周五下午3点半 → 15:30", r5.ok ? ISODT(r5.ev.start) : r5.error);
    QuickResult r6 = ParseQuick(L"明天 全天 复习", base, &st);
    Check(r6.ok && r6.ev.allDay && r6.ev.title == L"复习", L"全天事件", r6.ok ? L"allDay" : r6.error);
    QuickResult bad = ParseQuick(L"", base, &st);
    Check(!bad.ok, L"空输入被拒绝（负控）");
    QuickResult bad2 = ParseQuick(L"!!!", base, &st);
    Check(!bad2.ok, L"只有符号没有标题被拒绝（负控）");
    QuickResult tagOnly = ParseQuick(L"#学习 #作业", base, &st);
    Check(!tagOnly.ok, L"只有标签没有标题被拒绝（负控）");
    BulkParseResult br = ParseBulk(L"明天 9:00 交作业 #作业\n2026-11-01 10:00 组会\n!!!", base, &st);
    Check((int)br.events.size() == 2 && (int)br.errors.size() == 1,
          L"批量解析：2 条有效 + 1 条错误", Fmt(L"events=%d errors=%d", (int)br.events.size(), (int)br.errors.size()));
}
