#pragma once

struct Helpers abstract final {
	static bool SaveWindowPosition(HWND hWnd, PCWSTR name);
	static bool RestoreWindowPosition(HWND hWnd, PCWSTR name);
	static CString ToBinary(ULONGLONG value);
	static PCWSTR GetSystemDirectory();
	static PCWSTR GetWindowsDirectory();
	static COLORREF ParseColor(const CString& text);
	static inline const CString DefaultValueName{ L"(Default)" };
	static CString GetObjectName(HANDLE hObject, DWORD pid);
	static USHORT GetKeyObjectTypeIndex();
	static CString GetErrorText(DWORD error = ::GetLastError());
	static CString GetProcessNameById(DWORD pid);
	static bool CloseHandle(HANDLE hObject, DWORD pid);
};

