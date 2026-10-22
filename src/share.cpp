// share.cpp - 外网「忙闲」只读页 + 预约/留言（本机 HTTP 服务，隧道由外部程序转发）
#include <winsock2.h>
#include <shellapi.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <cwctype>
#include "app.h"
#include "json.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>

using namespace Gdiplus;
using namespace gfx;

namespace {

static void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
                int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    gfx::Text(g, s, px, bold, c, r, align, display, ellipsis);
}

CRITICAL_SECTION g_cs;
bool g_csInit = false;
volatile bool g_run = false;
HANDLE g_thread = nullptr;
SOCKET g_listen = INVALID_SOCKET;
bool g_serverOk = false;
std::string g_stateJson = "{\"busy\":false,\"until\":\"\",\"next\":\"\",\"blocks\":[],\"req\":true,\"now\":\"\"}";
std::wstring g_token;
bool g_allowReq = true;
int g_port = 17890;
int g_newReq = 0;
DWORD g_lastPostTick = 0;
std::wstring g_publicUrl;

std::string Utf8(const std::wstring& w) { return WideToUtf8(w); }
std::wstring W(const std::string& s) { return Utf8ToWide(s); }

void Lock() { if (g_csInit) EnterCriticalSection(&g_cs); }
void Unlock() { if (g_csInit) LeaveCriticalSection(&g_cs); }

std::wstring ShareDirFile(const wchar_t* name) { return PathJoin(GetDataDir(), name); }

std::wstring NewToken() {
    unsigned char buf[24] = {0};
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, buf, sizeof(buf), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        for (int i = 0; i < 24; ++i) buf[i] = (unsigned char)(rand() & 0xFF);
    }
    static const wchar_t* hex = L"0123456789abcdef";
    std::wstring s;
    for (unsigned char c : buf) { s += hex[c >> 4]; s += hex[c & 15]; }
    return s;
}

void SaveConfig() {
    std::wstring txt;
    txt += Fmt(L"enabled=%d\n", app.share.enabled ? 1 : 0);
    txt += Fmt(L"allowRequests=%d\n", app.share.allowRequests ? 1 : 0);
    txt += Fmt(L"useTailscale=%d\n", app.share.useTailscale ? 1 : 0);
    txt += Fmt(L"port=%d\n", app.share.port);
    txt += L"token=" + app.share.token + L"\n";
    txt += L"publicUrl=" + app.share.publicUrl + L"\n";
    WriteFileBytes(ShareDirFile(L"share.ini"), Utf8(txt));
}

void LoadConfig() {
    bool ok = false;
    std::string raw = ReadFileBytes(ShareDirFile(L"share.ini"), &ok);
    if (!ok || raw.empty()) return;
    std::wstring txt = W(raw);
    size_t p = 0;
    while (p < txt.size()) {
        size_t e = txt.find(L'\n', p);
        std::wstring line = txt.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p);
        p = (e == std::wstring::npos) ? txt.size() : e + 1;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = line.substr(0, eq), v = line.substr(eq + 1);
        while (!v.empty() && (v.back() == L'\r' || v.back() == L' ')) v.pop_back();
        if (k == L"enabled") app.share.enabled = (v == L"1");
        else if (k == L"allowRequests") app.share.allowRequests = (v == L"1");
        else if (k == L"useTailscale") app.share.useTailscale = (v == L"1");
        else if (k == L"port") app.share.port = std::max(1024, std::min(65535, _wtoi(v.c_str())));
        else if (k == L"token") app.share.token = v;
        else if (k == L"publicUrl") app.share.publicUrl = v;
    }
}

std::string UrlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') { out += ' '; continue; }
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hexv = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int a = hexv(s[i + 1]), b = hexv(s[i + 2]);
            if (a >= 0 && b >= 0) { out += (char)(a * 16 + b); i += 2; continue; }
        }
        out += s[i];
    }
    return out;
}

std::map<std::string, std::string> ParseForm(const std::string& body) {
    std::map<std::string, std::string> m;
    size_t p = 0;
    while (p < body.size()) {
        size_t e = body.find('&', p);
        std::string kv = body.substr(p, e == std::string::npos ? std::string::npos : e - p);
        p = (e == std::string::npos) ? body.size() : e + 1;
        size_t eq = kv.find('=');
        if (eq == std::string::npos) continue;
        m[UrlDecode(kv.substr(0, eq))] = UrlDecode(kv.substr(eq + 1));
    }
    return m;
}

std::string ParamToken(const std::string& path) {
    size_t q = path.find('?');
    if (q == std::string::npos) return "";
    std::string query = path.substr(q + 1);
    size_t sp = query.find(' ');
    if (sp != std::string::npos) query = query.substr(0, sp);
    size_t p = 0;
    while (p < query.size()) {
        size_t e = query.find('&', p);
        std::string kv = query.substr(p, e == std::string::npos ? std::string::npos : e - p);
        p = (e == std::string::npos) ? query.size() : e + 1;
        if (kv.rfind("t=", 0) == 0) return UrlDecode(kv.substr(2));
        if (kv.rfind("token=", 0) == 0) return UrlDecode(kv.substr(6));
    }
    return "";
}

// ---------- 忙闲快照（UI 线程构建） ----------
std::string HHMM(int min) {
    if (min < 0) min = 0;
    if (min > 24 * 60) min = 24 * 60;
    char b[8];
    snprintf(b, sizeof(b), "%02d:%02d", min / 60, min % 60);
    return b;
}

std::string BuildStateJson() {
    DateTime now = NowDT();
    DateTime day0 = StartOfDay(now);
    int baseSerial = DaySerial(day0.y, day0.mo, day0.d);
    auto occs = ExpandOccurrences(app.store, day0, AddDaysDT(day0, 31));
    std::vector<std::vector<std::pair<int,int>>> perDay(30);
    for (auto& o : occs) {
        if (o.ev.done || o.ev.status == ST_CANCELLED) continue;
        int ss = DaySerial(o.start.y, o.start.mo, o.start.d);
        int es = DaySerial(o.end.y, o.end.mo, o.end.d);
        int sMin = o.start.h * 60 + o.start.mi;
        int eMin = o.end.h * 60 + o.end.mi;
        if (o.ev.allDay) {
            int from = std::max(0, ss - baseSerial);
            int to = std::min(29, es - baseSerial);
            for (int d = from; d <= to; ++d) perDay[d].push_back({0, 24 * 60});
            continue;
        }
        int from = std::max(0, ss - baseSerial);
        int to = std::min(29, es - baseSerial);
        for (int d = from; d <= to; ++d) {
            int s = (d == ss - baseSerial) ? sMin : 0;
            int e = (d == es - baseSerial) ? eMin : 24 * 60;
            if (e > s) perDay[d].push_back({s, e});
        }
    }
    for (auto& day : perDay) {
        std::sort(day.begin(), day.end());
        std::vector<std::pair<int,int>> merged;
        for (auto& b : day) {
            if (merged.empty() || b.first > merged.back().second + 10) merged.push_back(b);
            else merged.back().second = std::max(merged.back().second, b.second);
        }
        day.swap(merged);
    }
    int nowMin = now.h * 60 + now.mi;
    auto& today = perDay[0];
    int until = -1;
    for (auto& b : today) if (nowMin >= b.first && nowMin < b.second) { until = b.second; break; }
    std::string nextFree;
    {
        int cur = nowMin;
        for (auto& b : today) {
            if (b.second <= cur) continue;
            if (b.first > cur) break;
            cur = b.second;
        }
        int nextStart = -1;
        for (auto& b : today) if (b.first > cur) { nextStart = b.first; break; }
        if (nextStart < 0) nextFree = HHMM(cur) + "-24:00";
        else nextFree = HHMM(cur) + "-" + HHMM(nextStart);
    }
    std::string js = "{";
    js += "\"busy\":" + std::string(until >= 0 ? "true" : "false");
    js += ",\"until\":\"" + (until >= 0 ? HHMM(until) : "") + "\"";
    js += ",\"next\":\"" + nextFree + "\"";
    js += ",\"now\":\"" + HHMM(nowMin) + "\"";
    js += ",\"req\":" + std::string(g_allowReq ? "true" : "false");
    js += ",\"days\":[";
    for (int d = 0; d < 30; ++d) {
        DateTime dt = AddDaysDT(day0, d);
        if (d) js += ",";
        std::string date = Utf8(Fmt(L"%04d-%02d-%02d", dt.y, dt.mo, dt.d));
        js += "{\"d\":\"" + date + "\",\"b\":[";
        for (size_t k = 0; k < perDay[d].size(); ++k) {
            if (k) js += ",";
            js += "[" + std::to_string(perDay[d][k].first) + "," + std::to_string(perDay[d][k].second) + "]";
        }
        js += "]}";
    }
    js += "]}";
    return js;
}

// ---------- 预约/留言 ----------
std::string ShareRequestsRaw() {
    bool ok = false;
    return ReadFileBytes(ShareDirFile(L"share-requests.json"), &ok);
}

void SaveRequests(const JValue& arr) {
    WriteFileBytes(ShareDirFile(L"share-requests.json"), JsonDump(arr, true));
}

void AppendRequest(const std::map<std::string, std::string>& f) {
    JValue arr = JValue::Arr();
    std::string raw = ShareRequestsRaw();
    if (!raw.empty()) {
        JValue old = JsonParse(raw, nullptr);
        if (old.IsArr()) arr = old;
    }
    JValue o = JValue::Obj();
    DateTime tn = NowDT();
    o.Set(L"time") = JValue(Fmt(L"%04d-%02d-%02d %02d:%02d", tn.y, tn.mo, tn.d, tn.h, tn.mi));
    o.Set(L"name") = JValue(W(f.count("name") ? f.at("name") : ""));
    o.Set(L"contact") = JValue(W(f.count("contact") ? f.at("contact") : ""));
    o.Set(L"kind") = JValue(W(f.count("kind") ? f.at("kind") : "message"));
    o.Set(L"prefer") = JValue(W(f.count("prefer") ? f.at("prefer") : ""));
    o.Set(L"minutes") = JValue(W(f.count("minutes") ? f.at("minutes") : ""));
    std::string msg = f.count("message") ? f.at("message") : "";
    if (msg.size() > 600) msg = msg.substr(0, 600);
    o.Set(L"message") = JValue(W(msg));
    arr.Push(o);
    SaveRequests(arr);
    Lock();
    ++g_newReq;
    Unlock();
}

struct ShareRequest {
    std::wstring time, name, contact, kind, prefer, minutes, message;
};

std::vector<ShareRequest> LoadRequests() {
    std::vector<ShareRequest> out;
    std::string raw = ShareRequestsRaw();
    if (raw.empty()) return out;
    JValue arr = JsonParse(raw, nullptr);
    if (!arr.IsArr()) return out;
    for (auto& v : arr.arr) {
        ShareRequest r;
        r.time = v.Get(L"time").AsStr();
        r.name = v.Get(L"name").AsStr();
        r.contact = v.Get(L"contact").AsStr();
        r.kind = v.Get(L"kind").AsStr();
        r.prefer = v.Get(L"prefer").AsStr();
        r.minutes = v.Get(L"minutes").AsStr();
        r.message = v.Get(L"message").AsStr();
        out.push_back(r);
    }
    return out;
}

// ---------- 预约/留言处理 ----------
void RemoveRequest(size_t idx) {
    JValue arr = JValue::Arr();
    std::string raw = ShareRequestsRaw();
    if (!raw.empty()) {
        JValue old = JsonParse(raw, nullptr);
        if (old.IsArr()) arr = old;
    }
    if (idx >= arr.arr.size()) return;
    arr.arr.erase(arr.arr.begin() + (long)idx);
    SaveRequests(arr);
}

void ParsePrefer(const std::wstring& prefer, DateTime& day, int& hour, int& minute) {
    DateTime now = NowDT();
    DateTime base = StartOfDay(now);
    day = AddDaysDT(base, 1);
    hour = 18;
    minute = 0;
    if (prefer.find(L"今天") != std::wstring::npos) day = base;
    else if (prefer.find(L"后天") != std::wstring::npos) day = AddDaysDT(base, 2);
    else if (prefer.find(L"明天") != std::wstring::npos) day = AddDaysDT(base, 1);
    for (size_t i = 0; i + 1 < prefer.size(); ++i) {
        if (prefer[i] != L'/') continue;
        size_t a = i, b = i + 1;
        while (a > 0 && iswdigit(prefer[a - 1])) --a;
        while (b < prefer.size() && iswdigit(prefer[b])) ++b;
        if (a == i || b == i + 1) continue;
        int mo = _wtoi(prefer.substr(a, i - a).c_str());
        int dd = _wtoi(prefer.substr(i + 1, b - i - 1).c_str());
        if (mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31) {
            day = MakeDT(now.y, mo, dd);
            if (CompareDT(day, base) < 0) day = MakeDT(now.y + 1, mo, dd);
        }
        break;
    }
    for (size_t i = 0; i + 1 < prefer.size(); ++i) {
        if (prefer[i] != L':') continue;
        size_t a = i, b = i + 1;
        while (a > 0 && iswdigit(prefer[a - 1])) --a;
        while (b < prefer.size() && iswdigit(prefer[b])) ++b;
        if (a < i && b > i + 1) {
            int h = _wtoi(prefer.substr(a, i - a).c_str());
            int mi = _wtoi(prefer.substr(i + 1, b - i - 1).c_str());
            if (h >= 0 && h <= 23 && mi >= 0 && mi <= 59) { hour = h; minute = mi; }
        }
        break;
    }
}

void AcceptShareRequest(size_t idx) {
    auto reqs = LoadRequests();
    if (idx >= reqs.size()) return;
    ShareRequest r = reqs[idx];
    if (r.kind != L"appointment") {
        RemoveRequest(idx);
        ShowToast(L"留言已标记为已读");
        UIInvalidate();
        return;
    }
    DateTime day;
    int hour = 18, minute = 0;
    ParsePrefer(r.prefer, day, hour, minute);
    int mins = 30;
    if (!r.minutes.empty()) {
        int v = _wtoi(r.minutes.c_str());
        if (v >= 5 && v <= 480) mins = v;
    }
    OpenEditorNew(day, hour, minute);
    app.editor.start = MakeDT(day.y, day.mo, day.d, hour, minute);
    app.editor.end = AddMinutesDT(app.editor.start, mins);
    app.editor.SyncTimeTexts();
    app.editor.title = L"预约：" + (r.name.empty() ? L"匿名" : r.name);
    std::wstring desc = L"来自分享页的预约请求";
    if (!r.prefer.empty()) desc += L"\n想约：" + r.prefer;
    if (!r.contact.empty()) desc += L"\n联系方式：" + r.contact;
    if (!r.message.empty()) desc += L"\n留言：" + r.message;
    app.editor.desc = desc;
    RemoveRequest(idx);
    ShowToast(L"已生成预约草稿，确认时间后保存", 4000);
    UIInvalidate();
}

void IgnoreShareRequest(size_t idx) {
    RemoveRequest(idx);
    ShowToast(L"已忽略这条请求");
    UIInvalidate();
}

// ---------- HTTP ----------
const char* kPage = R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex">
<title>星历 · 30 天忙闲</title>
<style>
:root{color-scheme:dark}
body{margin:0;background:#0f1117;color:#e8ecf4;font:15px/1.6 -apple-system,"Microsoft YaHei",sans-serif}
.wrap{max-width:640px;margin:0 auto;padding:22px 16px 44px}
.card{background:#171b24;border:1px solid #262d3a;border-radius:18px;padding:18px;margin-bottom:14px}
h1{font-size:17px;margin:0 0 2px}
.sub{color:#8b95a7;font-size:12px}
.now{font-size:30px;font-weight:700;margin:12px 0 0}
.now.busy{color:#ffb454}.now.free{color:#4ade80}
.until{color:#aab3c2;font-size:13px;margin-top:2px}
.title{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:10px}
.title b{font-size:14px}
.title span{color:#7d8798;font-size:11px}
.grid{display:grid;grid-template-columns:repeat(7,1fr);gap:6px}
.day{background:#10141c;border:1px solid #232a37;border-radius:10px;padding:6px 5px 7px;cursor:pointer;min-height:58px}
.day.today{border-color:#e3c177}
.day.sel{border-color:#7aa2ff;background:#141b28}
.dn{font-size:12px;font-weight:700;line-height:1.2}
.dw{color:#7d8798;font-size:10px;line-height:1.3}
.mini{position:relative;height:7px;border-radius:3px;background:#232a37;margin-top:5px;overflow:hidden}
.mb{position:absolute;top:0;height:100%;background:#ffb454}
.mini.empty{background:#1a202b}
.legend{color:#7d8798;font-size:11px;margin-top:8px}
.detail{margin-top:12px;color:#aab3c2;font-size:12px;line-height:1.8}
.detail b{color:#e8ecf4}
.chip{display:inline-block;background:#1e2430;border-radius:7px;padding:1px 8px;margin:2px 6px 2px 0;color:#cbd5e1}
label{display:block;color:#aab3c2;font-size:12px;margin:10px 0 4px}
input,textarea{width:100%;box-sizing:border-box;background:#0f1117;color:#e8ecf4;border:1px solid #303849;border-radius:10px;padding:10px 12px;font:inherit}
textarea{resize:vertical;min-height:64px}
button{margin-top:12px;width:100%;background:#e3c177;color:#1a1a1a;border:0;border-radius:12px;padding:12px;font-size:15px;font-weight:700}
button:active{opacity:.85}
.ok{color:#4ade80;font-size:13px;margin-top:8px}
.err{color:#ff6b7a;font-size:13px;margin-top:8px}
.foot{color:#6f7a8c;font-size:11px;text-align:center;margin-top:6px}
</style></head><body><div class="wrap">
<h1>星历 · 忙闲</h1><div class="sub" id="upd">加载中…</div>
<div class="card">
  <div class="now free" id="now">—</div>
  <div class="until" id="until"></div>
</div>
<div class="card">
  <div class="title"><b>未来 30 天</b><span>只显示忙/闲，不显示日程内容</span></div>
  <div class="grid" id="grid"></div>
  <div class="legend"><span style="color:#ffb454">■</span> 忙 &nbsp; <span style="color:#2a3240">■</span> 空闲 &nbsp; · 点击某天看时段</div>
  <div class="detail" id="detail">—</div>
</div>
<div class="card">
  <div style="font-weight:600;margin-bottom:2px">预约 / 留言</div>
  <div class="sub">对方会收到你的预约，你可以在星历里接受或忽略。</div>
  <form id="f">
    <label>称呼</label><input name="name" maxlength="40" placeholder="怎么称呼你">
    <label>想约时间（可选）</label><input name="prefer" maxlength="60" placeholder="例如 10/20 18:00">
    <label>时长（分钟，可选）</label><input name="minutes" maxlength="6" placeholder="30">
    <label>留言（可选）</label><textarea name="message" maxlength="600" placeholder="想说的话"></textarea>
    <button type="submit">发送</button>
    <div id="msg"></div>
  </form>
</div>
<div class="foot">本页面只能查看忙闲，不能修改任何日程 · 每 10 秒刷新</div>
</div>
<script>
const t = new URLSearchParams(location.search).get('t') || '';
const $ = id => document.getElementById(id);
const WD = ['日','一','二','三','四','五','六'];
let selected = 0;
let dayData = [];
function hh(m){ m=Math.max(0,Math.min(1440,m)); return String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0'); }
function dateLabel(s){ const p=s.split('-'); return p[1].replace(/^0/,'')+'/'+p[2].replace(/^0/,''); }
function wdLabel(s){ const d=new Date(s+'T12:00:00'); return '周'+WD[d.getDay()]; }
function renderDetail(){
  if(!dayData.length) return;
  const x = dayData[selected];
  let html = '<b>'+dateLabel(x.d)+' '+wdLabel(x.d)+'</b><br>';
  if(!x.b || !x.b.length){ html += '<span class="chip">全天空闲</span>'; }
  else {
    x.b.forEach(b=>{ html += '<span class="chip">忙 '+hh(b[0])+'–'+hh(b[1])+'</span>'; });
    html += '<div style="color:#6f7a8c;font-size:11px;margin-top:4px">其余时间空闲</div>';
  }
  $('detail').innerHTML = html;
}
function renderDays(){
  const g=$('grid'); g.innerHTML='';
  dayData.forEach((x,i)=>{
    const el=document.createElement('div');
    el.className='day'+(i===0?' today':'')+(i===selected?' sel':'');
    let bars='';
    (x.b||[]).forEach(b=>{
      const l=b[0]/1440*100, w=(b[1]-b[0])/1440*100;
      bars += '<div class="mb" style="left:'+l+'%;width:'+Math.max(1.5,w)+'%"></div>';
    });
    el.innerHTML='<div class="dn">'+dateLabel(x.d)+'</div><div class="dw">'+wdLabel(x.d)+'</div>'+
                 '<div class="mini'+((x.b&&x.b.length)?'':' empty')+'">'+bars+'</div>';
    el.onclick=()=>{ selected=i; renderDays(); renderDetail(); };
    g.appendChild(el);
  });
  renderDetail();
}
async function load(){
  try{
    const r=await fetch('/api/state?t='+encodeURIComponent(t),{cache:'no-store'});
    if(!r.ok) throw new Error('HTTP '+r.status);
    const d=await r.json();
    dayData = d.days||[];
    $('now').textContent = d.busy ? '现在：忙' : '现在：空闲';
    $('now').className = 'now ' + (d.busy?'busy':'free');
    $('until').textContent = d.busy ? ('到 '+(d.until||'')+(d.next?(' · 下一段空闲 '+d.next):'')) : (d.next?('下一段空闲 '+d.next):'');
    $('upd').textContent = '更新于 '+(d.now||'')+' · 未来 30 天';
    renderDays();
    if(d.req===false) $('f').style.display='none';
  }catch(e){
    $('now').textContent='暂时无法连接';
    $('now').className='now busy';
    $('until').textContent='分享者电脑可能离线';
  }
}
load(); setInterval(load,10000);
$('f').addEventListener('submit', async ev=>{
  ev.preventDefault();
  const fd=new FormData(ev.target); fd.set('kind','appointment');
  const body=new URLSearchParams(fd).toString();
  $('msg').textContent=''; $('msg').className='';
  try{
    const r=await fetch('/api/request?t='+encodeURIComponent(t),{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if(!r.ok) throw new Error('HTTP '+r.status);
    $('msg').textContent='已发送，对方会收到提醒。'; $('msg').className='ok';
    ev.target.reset();
  }catch(e){ $('msg').textContent='发送失败，可能分享已关闭。'; $('msg').className='err'; }
});
</script></body></html>)HTML";

std::string HttpHeader(int code, const char* ctype, size_t len, const char* extra = "") {
    const char* reason = code == 200 ? "OK" : (code == 403 ? "Forbidden" : (code == 404 ? "Not Found" : (code == 429 ? "Too Many Requests" : "Error")));
    std::string h = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n";
    h += std::string("Content-Type: ") + ctype + "\r\n";
    h += "Content-Length: " + std::to_string(len) + "\r\n";
    h += "Cache-Control: no-store\r\nX-Robots-Tag: noindex\r\nConnection: close\r\n";
    h += extra;
    h += "\r\n";
    return h;
}

void SendAll(SOCKET s, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        int n = send(s, data.data() + off, (int)(data.size() - off), 0);
        if (n <= 0) break;
        off += (size_t)n;
    }
}

void SendResponse(SOCKET s, int code, const char* ctype, const std::string& body, const char* extra = "") {
    std::string resp = HttpHeader(code, ctype, body.size(), extra) + body;
    SendAll(s, resp);
}

// ---------------- 账号同步（M1） ----------------
static std::wstring SyncAccountsPath() { return PathJoin(GetDataDir(), L"sync-accounts.json"); }
static std::wstring SyncDir() { return PathJoin(GetDataDir(), L"sync"); }
static std::wstring SyncUserPath(const std::wstring& user) { return PathJoin(SyncDir(), user + L".json"); }

static std::string Sha256Hex(const std::string& data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    std::string out(64, '0');
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return out;
    DWORD objLen = 0, cb = 0, hashLen = 0;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0) != 0 ||
        BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }
    std::vector<BYTE> obj(objLen), hash(hashLen);
    if (BCryptCreateHash(alg, &h, obj.data(), objLen, nullptr, 0, 0) == 0) {
        BCryptHashData(h, (PUCHAR)data.data(), (ULONG)data.size(), 0);
        BCryptFinishHash(h, hash.data(), hashLen, 0);
        static const char* hex = "0123456789abcdef";
        for (DWORD i = 0; i < hashLen; ++i) {
            out[i * 2] = hex[hash[i] >> 4];
            out[i * 2 + 1] = hex[hash[i] & 15];
        }
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}
static std::string RandomHex(int bytes) {
    std::vector<BYTE> buf(bytes);
    if (BCryptGenRandom(nullptr, buf.data(), bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        for (int i = 0; i < bytes; ++i) buf[i] = (BYTE)(GetTickCount() + i * 37);
    }
    static const char* hex = "0123456789abcdef";
    std::string out(bytes * 2, '0');
    for (int i = 0; i < bytes; ++i) {
        out[i * 2] = hex[buf[i] >> 4];
        out[i * 2 + 1] = hex[buf[i] & 15];
    }
    return out;
}
static std::string HashPassword(const std::string& pass, const std::string& salt) {
    std::string cur = salt + ":" + pass;
    for (int i = 0; i < 2000; ++i) cur = Sha256Hex(cur + salt);
    return cur;
}
static bool IsSafeUserName(const std::wstring& u) {
    if (u.empty() || u.size() > 32) return false;
    for (wchar_t c : u) {
        if (!(iswalnum(c) || c == L'_' || c == L'-')) return false;
    }
    return true;
}
static JValue LoadAccounts() {
    bool ok = false;
    std::string raw = ReadFileBytes(SyncAccountsPath(), &ok);
    if (ok && !raw.empty()) {
        JValue root = JsonParse(raw);
        if (root.IsObj() && root.Has(L"users")) return root;
    }
    JValue root = JValue::Obj();
    root.Set(L"users") = JValue::Arr();
    return root;
}
static void SaveAccounts(const JValue& root) {
    WriteFileBytesAtomic(SyncAccountsPath(), JsonDump(root, true));
}
static std::vector<JValue> CopyUsers(const JValue& root) {
    std::vector<JValue> out;
    if (root.Has(L"users") && root.Get(L"users").IsArr())
        for (auto& u : root.Get(L"users").arr) out.push_back(u);
    return out;
}
static void SetUsers(JValue& root, const std::vector<JValue>& users) {
    JValue a = JValue::Arr();
    for (auto& u : users) a.Push(u);
    root.Set(L"users") = a;
}
static int FindUserIndex(const JValue& root, const std::wstring& name) {
    if (!root.Has(L"users") || !root.Get(L"users").IsArr()) return -1;
    const auto& arr = root.Get(L"users").arr;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].IsObj() && arr[i].Has(L"name") && arr[i].Get(L"name").AsStr() == name) return (int)i;
    }
    return -1;
}
static int FindUserIndexByToken(const JValue& root, const std::string& token) {
    if (token.empty() || !root.Has(L"users") || !root.Get(L"users").IsArr()) return -1;
    const auto& arr = root.Get(L"users").arr;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].IsObj() && arr[i].Has(L"token") && WideToUtf8(arr[i].Get(L"token").AsStr()) == token) return (int)i;
    }
    return -1;
}
static void EnsureSyncDir() {
    std::wstring dir = SyncDir();
    CreateDirectoryW(dir.c_str(), nullptr);
}

// 同步 API：/api/auth/register|login、/api/sync/push|pull
// 返回 true 表示已处理（调用方直接 return）
static bool HandleSyncApi(SOCKET s, const std::string& method, const std::string& clean,
                          const std::string& path, const std::string& body) {
    auto reply = [&](int code, const std::string& js) {
        SendResponse(s, code, "application/json; charset=utf-8", js);
    };
    auto replyErr = [&](int code, const char* err) {
        std::string js = std::string("{\"ok\":false,\"err\":\"") + err + "\"}";
        reply(code, js);
    };
    if (clean != "/api/auth/register" && clean != "/api/auth/login" &&
        clean != "/api/sync/push" && clean != "/api/sync/pull") {
        replyErr(404, "not_found");
        return true;
    }
    JValue req = JsonParse(body);
    std::wstring user = req.Has(L"user") ? req.Get(L"user").AsStr() : L"";
    std::wstring pass = req.Has(L"pass") ? req.Get(L"pass").AsStr() : L"";

    if (method == "POST" && (clean == "/api/auth/register" || clean == "/api/auth/login")) {
        if (!IsSafeUserName(user) || pass.size() < 6) { replyErr(400, "bad_user"); return true; }
        JValue root = LoadAccounts();
        auto users = CopyUsers(root);
        int idx = FindUserIndex(root, user);
        std::string salt = RandomHex(8);
        std::string userUtf8 = WideToUtf8(user);
        if (clean == "/api/auth/register") {
            if (idx >= 0) { replyErr(409, "exists"); return true; }
            JValue item = JValue::Obj();
            item.Set(L"name") = JValue(user);
            item.Set(L"salt") = JValue(Utf8ToWide(salt));
            item.Set(L"hash") = JValue(Utf8ToWide(HashPassword(userUtf8, salt)));
            item.Set(L"token") = JValue(Utf8ToWide(RandomHex(16)));
            users.push_back(item);
            SetUsers(root, users);
            SaveAccounts(root);
            std::string token = WideToUtf8(item.Get(L"token").AsStr());
            reply(200, "{\"ok\":true,\"token\":\"" + token + "\"}");
            return true;
        }
        if (idx < 0) { replyErr(401, "no_user"); return true; }
        std::string storedSalt = WideToUtf8(users[idx].Get(L"salt").AsStr());
        std::string storedHash = WideToUtf8(users[idx].Get(L"hash").AsStr());
        if (HashPassword(userUtf8, storedSalt) != storedHash) { replyErr(401, "bad_pass"); return true; }
        std::string token = RandomHex(16);
        users[idx].Set(L"token") = JValue(Utf8ToWide(token));
        SetUsers(root, users);
        SaveAccounts(root);
        reply(200, "{\"ok\":true,\"token\":\"" + token + "\"}");
        return true;
    }

    // 同步数据接口：token 在 query
    std::string token = ParamToken(path);
    JValue root = LoadAccounts();
    int uidx = FindUserIndexByToken(root, token);
    if (uidx < 0) { replyErr(403, "bad_token"); return true; }
    std::wstring uname = root.Get(L"users").arr[uidx].Get(L"name").AsStr();
    EnsureSyncDir();
    std::wstring file = SyncUserPath(uname);
    if (method == "GET" && clean == "/api/sync/pull") {
        bool ok = false;
        std::string store = ReadFileBytes(file, &ok);
        if (!ok || store.empty()) store = "{}";
        JValue sv = JsonParse(store);
        if (!sv.IsObj()) sv = JValue::Obj();
        JValue out = JValue::Obj();
        out.Set(L"ok") = JValue(true);
        out.Set(L"store") = sv;
        reply(200, JsonDump(out, false));
        return true;
    }
    if (method == "POST" && clean == "/api/sync/push") {
        if (!req.Has(L"store") || !req.Get(L"store").IsObj()) { replyErr(400, "no_store"); return true; }
        WriteFileBytesAtomic(file, JsonDump(req.Get(L"store"), true));
        reply(200, "{\"ok\":true}");
        return true;
    }
    replyErr(405, "bad_method");
    return true;
}

void HandleClient(SOCKET s) {
    std::string req;
    char buf[4096];
    while (req.size() < 16384) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) return;
        req.append(buf, (size_t)n);
        if (req.find("\r\n\r\n") != std::string::npos) break;
    }
    size_t hdrEnd = req.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return;
    std::string head = req.substr(0, hdrEnd);
    std::string body = req.substr(hdrEnd + 4);

    size_t sp1 = head.find(' ');
    size_t sp2 = head.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return;
    std::string method = head.substr(0, sp1);
    std::string path = head.substr(sp1 + 1, sp2 - sp1 - 1);

    int contentLen = 0;
    {
        std::string lower = head;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        size_t hp = lower.find("\r\ncontent-length:");
        if (hp != std::string::npos) contentLen = atoi(head.c_str() + hp + 17);
    }
    while ((int)body.size() < contentLen && body.size() < 2 * 1024 * 1024) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        body.append(buf, (size_t)n);
    }
    if (body.size() > 2 * 1024 * 1024) body.resize(2 * 1024 * 1024);

    std::string clean = path;
    size_t q = clean.find('?');
    if (q != std::string::npos) clean = clean.substr(0, q);

    // 账号同步接口用自己的账号 token，不走分享 token
    if (clean.rfind("/api/auth/", 0) == 0 || clean.rfind("/api/sync/", 0) == 0) {
        if (HandleSyncApi(s, method, clean, path, body)) return;
    }

    std::wstring token;
    Lock();
    token = g_token;
    Unlock();

    std::string given = ParamToken(path);
    if (given.empty() || given != Utf8(token)) {
        SendResponse(s, 403, "text/plain; charset=utf-8", "forbidden");
        return;
    }

    if (method == "GET" && clean == "/") {
        std::string page = kPage;
        SendResponse(s, 200, "text/html; charset=utf-8", page);
        return;
    }
    if (method == "GET" && clean == "/busy") {
        SendResponse(s, 200, "text/html; charset=utf-8", kPage);
        return;
    }
    if (method == "GET" && clean == "/api/state") {
        std::string js;
        Lock(); js = g_stateJson; Unlock();
        SendResponse(s, 200, "application/json; charset=utf-8", js);
        return;
    }
    if (method == "POST" && clean == "/api/request") {
        if (!app.share.allowRequests) { SendResponse(s, 403, "application/json; charset=utf-8", "{\"ok\":false,\"err\":\"closed\"}"); return; }
        DWORD now = GetTickCount();
        if (g_lastPostTick && now - g_lastPostTick < 900) { SendResponse(s, 429, "application/json; charset=utf-8", "{\"ok\":false,\"err\":\"slow\"}"); return; }
        g_lastPostTick = now;
        auto form = ParseForm(body);
        AppendRequest(form);
        SendResponse(s, 200, "application/json; charset=utf-8", "{\"ok\":true}");
        return;
    }
    SendResponse(s, 404, "text/plain; charset=utf-8", "not found");
}

DWORD WINAPI ServerThread(LPVOID) {
    while (g_run) {
        SOCKET c = accept(g_listen, nullptr, nullptr);
        if (c == INVALID_SOCKET) break;
        HandleClient(c);
        // 先 shutdown(SD_SEND) 再关闭：大响应（如 80 条日程的 sync pull）时让内核把剩余
        // 数据发完并正常 FIN，避免客户端（Android HttpURLConnection）读到 RST 报
        // "unexpected end of stream"。
        shutdown(c, SD_SEND);
        closesocket(c);
    }
    return 0;
}

void StartServer() {
    if (g_run) return;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { g_serverOk = false; return; }
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) { WSACleanup(); g_serverOk = false; return; }
    BOOL yes = TRUE;
    setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)app.share.port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 监听所有网卡：手机端可经局域网访问同步 API
    if (bind(g_listen, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(g_listen, 8) != 0) {
        closesocket(g_listen); g_listen = INVALID_SOCKET; WSACleanup();
        g_serverOk = false;
        DebugLog(L"share: bind/listen failed port=%d", app.share.port);
        return;
    }
    g_run = true;
    g_thread = CreateThread(nullptr, 0, ServerThread, nullptr, 0, nullptr);
    g_serverOk = true;
    DebugLog(L"share: server started on 0.0.0.0:%d (LAN 可访问)", app.share.port);
}

void StopServer() {
    if (!g_run) return;
    g_run = false;
    if (g_listen != INVALID_SOCKET) { closesocket(g_listen); g_listen = INVALID_SOCKET; }
    if (g_thread) { WaitForSingleObject(g_thread, 2000); CloseHandle(g_thread); g_thread = nullptr; }
    WSACleanup();
    g_serverOk = false;
    DebugLog(L"share: server stopped");
}


// ---------- Tailscale Funnel ----------
std::wstring FindTailscaleExe() {
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = SearchPathW(nullptr, L"tailscale.exe", nullptr, MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH) return buf;
    const wchar_t* paths[] = {
        L"C:\\Program Files\\Tailscale\\tailscale.exe",
        L"C:\\Program Files (x86)\\Tailscale\\tailscale.exe"
    };
    for (auto p : paths) if (FileExists(p)) return p;
    return L"";
}

std::string RunCapture(const std::wstring& cmd, DWORD timeoutMs, DWORD& exitCode) {
    exitCode = 1;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return "";
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;
    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmd;
    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr); return "";
    }
    CloseHandle(wr);
    std::string out;
    char buf[4096];
    DWORD start = GetTickCount();
    for (;;) {
        DWORD avail = 0;
        while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD got = 0;
            DWORD want = std::min<DWORD>(avail, sizeof(buf));
            if (!ReadFile(rd, buf, want, &got, nullptr) || got == 0) break;
            out.append(buf, got);
        }
        DWORD wr2 = WaitForSingleObject(pi.hProcess, 50);
        if (wr2 == WAIT_OBJECT_0) {
            while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
                DWORD got = 0;
                DWORD want = std::min<DWORD>(avail, sizeof(buf));
                if (!ReadFile(rd, buf, want, &got, nullptr) || got == 0) break;
                out.append(buf, got);
            }
            break;
        }
        if (GetTickCount() - start > timeoutMs) { TerminateProcess(pi.hProcess, 1); break; }
    }
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(rd);
    return out;
}

std::wstring ParseFunnelUrl(const std::string& out) {
    std::wstring w = W(out);
    size_t p = w.find(L"https://");
    if (p == std::wstring::npos) return L"";
    size_t e = p;
    while (e < w.size() && !iswspace(w[e]) && w[e] != L'"' && w[e] != L'\'' && w[e] != L'>' && w[e] != L')') ++e;
    std::wstring url = w.substr(p, e - p);
    while (!url.empty() && (url.back() == L'/' || url.back() == L'.')) url.pop_back();
    return url;
}

bool TailscaleFunnelStart(int port, std::wstring& url, std::wstring& err) {
    std::wstring exe = FindTailscaleExe();
    if (exe.empty()) { err = L"未检测到 Tailscale，请先安装并登录"; return false; }
    DWORD code = 0;
    std::string out = RunCapture(L"\"" + exe + L"\" funnel --bg " + std::to_wstring(port), 20000, code);
    DebugLog(L"share: funnel start exit=%lu out=%s", (unsigned long)code, W(out).c_str());
    url = ParseFunnelUrl(out);
    if (url.empty()) {
        DWORD code2 = 0;
        std::string st = RunCapture(L"\"" + exe + L"\" funnel status", 15000, code2);
        url = ParseFunnelUrl(st);
        if (url.empty()) {
            std::wstring text = W(out + "\n" + st);
            if (text.find(L"not logged in") != std::wstring::npos || text.find(L"logged out") != std::wstring::npos)
                err = L"Tailscale 未登录，请先运行 tailscale up";
            else if (text.find(L"Funnel") != std::wstring::npos)
                err = L"Funnel 未启用或需要管理员在 Tailscale 后台开启；输出：" + text.substr(0, 160);
            else
                err = L"未能获取 Funnel 地址：" + text.substr(0, 160);
            return false;
        }
    }
    app.share.publicUrl = url;
    SaveConfig();
    return true;
}

void TailscaleFunnelStop() {
    std::wstring exe = FindTailscaleExe();
    if (exe.empty()) return;
    DWORD code = 0;
    std::string out = RunCapture(L"\"" + exe + L"\" funnel reset", 12000, code);
    DebugLog(L"share: funnel reset exit=%lu out=%s", (unsigned long)code, W(out).c_str());
    app.share.publicUrl.clear();
    SaveConfig();
}

std::wstring TailscaleStateText() {
    std::wstring exe = FindTailscaleExe();
    if (exe.empty()) return L"未安装 Tailscale";
    if (!app.share.publicUrl.empty()) return L"Funnel：" + app.share.publicUrl;
    return L"已安装，未开启";
}

void CopyText(const std::wstring& text) {
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h) {
        void* p = GlobalLock(h);
        if (p) { memcpy(p, text.c_str(), bytes); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
        else GlobalFree(h);
    }
    CloseClipboard();
}

std::wstring LocalShareUrl() {
    return Fmt(L"http://127.0.0.1:%d/busy?t=%s", app.share.port, app.share.token.c_str());
}

std::wstring PublicShareUrl() {
    std::wstring base = app.share.publicUrl;
    while (!base.empty() && (base.back() == L'/' || base.back() == L' ')) base.pop_back();
    if (base.empty()) return LocalShareUrl();
    return base + Fmt(L"/busy?t=%s", app.share.token.c_str());
}

void ResetToken() {
    app.share.token = NewToken();
    SaveConfig();
    Lock(); g_token = app.share.token; Unlock();
    ShowToast(L"分享链接已重置，旧链接立即失效");
}

void ToggleShare() {
    app.share.enabled = !app.share.enabled;
    SaveConfig();
    if (app.share.enabled) {
        StartServer();
        if (g_serverOk) ShowToast(L"忙闲分享已开启（本机端口 " + std::to_wstring(app.share.port) + L"）");
        else ShowToast(L"分享启动失败：端口被占用或无网络权限");
        if (app.share.useTailscale && g_serverOk) {
            std::wstring url, err;
            if (TailscaleFunnelStart(app.share.port, url, err)) ShowToast(L"Tailscale Funnel：" + url, 5000);
            else ShowToast(L"Tailscale 隧道失败：" + err, 6000);
        }
    } else {
        if (app.share.useTailscale) TailscaleFunnelStop();
        StopServer();
        ShowToast(L"忙闲分享已关闭");
    }
    UIInvalidate();
}

} // namespace

// ================= 对外接口 =================
void ShareInit() {
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
    LoadConfig();
    if (app.share.token.empty()) { app.share.token = NewToken(); SaveConfig(); }
    g_token = app.share.token;
    g_allowReq = app.share.allowRequests;
    g_port = app.share.port;
    if (app.share.enabled) StartServer();
}

void ShareShutdown() {
    StopServer();
    if (g_csInit) { DeleteCriticalSection(&g_cs); g_csInit = false; }
}

void ShareTick() {
    static std::wstring lastUrl;
    if (app.share.publicUrl != lastUrl) {
        lastUrl = app.share.publicUrl;
        SaveConfig();
    }
    if (!app.share.enabled) return;
    std::string js = BuildStateJson();
    Lock();
    g_stateJson = js;
    g_allowReq = app.share.allowRequests;
    g_token = app.share.token;
    Unlock();
    bool notify = false;
    Lock();
    if (g_newReq > 0) { g_newReq = 0; notify = true; }
    Unlock();
    if (notify) {
        ShowToast(L"有人发来了预约/留言，打开「忙闲分享」查看", 4000);
        if (app.overlay == OV_SHARE) UIInvalidate();
    }
}

void ShareProbeStart(int port) {
    app.share.enabled = true;
    app.share.allowRequests = true;
    app.share.port = port;
    if (app.share.token.empty()) app.share.token = NewToken();
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
    g_token = app.share.token;
    g_allowReq = true;
    StartServer();
    DebugLog(L"probe-share: port=%d token=%s ok=%d", port, app.share.token.c_str(), g_serverOk ? 1 : 0);
}

void OpenSharePanel() {
    app.overlay = OV_SHARE;
    UIInvalidate();
}

void DrawShareOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    UI::Dim(g, W, H, 150);
    float pw = S(760), ph = S(740);
    float x = (W - pw) / 2, y = (H - ph) / 2;
    RectF panel(x, y, pw, ph);
    Shadow(g, panel, S(18), 10, 30);
    FillRoundRect(g, panel, S(16), th.card);
    StrokeRoundRect(g, panel, S(16), th.border, S(1));

    Txt(g, L"忙闲分享", S(16), true, th.text, RectF(x + S(24), y + S(18), S(200), S(26)));
    Txt(g, g_serverOk ? L"服务运行中" : (app.share.enabled ? L"服务未启动" : L"未开启"),
        S(9.5f), false, g_serverOk ? th.success : th.dim2,
        RectF(x + pw - S(180), y + S(22), S(156), S(18)), ALIGN_RIGHT);

    float ty = y + S(56);
    UI::CheckBox(g, RectF(x + S(24), ty, S(240), S(24)), CMD_SHARE_TOGGLE, L"开启忙闲分享", app.share.enabled, []() { ToggleShare(); });
    UI::CheckBox(g, RectF(x + S(300), ty, S(260), S(24)), CMD_SHARE_ALLOW, L"允许预约/留言", app.share.allowRequests, []() {
        app.share.allowRequests = !app.share.allowRequests;
        SaveConfig();
        g_allowReq = app.share.allowRequests;
        UIInvalidate();
    });
    ty += S(36);

    std::wstring tsExe = FindTailscaleExe();
    if (!tsExe.empty()) {
        UI::CheckBox(g, RectF(x + S(24), ty, S(300), S(24)), CMD_SHARE_TAILSCALE, L"自动用 Tailscale Funnel 分享", app.share.useTailscale, []() {
            app.share.useTailscale = !app.share.useTailscale;
            SaveConfig();
            if (app.share.enabled && app.share.useTailscale) {
                std::wstring url, err;
                if (TailscaleFunnelStart(app.share.port, url, err)) ShowToast(L"Funnel：" + url, 5000);
                else ShowToast(L"Tailscale：" + err, 6000);
            } else if (!app.share.useTailscale && app.share.enabled) {
                TailscaleFunnelStop();
                ShowToast(L"Tailscale Funnel 已关闭");
            }
            UIInvalidate();
        });
        Txt(g, TailscaleStateText(), S(9), false, th.dim2,
            RectF(x + S(340), ty + S(3), pw - S(364), S(18)), ALIGN_LEFT, false, true);
    } else {
        Txt(g, L"未检测到 Tailscale：安装并登录后可一键开 Funnel（推荐）",
            S(9.5f), false, th.dim2, RectF(x + S(24), ty + S(3), pw - S(220), S(18)), ALIGN_LEFT, false, true);
        UI::Btn(g, RectF(x + pw - S(176), ty - S(1), S(152), S(26)), CMD_SHARE_DOWNLOAD, L"下载 Tailscale", 1, ICO_ARROW_RIGHT, []() {
            ShellExecuteW(nullptr, L"open", L"https://tailscale.com/download/windows", nullptr, nullptr, SW_SHOWNORMAL);
        });
    }
    ty += S(38);

    Txt(g, L"本机地址（开启后用浏览器可直接测试；公网需隧道转发 127.0.0.1:" + std::to_wstring(app.share.port) + L"）",
        S(9), false, th.dim2, RectF(x + S(24), ty, pw - S(48), S(16)));
    ty += S(18);
    Txt(g, LocalShareUrl(), S(9.5f), false, th.text, RectF(x + S(24), ty, pw - S(48), S(18)), ALIGN_LEFT, false, true);
    ty += S(24);
    Txt(g, L"公网地址（Tailscale Funnel 会自动填写；其他通道手动填）", S(9), false, th.dim2, RectF(x + S(24), ty, pw - S(48), S(16)));
    RectF urlBox(x + S(24), ty + S(18), pw - S(48), S(34));
    FillRoundRect(g, urlBox, S(8), th.panel2);
    StrokeRoundRect(g, urlBox, S(8), th.borderSoft, S(1));
    UIEdit(9710, RectF(urlBox.X + S(10), urlBox.Y + S(6), urlBox.Width - S(20), S(22)), &app.share.publicUrl, false, S(10.5f));
    ty += S(62);

    UI::Btn(g, RectF(x + S(24), ty, S(150), S(30)), CMD_SHARE_COPY, L"复制链接", 1, ICO_COPY, []() {
        if (!app.share.enabled) { ShowToast(L"请先勾选「开启忙闲分享」", 3500); return; }
        CopyText(PublicShareUrl());
        ShowToast(L"链接已复制：" + PublicShareUrl(), 4000);
    });
    UI::Btn(g, RectF(x + S(184), ty, S(150), S(30)), CMD_SHARE_OPEN_LOCAL, L"打开本机页面", 1, ICO_CAL, []() {
        if (!app.share.enabled) { ShowToast(L"请先勾选「开启忙闲分享」", 3500); return; }
        ShellExecuteW(nullptr, L"open", LocalShareUrl().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    });
    UI::Btn(g, RectF(x + S(344), ty, S(150), S(30)), CMD_SHARE_RESET, L"重置链接", 1, ICO_REFRESH, []() { ResetToken(); });
    UI::Btn(g, RectF(x + S(504), ty, S(150), S(30)), CMD_SHARE_REQ_CLEAR, L"清空请求", 1, ICO_TRASH, []() {
        WriteFileBytes(ShareDirFile(L"share-requests.json"), "[]");
        ShowToast(L"已清空预约/留言");
        UIInvalidate();
    });
    ty += S(44);

    Txt(g, L"最近预约/留言（预约可一键生成草稿，留言点已读）", S(10), true, th.dim2, RectF(x + S(24), ty, pw - S(48), S(16)));
    ty += S(20);
    auto reqs = LoadRequests();
    int shown = (int)std::min<size_t>(reqs.size(), 3);
    if (shown == 0) {
        Txt(g, L"还没有预约或留言。", S(9.5f), false, th.dim2, RectF(x + S(24), ty, pw - S(48), S(18)));
        ty += S(20);
    } else {
        for (int i = (int)reqs.size() - 1; i >= (int)reqs.size() - shown; --i) {
            auto& r = reqs[i];
            bool isAppt = r.kind == L"appointment";
            RectF row(x + S(20), ty, pw - S(40), S(42));
            FillRoundRect(g, row, S(8), WithAlpha(th.panel2, 120));
            std::wstring kind = isAppt ? L"预约" : L"留言";
            std::wstring line = kind + L" · " + r.time + L" · " + (r.name.empty() ? L"匿名" : r.name);
            if (!r.prefer.empty()) line += L" · 想约 " + r.prefer;
            if (!r.minutes.empty()) line += L"（" + r.minutes + L" 分钟）";
            Txt(g, line, S(8.5f), true, th.text,
                RectF(row.X + S(10), row.Y + S(3), row.Width - S(132), S(16)), ALIGN_LEFT, false, true);
            if (!r.message.empty()) {
                Txt(g, r.message, S(8), false, th.dim,
                    RectF(row.X + S(10), row.Y + S(21), row.Width - S(132), S(14)), ALIGN_LEFT, false, true);
            }
            UI::Btn(g, RectF(row.X + row.Width - S(116), row.Y + S(9), S(52), S(24)), 0,
                    isAppt ? L"接受" : L"已读", 2, -1, [i]() { AcceptShareRequest((size_t)i); });
            UI::Btn(g, RectF(row.X + row.Width - S(60), row.Y + S(9), S(52), S(24)), 0,
                    L"忽略", 2, -1, [i]() { IgnoreShareRequest((size_t)i); });
            ty += S(46);
        }
    }

    ty = std::max(ty, y + ph - S(92));
    ty = std::max(ty, y + ph - S(92));
    Txt(g, L"只对外公开忙/闲，不包含日程标题、地点、备注。页面每 10 秒刷新；电脑休眠/断网时不可访问。",
        S(8.5f), false, th.dim2, RectF(x + S(24), ty, pw - S(48), S(16)), ALIGN_LEFT, false, true);
    ty += S(17);
    Txt(g, L"Tailscale 未登录时先运行 tailscale up；未启用 Funnel 时按提示到 Tailscale 后台开启。",
        S(8.5f), false, th.dim2, RectF(x + S(24), ty, pw - S(48), S(16)), ALIGN_LEFT, false, true);

    UI::Btn(g, RectF(x + pw - S(112), y + ph - S(52), S(88), S(32)), CMD_SHARE_CLOSE, L"关闭", 1, -1, []() {
        SaveConfig();
        app.overlay = OV_NONE;
        UIInvalidate();
    });
}
