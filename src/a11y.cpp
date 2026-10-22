// a11y.cpp - 无障碍：最小 MSAA（IAccessible）实现，让读屏软件能识别主窗口与当前视图
#include "app.h"
#include <oleacc.h>
#include <oleauto.h>
#include <atomic>

namespace {

class CAcc : public IAccessible {
public:
    // ---------- IUnknown ----------
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_IDispatch || riid == IID_IAccessible) {
            *ppv = static_cast<IAccessible*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --m_ref;
        if (r == 0) delete this;
        return r;
    }

    // ---------- IDispatch ----------
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override {
        if (pctinfo) *pctinfo = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo** pptinfo) override {
        if (pptinfo) *pptinfo = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR* rgszNames, UINT cNames, LCID, DISPID* rgDispId) override {
        if (cNames == 1 && rgszNames && rgDispId && rgszNames[0]) {
            rgDispId[0] = DISPID_VALUE;
            return S_OK;
        }
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override {
        return E_NOTIMPL;
    }

    // ---------- IAccessible ----------
    HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** ppdispParent) override {
        if (ppdispParent) *ppdispParent = nullptr;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accChildCount(long* pcountChildren) override {
        if (pcountChildren) *pcountChildren = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accChild(VARIANT, IDispatch** ppdispChild) override {
        if (ppdispChild) *ppdispChild = nullptr;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accName(VARIANT varChild, BSTR* pszName) override {
        if (!pszName) return E_INVALIDARG;
        *pszName = nullptr;
        if (varChild.vt != VT_I4 || varChild.lVal != CHILDID_SELF) return E_INVALIDARG;
        static const wchar_t* views[] = { L"月视图", L"周视图", L"日视图", L"年视图", L"日程列表" };
        std::wstring name = L"星历";
        int v = app.view;
        if (v >= 0 && v < 5) {
            name += L" - ";
            name += views[v];
        }
        name += L"，";
        name += ISODate(app.now);
        *pszName = SysAllocString(name.c_str());
        return *pszName ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE get_accValue(VARIANT, BSTR* pszValue) override {
        if (pszValue) *pszValue = nullptr;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT varChild, BSTR* pszDescription) override {
        if (!pszDescription) return E_INVALIDARG;
        *pszDescription = nullptr;
        if (varChild.vt != VT_I4 || varChild.lVal != CHILDID_SELF) return E_INVALIDARG;
        std::wstring desc = app.store.events.empty()
            ? L"还没有日程"
            : Fmt(L"共 %d 条日程，%d 个标签", (int)app.store.events.size(), (int)app.store.tags.size());
        *pszDescription = SysAllocString(desc.c_str());
        return *pszDescription ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE get_accRole(VARIANT varChild, VARIANT* pvarRole) override {
        if (!pvarRole) return E_INVALIDARG;
        VariantInit(pvarRole);
        if (varChild.vt != VT_I4 || varChild.lVal != CHILDID_SELF) return E_INVALIDARG;
        pvarRole->vt = VT_I4;
        pvarRole->lVal = ROLE_SYSTEM_WINDOW;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accState(VARIANT varChild, VARIANT* pvarState) override {
        if (!pvarState) return E_INVALIDARG;
        VariantInit(pvarState);
        if (varChild.vt != VT_I4 || varChild.lVal != CHILDID_SELF) return E_INVALIDARG;
        pvarState->vt = VT_I4;
        pvarState->lVal = STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT, BSTR* pszHelp) override {
        if (pszHelp) *pszHelp = nullptr;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR*, VARIANT, long*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT, BSTR* pszKeyboardShortcut) override {
        if (pszKeyboardShortcut) *pszKeyboardShortcut = nullptr;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* pvarChild) override {
        if (!pvarChild) return E_INVALIDARG;
        VariantInit(pvarChild);
        pvarChild->vt = VT_I4;
        pvarChild->lVal = CHILDID_SELF;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* pvarChildren) override {
        if (!pvarChildren) return E_INVALIDARG;
        VariantInit(pvarChildren);
        pvarChildren->vt = VT_EMPTY;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT, BSTR* pszDefaultAction) override {
        if (pszDefaultAction) *pszDefaultAction = nullptr;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accSelect(long, VARIANT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE accLocation(long* pxLeft, long* pyTop, long* pcxWidth, long* pcyHeight, VARIANT varChild) override {
        if (varChild.vt != VT_I4 || varChild.lVal != CHILDID_SELF) return E_INVALIDARG;
        RECT rc{};
        if (m_hwnd && GetWindowRect(m_hwnd, &rc)) {
            if (pxLeft) *pxLeft = rc.left;
            if (pyTop) *pyTop = rc.top;
            if (pcxWidth) *pcxWidth = rc.right - rc.left;
            if (pcyHeight) *pcyHeight = rc.bottom - rc.top;
            return S_OK;
        }
        return E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE accNavigate(long, VARIANT, VARIANT* pvarEndUpAt) override {
        if (pvarEndUpAt) {
            VariantInit(pvarEndUpAt);
            pvarEndUpAt->vt = VT_EMPTY;
        }
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accHitTest(long, long, VARIANT* pvarChild) override {
        if (!pvarChild) return E_INVALIDARG;
        VariantInit(pvarChild);
        pvarChild->vt = VT_I4;
        pvarChild->lVal = CHILDID_SELF;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE put_accName(VARIANT, BSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE put_accValue(VARIANT, BSTR) override { return E_NOTIMPL; }

    void SetHwnd(HWND h) { m_hwnd = h; }

private:
    std::atomic<long> m_ref{ 1 };
    HWND m_hwnd = nullptr;
};

CAcc* g_acc = nullptr;
bool g_inited = false;

}  // namespace

void A11yInit() {
    if (g_inited) return;
    g_inited = true;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);   // 失败（已初始化/模式不同）可忽略
    g_acc = new CAcc();
}

LRESULT A11yHandleGetObject(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    if (lParam != OBJID_CLIENT) return -1;   // 交给 DefWindowProc
    A11yInit();
    g_acc->SetHwnd(hwnd);
    return LresultFromObject(IID_IAccessible, wParam, static_cast<IAccessible*>(g_acc));
}
