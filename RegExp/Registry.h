#pragma once

#include "RegistryKey.h"

struct Hive {
	std::wstring Key;
	std::wstring Path;
};

struct RegistryItem {
	CString Name;
	mutable CString Value;
	// the Details column text, computed once
	mutable CString Details;
	mutable bool HasDetails{ false };
	mutable DWORD Type;
	mutable DWORD Size{ 0 };
	FILETIME TimeStamp{};
	bool Key : 1 { false };

	// after the data changed
	void ResetData() const {
		Value.Empty();
		Details.Empty();
		HasDetails = false;
	}
};

// what Registry::GetKeyInfo finds out about a key
struct KeyInfo {
	CString KernelPath;		// where the key really is (\REGISTRY\...), e.g. which hive an HKEY_CLASSES_ROOT key comes from
	FILETIME LastWrite{};
	DWORD SubKeys{ 0 }, Values{ 0 };
	CString Class;
	CString Owner;			// DOMAIN\name, or a SID
	bool Volatile{ false };
	bool IsLink{ false };
	CString LinkTarget;
	CString HiveKey;		// the root of the hive the key is in (\REGISTRY\...)
	CString HiveFile;		// empty for hives kept only in memory
	bool IsHiveRoot{ false };
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

struct Registry final {
	static DWORD EnumSubKeys(HKEY key, std::function<bool(PCWSTR, const FILETIME&)> handler);
	static DWORD EnumKeyValues(HKEY key, const std::function<bool(DWORD, PCWSTR, DWORD)>& handler);
	static CString QueryStringValue(RegistryKey& key, PCWSTR name);
	static CString StdRegPathToRealPath(const CString& path);
	static CString GetRegTypeAsString(DWORD type);
	// the value column text; numbers are shown in hex and decimal, the decimal first if requested
	static CString GetDataAsString(RegistryKey& key, const RegistryItem& item, bool decimalFirst = false);
	static HKEY OpenRealRegistryKey(PCWSTR path = nullptr, DWORD access = KEY_READ);
	static HKEY CreateRealRegistryKey(PCWSTR path, DWORD access = KEY_READ);
	static bool RenameKey(HKEY hKey, PCWSTR name, PCWSTR newName);
	static bool RenameValue(HKEY hKey, PCWSTR path, PCWSTR oldName, PCWSTR newName);
	static bool CopyKey(HKEY hKey, PCWSTR path, HKEY htarget);
	static bool CopyValue(HKEY hSource, HKEY hTarget, PCWSTR sourceName, PCWSTR targetName);
	static DWORD GetSubKeyCount(HKEY hKey, DWORD* values = 0, FILETIME* ft = nullptr);

	static RegistryKey OpenKey(const CString& path, DWORD access, bool* root = nullptr);
	static CRegKey CreateKey(const CString& path, DWORD access);
	// splits a standard or remote path into its root key handle and subkey; nullptr if the root is unknown
	static HKEY GetRootKey(const CString& path, CString& subKey);
	static bool IsKeyLink(HKEY hKey, PCWSTR path, CString& linkPath);
	// a symbolic link key pointing at a kernel path (\REGISTRY\...)
	static LSTATUS CreateLinkKey(HKEY hParent, PCWSTR name, CString const& target, bool isVolatile = false);
	// deletes the link key itself; RegDeleteTree would delete the target's contents instead
	static LSTATUS DeleteLinkKey(HKEY hParent, PCWSTR name);
	// a standard or real path as a kernel path, for link targets; empty if there is none (e.g. HKEY_CLASSES_ROOT, a merged view)
	static CString StdPathToKernelPath(CString const& path);
	// details of a key (a link key itself, not its target); false if it can't be opened
	static bool GetKeyInfo(CString const& path, KeyInfo& info);
	static CString ExpandStrings(const CString& text);

	static bool ConnectRegistry(PCWSTR computerName);
	static bool Disconnect(PCWSTR computerName);

	static const std::vector<Hive>& GetHiveList(bool refresh = false);
	static bool IsHiveKey(const CString& path);
	static bool IsKeyValid(HKEY h);
	static std::vector<HandleInfo> EnumKeyHandles(bool hideInaccessible);

	static inline const struct {
		PCWSTR text;
		PCWSTR stext;
		HKEY hKey;
	} Keys[] {
		{ L"HKEY_CLASSES_ROOT", L"HKCR", HKEY_CLASSES_ROOT },
		{ L"HKEY_CURRENT_USER", L"HKCU", HKEY_CURRENT_USER },
		{ L"HKEY_LOCAL_MACHINE", L"HKLM", HKEY_LOCAL_MACHINE },
		{ L"HKEY_USERS", L"HKU", HKEY_USERS },
		{ L"HKEY_CURRENT_CONFIG", L"HKCC", HKEY_CURRENT_CONFIG },
		{ L"HKEY_PERFORMANCE_DATA", L"", HKEY_PERFORMANCE_DATA },
		{ L"HKEY_PERFORMANCE_TEXT", L"", HKEY_PERFORMANCE_TEXT },
		{ L"HKEY_PERFORMANCE_NLSTEXT", L"", HKEY_PERFORMANCE_NLSTEXT },
		{ L"HKEY_CURRENT_USER_LOCAL_SETTINGS", L"", HKEY_CURRENT_USER_LOCAL_SETTINGS },
	};

private:
	inline static std::map<CString, RemoteRegistry> m_Remotes;
	inline static std::vector<Hive> m_Hives;
};
