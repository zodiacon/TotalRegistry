#include "pch.h"
#include "Registry.h"
#include "NtDll.h"
#include "Helpers.h"
#include "ValueDecoder.h"
#include <sddl.h>

#pragma comment(lib, "ntdll")

DWORD Registry::EnumSubKeys(HKEY key, std::function<bool(PCWSTR, const FILETIME&)> handler) {
	ATLASSERT(IsKeyValid(key));
	WCHAR name[512];
	FILETIME lastWrite;
	LSTATUS error;
	for (DWORD i = 0;; ++i) {
		DWORD len = _countof(name);
		error = ::RegEnumKeyEx(key, i, name, &len, nullptr, nullptr, nullptr, &lastWrite);
		if (ERROR_SUCCESS != error)
			break;

		if (!handler(name, lastWrite))
			break;
	}

	return ERROR_NO_MORE_ITEMS == error ? ERROR_SUCCESS : error;
}

HKEY Registry::OpenRealRegistryKey(PCWSTR path, DWORD access) {
	HANDLE hKey = nullptr;
	UNICODE_STRING keyName;
	RtlInitUnicodeString(&keyName, path ? path : L"\\REGISTRY");
	OBJECT_ATTRIBUTES keyAttr;
	InitializeObjectAttributes(&keyAttr, &keyName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
	auto status = ::NtOpenKey(&hKey, access, &keyAttr);
	::SetLastError(::RtlNtStatusToDosError(status));
	return (HKEY)hKey;
}

HKEY Registry::CreateRealRegistryKey(PCWSTR path, DWORD access) {
	UNICODE_STRING keyName;
	RtlInitUnicodeString(&keyName, path);
	OBJECT_ATTRIBUTES keyAttr;
	InitializeObjectAttributes(&keyAttr, &keyName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
	HANDLE hKey{ nullptr };
	auto status = ::NtCreateKey(&hKey, access, &keyAttr, 0, nullptr, 0, nullptr);
	::SetLastError(::RtlNtStatusToDosError(status));
	return (HKEY)hKey;
}

DWORD Registry::EnumKeyValues(HKEY key, const std::function<bool(DWORD, PCWSTR, DWORD)>& handler) {
	ATLASSERT(IsKeyValid(key));
	// value names can be up to 16383 characters
	const DWORD maxName = 16384;
	auto name = std::make_unique<WCHAR[]>(maxName);
	DWORD type;
	int i;
	DWORD error;
	for (i = 0; ; ++i) {
		DWORD lname = maxName;
		DWORD size = 0;
		error = ::RegEnumValue(key, i, name.get(), &lname, nullptr, &type, nullptr, &size);
		if (ERROR_NO_MORE_ITEMS == error)
			break;
		else if (error != ERROR_SUCCESS)
			break;
		if (!handler(type, name.get(), size))
			break;
	}

	if (error != ERROR_NO_MORE_ITEMS)
		::SetLastError(error);
	return i;
}

CString Registry::QueryStringValue(RegistryKey& key, PCWSTR name) {
	ULONG len = 0;
	key.QueryStringValue(name, nullptr, &len);
	if (len == 0)
		return L"";

	auto value = std::make_unique<WCHAR[]>(len);
	key.QueryStringValue(name, value.get(), &len);
	return value.get();
}

CString Registry::StdRegPathToRealPath(const CString& path) {
	CString result(path);
	result.Replace(L"HKEY_LOCAL_MACHINE\\", L"\\REGISTRY\\MACHINE\\");
	result.Replace(L"HKEY_USERS\\", L"\\REGISTRY\\USER\\");

	return result;
}

RegistryKey Registry::OpenKey(const CString& rawpath, DWORD access, bool* root) {
	if (root)
		*root = false;

	auto path = Helpers::NormalizePath(rawpath);
	RegistryKey key;
	if (path.Left(2) == L"\\\\") {
		// remote Registry
		auto index = path.Find(L"\\", 2);
		if (index < 0)
			return key;

		auto name = path.Mid(2, index - 2);
		auto& rr = m_Remotes[name];
		index = path.Find(L"\\HKEY_LOCAL_MACHINE");
		HKEY hRoot = index >= 0 ? rr.hLocal : rr.hUsers;
		if (index < 0)
			index = path.Find(L"\\HKEY_USERS");
		ATLASSERT(index >= 0);
		index = path.Find(L"\\", index + 1);
		if (index < 0) {
			key.Attach(hRoot, false);
		}
		else {
			auto subpath = path.Mid(index + 1);
			auto error = key.Open(hRoot, subpath, access);
			::SetLastError(error);
		}
		return key;
	}
	if (path[0] == L'\\') {
		// real registry
		key.Attach(OpenRealRegistryKey(path, access));
	}
	else {
		auto bs = path.Find(L'\\');
		CString keyname = path;
		if (bs >= 0) {
			ATLASSERT(bs >= 0);
			keyname = path.Left(bs);
		}
		keyname.TrimRight(L":");
		auto pair = std::find_if(std::begin(Keys), std::end(Keys), [&](auto& k) { return _wcsicmp(k.text, keyname) == 0 || (*k.stext && _wcsicmp(k.stext, keyname) == 0); });
		if (pair == std::end(Keys)) {
			::SetLastError(ERROR_PATH_NOT_FOUND);
			return key;
		}
		if (bs >= 0) {
			auto error = key.Open(pair->hKey, path.Mid(bs + 1), access);
			::SetLastError(error);
		}
		else {
			key.Attach(pair->hKey, false);
			if(root)
				*root = true;
		}
	}
	return key;
}

CRegKey Registry::CreateKey(const CString& path, DWORD access) {
	CRegKey key;
	if (path.IsEmpty()) {
		::SetLastError(ERROR_INVALID_PARAMETER);
		return key;
	}
	if (path[0] == L'\\' && path.Left(2) != L"\\\\") {
		// real registry
		key.Attach(CreateRealRegistryKey(path, access));
		return key;
	}

	CString subKey;
	auto hRoot = GetRootKey(path, subKey);
	if (!hRoot) {
		::SetLastError(ERROR_PATH_NOT_FOUND);
		return key;
	}
	if (subKey.IsEmpty()) {
		// root keys cannot be created
		::SetLastError(ERROR_INVALID_PARAMETER);
		return key;
	}
	auto error = key.Create(hRoot, subKey, nullptr, 0, access);
	::SetLastError(error);
	return key;
}

HKEY Registry::GetRootKey(const CString& path, CString& subKey) {
	auto rest = path;
	const std::map<CString, RemoteRegistry>::value_type* remote = nullptr;
	if (rest.Left(2) == L"\\\\") {
		// remote Registry: \\computer\HKEY_LOCAL_MACHINE\... or \\computer\HKEY_USERS\...
		auto bs = rest.Find(L'\\', 2);
		if (bs < 0)
			return nullptr;
		auto computer = rest.Mid(2, bs - 2);
		for (auto& entry : m_Remotes)
			if (entry.first.CompareNoCase(computer) == 0)
				remote = &entry;
		if (!remote)
			return nullptr;
		rest = rest.Mid(bs + 1);
	}

	auto bs = rest.Find(L'\\');
	auto rootName = bs < 0 ? rest : rest.Left(bs);
	rootName.TrimRight(L":");
	subKey = bs < 0 ? CString() : rest.Mid(bs + 1);
	subKey.Trim(L"\\");

	auto pair = std::find_if(std::begin(Keys), std::end(Keys), [&](auto& k) {
		return _wcsicmp(k.text, rootName) == 0 || (*k.stext && _wcsicmp(k.stext, rootName) == 0);
		});
	if (pair == std::end(Keys))
		return nullptr;

	if (remote) {
		if (pair->hKey == HKEY_LOCAL_MACHINE)
			return remote->second.hLocal;
		if (pair->hKey == HKEY_USERS)
			return remote->second.hUsers;
		return nullptr;
	}
	return pair->hKey;
}

bool Registry::RenameKey(HKEY hKey, PCWSTR name, PCWSTR newName) {
	auto error = ::RegRenameKey(hKey, name, newName);
	::SetLastError(error);
	return ERROR_SUCCESS == error;
}

const std::vector<Hive>& Registry::GetHiveList(bool refresh) {
	if (refresh)
		m_Hives.clear();
	if (!m_Hives.empty())
		return m_Hives;

	RegistryKey key;
	key.Open(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\hivelist", KEY_QUERY_VALUE);
	if(!key)
		return m_Hives;

	EnumKeyValues(key, [&](auto type, auto name, auto size) {
		if (type == REG_SZ && name && *name) {
			auto value = QueryStringValue(key, name);
			m_Hives.push_back({ name, (PCWSTR)value });
		}
		return true;
		});
	return m_Hives;
}

bool Registry::IsHiveKey(const CString& path) {
	const auto& hives = GetHiveList();
	bool stdReg = path[0] != L'\\';
	return std::find_if(hives.begin(), hives.end(), [&](auto& hive) { return _wcsicmp(hive.Key.c_str(), stdReg ? StdRegPathToRealPath(path) : path) == 0; }) != hives.end();
}

CString Registry::ExpandStrings(const CString& text) {
	WCHAR buffer[1024];
	buffer[0] = 0;
	::ExpandEnvironmentStrings(text, buffer, _countof(buffer));
	return buffer;
}

bool Registry::ConnectRegistry(PCWSTR computerName) {
	HKEY hLocal{ nullptr }, hUsers{ nullptr };
	auto error = ::RegConnectRegistry(CString(L"\\\\") + computerName, HKEY_LOCAL_MACHINE, &hLocal);
	if (error == ERROR_SUCCESS)
		error = ::RegConnectRegistry(CString(L"\\\\") + computerName, HKEY_USERS, &hUsers);

	if (error) {
		::RegCloseKey(hLocal);
		::SetLastError(error);
		return false;
	}
	RemoteRegistry rr;
	rr.hLocal = hLocal;
	rr.hUsers = hUsers;
	rr.ComputerName = computerName;
	m_Remotes.insert({ computerName, rr });
	ATLASSERT(IsKeyValid(rr.hLocal));
	ATLASSERT(IsKeyValid(rr.hUsers));

	return true;
}

bool Registry::Disconnect(PCWSTR computerName) {
	auto it = m_Remotes.find(computerName);
	if (it == m_Remotes.end())
		return false;

	auto& rr = it->second;
	::RegCloseKey(rr.hLocal);
	::RegCloseKey(rr.hUsers);
	m_Remotes.erase(it);
	return true;
}

CString Registry::GetRegTypeAsString(DWORD type) {
	switch (type) {
		case REG_KEY: return L"Key";
		case REG_SZ: return L"REG_SZ";
		case REG_DWORD: return L"REG_DWORD";
		case REG_DWORD_BIG_ENDIAN: return L"REG_DWORD_BIG_ENDIAN";
		case REG_MULTI_SZ: return L"REG_MULTI_SZ";
		case REG_QWORD: return L"REG_QWORD";
		case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
		case REG_NONE: return L"REG_NONE";
		case REG_LINK: return L"REG_LINK";
		case REG_BINARY: return L"REG_BINARY";
		case REG_RESOURCE_REQUIREMENTS_LIST: return L"REG_RESOURCE_REQUIREMENTS_LIST";
		case REG_RESOURCE_LIST: return L"REG_RESOURCE_LIST";
		case REG_FULL_RESOURCE_DESCRIPTOR: return L"REG_FULL_RESOURCE_DESCRIPTOR";
	}
	return std::format("{} (0x{:X})", type, type).c_str();
}

CString Registry::GetDataAsString(RegistryKey& key, const RegistryItem& item, bool decimalFirst) {
	auto realsize = item.Size;
	ULONG size = std::min(realsize, 512UL) / sizeof(WCHAR);
	LSTATUS status;
	CString text;
	DWORD type;

	switch (item.Type) {
		case REG_SZ:
		case REG_EXPAND_SZ:
			text = QueryStringValue(key, item.Name).Left(size);
			break;

		case REG_LINK:
			//text = GetLinkPath();
			break;

		case REG_MULTI_SZ:
			size *= 2;
			status = ::RegQueryValueEx(key.Get(), item.Name, nullptr, &type, (PBYTE)text.GetBufferSetLength(size / 2), &size);
			if (status == ERROR_SUCCESS) {
				auto p = text.GetBuffer();
				while (*p) {
					p += ::wcslen(p);
					*p = L' ';
					p++;
				}
			}
			break;

		case REG_DWORD:
		{
			DWORD value;
			if (ERROR_SUCCESS == key.QueryDWORDValue(item.Name, value)) {
				text = ValueDecoder::FormatNumber(value, sizeof(value), decimalFirst);
			}
			break;
		}

		case REG_DWORD_BIG_ENDIAN:
		{
			DWORD value;
			ULONG bytes = sizeof(value);
			if (ERROR_SUCCESS == key.QueryValue(item.Name, nullptr, &value, &bytes) && bytes == sizeof(value)) {
				text = ValueDecoder::FormatNumber(_byteswap_ulong(value), sizeof(value), decimalFirst);
			}
			break;
		}

		case REG_QWORD:
		{
			ULONGLONG value;
			if (ERROR_SUCCESS == key.QueryQWORDValue(item.Name, value)) {
				text = ValueDecoder::FormatNumber(value, sizeof(value), decimalFirst);
			}
			break;
		}

		default:
			// binary, resource lists, REG_NONE and unknown types: the first bytes
			CString digit;
			// item.Size may be stale (-1) after an edit
			ULONG bytes = 0;
			if (ERROR_SUCCESS != key.QueryValue(item.Name, nullptr, nullptr, &bytes))
				break;
			auto buffer = std::make_unique<BYTE[]>(bytes);
			auto status = key.QueryValue(item.Name, nullptr, buffer.get(), &bytes);
			if (status == ERROR_SUCCESS) {
				for (DWORD i = 0; i < std::min<ULONG>(bytes, 64); i++) {
					digit.Format(L"%02X ", buffer[i]);
					text += digit;
				}
			}
			break;
	}

	return text.GetLength() < 1024 ? text : text.Mid(0, 1024);
}

bool Registry::IsKeyLink(HKEY hKey, PCWSTR path, CString& link) {
	HKEY hLinkKey;
	auto error = ::RegOpenKeyExW(hKey, path, REG_OPTION_OPEN_LINK, KEY_READ, &hLinkKey);
	if (ERROR_SUCCESS == error) {
		DWORD type = 0;
		WCHAR linkPath[512] = { 0 };
		DWORD size = sizeof(linkPath) - sizeof(WCHAR);
		auto error = ::RegQueryValueEx(hLinkKey, L"SymbolicLinkValue", nullptr, &type, (BYTE*)linkPath, &size);
		::RegCloseKey(hLinkKey);
		if (type == REG_LINK) {
			// link
			link = linkPath;
			return true;
		}
	}
	return false;
}

LSTATUS Registry::CreateLinkKey(HKEY hParent, PCWSTR name, CString const& target, bool isVolatile) {
	HKEY hKey;
	DWORD disp;
	auto error = ::RegCreateKeyEx(hParent, name, 0, nullptr, REG_OPTION_CREATE_LINK | (isVolatile ? REG_OPTION_VOLATILE : 0),
		KEY_ALL_ACCESS | KEY_CREATE_LINK, nullptr, &hKey, &disp);
	if (error != ERROR_SUCCESS)
		return error;
	if (disp == REG_OPENED_EXISTING_KEY) {
		::RegCloseKey(hKey);
		return ERROR_ALREADY_EXISTS;
	}
	// the target is stored without a terminating NULL
	error = ::RegSetValueEx(hKey, L"SymbolicLinkValue", 0, REG_LINK, (BYTE const*)target.GetString(), target.GetLength() * sizeof(WCHAR));
	if (error != ERROR_SUCCESS)
		::NtDeleteKey(hKey);
	::RegCloseKey(hKey);
	return error;
}

LSTATUS Registry::DeleteLinkKey(HKEY hParent, PCWSTR name) {
	HKEY hKey;
	auto error = ::RegOpenKeyEx(hParent, name, REG_OPTION_OPEN_LINK, DELETE, &hKey);
	if (error != ERROR_SUCCESS)
		return error;
	auto status = ::NtDeleteKey(hKey);
	::RegCloseKey(hKey);
	return status >= 0 ? ERROR_SUCCESS : ::RtlNtStatusToDosError(status);
}

CString Registry::StdPathToKernelPath(CString const& path) {
	if (path.Left(1) == L"\\" && path.Left(2) != L"\\\\")
		return path;	// already a real path

	CString subKey;
	auto hRoot = GetRootKey(path, subKey);
	CString kernel;
	if (hRoot == HKEY_LOCAL_MACHINE)
		kernel = L"\\REGISTRY\\MACHINE";
	else if (hRoot == HKEY_USERS)
		kernel = L"\\REGISTRY\\USER";
	else if (hRoot == HKEY_CURRENT_USER)
		kernel = L"\\REGISTRY\\USER\\" + Helpers::GetCurrentUserSid();
	else if (hRoot == HKEY_CURRENT_CONFIG)
		kernel = L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current";
	else
		return L"";
	return subKey.IsEmpty() ? kernel : kernel + L"\\" + subKey;
}

bool Registry::GetKeyInfo(CString const& path, KeyInfo& info) {
	info = KeyInfo();
	bool remote = path.Left(2) == L"\\\\";

	// a link key is described itself, so it's opened without following the link
	RegistryKey key;
	if (auto bs = path.ReverseFind(L'\\'); bs > 0) {
		auto parent = OpenKey(path.Left(bs), KEY_READ);
		auto name = path.Mid(bs + 1);
		if (parent && IsKeyLink(parent, name, info.LinkTarget)) {
			info.IsLink = true;
			HKEY hKey;
			if (ERROR_SUCCESS == ::RegOpenKeyEx(parent, name, REG_OPTION_OPEN_LINK, KEY_READ, &hKey))
				key.Attach(hKey);
		}
	}
	if (!key)
		key = OpenKey(path, KEY_READ);
	if (!key)
		return false;

	WCHAR className[256];
	DWORD classLen = _countof(className);
	if (ERROR_SUCCESS == ::RegQueryInfoKey(key.Get(), className, &classLen, nullptr, &info.SubKeys, nullptr, nullptr,
		&info.Values, nullptr, nullptr, nullptr, &info.LastWrite))
		info.Class.SetString(className, classLen);

	// the key object's real name and flags; not available for predefined (pseudo) handles
	BYTE buffer[4096];
	ULONG len;
	if (::NtQueryKey(key.Get(), KeyInformationClass::Name, buffer, sizeof(buffer), &len) >= 0) {
		ULONG nameLength;
		memcpy(&nameLength, buffer, sizeof(nameLength));
		info.KernelPath.SetString((PCWSTR)(buffer + sizeof(ULONG)), std::min<ULONG>(nameLength, len - sizeof(ULONG)) / sizeof(WCHAR));
	}
	else if (!remote)
		info.KernelPath = StdPathToKernelPath(path);
	ULONG flags[3]{};
	if (::NtQueryKey(key.Get(), KeyInformationClass::Flags, flags, sizeof(flags), &len) >= 0) {
		info.Volatile = (flags[1] & KeyFlagVolatile) != 0;
		info.IsLink |= (flags[1] & KeyFlagLink) != 0;
	}

	DWORD size = 0;
	::RegGetKeySecurity(key.Get(), OWNER_SECURITY_INFORMATION, nullptr, &size);
	std::vector<BYTE> sd(size);
	PSID owner = nullptr;
	BOOL defaulted;
	if (size && ERROR_SUCCESS == ::RegGetKeySecurity(key.Get(), OWNER_SECURITY_INFORMATION, sd.data(), &size)
		&& ::GetSecurityDescriptorOwner(sd.data(), &owner, &defaulted) && owner) {
		WCHAR name[256], domain[256];
		DWORD nameLen = _countof(name), domainLen = _countof(domain);
		SID_NAME_USE use;
		if (::LookupAccountSid(nullptr, owner, name, &nameLen, domain, &domainLen, &use))
			info.Owner = *domain ? CString(domain) + L"\\" + name : CString(name);
		else {
			PWSTR text;
			if (::ConvertSidToStringSid(owner, &text)) {
				info.Owner = text;
				::LocalFree(text);
			}
		}
	}

	// the hive: the longest hive root the key is under (the hive list is of the local machine)
	if (!remote && !info.KernelPath.IsEmpty()) {
		for (auto& hive : GetHiveList(true)) {
			CString hiveKey(hive.Key.c_str());
			bool under = info.KernelPath.GetLength() >= hiveKey.GetLength() && _wcsnicmp(info.KernelPath, hiveKey, hiveKey.GetLength()) == 0
				&& (info.KernelPath.GetLength() == hiveKey.GetLength() || info.KernelPath[hiveKey.GetLength()] == L'\\');
			if (under && hiveKey.GetLength() > info.HiveKey.GetLength()) {
				info.HiveKey = hiveKey;
				info.HiveFile = hive.Path.empty() ? CString() : Helpers::GetWin32PathFromNTPath(hive.Path.c_str());
			}
		}
		info.IsHiveRoot = !info.HiveKey.IsEmpty() && info.KernelPath.CompareNoCase(info.HiveKey) == 0;
	}
	return true;
}

bool Registry::RenameValue(HKEY hKey, PCWSTR path, PCWSTR oldName, PCWSTR newName) {
	CRegKey key;
	key.Open(hKey, path, KEY_QUERY_VALUE | KEY_SET_VALUE);
	if (!key)
		return false;

	DWORD bytes = 0;
	DWORD type;
	key.QueryValue(oldName, &type, nullptr, &bytes);
	if (bytes == 0)
		return false;

	auto buffer = std::make_unique<BYTE[]>(bytes);
	if (ERROR_SUCCESS != key.QueryValue(oldName, &type, buffer.get(), &bytes))
		return false;

	if (ERROR_SUCCESS != key.SetValue(newName, type, buffer.get(), bytes))
		return false;

	return ERROR_SUCCESS == key.DeleteValue(oldName);
}

bool Registry::CopyKey(HKEY hKey, PCWSTR path, HKEY htarget) {
	auto error = ::RegCopyTree(hKey, path, htarget);
	::SetLastError(error);
	return ERROR_SUCCESS == error;
}

bool Registry::CopyValue(HKEY hSource, HKEY hTarget, PCWSTR sourceName, PCWSTR targetName) {
	DWORD size = 0;
	DWORD type;
	auto error = ::RegQueryValueEx(hSource, sourceName, nullptr, &type, nullptr, &size);
	::SetLastError(error);
	if (error != ERROR_SUCCESS)
		return false;

	std::unique_ptr<BYTE[]> data;
	if (size) {
		data = std::make_unique<BYTE[]>(size);
		if (!data) {
			::SetLastError(ERROR_OUTOFMEMORY);
			return false;
		}
		auto error = ::RegQueryValueEx(hSource, sourceName, nullptr, nullptr, data.get(), &size);
		::SetLastError(error);
		if (ERROR_SUCCESS != error)
			return false;
	}
	error = ::RegSetValueEx(hTarget, targetName, 0, type, data.get(), size);
	::SetLastError(error);
	return ERROR_SUCCESS == error;
}

DWORD Registry::GetSubKeyCount(HKEY hKey, DWORD* values, FILETIME* ft) {
	DWORD subkeys = 0;
	auto error = ::RegQueryInfoKey(hKey, nullptr, 0, nullptr, &subkeys, nullptr, nullptr, values, nullptr, nullptr, nullptr, ft);
	::SetLastError(error);
	return subkeys;
}

bool Registry::IsKeyValid(HKEY h) {
	if (h == nullptr)
		return true;

	WCHAR name[16];
	DWORD type, lname = _countof(name);
	return ::RegEnumValue(h, 0, name, &lname, nullptr, &type, nullptr, nullptr) != ERROR_INVALID_HANDLE;
}

std::vector<HandleInfo> Registry::EnumKeyHandles(bool hideInaccessible) {
	DWORD size = 1 << 24;
	void* buffer;
	do {
		buffer = ::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!buffer)
			break;
		auto status = NtQuerySystemInformation(SystemInformationClass::ExtendedHandleInformation, buffer, size, nullptr);
		if (status == 0)
			break;
		if (status == STATUS_INFO_LENGTH_MISMATCH) {
			::VirtualFree(buffer, 0, MEM_RELEASE);
			size *= 2;
		}
		else {
			break;
		}
	} while (true);
	
	std::vector<HandleInfo> handles;

	if (!buffer)
		return handles;

	auto p = (PSYSTEM_HANDLE_INFORMATION_EX)buffer;
	handles.reserve(p->NumberOfHandles / 5);	// estimate
	auto keyType = Helpers::GetKeyObjectTypeIndex();
	for (ULONG i = 0; i < p->NumberOfHandles; i++) {
		auto& hi = p->Handles[i];
		if (hi.ObjectTypeIndex != keyType)
			continue;

		HandleInfo info;
		info.Access = hi.GrantedAccess;
		info.Object = hi.Object;
		info.Handle = static_cast<ULONG>(hi.HandleValue);
		info.ProcessId = static_cast<ULONG>(hi.UniqueProcessId);
		info.Attributes = hi.HandleAttributes;
		info.Name = Helpers::GetObjectName(ULongToHandle(info.Handle), info.ProcessId);
		if (info.Name.IsEmpty()) {
			if (hideInaccessible)
				continue;
			info.Name = L"<" + Helpers::GetErrorText() + L">";
		}
		info.ProcessName = Helpers::GetProcessNameById(info.ProcessId);
		handles.push_back(std::move(info));
	}
	handles.shrink_to_fit();

	::VirtualFree(buffer, 0, MEM_RELEASE);
	return handles;
}

