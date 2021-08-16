#include "pch.h"
#include "Helpers.h"
#include "AppSettings.h"

bool Helpers::SaveWindowPosition(HWND hWnd, PCWSTR name) {
    CRect rc;
    ::GetWindowRect(hWnd, &rc);
    AppSettings::Get().Set(name, rc);
    return true;
}

bool Helpers::RestoreWindowPosition(HWND hWnd, PCWSTR name) {
    auto rc = AppSettings::Get().GetBinary<CRect>(name);
    if (rc) {
        ::SetWindowPos(hWnd, nullptr, rc->left, rc->top, rc->Width(), rc->Height(), SWP_NOREPOSITION);
        return true;
    }
    return false;
}

CString Helpers::ToBinary(ULONGLONG value) {
    CString svalue;

    while (value) {
        svalue = ((value & 1) ? L"1" : L"0") + svalue;
        value >>= 1;
    }
    if (svalue.IsEmpty())
        svalue = L"0";
    return svalue;
}

PCWSTR Helpers::GetSystemDirectory() {
    static WCHAR dir[MAX_PATH];
    if (dir[0] == 0)
        ::GetSystemDirectory(dir, _countof(dir));

    return dir;
}

PCWSTR Helpers::GetWindowsDirectory() {
    static WCHAR dir[MAX_PATH];
    if (dir[0] == 0)
        ::GetWindowsDirectory(dir, _countof(dir));

    return dir;
}

COLORREF Helpers::ParseColor(const CString& text) {
    int index = 0;
    CString token;
    COLORREF color = 0;
    int shift = 0;
    while (!(token = text.Tokenize(L" ", index)).IsEmpty()) {
        if (shift > 24)
            return CLR_INVALID;
        int n = _wtoi(token);
        if (n < 0 || n > 255)
            return CLR_INVALID;
        color |= (n << shift);
        shift += 8;
    }
    return color;
}
