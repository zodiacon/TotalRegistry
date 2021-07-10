#pragma once

struct Hive {
	CString Key;
	CString Path;
};

struct RegistryItem {
	CString Name;
	mutable CString Value;
	DWORD Type;
	DWORD Size{ 0 };
	FILETIME TimeStamp{};
	bool Key : 1 { false };
};

const DWORD REG_KEY = 0x1111;
const DWORD REG_KEY_UP = 0x1112;

struct Registry {
	static DWORD EnumSubKeys(HKEY key, std::function<bool(PCWSTR, const FILETIME&)> handler);
	static DWORD EnumKeyValues(HKEY key, const std::function<void(DWORD, PCWSTR, DWORD)>& handler);
	static CString QueryStringValue(CRegKey& key, PCWSTR name);
	static CString StdRegPathToRealPath(const CString& path);
	static PCWSTR GetRegTypeAsString(DWORD type);
	static CString GetDataAsString(CRegKey& key, const RegistryItem& item);
	static HKEY OpenRealRegistryKey(PCWSTR path = nullptr, DWORD access = KEY_READ);
	static HKEY CreateRealRegistryKey(PCWSTR path, DWORD access = KEY_READ);
	static bool RenameKey(HKEY hKey, PCWSTR name, PCWSTR newName);
	static bool RenameValue(HKEY hKey, PCWSTR path, PCWSTR oldName, PCWSTR newName);
	static bool CopyKey(HKEY hKey, PCWSTR path, HKEY htarget);
	static DWORD GetSubKeyCount(HKEY hKey, DWORD* values = 0);

	static CRegKey OpenKey(const CString& path, DWORD access);
	static CRegKey CreateKey(const CString& path, DWORD access);
	static bool IsKeyLink(HKEY hKey, PCWSTR path);
	static CString ExpandStrings(const CString& text);

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
