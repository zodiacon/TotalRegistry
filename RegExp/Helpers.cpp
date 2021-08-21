#include "pch.h"
#include "Helpers.h"
#include "AppSettings.h"
#include "NtDll.h"
#include "SecurityHelper.h"
#include <TlHelp32.h>
#include <wil\resource.h>

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

CString Helpers::GetObjectName(HANDLE hObject, DWORD pid) {
    auto h = SecurityHelper::DupHandle(hObject, pid, GENERIC_READ);
    if (h) {
        BYTE buffer[2048];
        auto status = NtQueryObject(h, ObjectNameInformation, buffer, sizeof(buffer), nullptr);
        ::CloseHandle(h);
        if (STATUS_SUCCESS == status) {
            auto str = reinterpret_cast<UNICODE_STRING*>(buffer);
            return CString(str->Buffer, str->Length / sizeof(WCHAR));
        }
    }
    return L"";
}

USHORT Helpers::GetKeyObjectTypeIndex() {
    static USHORT keyIndex = 0;
    if (keyIndex == 0) {
        const ULONG len = 1 << 14;
        auto buffer = std::make_unique<BYTE[]>(len);
        if (!NT_SUCCESS(NtQueryObject(nullptr, ObjectTypesInformation, buffer.get(), len, nullptr)))
            return 0;

        auto p = reinterpret_cast<OBJECT_TYPES_INFORMATION*>(buffer.get());
        auto raw = &p->TypeInformation[0];
        for (ULONG i = 0; i < p->NumberOfTypes; i++) {
            if (raw->TypeName.Buffer == CString(L"Key")) {
                keyIndex = raw->TypeIndex;
                break;
            }
            auto temp = (BYTE*)raw + sizeof(OBJECT_TYPE_INFORMATION) + raw->TypeName.MaximumLength;
            temp += sizeof(PVOID) - 1;
            raw = reinterpret_cast<OBJECT_TYPE_INFORMATION*>((ULONG_PTR)temp / sizeof(PVOID) * sizeof(PVOID));
        }
    }
    return keyIndex;
}

CString Helpers::GetErrorText(DWORD error) {
    ATLASSERT(error);
    PWSTR buffer;
    CString msg;
    if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, nullptr)) {
        msg = buffer;
        ::LocalFree(buffer);
        msg.Trim(L"\n\r");
    }
    return msg;
}

CString Helpers::GetProcessNameById(DWORD pid) {
    static std::unordered_map<DWORD, CString> processes;
    if (processes.empty()) {
        auto hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        ATLASSERT(hSnapshot != INVALID_HANDLE_VALUE);
        if (hSnapshot == INVALID_HANDLE_VALUE)
            return L"";

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        ::Process32First(hSnapshot, &pe);

        while (::Process32Next(hSnapshot, &pe)) {
            processes.insert({ pe.th32ProcessID, pe.szExeFile });
        }
        ::CloseHandle(hSnapshot);
    }

    if (auto it = processes.find(pid); it != processes.end()) {
        wil::unique_handle hProcess(::OpenProcess(SYNCHRONIZE, FALSE, pid));
        if (hProcess && ::WaitForSingleObject(hProcess.get(), 0) == WAIT_OBJECT_0) {
            // process is dead
            processes.erase(pid);
        }
        else {
            return it->second;
        }
    }
    wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!hProcess)
        return L"";

    WCHAR path[MAX_PATH * 2];
    DWORD size = _countof(path);
    CString name;
    if (::QueryFullProcessImageName(hProcess.get(), 0, path, &size)) {
        name = wcsrchr(path, L'\\') + 1;
        processes.insert({ pid, name });
    }

    return name;
}

bool Helpers::CloseHandle(HANDLE hObject, DWORD pid) {
    wil::unique_handle hProcess(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid));
    if (!hProcess)
        return false;
    return ::DuplicateHandle(hProcess.get(), hObject, nullptr, nullptr, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
}
