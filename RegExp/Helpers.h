#pragma once

struct Helpers abstract final {
	static bool SaveWindowPosition(HWND hWnd, PCWSTR name);
	static bool RestoreWindowPosition(HWND hWnd, PCWSTR name);
	static CString ToBinary(ULONGLONG value);

	static inline const CString DefaultValueName{ L"(Default)" };
};

