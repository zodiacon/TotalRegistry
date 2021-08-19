#pragma once

struct SecurityHelper abstract final {
	static bool IsRunningElevated();
	static bool RunElevated();
	static bool EnablePrivilege(PCWSTR privName, bool enable);
	static HANDLE DupHandle(HANDLE hSource, DWORD sourcePid, DWORD access = 0);
};
