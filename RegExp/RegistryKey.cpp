#include "pch.h"
#include "RegistryKey.h"
#include "Registry.h"

RegistryKey::RegistryKey(HKEY hKey, bool own) noexcept : _hKey(hKey), _own(own) {
	ATLASSERT(IsValid());
	CheckPredefinedKey();
}

RegistryKey::RegistryKey(RegistryKey&& other) noexcept {
	_hKey = other._hKey;
	_own = other._own;
	CheckPredefinedKey();
	ATLASSERT(IsValid());
	other._hKey = nullptr;
}

RegistryKey& RegistryKey::operator=(RegistryKey&& other) noexcept {
	Close();
	_hKey = other._hKey;
	_own = other._own;
	CheckPredefinedKey();
	ATLASSERT(IsValid());
	other._hKey = nullptr;
	return *this;
}

void RegistryKey::Close() {
	if (_own && _hKey) {
		::RegCloseKey(_hKey);
	}
	_hKey = nullptr;
}

HKEY RegistryKey::Detach() {
	auto h = _hKey;
	_hKey = nullptr;
	return h;
}

void RegistryKey::Attach(HKEY hKey, bool own) {
	Close();
	_hKey = hKey;
	_own = own;
	ATLASSERT(IsValid());
	CheckPredefinedKey();
}

bool RegistryKey::IsValid() const {
	return Registry::IsKeyValid(_hKey);
}

LSTATUS RegistryKey::Open(HKEY parent, PCWSTR path, DWORD access) {
	ATLASSERT(_hKey == nullptr);
	auto error = ::RegOpenKeyEx(parent, path, 0, access, &_hKey);
	CheckPredefinedKey();
	return error;
}

LSTATUS RegistryKey::Create(HKEY parent, PCWSTR path, DWORD access) {
	ATLASSERT(_hKey == nullptr);
	auto error = ::RegCreateKeyEx(parent, path, 0, nullptr, 0, access, nullptr, &_hKey, nullptr);
	CheckPredefinedKey();
	return error;
}

_Use_decl_annotations_
LSTATUS RegistryKey::SetStringValue(LPCTSTR pszValueName, LPCTSTR pszValue, DWORD dwType) noexcept {
	ATLASSUME(_hKey);
	ATLENSURE_RETURN_VAL(pszValue != NULL, ERROR_INVALID_DATA);
	ATLASSERT((dwType == REG_SZ) || (dwType == REG_EXPAND_SZ));

	return ::RegSetValueEx(_hKey, pszValueName, 0, dwType, reinterpret_cast<const BYTE*>(pszValue), (static_cast<DWORD>(_tcslen(pszValue)) + 1) * sizeof(TCHAR));
}

_Use_decl_annotations_
LSTATUS RegistryKey::SetMultiStringValue(LPCTSTR pszValueName, LPCTSTR pszValue) noexcept {
	LPCTSTR pszTemp;
	ULONG nBytes;
	ULONG nLength;

	ATLASSUME(_hKey);

	nBytes = 0;
	pszTemp = pszValue;
	do {
		nLength = static_cast<ULONG>(_tcslen(pszTemp)) + 1;
		pszTemp += nLength;
		nBytes += nLength * sizeof(TCHAR);
	} while (nLength != 1);

	return ::RegSetValueEx(_hKey, pszValueName, 0, REG_MULTI_SZ, reinterpret_cast<const BYTE*>(pszValue), nBytes);
}

LSTATUS RegistryKey::SetValue(_In_opt_z_ LPCTSTR pszValueName, _In_ DWORD dwType, _In_opt_ const void* pValue, _In_ ULONG nBytes) noexcept {
	ATLASSUME(_hKey);
	return ::RegSetValueEx(_hKey, pszValueName, 0, dwType, static_cast<const BYTE*>(pValue), nBytes);
}

LSTATUS RegistryKey::SetBinaryValue(_In_opt_z_ LPCTSTR pszValueName, _In_opt_ const void* pData, _In_ ULONG nBytes) noexcept {
	ATLASSUME(_hKey);
	return ::RegSetValueEx(_hKey, pszValueName, 0, REG_BINARY, reinterpret_cast<const BYTE*>(pData), nBytes);
}

LSTATUS RegistryKey::SetDWORDValue(_In_opt_z_ LPCTSTR pszValueName, _In_ DWORD dwValue) noexcept {
	ATLASSUME(_hKey);
	return ::RegSetValueEx(_hKey, pszValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(DWORD));
}

LSTATUS RegistryKey::SetQWORDValue(_In_opt_z_ LPCTSTR pszValueName, _In_ ULONGLONG qwValue) noexcept {
	ATLASSUME(_hKey);
	return ::RegSetValueEx(_hKey, pszValueName, 0, REG_QWORD, reinterpret_cast<const BYTE*>(&qwValue), sizeof(ULONGLONG));
}

LSTATUS RegistryKey::SetValue(_In_ DWORD dwValue, _In_opt_z_ LPCTSTR pszValueName) noexcept {
	ATLASSUME(_hKey);
	return SetDWORDValue(pszValueName, dwValue);
}

_Use_decl_annotations_
LSTATUS RegistryKey::QueryStringValue(LPCTSTR pszValueName, LPTSTR pszValue, ULONG* pnChars) noexcept {
	LONG lRes;
	DWORD dwType;
	ULONG nBytes;

	ATLASSUME(_hKey != NULL);
	ATLASSERT(pnChars != NULL);

	nBytes = (*pnChars) * sizeof(TCHAR);
	*pnChars = 0;
	lRes = ::RegQueryValueEx(_hKey, pszValueName, NULL, &dwType, reinterpret_cast<LPBYTE>(pszValue), &nBytes);

	if (lRes != ERROR_SUCCESS) {
		return lRes;
	}

	if (dwType != REG_SZ && dwType != REG_EXPAND_SZ) {
		return ERROR_INVALID_DATA;
	}

	if (pszValue != NULL) {
		if (nBytes != 0) {
			if ((nBytes % sizeof(TCHAR) != 0) || (pszValue[nBytes / sizeof(TCHAR) - 1] != 0)) {
				return ERROR_INVALID_DATA;
			}
		}
		else {
			pszValue[0] = _T('\0');
		}
	}

	*pnChars = nBytes / sizeof(WCHAR);
	return ERROR_SUCCESS;
}

_Use_decl_annotations_
LSTATUS RegistryKey::QueryMultiStringValue(LPCTSTR pszValueName, LPTSTR pszValue, ULONG* pnChars) noexcept {
	LONG lRes;
	DWORD dwType;
	ULONG nBytes;

	ATLASSUME(_hKey);
	ATLASSERT(pnChars);

	if (pszValue != nullptr && *pnChars < 2)
		return ERROR_INSUFFICIENT_BUFFER;

	nBytes = (*pnChars) * sizeof(WCHAR);
	*pnChars = 0;

	lRes = ::RegQueryValueEx(_hKey, pszValueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(pszValue),	&nBytes);
	if (lRes != ERROR_SUCCESS)
		return lRes;
	if (dwType != REG_MULTI_SZ)
		return ERROR_INVALID_DATA;
	if (pszValue != nullptr && (nBytes % sizeof(WCHAR) != 0 || 
		nBytes / sizeof(WCHAR) < 1 || pszValue[nBytes / sizeof(WCHAR) - 1] != 0 || 
		((nBytes / sizeof(WCHAR)) > 1 && pszValue[nBytes / sizeof(TCHAR) - 2] != 0)))
		return ERROR_INVALID_DATA;

	*pnChars = nBytes / sizeof(WCHAR);

	return ERROR_SUCCESS;
}

_Use_decl_annotations_
LONG RegistryKey::QueryBinaryValue(LPCTSTR pszValueName, void* pValue, ULONG* pnBytes) noexcept {
	LONG lRes;
	DWORD dwType;

	ATLASSERT(pnBytes);
	ATLASSUME(_hKey);

	lRes = ::RegQueryValueEx(_hKey, pszValueName, nullptr, &dwType, static_cast<LPBYTE>(pValue), pnBytes);
	if (lRes != ERROR_SUCCESS)
		return lRes;
	if (dwType != REG_BINARY && dwType != REG_RESOURCE_LIST && dwType != REG_RESOURCE_REQUIREMENTS_LIST && dwType != REG_FULL_RESOURCE_DESCRIPTOR)
		return ERROR_INVALID_DATA;

	return ERROR_SUCCESS;
}

_Use_decl_annotations_
LSTATUS RegistryKey::QueryValue(LPCTSTR pszValueName, DWORD* pdwType, void* pData, ULONG* pnBytes) noexcept {
	ATLASSUME(_hKey != NULL);

	return(::RegQueryValueEx(_hKey, pszValueName, NULL, pdwType, static_cast<LPBYTE>(pData), pnBytes));
}

_Use_decl_annotations_
LSTATUS RegistryKey::QueryDWORDValue(LPCTSTR pszValueName, DWORD& dwValue) noexcept {
	LONG lRes;
	ULONG nBytes;
	DWORD dwType;

	ATLASSUME(_hKey != NULL);

	nBytes = sizeof(DWORD);
	lRes = ::RegQueryValueEx(_hKey, pszValueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(&dwValue),
		&nBytes);
	if (lRes != ERROR_SUCCESS)
		return lRes;
	if (dwType != REG_DWORD)
		return ERROR_INVALID_DATA;

	return ERROR_SUCCESS;
}

_Use_decl_annotations_
LSTATUS RegistryKey::QueryQWORDValue(LPCTSTR pszValueName, ULONGLONG& qwValue) noexcept {
	LONG lRes;
	ULONG nBytes;
	DWORD dwType;

	ATLASSUME(_hKey);

	nBytes = sizeof(ULONGLONG);
	lRes = ::RegQueryValueEx(_hKey, pszValueName, NULL, &dwType, reinterpret_cast<LPBYTE>(&qwValue), &nBytes);
	if (lRes != ERROR_SUCCESS)
		return lRes;
	if (dwType != REG_QWORD)
		return ERROR_INVALID_DATA;

	return ERROR_SUCCESS;
}

_Use_decl_annotations_
LSTATUS RegistryKey::DeleteValue(LPCTSTR lpszValue) noexcept {
	ATLASSUME(_hKey);
	return ::RegDeleteValue(_hKey, (LPTSTR)lpszValue);
}

void RegistryKey::CheckPredefinedKey() {
	if ((DWORD_PTR)_hKey & 0xf000000000000000)	// predefined key
		_own = false;
}

