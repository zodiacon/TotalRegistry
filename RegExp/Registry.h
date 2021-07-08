#pragma once

struct Hive {
	CString Key;
	CString Path;
};

struct RegistryItem {
	CString Name;
	CString Path;
	DWORD Type;
	DWORD Size;
};

struct Registry {
	static DWORD EnumSubKeys(CRegKey& key, std::function<bool(PCWSTR, const FILETIME&)> handler);
	static DWORD EnumKeyValues(CRegKey& key, const std::function<void(DWORD, PCWSTR, DWORD)>& handler);
	static CString QueryStringValue(CRegKey& key, PCWSTR name);
	static CString StdRegPathToRealPath(const CString& path);
	static PCWSTR GetRegTypeAsString(DWORD type);
	static CString GetDataAsString(CRegKey& key, const RegistryItem& item);
	static HKEY OpenRealRegistryKey(PCWSTR path = nullptr, DWORD access = KEY_READ);
	static bool RenameKey(HKEY hKey, PCWSTR name, PCWSTR newName);
	static bool RenameValue(HKEY hKey, PCWSTR oldName, PCWSTR newName);
	static CRegKey OpenKey(const CString& path, DWORD access);

	const std::vector<Hive>& GetHiveList(bool refresh = false) const;
	bool IsHiveKey(const CString& path) const;

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
	mutable std::vector<Hive> _hives;
};
