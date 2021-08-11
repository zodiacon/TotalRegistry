#pragma once

struct RegExportImport {
	bool Export(PCWSTR key, PCWSTR path) const;
	bool Import(PCWSTR path, HKEY hKey);

private:
	bool ExportKeys(HKEY hKey, HANDLE hFile, PCWSTR section) const;
	bool ExportKey(HKEY hKey, HANDLE hFile, PCWSTR section) const;
	static CString BytesToString(BYTE const* data, DWORD count);
	static bool WriteString(HANDLE hFile, CString const& text);
};

