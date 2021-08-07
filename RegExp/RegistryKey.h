#pragma once

struct RegistryKey {
	explicit RegistryKey(HKEY hKey = nullptr, bool own = true) noexcept;
	RegistryKey(const RegistryKey&) = delete;
	RegistryKey& operator=(const RegistryKey&) = delete;
	RegistryKey(RegistryKey&&) noexcept;
	RegistryKey& operator=(RegistryKey&&) noexcept;

	~RegistryKey() {
		Close();
	}

	void Close();
	HKEY Detach();
	void Attach(HKEY hKey, bool own = true);

	bool IsValid() const;

	operator bool() const {
		return _hKey != nullptr;
	}

	HKEY* AddressOf() {
		return &_hKey;
	}

	operator HKEY() const {
		return _hKey;
	}

	HKEY Get() const {
		return _hKey;
	}

	LSTATUS Open(HKEY parent, PCWSTR path, DWORD access = KEY_READ | KEY_WRITE);
	LSTATUS SetValue(_In_opt_z_ LPCTSTR pszValueName, _In_ DWORD dwType, _In_opt_ const void* pValue, _In_ ULONG nBytes) noexcept;
	LSTATUS SetBinaryValue(_In_opt_z_ LPCTSTR pszValueName,	_In_opt_ const void* pValue, _In_ ULONG nBytes) noexcept;
	LSTATUS SetDWORDValue(_In_opt_z_ LPCTSTR pszValueName, _In_ DWORD dwValue) noexcept;
	LSTATUS SetQWORDValue(_In_opt_z_ LPCTSTR pszValueName, _In_ ULONGLONG qwValue) noexcept;
	LSTATUS SetStringValue(_In_opt_z_ LPCTSTR pszValueName,	_In_opt_z_ LPCTSTR pszValue, _In_ DWORD dwType = REG_SZ) noexcept;
	LSTATUS SetMultiStringValue(_In_opt_z_ LPCTSTR pszValueName, _In_z_ LPCTSTR pszValue) noexcept;
	LSTATUS SetValue(_In_ DWORD dwValue, _In_opt_z_ LPCTSTR pszValueName) noexcept;
	LSTATUS QueryValue(_Out_writes_to_opt_(*pdwCount, *pdwCount) LPTSTR szValue, _In_opt_z_ LPCTSTR lpszValueName, _Inout_ DWORD* pdwCount);
	LSTATUS QueryValue(_In_opt_z_ LPCTSTR pszValueName,	_Out_opt_ DWORD* pdwType, _Out_opt_ void* pData, _Inout_ ULONG* pnBytes) noexcept;
	LSTATUS QueryBinaryValue(_In_opt_z_ LPCTSTR pszValueName, _Out_opt_ void* pValue, _Inout_opt_ ULONG* pnBytes) noexcept;
	LSTATUS QueryDWORDValue(_In_opt_z_ LPCTSTR pszValueName, _Out_ DWORD& dwValue) noexcept;
	LSTATUS QueryQWORDValue(_In_opt_z_ LPCTSTR pszValueName, _Out_ ULONGLONG& qwValue) noexcept;
	LSTATUS QueryStringValue(_In_opt_z_ LPCTSTR pszValueName, _Out_writes_to_opt_(*pnChars, *pnChars) LPTSTR pszValue, _Inout_ ULONG* pnChars) noexcept;
	LSTATUS QueryMultiStringValue(_In_opt_z_ LPCTSTR pszValueName, _Out_writes_to_opt_(*pnChars, *pnChars) LPTSTR pszValue,	_Inout_ ULONG* pnChars) noexcept;
	LSTATUS DeleteSubKey(_In_z_ LPCTSTR lpszSubKey) noexcept;
	LSTATUS DeleteValue(_In_z_ LPCTSTR lpszValue) noexcept;

private:
	void CheckPredefinedKey();

	HKEY _hKey;
	bool _own;
};

