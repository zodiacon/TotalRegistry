#include "pch.h"
#include "Registry.h"
#include <winternl.h>

extern "C" NTSYSCALLAPI NTSTATUS NTAPI NtOpenKey(
	_Out_ PHANDLE KeyHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes
);

#pragma comment(lib, "ntdll")

DWORD Registry::EnumSubKeys(CRegKey& key, std::function<bool(PCWSTR, const FILETIME&)> handler) {
	WCHAR name[512];
	FILETIME lastWrite;
	for (DWORD i = 0;; ++i) {
		DWORD len = _countof(name);
		if (ERROR_SUCCESS != key.EnumKey(i, name, &len, &lastWrite))
			break;

		if (!handler(name, lastWrite))
			break;
	}
	return ERROR_SUCCESS;
}

HKEY Registry::OpenRealRegistryKey(PCWSTR path, DWORD access) {
	UNICODE_STRING keyName;
	RtlInitUnicodeString(&keyName, path ? path : L"\\REGISTRY");
	OBJECT_ATTRIBUTES keyAttr;
	InitializeObjectAttributes(&keyAttr, &keyName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
	HANDLE hKey{ nullptr };
	::NtOpenKey(&hKey, access, &keyAttr);
	return (HKEY)hKey;
}

DWORD Registry::EnumKeyValues(CRegKey& key, const std::function<void(DWORD, PCWSTR, DWORD)>& handler) {
	WCHAR name[256];
	DWORD type;
	int i;
	DWORD error;
	for (i = 0; ; ++i) {
		DWORD lname = _countof(name);
		DWORD size = 0;
		error = ::RegEnumValue(key.m_hKey, i, name, &lname, nullptr, &type, nullptr, &size);
		if (ERROR_NO_MORE_ITEMS == error)
			break;
		else if (error != ERROR_SUCCESS)
			break;
		handler(type, name, size);
	}
	if (error != ERROR_NO_MORE_ITEMS)
		::SetLastError(error);
	return i;
}

CString Registry::QueryStringValue(CRegKey& key, PCWSTR name) {
	ULONG len = 0;
	key.QueryStringValue(name, nullptr, &len);
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

CRegKey Registry::OpenKey(const CString& path, DWORD access) {
	CRegKey key;
	if (path[0] == L'\\') {
		// real registry
		key.Attach(OpenRealRegistryKey(path, access));
	}
	else {
		auto bs = path.Find(L'\\');
		ATLASSERT(bs >= 0);
		auto keyname = path.Left(bs);
		auto pair = std::find_if(std::begin(Keys), std::end(Keys), [&](auto& k) { return k.text == keyname; });
		ATLASSERT(pair != std::end(Keys));
		key.Open(pair->hKey, path.Mid(bs + 1), access);
		return key;
	}
	return key;
}

bool Registry::RenameKey(HKEY hKey, PCWSTR name, PCWSTR newName) {
	return ERROR_SUCCESS == ::RegRenameKey(hKey, name, newName);
}

const std::vector<Hive>& Registry::GetHiveList(bool refresh) const {
	if (refresh)
		_hives.clear();
	if (!_hives.empty())
		return _hives;

	CRegKey key;
	key.Open(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\hivelist", KEY_READ);
	if(!key)
		return _hives;

	EnumKeyValues(key, [&](auto type, auto name, auto size) {
		if (type == REG_SZ && name && *name) {
			auto value = QueryStringValue(key, name);
			_hives.push_back({ name, value });
		}
		return true;
		});
	return _hives;
}

bool Registry::IsHiveKey(const CString& path) const {
	const auto& hives = GetHiveList();
	bool stdReg = path[0] != L'\\';
	return std::find_if(hives.begin(), hives.end(), [&](auto& hive) { return hive.Key.CompareNoCase(stdReg ? StdRegPathToRealPath(path) : path) == 0; }) != hives.end();
}

PCWSTR Registry::GetRegTypeAsString(DWORD type) {
	switch (type) {
		case REG_SZ: return L"REG_SZ";
		case REG_DWORD: return L"REG_DWORD";
		case REG_MULTI_SZ: return L"REG_MULTI_SZ";
		case REG_QWORD: return L"REG_QDWORD";
		case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
		case REG_NONE: return L"REG_NONE";
		case REG_LINK: return L"REG_LINK";
		case REG_BINARY: return L"REG_BINARY";
		case REG_RESOURCE_REQUIREMENTS_LIST: return L"REG_RESOURCE_REQUIREMENTS_LIST";
		case REG_RESOURCE_LIST: return L"REG_RESOURCE_LIST";
		case REG_FULL_RESOURCE_DESCRIPTOR: return L"REG_FULL_RESOURCE_DESCRIPTOR";
	}
	return L"Unknown";
}

CString Registry::GetDataAsString(CRegKey& key, const RegistryItem& item) {
	ULONG realsize = item.Size;
	ULONG size = (realsize > (1 << 12) ? (1 << 12) : realsize) / sizeof(WCHAR);
	LRESULT status;
	CString text;

	switch (item.Type) {
		case REG_SZ:
		case REG_EXPAND_SZ:
			text.Preallocate(size + 1);
			status = key.QueryStringValue(item.Name, text.GetBuffer(), &size);
			break;

		case REG_LINK:
			//text = GetLinkPath();
			break;

		case REG_MULTI_SZ:
			text.Preallocate(size + 1);
			status = key.QueryMultiStringValue(item.Name, text.GetBuffer(), &size);
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
				text.Format(L"0x%08X (%u)", value, value);
			}
			break;
		}

		case REG_QWORD:
		{
			ULONGLONG value;
			if (ERROR_SUCCESS == key.QueryQWORDValue(item.Name, value)) {
				auto fmt = value < (1LL << 32) ? L"0x%08llX (%llu)" : L"0x%016llX (%llu)";
				text.Format(fmt, value, value);
			}
			break;
		}

		case REG_BINARY:
			CString digit;
			auto buffer = std::make_unique<BYTE[]>(item.Size);
			if (buffer == nullptr)
				break;
			ULONG bytes = item.Size;
			auto status = key.QueryBinaryValue(item.Name, buffer.get(), &bytes);
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
