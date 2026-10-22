// db.cpp - SQLite 存储引擎（动态加载 Windows 自带 winsqlite3.dll，不打包第三方库）
#include "app.h"
#include "json.h"
#include <ctime>
#include <string>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

namespace {

struct sqlite3;
struct sqlite3_stmt;

constexpr int SQLITE_OK = 0;
constexpr int SQLITE_ROW = 100;
constexpr int SQLITE_DONE = 101;
constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
constexpr int SQLITE_OPEN_CREATE = 0x00000004;
#define SQLITE_TRANSIENT ((void (*)(void*))-1)

struct DbApi {
    HMODULE mod = nullptr;
    int (*open_v2)(const char*, sqlite3**, int, const char*) = nullptr;
    int (*close_v2)(sqlite3*) = nullptr;
    const char* (*errmsg)(sqlite3*) = nullptr;
    int (*exec)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**) = nullptr;
    int (*prepare_v2)(sqlite3*, const char*, int, sqlite3_stmt**, const char**) = nullptr;
    int (*step)(sqlite3_stmt*) = nullptr;
    int (*finalize)(sqlite3_stmt*) = nullptr;
    int (*reset)(sqlite3_stmt*) = nullptr;
    const unsigned char* (*column_text)(sqlite3_stmt*, int) = nullptr;
    long long (*column_int64)(sqlite3_stmt*, int) = nullptr;
    int (*column_int)(sqlite3_stmt*, int) = nullptr;
    int (*column_count)(sqlite3_stmt*, int) = nullptr;
    int (*bind_text)(sqlite3_stmt*, int, const char*, int, void (*)(void*)) = nullptr;
    int (*bind_int64)(sqlite3_stmt*, int, long long) = nullptr;
    int (*bind_int)(sqlite3_stmt*, int, int) = nullptr;
    int (*bind_null)(sqlite3_stmt*, int) = nullptr;
    int (*busy_timeout)(sqlite3*, int) = nullptr;
    void (*free_fn)(void*) = nullptr;
};

DbApi g_api;
bool g_apiTried = false;
int g_engine = -1;   // -1 未知, 0 JSON, 1 SQLite

bool LoadApi() {
    if (g_apiTried) return g_api.mod != nullptr;
    g_apiTried = true;
    g_api.mod = LoadLibraryW(L"winsqlite3.dll");
    if (!g_api.mod) { DebugLog(L"SQLite: 未找到 winsqlite3.dll"); return false; }
    auto sym = [](const char* n) { return (void*)GetProcAddress(g_api.mod, n); };
    g_api.open_v2 = (decltype(g_api.open_v2))sym("sqlite3_open_v2");
    g_api.close_v2 = (decltype(g_api.close_v2))sym("sqlite3_close_v2");
    g_api.errmsg = (decltype(g_api.errmsg))sym("sqlite3_errmsg");
    g_api.exec = (decltype(g_api.exec))sym("sqlite3_exec");
    g_api.prepare_v2 = (decltype(g_api.prepare_v2))sym("sqlite3_prepare_v2");
    g_api.step = (decltype(g_api.step))sym("sqlite3_step");
    g_api.finalize = (decltype(g_api.finalize))sym("sqlite3_finalize");
    g_api.reset = (decltype(g_api.reset))sym("sqlite3_reset");
    g_api.column_text = (decltype(g_api.column_text))sym("sqlite3_column_text");
    g_api.column_int64 = (decltype(g_api.column_int64))sym("sqlite3_column_int64");
    g_api.column_int = (decltype(g_api.column_int))sym("sqlite3_column_int");
    g_api.column_count = (decltype(g_api.column_count))sym("sqlite3_column_count");
    g_api.bind_text = (decltype(g_api.bind_text))sym("sqlite3_bind_text");
    g_api.bind_int64 = (decltype(g_api.bind_int64))sym("sqlite3_bind_int64");
    g_api.bind_int = (decltype(g_api.bind_int))sym("sqlite3_bind_int");
    g_api.bind_null = (decltype(g_api.bind_null))sym("sqlite3_bind_null");
    g_api.busy_timeout = (decltype(g_api.busy_timeout))sym("sqlite3_busy_timeout");
    g_api.free_fn = (decltype(g_api.free_fn))sym("sqlite3_free");
    bool ok = g_api.open_v2 && g_api.close_v2 && g_api.exec && g_api.prepare_v2 && g_api.step &&
              g_api.finalize && g_api.column_text && g_api.bind_text && g_api.bind_int64;
    if (!ok) DebugLog(L"SQLite: winsqlite3.dll 缺少必要符号");
    return ok;
}

std::wstring DbPath() { return PathJoin(GetDataDir(), L"calendar.db"); }
std::wstring ConfPath() { return PathJoin(GetDataDir(), L"storage.conf"); }

void SetEngineFlag(bool sqlite) {
    g_engine = sqlite ? 1 : 0;
    EnsureDir(GetDataDir());
    WriteFileBytesAtomic(ConfPath(), sqlite ? "sqlite\n" : "json\n");
}

bool Exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = g_api.exec(db, sql, nullptr, nullptr, &err);
    if (err) { DebugLog(L"SQLite exec 失败: %s", Utf8ToWide(err).c_str()); if (g_api.free_fn) g_api.free_fn(err); }
    return rc == SQLITE_OK;
}

bool Prepare(sqlite3* db, const char* sql, sqlite3_stmt** st) {
    return g_api.prepare_v2(db, sql, -1, st, nullptr) == SQLITE_OK;
}

long long EpochFromIso(const std::string& iso) {
    // "2026-10-18T09:00" 或 "2026-10-18"
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    if (sscanf_s(iso.c_str(), "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) < 3) return 0;
    tm t{};
    t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi;
    t.tm_isdst = -1;
    return (long long)mktime(&t);
}

std::string IsoNow() { return WideToUtf8(ISODate(NowDT()) + L"T" + TimeStr(NowDT())); }

int CountEventRows(sqlite3* db) {
    sqlite3_stmt* st = nullptr;
    int n = 0;
    if (Prepare(db, "SELECT COUNT(*) FROM events", &st)) {
        if (g_api.step(st) == SQLITE_ROW) n = g_api.column_int(st, 0);
        g_api.finalize(st);
    }
    return n;
}

}  // namespace

bool DbEngineEnabled() {
    if (g_engine < 0) {
        bool ok = false;
        std::string conf = ReadFileBytes(ConfPath(), &ok);
        g_engine = (ok && conf.find("sqlite") != std::string::npos) ? 1 : 0;
    }
    return g_engine == 1;
}

bool DbAvailable() { return LoadApi(); }

bool DbLoadJson(std::string& out) {
    out.clear();
    if (!LoadApi()) return false;
    sqlite3* db = nullptr;
    if (g_api.open_v2(WideToUtf8(DbPath()).c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        if (db) g_api.close_v2(db);
        return false;
    }
    g_api.busy_timeout(db, 3000);
    sqlite3_stmt* st = nullptr;
    bool ok = false;
    if (Prepare(db, "SELECT value FROM kv WHERE key='store_json'", &st)) {
        if (g_api.step(st) == SQLITE_ROW) {
            const unsigned char* t = g_api.column_text(st, 0);
            if (t) { out = (const char*)t; ok = !out.empty(); }
        }
        g_api.finalize(st);
    }
    g_api.close_v2(db);
    return ok;
}

bool DbSaveJson(const std::string& json) {
    if (!LoadApi()) return false;
    sqlite3* db = nullptr;
    if (g_api.open_v2(WideToUtf8(DbPath()).c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        if (db) g_api.close_v2(db);
        return false;
    }
    g_api.busy_timeout(db, 5000);
    bool ok = Exec(db, "PRAGMA journal_mode=WAL;") && Exec(db, "PRAGMA synchronous=NORMAL;") &&
              Exec(db, "CREATE TABLE IF NOT EXISTS kv(key TEXT PRIMARY KEY, value TEXT NOT NULL);") &&
              Exec(db, "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);") &&
              Exec(db, "CREATE TABLE IF NOT EXISTS events(id TEXT PRIMARY KEY, uid TEXT, title TEXT, start_iso TEXT, "
                        "start_epoch INTEGER, end_epoch INTEGER, done INTEGER, status INTEGER, json TEXT NOT NULL);") &&
              Exec(db, "CREATE INDEX IF NOT EXISTS idx_events_start ON events(start_epoch);") &&
              Exec(db, "CREATE INDEX IF NOT EXISTS idx_events_uid ON events(uid);") &&
              Exec(db, "CREATE INDEX IF NOT EXISTS idx_events_title ON events(title);") &&
              Exec(db, "CREATE INDEX IF NOT EXISTS idx_events_done ON events(done);") &&
              Exec(db, "CREATE TABLE IF NOT EXISTS tags(id TEXT PRIMARY KEY, name TEXT, color TEXT, builtin INTEGER);");
    if (!ok) { g_api.close_v2(db); return false; }

    JValue root = JsonParse(json);
    if (!root.IsObj()) { Exec(db, "ROLLBACK;"); g_api.close_v2(db); return false; }
    if (!Exec(db, "BEGIN IMMEDIATE;")) { g_api.close_v2(db); return false; }
    bool tx = Exec(db, "DELETE FROM events;") && Exec(db, "DELETE FROM tags;");
    // kv 整包（无损加载）
    if (tx) {
        sqlite3_stmt* st = nullptr;
        if (Prepare(db, "INSERT OR REPLACE INTO kv(key,value) VALUES('store_json',?1)", &st)) {
            g_api.bind_text(st, 1, json.c_str(), (int)json.size(), SQLITE_TRANSIENT);
            tx = (g_api.step(st) == SQLITE_DONE);
            g_api.finalize(st);
        } else tx = false;
    }
    // meta
    if (tx) {
        sqlite3_stmt* st = nullptr;
        if (Prepare(db, "INSERT OR REPLACE INTO meta(key,value) VALUES(?1,?2)", &st)) {
            auto setMeta = [&](const char* k, const std::string& v) {
                g_api.reset(st);
                g_api.bind_text(st, 1, k, -1, SQLITE_TRANSIENT);
                g_api.bind_text(st, 2, v.c_str(), (int)v.size(), SQLITE_TRANSIENT);
                return g_api.step(st) == SQLITE_DONE;
            };
            tx = setMeta("schema_version", "1") && setMeta("data_format", std::to_string(kDataFormatVersion)) &&
                 setMeta("app_version", WideToUtf8(kAppVersion)) && setMeta("saved_at", IsoNow());
            g_api.finalize(st);
        } else tx = false;
    }
    // events 索引行（真实表，方便以后按时间/UID/标题查询）
    if (tx) {
        sqlite3_stmt* st = nullptr;
        if (Prepare(db, "INSERT OR REPLACE INTO events(id,uid,title,start_iso,start_epoch,end_epoch,done,status,json) "
                        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)", &st)) {
            for (auto& ev : root.Get(L"events").arr) {
                std::string id = WideToUtf8(ev.Get(L"id").AsStr());
                std::string uid = WideToUtf8(ev.Get(L"uid").AsStr());
                std::string title = WideToUtf8(ev.Get(L"title").AsStr());
                std::string startIso = WideToUtf8(ev.Get(L"start").AsStr());
                std::string endIso = WideToUtf8(ev.Get(L"end").AsStr());
                g_api.reset(st);
                g_api.bind_text(st, 1, id.c_str(), (int)id.size(), SQLITE_TRANSIENT);
                g_api.bind_text(st, 2, uid.c_str(), (int)uid.size(), SQLITE_TRANSIENT);
                g_api.bind_text(st, 3, title.c_str(), (int)title.size(), SQLITE_TRANSIENT);
                g_api.bind_text(st, 4, startIso.c_str(), (int)startIso.size(), SQLITE_TRANSIENT);
                g_api.bind_int64(st, 5, EpochFromIso(startIso));
                g_api.bind_int64(st, 6, EpochFromIso(endIso));
                g_api.bind_int(st, 7, ev.Get(L"done").AsBool(false) ? 1 : 0);
                g_api.bind_int(st, 8, ev.Get(L"status").AsInt(0));
                g_api.bind_text(st, 9, "{}", -1, SQLITE_TRANSIENT);
                if (g_api.step(st) != SQLITE_DONE) { tx = false; break; }
            }
            g_api.finalize(st);
        } else tx = false;
    }
    // tags
    if (tx) {
        sqlite3_stmt* st = nullptr;
        if (Prepare(db, "INSERT OR REPLACE INTO tags(id,name,color,builtin) VALUES(?1,?2,?3,?4)", &st)) {
            for (auto& tag : root.Get(L"tags").arr) {
                std::string id = WideToUtf8(tag.Get(L"id").AsStr());
                std::string name = WideToUtf8(tag.Get(L"name").AsStr());
                std::string color = WideToUtf8(tag.Get(L"color").AsStr());
                g_api.reset(st);
                g_api.bind_text(st, 1, id.c_str(), (int)id.size(), SQLITE_TRANSIENT);
                g_api.bind_text(st, 2, name.c_str(), (int)name.size(), SQLITE_TRANSIENT);
                g_api.bind_text(st, 3, color.c_str(), (int)color.size(), SQLITE_TRANSIENT);
                g_api.bind_int(st, 4, tag.Get(L"builtin").AsBool(false) ? 1 : 0);
                if (g_api.step(st) != SQLITE_DONE) { tx = false; break; }
            }
            g_api.finalize(st);
        } else tx = false;
    }
    if (tx) tx = Exec(db, "COMMIT;");
    else Exec(db, "ROLLBACK;");
    g_api.close_v2(db);
    if (!tx) DebugLog(L"SQLite: 保存失败");
    return tx;
}

int DbEventRowCount() {
    if (!LoadApi()) return -1;
    sqlite3* db = nullptr;
    if (g_api.open_v2(WideToUtf8(DbPath()).c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        if (db) g_api.close_v2(db);
        return -1;
    }
    int n = CountEventRows(db);
    g_api.close_v2(db);
    return n;
}

bool SqliteEnableStorage(std::wstring& err) {
    err.clear();
    if (!LoadApi()) { err = L"系统没有 winsqlite3.dll（需要 Windows 10+）"; return false; }
    DoSaveStore();
    bool ok = false;
    std::string json = ReadFileBytes(GetCalendarPath(), &ok);
    if (!ok || json.empty()) { err = L"当前 JSON 数据为空，不能迁移"; return false; }
    Store tmp;
    std::wstring jerr;
    if (!tmp.FromJsonStr(json, &jerr)) { err = L"当前 JSON 校验失败：" + jerr; return false; }
    std::wstring bakDir = PathJoin(GetDataDir(), L"backup");
    EnsureDir(bakDir);
    std::wstring stamp = ISODate(NowDT()) + L"-" + Fmt(L"%02d%02d", NowDT().h, NowDT().mi);
    CopyFileSimple(GetCalendarPath(), PathJoin(bakDir, L"json-before-sqlite-" + stamp + L".json"));
    if (FileExists(DbPath()))
        CopyFileSimple(DbPath(), PathJoin(bakDir, L"db-before-sqlite-" + stamp + L".db"));
    if (!DbSaveJson(json)) { err = L"写入 SQLite 失败"; return false; }
    std::string check;
    if (!DbLoadJson(check) || check != json) { err = L"SQLite 回读校验失败，已保持 JSON 存储"; return false; }
    SetEngineFlag(true);
    DebugLog(L"SQLite 迁移完成：events=%d，备份=json-before-sqlite-%s.json", (int)tmp.events.size(), stamp.c_str());
    return true;
}

bool SqliteRollbackToJson(std::wstring& err) {
    err.clear();
    DoSaveStore();
    bool ok = false;
    std::string json = ReadFileBytes(GetCalendarPath(), &ok);
    if (!ok || json.empty()) { err = L"JSON 镜像不存在，无法回退"; return false; }
    Store tmp;
    std::wstring jerr;
    if (!tmp.FromJsonStr(json, &jerr)) { err = L"JSON 镜像损坏：" + jerr; return false; }
    if (FileExists(DbPath())) {
        EnsureDir(PathJoin(GetDataDir(), L"backup"));
        CopyFileSimple(DbPath(), PathJoin(GetDataDir(), L"backup/calendar-before-rollback.db"));
    }
    SetEngineFlag(false);
    DebugLog(L"SQLite 回退完成：JSON events=%d", (int)tmp.events.size());
    return true;
}

// ------------------------- 管理界面 -------------------------
void OpenDbManager() {
    app.overlay = OV_DB;
    UIInvalidate();
}

void DrawDbManagerOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(680), ph = S(480);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 8, 26);
    FillRoundRect(g, panel, S(18), th.card);
    StrokeRoundRect(g, panel, S(18), th.border, S(1));
    Text(g, L"存储引擎", S(17), true, th.text, RectF(x + S(24), y + S(16), S(300), S(26)));
    Text(g, L"JSON 轻量、可读；SQLite 单文件 + 事务 + 索引，为数据量和多端同步做准备。",
         S(9), false, th.dim2, RectF(x + S(24), y + S(46), pw - S(48), S(18)), ALIGN_LEFT, false, true);
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_IMPORT_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });

    bool sqlite = DbEngineEnabled();
    RectF stBox(x + S(24), y + S(80), pw - S(48), S(108));
    FillRoundRect(g, stBox, S(10), WithAlpha(th.bg, 170));
    StrokeRoundRect(g, stBox, S(10), th.borderSoft, S(1));
    Text(g, sqlite ? L"当前：SQLite 存储" : L"当前：JSON 存储", S(14), true, sqlite ? th.success : th.text,
         RectF(stBox.X + S(16), stBox.Y + S(12), S(320), S(24)));
    int dbEvents = DbEventRowCount();
    Text(g, Fmt(L"日程库存：%d 条 · SQLite 索引行：%d 条 · winsqlite3：%s",
                (int)app.store.events.size(), dbEvents < 0 ? 0 : dbEvents, DbAvailable() ? L"可用" : L"不可用"),
         S(9.5f), false, th.dim, RectF(stBox.X + S(16), stBox.Y + S(44), stBox.Width - S(32), S(18)));
    Text(g, sqlite ? L"每次保存：先写 JSON 镜像，再事务写入 calendar.db；启动优先读数据库，失败自动回退 JSON。"
                   : L"默认 JSON 存储；迁移前自动备份 JSON 与旧数据库，可一键回退。",
         S(9), false, th.dim2, RectF(stBox.X + S(16), stBox.Y + S(68), stBox.Width - S(32), S(32)), ALIGN_LEFT, false, true);

    float by = y + S(218);
    UI::Btn(g, RectF(x + S(24), by, S(170), S(36)), CMD_IMPORT_OK, L"迁移到 SQLite", sqlite ? 1 : 0, ICO_FOLDER, []() {
        if (DbEngineEnabled()) { ShowToast(L"已经在使用 SQLite 存储"); return; }
        std::wstring err;
        if (SqliteEnableStorage(err)) ShowToast(L"已迁移到 SQLite（JSON 镜像保留）", 5000);
        else ShowToast(err.empty() ? L"迁移失败" : err, 6000);
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + S(206), by, S(160), S(36)), CMD_IMPORT_OK + 1, L"回退到 JSON", sqlite ? 0 : 1, ICO_REFRESH, []() {
        if (!DbEngineEnabled()) { ShowToast(L"当前已经是 JSON 存储"); return; }
        std::wstring err;
        if (SqliteRollbackToJson(err)) ShowToast(L"已回退到 JSON 存储（数据库已备份）", 5000);
        else ShowToast(err.empty() ? L"回退失败" : err, 6000);
        UIInvalidate();
    });
    UI::Btn(g, RectF(x + S(378), by, S(140), S(36)), CMD_IMPORT_OK + 2, L"打开数据目录", 1, ICO_FOLDER, []() {
        ShellExecuteW(nullptr, L"open", GetDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    });

    Text(g, L"备份位置", S(10), false, th.dim2, RectF(x + S(24), y + S(286), S(120), S(18)));
    Text(g, L"迁移前：data\\backup\\json-before-sqlite-*.json（及旧 db 备份）\n回退前：data\\backup\\calendar-before-rollback.db",
         S(9), false, th.dim, RectF(x + S(24), y + S(308), pw - S(48), S(44)), ALIGN_LEFT, false, true);
    Text(g, L"风险控制：迁移会先验证 JSON；写入后回读比对；不一致则保持 JSON 并报错，不覆盖原始数据。",
         S(8.5f), false, th.dim2, RectF(x + S(24), y + S(362), pw - S(48), S(32)), ALIGN_LEFT, false, true);

    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)), CMD_IMPORT_CANCEL, L"关闭", 0, -1, []() {
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
