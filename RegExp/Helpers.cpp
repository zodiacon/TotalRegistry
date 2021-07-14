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
