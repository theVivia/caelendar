// backup.cpp - 加密备份包（AES-256-GCM + PBKDF2-SHA256）
#include "app.h"
#include <bcrypt.h>
#include <commdlg.h>
#include <cstring>
#include <vector>

using namespace Gdiplus;
using namespace gfx;

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace {

#pragma pack(push, 1)
struct BackupHeader {
    char magic[8];        // XLBACK02
    uint32_t iterations;  // PBKDF2 迭代次数
    uint8_t salt[16];
    uint8_t nonce[12];
    uint64_t plainLen;
    uint8_t tag[16];
};
#pragma pack(pop)

constexpr size_t kAadLen = 48;   // magic..plainLen（不含 tag）
constexpr size_t kHeaderSize = sizeof(BackupHeader);
constexpr uint32_t kIterations = 200000;

std::string Utf8Password(const std::wstring& pwd) {
    return WideToUtf8(pwd);
}

std::wstring NtErr(const wchar_t* what, NTSTATUS st) {
    return Fmt(L"%s失败（0x%08X）", what, (unsigned)st);
}

bool DeriveKey(const std::wstring& password, const uint8_t salt[16], uint32_t iterations,
               uint8_t key[32], std::wstring& err) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!NT_SUCCESS(st)) { err = NtErr(L"打开 SHA256", st); return false; }
    std::string pwd = Utf8Password(password);
    st = BCryptDeriveKeyPBKDF2(hAlg, (PUCHAR)pwd.data(), (ULONG)pwd.size(), (PUCHAR)salt, 16,
                               iterations, key, 32, 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!NT_SUCCESS(st)) { err = NtErr(L"PBKDF2 派生密钥", st); return false; }
    return true;
}

bool AesGcm(bool encrypt, const uint8_t key[32], BackupHeader& h, const uint8_t* in, size_t inLen,
            uint8_t* out, std::wstring& err) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!NT_SUCCESS(st)) { err = NtErr(L"打开 AES", st); return false; }
    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(st)) { BCryptCloseAlgorithmProvider(hAlg, 0); err = NtErr(L"设置 GCM", st); return false; }
    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key, 32, 0);
    if (!NT_SUCCESS(st)) { BCryptCloseAlgorithmProvider(hAlg, 0); err = NtErr(L"创建密钥", st); return false; }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ai;
    BCRYPT_INIT_AUTH_MODE_INFO(ai);
    ai.pbNonce = (PUCHAR)h.nonce;
    ai.cbNonce = 12;
    ai.pbAuthData = (PUCHAR)&h;
    ai.cbAuthData = (ULONG)kAadLen;
    ai.pbTag = (PUCHAR)h.tag;
    ai.cbTag = 16;

    ULONG outLen = 0;
    if (encrypt)
        st = BCryptEncrypt(hKey, (PUCHAR)in, (ULONG)inLen, &ai, nullptr, 0, out, (ULONG)inLen, &outLen, 0);
    else
        st = BCryptDecrypt(hKey, (PUCHAR)in, (ULONG)inLen, &ai, nullptr, 0, out, (ULONG)inLen, &outLen, 0);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!NT_SUCCESS(st)) {
        err = encrypt ? NtErr(L"AES 加密", st) : L"密码错误或备份文件损坏";
        return false;
    }
    if (outLen != inLen) { err = L"数据长度校验失败"; return false; }
    return true;
}

bool ReadWhole(const std::wstring& path, std::string& out) {
    bool ok = false;
    out = ReadFileBytes(path, &ok);
    return ok;
}

bool SaveDialog(std::wstring& path) {
    wchar_t file[1024] = { 0 };
    std::wstring name = Fmt(L"星历备份-%d%02d%02d.xlbak", app.now.y, app.now.mo, app.now.d);
    wcsncpy_s(file, name.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"CAELENDAR 加密备份\0*.xlbak\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrDefExt = L"xlbak";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    path = file;
    if (path.size() < 6 || path.substr(path.size() - 6) != L".xlbak") path += L".xlbak";
    return true;
}

bool OpenFileDialog(std::wstring& path) {
    wchar_t file[1024] = { 0 };
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"CAELENDAR 加密备份\0*.xlbak\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;
    path = file;
    return true;
}

void Txt(Graphics& g, const std::wstring& s, float px, bool bold, Color c, RectF r,
         int align = ALIGN_LEFT, bool display = false, bool ellipsis = true) {
    Text(g, s, px, bold, c, r, align, display, ellipsis);
}

void Frame(Graphics& g, float W, float H, float pw, float ph, float& px, float& py) {
    const Theme& th = TH();
    pw = std::min(pw, W - S(40));
    ph = std::min(ph, H - S(30));
    px = (W - pw) / 2;
    py = (H - ph) / 2;
    RectF r(px, py, pw, ph);
    Shadow(g, r, S(18), 8, 26);
    FillRoundRect(g, r, S(18), th.card);
    StrokeRoundRect(g, r, S(18), WithAlpha(th.border, 255), S(1));
    FillVGradient(g, RectF(px, py, pw, S(60)), WithAlpha(th.accent, 10), WithAlpha(th.accent, 0));
}

}  // namespace

bool EncryptBackupFile(const std::wstring& path, const std::string& plain, const std::wstring& password,
                       std::wstring& err) {
    if (password.empty()) { err = L"密码不能为空"; return false; }
    if (plain.empty()) { err = L"没有可备份的数据"; return false; }
    BackupHeader h{};
    memcpy(h.magic, "XLBACK02", 8);
    h.iterations = kIterations;
    h.plainLen = plain.size();
    BCryptGenRandom(nullptr, h.salt, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    BCryptGenRandom(nullptr, h.nonce, 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    uint8_t key[32] = { 0 };
    if (!DeriveKey(password, h.salt, h.iterations, key, err)) return false;
    std::vector<uint8_t> cipher(plain.size());
    if (!AesGcm(true, key, h, (const uint8_t*)plain.data(), plain.size(), cipher.data(), err)) return false;
    std::vector<uint8_t> out(kHeaderSize + cipher.size());
    memcpy(out.data(), &h, kHeaderSize);
    memcpy(out.data() + kHeaderSize, cipher.data(), cipher.size());
    if (!WriteFileBytesAtomic(path, std::string((const char*)out.data(), out.size()))) {
        err = L"写入备份文件失败";
        return false;
    }
    return true;
}

bool DecryptBackupFile(const std::wstring& path, const std::wstring& password, std::string& plain,
                       std::wstring& err) {
    std::string raw;
    if (!ReadWhole(path, raw)) { err = L"无法读取备份文件"; return false; }
    if (raw.size() < kHeaderSize) { err = L"不是有效的星历备份文件"; return false; }
    BackupHeader h{};
    memcpy(&h, raw.data(), kHeaderSize);
    if (memcmp(h.magic, "XLBACK02", 8) != 0) { err = L"不是有效的星历备份文件"; return false; }
    if (h.iterations < 10000 || h.iterations > 5000000) { err = L"备份文件参数异常"; return false; }
    if (h.plainLen > 64u * 1024 * 1024) { err = L"备份数据过大"; return false; }
    if (raw.size() != kHeaderSize + (size_t)h.plainLen) { err = L"备份文件长度不匹配"; return false; }
    if (password.empty()) { err = L"密码不能为空"; return false; }
    uint8_t key[32] = { 0 };
    if (!DeriveKey(password, h.salt, h.iterations, key, err)) return false;
    std::vector<uint8_t> out((size_t)h.plainLen);
    if (!AesGcm(false, key, h, (const uint8_t*)raw.data() + kHeaderSize, (size_t)h.plainLen, out.data(), err))
        return false;
    plain.assign((const char*)out.data(), out.size());
    return true;
}

void OpenBackupExport() {
    app.backupImport = false;
    app.backupPassword.clear();
    app.backupPath.clear();
    app.backupError.clear();
    app.overlay = OV_BACKUP;
    UIInvalidate();
}

void OpenBackupImport() {
    app.backupImport = true;
    app.backupPassword.clear();
    app.backupError.clear();
    app.backupPath.clear();
    app.overlay = OV_BACKUP;
    UIInvalidate();
}

void DrawBackupOverlay(Graphics& g, float W, float H) {
    const Theme& th = TH();
    float pw = S(600), ph = S(380);
    float x, y;
    Frame(g, W, H, pw, ph, x, y);
    bool importMode = app.backupImport;
    Txt(g, importMode ? L"导入加密备份" : L"导出加密备份", S(18), true, th.text,
        RectF(x + S(24), y + S(16), S(300), S(28)));
    UI::IconBtn(g, RectF(x + pw - S(44), y + S(14), S(28), S(28)), CMD_BACKUP_CANCEL, ICO_CLOSE, 13, []() {
        app.overlay = OV_SETTINGS;
        UIInvalidate();
    });
    Txt(g, importMode ? L"选择 .xlbak 文件并输入导出时设置的密码，确认后导入。"
                      : L"备份包含全部日程/标签/设置，使用 AES-256-GCM 加密；密码无法找回。",
        S(9.5f), false, th.dim2, RectF(x + S(24), y + S(48), pw - S(48), S(32)), ALIGN_LEFT, false, true);

    Txt(g, L"备份文件", S(9.5f), false, th.dim2, RectF(x + S(24), y + S(90), S(90), S(18)));
    RectF fileBox(x + S(24), y + S(110), pw - S(48), S(34));
    FillRoundRect(g, fileBox, S(8), WithAlpha(th.panel2, 190));
    StrokeRoundRect(g, fileBox, S(8), th.borderSoft, S(1));
    Txt(g, app.backupPath.empty() ? L"（尚未选择）" : app.backupPath, S(9.5f), false,
        app.backupPath.empty() ? th.dim2 : th.text, RectF(fileBox.X + S(10), fileBox.Y, fileBox.Width - S(20), fileBox.Height), ALIGN_VCENTER, false, true);
    UI::Btn(g, RectF(x + S(24), y + S(152), S(130), S(30)), CMD_BACKUP_PICK_FILE,
            importMode ? L"选择备份文件" : L"选择保存位置", 1, ICO_FOLDER, []() {
                std::wstring path;
                if (app.backupImport) {
                    if (!OpenFileDialog(path)) return;
                } else {
                    if (!SaveDialog(path)) return;
                }
                app.backupPath = path;
                UIInvalidate();
            });

    Txt(g, L"密码", S(9.5f), false, th.dim2, RectF(x + S(24), y + S(198), S(90), S(18)));
    RectF passBox(x + S(24), y + S(218), pw - S(48), S(36));
    FillRoundRect(g, passBox, S(8), WithAlpha(th.panel2, 190));
    StrokeRoundRect(g, passBox, S(8), th.borderSoft, S(1));
    UIEdit(9701, RectF(passBox.X + S(10), passBox.Y + S(8), passBox.Width - S(20), S(20)),
           &app.backupPassword, false, S(12));
    if (!app.backupError.empty())
        Txt(g, app.backupError, S(9), false, th.danger, RectF(x + S(24), y + S(260), pw - S(48), S(18)));
    UI::Btn(g, RectF(x + pw - S(24) - S(90) - S(110) - S(10), y + ph - S(52), S(110), S(34)),
            CMD_BACKUP_CANCEL, L"取消", 1, -1, []() {
                app.overlay = OV_SETTINGS;
                UIInvalidate();
            });
    UI::Btn(g, RectF(x + pw - S(24) - S(90), y + ph - S(52), S(90), S(34)),
            CMD_BACKUP_CONFIRM, importMode ? L"导入" : L"导出", 0, ICO_CHECK, []() {
                if (!app.backupImport) {
                    if (app.backupPath.empty()) {
                        std::wstring path;
                        if (!SaveDialog(path)) return;
                        app.backupPath = path;
                    }
                    if (app.backupPassword.empty()) { app.backupError = L"请输入密码"; UIInvalidate(); return; }
                    bool ok = false;
                    std::string plain = ReadFileBytes(GetCalendarPath(), &ok);
                    if (!ok || plain.empty()) { app.backupError = L"当前数据文件为空或无法读取"; UIInvalidate(); return; }
                    std::wstring err;
                    if (!EncryptBackupFile(app.backupPath, plain, app.backupPassword, err)) {
                        app.backupError = err;
                        UIInvalidate();
                        return;
                    }
                    DebugLog(L"导出加密备份: %s", app.backupPath.c_str());
                    app.overlay = OV_NONE;
                    ShowToast(L"已导出加密备份", 3500);
                    return;
                }
                if (app.backupPath.empty()) { app.backupError = L"请先选择备份文件"; UIInvalidate(); return; }
                if (app.backupPassword.empty()) { app.backupError = L"请输入密码"; UIInvalidate(); return; }
                std::string plain;
                std::wstring err;
                if (!DecryptBackupFile(app.backupPath, app.backupPassword, plain, err)) {
                    app.backupError = err;
                    UIInvalidate();
                    return;
                }
                Store tmp;
                std::wstring jerr;
                if (!tmp.FromJsonStr(plain, &jerr)) {
                    app.backupError = L"解密成功但数据格式无效：" + jerr;
                    UIInvalidate();
                    return;
                }
                int n = (int)tmp.events.size();
                OpenConfirm(L"导入加密备份",
                            Fmt(L"备份包含 %d 条日程。导入会覆盖当前数据，导入前会自动备份当前数据。", n),
                            [plain]() {
                                EnsureDir(PathJoin(GetDataDir(), L"backup"));
                                std::wstring pre = PathJoin(GetDataDir(), L"backup/manual-before-import-" +
                                                            ISODate(app.now) + L"-" + Fmt(L"%02d%02d", app.now.h, app.now.mi) + L".json");
                                CopyFileSimple(GetCalendarPath(), pre);
                                if (!WriteFileBytesAtomic(GetCalendarPath(), plain)) {
                                    ShowToast(L"写入导入数据失败");
                                    return;
                                }
                                Store tmp;
                                std::wstring err2;
                                if (!tmp.FromJsonStr(plain, &err2)) { ShowToast(L"导入数据解析失败"); return; }
                                app.store = tmp;
                                ApplyThemeFromSettings();
                                ApplyFontSettings();
                                InvalidateOccs();
                                app.selected.clear();
                                app.overlay = OV_NONE;
                                ShowToast(L"已导入加密备份", 3500);
                                UIInvalidate();
                            }, L"导入", L"取消");
            });
}
