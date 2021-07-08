#include "pch.h"
#include "SecurityHelper.h"

bool SecurityHelper::IsRunningElevated() {
	static bool runningElevated = false;
	static bool runningElevatedCheck = false;
	if (runningElevatedCheck)
		return runningElevated;

	runningElevatedCheck = true;
	HANDLE hToken;
	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken))
		return false;

	TOKEN_ELEVATION te;
	DWORD len;
	if (::GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &len)) {
		runningElevated = te.TokenIsElevated ? true : false;
	}
	::CloseHandle(hToken);
	return runningElevated;
}

bool SecurityHelper::RunElevated() {
	WCHAR path[MAX_PATH];
	::GetModuleFileName(nullptr, path, _countof(path));
	return (INT_PTR)::ShellExecute(nullptr, L"runas", path, nullptr, nullptr, SW_SHOWDEFAULT) > 31;
}
