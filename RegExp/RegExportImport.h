#pragma once

struct RegFileValue {
	CString Name;
	DWORD Type{ REG_NONE };
	std::vector<BYTE> Data;
	bool Delete{ false };
};

struct RegFileKey {
	CString Path;
	bool Delete{ false };
	std::vector<RegFileValue> Values;
};

struct RegExportImport {
	bool Export(PCWSTR key, PCWSTR path) const;
	static bool Parse(PCWSTR path, std::vector<RegFileKey>& keys, CString& error);

private:
	bool ExportKeys(HKEY hKey, HANDLE hFile, PCWSTR section) const;
	bool ExportKey(HKEY hKey, HANDLE hFile, PCWSTR section) const;
	static CString BytesToString(BYTE const* data, DWORD count);
	static CString EscapeString(CString text);
	static bool WriteString(HANDLE hFile, CString const& text);

	static CString DecodeText(std::vector<BYTE> const& buffer);
	static bool ParseQuoted(CString const& text, int& pos, CString& result);
	static bool ParseData(CString const& text, bool unicode, RegFileValue& value);
	static bool ParseHex(CString text, std::vector<BYTE>& bytes);
	static bool IsValidKeyPath(CString const& path);
};
