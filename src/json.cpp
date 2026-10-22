// json.cpp
#include "json.h"
#include "util.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

const JValue& JValue::Get(const std::wstring& k) const {
    static const JValue nul;
    if (type != OBJECT) return nul;
    auto it = obj.find(k);
    return it == obj.end() ? nul : it->second;
}
JValue& JValue::Set(const std::wstring& k) {
    type = OBJECT;
    return obj[k];
}

namespace {

struct Parser {
    const std::wstring& s;
    size_t i = 0;
    int depth = 0;
    std::wstring err;
    explicit Parser(const std::wstring& src) : s(src) {}

    bool Fail(const wchar_t* msg) {
        if (err.empty()) err = Fmt(L"%s @%d", msg, (int)i);
        return false;
    }
    void Ws() {
        while (i < s.size() && (s[i] == L' ' || s[i] == L'\t' || s[i] == L'\n' || s[i] == L'\r')) ++i;
    }
    bool ParseValue(JValue& out) {
        if (++depth > 200) return Fail(L"nest too deep");
        Ws();
        if (i >= s.size()) return Fail(L"unexpected end");
        wchar_t c = s[i];
        bool ok = true;
        if (c == L'{') ok = ParseObj(out);
        else if (c == L'[') ok = ParseArr(out);
        else if (c == L'"') { out.type = JValue::STRING; ok = ParseString(out.str); }
        else if (c == L't') { ok = Match(L"true"); out.type = JValue::BOOL; out.b = true; }
        else if (c == L'f') { ok = Match(L"false"); out.type = JValue::BOOL; out.b = false; }
        else if (c == L'n') { ok = Match(L"null"); out.type = JValue::NUL; }
        else if (c == L'-' || (c >= L'0' && c <= L'9')) ok = ParseNumber(out);
        else return Fail(L"bad token");
        --depth;
        return ok;
    }
    bool Match(const wchar_t* lit) {
        size_t n = wcslen(lit);
        if (s.compare(i, n, lit) != 0) return Fail(L"bad literal");
        i += n;
        return true;
    }
    bool ParseObj(JValue& out) {
        out.type = JValue::OBJECT;
        ++i; // {
        Ws();
        if (i < s.size() && s[i] == L'}') { ++i; return true; }
        while (true) {
            Ws();
            if (i >= s.size() || s[i] != L'"') return Fail(L"expect key");
            std::wstring key;
            if (!ParseString(key)) return false;
            Ws();
            if (i >= s.size() || s[i] != L':') return Fail(L"expect ':'");
            ++i;
            JValue v;
            if (!ParseValue(v)) return false;
            out.obj[key] = std::move(v);
            Ws();
            if (i < s.size() && s[i] == L',') { ++i; continue; }
            if (i < s.size() && s[i] == L'}') { ++i; return true; }
            return Fail(L"expect ',' or '}'");
        }
    }
    bool ParseArr(JValue& out) {
        out.type = JValue::ARRAY;
        ++i;
        Ws();
        if (i < s.size() && s[i] == L']') { ++i; return true; }
        while (true) {
            JValue v;
            if (!ParseValue(v)) return false;
            out.arr.push_back(std::move(v));
            Ws();
            if (i < s.size() && s[i] == L',') { ++i; continue; }
            if (i < s.size() && s[i] == L']') { ++i; return true; }
            return Fail(L"expect ',' or ']'");
        }
    }
    bool ParseString(std::wstring& out) {
        ++i; // "
        out.clear();
        while (i < s.size()) {
            wchar_t c = s[i++];
            if (c == L'"') return true;
            if (c == L'\\') {
                if (i >= s.size()) return Fail(L"bad escape");
                wchar_t e = s[i++];
                switch (e) {
                case L'"': out += L'"'; break;
                case L'\\': out += L'\\'; break;
                case L'/': out += L'/'; break;
                case L'b': out += L'\b'; break;
                case L'f': out += L'\f'; break;
                case L'n': out += L'\n'; break;
                case L'r': out += L'\r'; break;
                case L't': out += L'\t'; break;
                case L'u': {
                    unsigned cp = 0;
                    if (!Hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (i + 1 < s.size() && s[i] == L'\\' && s[i + 1] == L'u') {
                            i += 2;
                            unsigned lo = 0;
                            if (!Hex4(lo)) return false;
                            if (lo >= 0xDC00 && lo <= 0xDFFF)
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                    }
                    if (cp <= 0xFFFF) out += (wchar_t)cp;
                    else {
                        cp -= 0x10000;
                        out += (wchar_t)(0xD800 + (cp >> 10));
                        out += (wchar_t)(0xDC00 + (cp & 0x3FF));
                    }
                    break;
                }
                default: return Fail(L"bad escape char");
                }
            } else {
                out += c;
            }
        }
        return Fail(L"unterminated string");
    }
    bool Hex4(unsigned& v) {
        if (i + 4 > s.size()) return Fail(L"bad \\u");
        v = 0;
        for (int k = 0; k < 4; ++k) {
            wchar_t c = s[i++];
            int d = 0;
            if (c >= L'0' && c <= L'9') d = c - L'0';
            else if (c >= L'a' && c <= L'f') d = c - L'a' + 10;
            else if (c >= L'A' && c <= L'F') d = c - L'A' + 10;
            else return Fail(L"bad hex");
            v = v * 16 + d;
        }
        return true;
    }
    bool ParseNumber(JValue& out) {
        size_t start = i;
        if (i < s.size() && s[i] == L'-') ++i;
        while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') ++i;
        if (i < s.size() && s[i] == L'.') { ++i; while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') ++i; }
        if (i < s.size() && (s[i] == L'e' || s[i] == L'E')) {
            ++i;
            if (i < s.size() && (s[i] == L'+' || s[i] == L'-')) ++i;
            while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') ++i;
        }
        if (i == start) return Fail(L"bad number");
        std::wstring t = s.substr(start, i - start);
        out.type = JValue::NUMBER;
        out.num = wcstod(t.c_str(), nullptr);
        return true;
    }
};

void EscapeTo(std::string& out, const std::wstring& s) {
    for (wchar_t c : s) {
        switch (c) {
        case L'"': out += "\\\""; break;
        case L'\\': out += "\\\\"; break;
        case L'\b': out += "\\b"; break;
        case L'\f': out += "\\f"; break;
        case L'\n': out += "\\n"; break;
        case L'\r': out += "\\r"; break;
        case L'\t': out += "\\t"; break;
        default:
            if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c); out += buf; }
            else out += WideToUtf8(std::wstring(1, c));
        }
    }
}

void DumpRec(const JValue& v, std::string& out, bool pretty, int indent) {
    auto nl = [&](int ind) {
        if (!pretty) return;
        out += "\n";
        out.append((size_t)ind * 2, ' ');
    };
    switch (v.type) {
    case JValue::NUL: out += "null"; break;
    case JValue::BOOL: out += v.b ? "true" : "false"; break;
    case JValue::NUMBER: {
        double d = v.num;
        if (d == std::floor(d) && std::fabs(d) < 1e15) {
            char buf[64]; snprintf(buf, sizeof(buf), "%.0f", d); out += buf;
        } else {
            char buf[64]; snprintf(buf, sizeof(buf), "%.10g", d); out += buf;
        }
        break;
    }
    case JValue::STRING: out += '"'; EscapeTo(out, v.str); out += '"'; break;
    case JValue::ARRAY: {
        out += '[';
        bool first = true;
        for (auto& e : v.arr) {
            if (!first) out += ',';
            first = false;
            nl(indent + 1);
            DumpRec(e, out, pretty, indent + 1);
        }
        if (!v.arr.empty()) nl(indent);
        out += ']';
        break;
    }
    case JValue::OBJECT: {
        out += '{';
        bool first = true;
        for (auto& kv : v.obj) {
            if (!first) out += ',';
            first = false;
            nl(indent + 1);
            out += '"'; EscapeTo(out, kv.first); out += "\":";
            if (pretty) out += ' ';
            DumpRec(kv.second, out, pretty, indent + 1);
        }
        if (!v.obj.empty()) nl(indent);
        out += '}';
        break;
    }
    }
}

} // namespace

JValue JsonParse(const std::string& utf8, std::wstring* err) {
    std::wstring ws = Utf8ToWide(utf8);
    Parser p(ws);
    JValue v;
    if (!p.ParseValue(v)) {
        if (err) *err = p.err;
        return JValue();
    }
    p.Ws();
    if (p.i != ws.size()) {
        if (err) *err = Fmt(L"trailing chars @%d", (int)p.i);
        return JValue();
    }
    if (err) err->clear();
    return v;
}

std::string JsonDump(const JValue& v, bool pretty) {
    std::string out;
    DumpRec(v, out, pretty, 0);
    return out;
}
