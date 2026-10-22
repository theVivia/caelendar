// json.h - 极简 JSON（宽字符内部表示，UTF-8 存取）
#pragma once
#include <string>
#include <vector>
#include <map>

class JValue;
using JObj = std::map<std::wstring, JValue>;
using JArr = std::vector<JValue>;

class JValue {
public:
    enum Type { NUL = 0, BOOL, NUMBER, STRING, ARRAY, OBJECT };
    Type type = NUL;
    bool b = false;
    double num = 0;
    std::wstring str;
    JArr arr;
    JObj obj;

    JValue() = default;
    JValue(bool v) : type(BOOL), b(v) {}
    JValue(double v) : type(NUMBER), num(v) {}
    JValue(int v) : type(NUMBER), num((double)v) {}
    JValue(const wchar_t* v) : type(STRING), str(v) {}
    JValue(const std::wstring& v) : type(STRING), str(v) {}

    static JValue Arr() { JValue v; v.type = ARRAY; return v; }
    static JValue Obj() { JValue v; v.type = OBJECT; return v; }
    static JValue Str(const std::wstring& s) { return JValue(s); }
    static JValue Num(double d) { return JValue(d); }
    static JValue Bool(bool x) { return JValue(x); }

    bool IsNull() const { return type == NUL; }
    bool IsArr() const { return type == ARRAY; }
    bool IsObj() const { return type == OBJECT; }
    bool IsStr() const { return type == STRING; }
    bool IsNum() const { return type == NUMBER; }
    bool IsBool() const { return type == BOOL; }

    int AsInt(int def = 0) const { return type == NUMBER ? (int)num : (type == BOOL ? (b ? 1 : 0) : def); }
    double AsNum(double def = 0) const { return type == NUMBER ? num : (type == BOOL ? (b ? 1.0 : 0.0) : def); }
    bool AsBool(bool def = false) const { return type == BOOL ? b : (type == NUMBER ? num != 0 : def); }
    std::wstring AsStr(const std::wstring& def = L"") const { return type == STRING ? str : def; }

    void Push(const JValue& v) { type = ARRAY; arr.push_back(v); }
    bool Has(const std::wstring& k) const { return type == OBJECT && obj.find(k) != obj.end(); }
    const JValue& Get(const std::wstring& k) const;
    JValue& Set(const std::wstring& k);
    size_t Size() const { return type == ARRAY ? arr.size() : (type == OBJECT ? obj.size() : 0); }
};

// 解析（失败时返回 null 并写 err）
JValue JsonParse(const std::string& utf8, std::wstring* err = nullptr);
// 序列化
std::string JsonDump(const JValue& v, bool pretty = true);
