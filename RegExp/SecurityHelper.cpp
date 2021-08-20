#include "pch.h"
#include "SecurityHelper.h"
#include <wil\resource.h>

bool SecurityHelper::IsRunningElevated() {
	static bool runningElevated = false;
	static bool runningElevatedCheck = false;
	if (runningElevatedCheck)
		return runningElevated;

	runningElevatedCheck = true;
	wil::unique_handle hToken;
	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, hToken.addressof()))
		return false;

	TOKEN_ELEVATION te;
	DWORD len;
	if (::GetTokenInformation(hToken.get(), TokenElevation, &te, sizeof(te), &len)) {
		runningElevated = te.TokenIsElevated ? true : false;
	}
	return runningElevated;
}

bool SecurityHelper::RunElevated() {
	WCHAR path[MAX_PATH];
	::GetModuleFileName(nullptr, path, _countof(path));
	return (INT_PTR)::ShellExecute(nullptr, L"runas", path, nullptr, nullptr, SW_SHOWDEFAULT) > 31;
}

bool SecurityHelper::EnablePrivilege(PCWSTR privName, bool enable) {
	wil::unique_handle hToken;
	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, hToken.addressof()))
		return false;

	bool result = false;
	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
	if (::LookupPrivilegeValue(nullptr, privName,
		&tp.Privileges[0].Luid)) {
		if (::AdjustTokenPrivileges(hToken.get(), FALSE, &tp, sizeof(tp),
			nullptr, nullptr))
			result = ::GetLastError() == ERROR_SUCCESS;
	}
	return result;
}

HANDLE SecurityHelper::DupHandle(HANDLE hSource, DWORD sourcePid, DWORD access) {
	wil::unique_handle hProcess(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, sourcePid));
	if (!hProcess)
		return nullptr;

	HANDLE h{ nullptr };
	::DuplicateHandle(hProcess.get(), hSource, ::GetCurrentProcess(), &h, access, FALSE, access == 0 ? DUPLICATE_SAME_ACCESS : 0);
	return h;
}
