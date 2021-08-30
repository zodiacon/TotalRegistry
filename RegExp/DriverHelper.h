#pragma once

struct DriverHelper abstract final {
	static bool LoadDriver();
	static bool InstallDriver();
	static bool IsDriverLoaded();
	static bool OpenDevice();
	static HANDLE DupHandle(HANDLE hObject, ULONG pid, ACCESS_MASK access, DWORD flags);
	static HANDLE OpenKey(PCWSTR name, ACCESS_MASK access);
};
