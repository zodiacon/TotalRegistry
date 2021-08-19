#include "pch.h"
#include "RegExportImport.h"
#include "Registry.h"

bool RegExportImport::Export(PCWSTR keyPath, PCWSTR path) const {
	auto hFile = ::CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	WriteString(hFile, L"Windows Registry Editor Version 5.00\n\n");

	auto key = Registry::OpenKey(keyPath, KEY_READ);
	if (!key)
		return false;

	bool success = ExportKeys(key, hFile, keyPath);
	::CloseHandle(hFile);
	return success;
}

bool RegExportImport::ExportKeys(HKEY hKey, HANDLE hFile, PCWSTR section) const {
	ExportKey(hKey, hFile, section);
	Registry::EnumSubKeys(hKey, [&](auto name, const auto&) {
		RegistryKey subKey;
		subKey.Open(hKey, name, KEY_READ);
		if(subKey)
			ExportKeys(subKey, hFile, section + CString(L"\\") + name);
		return true;
	});
	return true;
}

bool RegExportImport::ExportKey(HKEY hKey, HANDLE hFile, PCWSTR section) const {
	WriteString(hFile, CString(L"[") + section + L"]\n");

	RegistryKey key(hKey, false);
	Registry::EnumKeyValues(hKey, [&](auto type, auto name, auto size) {
		CString sname(name);
		if (sname.IsEmpty())
			sname = L"@";
		else
			sname = L"\"" + sname + L"\"";

		auto data = std::make_unique<BYTE[]>(size + 3);
		auto count{ size };
		if (ERROR_SUCCESS != key.QueryValue(name, nullptr, data.get(), &count))
			return true;

		switch (type) {
			case REG_DWORD: 
				WriteString(hFile, sname + (L"=dword:" + std::format(L"{:08x}\n", *(DWORD*)data.get())).c_str());
				break;
			
			case REG_SZ: {
				CString text(data.get());
				text.Replace(L"\"", L"\\\"");
				WriteString(hFile, sname + L"=\"" + text + L"\"\n");
				break;
			}

			case REG_BINARY:
				WriteString(hFile, sname + L"=hex:" + BytesToString(data.get(), count) + L"\n");
				break;

			default:
				WriteString(hFile, sname + std::format(L"=hex({:x}):", type).c_str() 
					+ BytesToString(data.get(), count) + L"\n");
				break;
		}

		return true;
	});
	WriteString(hFile, L"\n");
	return true;
}

CString RegExportImport::BytesToString(BYTE const* data, DWORD count) {
	CString text, chars;
	for (DWORD i = 0; i < count; i++) {
		chars.Format(L"%02X,", data[i]);
		if (i % 32 == 31 && i < count - 1)
			text += L"\\\n";
		text += chars;
	}
	return text.Left(text.GetLength() - 1);
}

bool RegExportImport::WriteString(HANDLE hFile, CString const& text) {
	DWORD bytes;
	return ::WriteFile(hFile, text.GetString(), text.GetLength() * sizeof(WCHAR), &bytes, nullptr);
}
