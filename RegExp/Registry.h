#pragma once

#include "RegistryKey.h"

struct Hive {
	std::wstring Key;
	std::wstring Path;
};

struct RegistryItem {
	CString Name;
	mutable CString Value;
	mutable DWORD Type;
	mutable DWORD Size{ 0 };
	FILETIME TimeStamp{};
	bool Key : 1 { false };
};

struct RemoteRegistry {
	HKEY hLocal, hUsers;
	CString ComputerName;
};

const DWORD REG_KEY = 0x1111;
const DWORD REG_KEY_UP = 0x1112;

struct HandleInfo {
	ULONG Handle;
	PVOID Object;
	DWORD Access;
	DWORD ProcessId;
	ULONG Attributes;
	CString Name;
	CString ProcessName;
};

struct Registry abstract final {
	static DWORD EnumSubKeys(HKEY key, std::function<bool(PCWSTR, const FILETIME&)> handler);
	static DWORD EnumKeyValues(HKEY key, const std::function<void(DWORD, PCWSTR, DWORD)>& handler);
	static CString QueryStringValue(RegistryKey& key, PCWSTR name);
	static CString StdRegPathToRealPath(const CString& path);
	static PCWSTR GetRegTypeAsString(DWORD type);
	static CString GetDataAsString(RegistryKey& key, const RegistryItem& item);
	static HKEY OpenRealRegistryKey(PCWSTR path = nullptr, DWORD access = KEY_READ);
	static HKEY CreateRealRegistryKey(PCWSTR path, DWORD access = KEY_READ);
	static bool RenameKey(HKEY hKey, PCWSTR name, PCWSTR newName);
	static bool RenameValue(HKEY hKey, PCWSTR path, PCWSTR oldName, PCWSTR newName);
	static bool CopyKey(HKEY hKey, PCWSTR path, HKEY htarget);
	static bool CopyValue(HKEY hSource, HKEY hTarget, PCWSTR sourceName, PCWSTR targetName);
	static DWORD GetSubKeyCount(HKEY hKey, DWORD* values = 0, FILETIME* ft = nullptr);

	static RegistryKey OpenKey(const CString& path, DWORD access, bool* root = nullptr);
	static CRegKey CreateKey(const CString& path, DWORD access);
	static bool IsKeyLink(HKEY hKey, PCWSTR path, CString& linkPath);
	static CString ExpandStrings(const CString& text);

	static bool ConnectRegistry(PCWSTR computerName);
	static bool Disconnect(PCWSTR computerName);

	static const std::vector<Hive>& GetHiveList(bool refresh = false);
	static bool IsHiveKey(const CString& path);
	static bool IsKeyValid(HKEY h);
	static std::vector<HandleInfo> EnumKeyHandles(bool hideInaccessible);

	static inline const struct {
		PCWSTR text;
		HKEY hKey;
	} Keys[] {
		{ L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT },
		{ L"HKEY_CURRENT_USER", HKEY_CURRENT_USER },
		{ L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE },
		{ L"HKEY_USERS", HKEY_USERS },
		{ L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG },
		{ L"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA },
		{ L"HKEY_PERFORMANCE_TEXT", HKEY_PERFORMANCE_TEXT },
		{ L"HKEY_PERFORMANCE_NLSTEXT", HKEY_PERFORMANCE_NLSTEXT },
		{ L"HKEY_CURRENT_USER_LOCAL_SETTINGS", HKEY_CURRENT_USER_LOCAL_SETTINGS },
	};

private:
	inline static std::map<CString, RemoteRegistry> _remotes;
	inline static std::vector<Hive> _hives;
};
