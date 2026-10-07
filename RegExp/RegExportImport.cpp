#include "pch.h"
#include "RegExportImport.h"
#include "Registry.h"
#include "Helpers.h"
#include <wil\resource.h>
#include <set>

bool RegExportImport::Export(PCWSTR keyPath, PCWSTR path) const {
	auto key = Registry::OpenKey(keyPath, KEY_READ);
	if (!key)
		return false;

	wil::unique_hfile hFile(::CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	if (!hFile)
		return false;

	// RegEdit requires a UTF-16LE BOM for "Version 5.00" files
	static const BYTE bom[] = { 0xFF, 0xFE };
	DWORD bytes;
	if (!::WriteFile(hFile.get(), bom, sizeof(bom), &bytes, nullptr))
		return false;

	if (!WriteString(hFile.get(), L"Windows Registry Editor Version 5.00\r\n\r\n"))
		return false;

	return ExportKeys(key, hFile.get(), keyPath);
}

bool RegExportImport::ExportKeys(HKEY hKey, HANDLE hFile, PCWSTR section) const {
	if (!ExportKey(hKey, hFile, section))
		return false;

	bool success = true;
	Registry::EnumSubKeys(hKey, [&](auto name, const auto&) {
		RegistryKey subKey;
		subKey.Open(hKey, name, KEY_READ);
		if (subKey && !ExportKeys(subKey, hFile, section + CString(L"\\") + name)) {
			success = false;
			return false;
		}
		return true;
	});
	return success;
}

bool RegExportImport::ExportKey(HKEY hKey, HANDLE hFile, PCWSTR section) const {
	if (!WriteString(hFile, CString(L"[") + section + L"]\r\n"))
		return false;

	bool success = true;
	RegistryKey key(hKey, false);
	Registry::EnumKeyValues(hKey, [&](auto type, auto name, auto size) {
		CString sname(name);
		if (sname.IsEmpty())
			sname = L"@";
		else
			sname = L"\"" + EscapeString(sname) + L"\"";

		// extra zeroed bytes guarantee a NULL terminator for strings
		auto data = std::make_unique<BYTE[]>(size + 4);
		auto count{ size };
		if (ERROR_SUCCESS != key.QueryValue(name, nullptr, data.get(), &count))
			return true;

		CString line;
		if (type == REG_DWORD && count == sizeof(DWORD)) {
			line = sname + std::format(L"=dword:{:08x}", *(DWORD*)data.get()).c_str();
		}
		else if (type == REG_SZ && count % sizeof(WCHAR) == 0 && (count == 0 || wcslen((PCWSTR)data.get()) >= count / sizeof(WCHAR) - 1)) {
			// only use the string form if there are no embedded NULLs
			line = sname + L"=\"" + EscapeString((PCWSTR)data.get()) + L"\"";
		}
		else if (type == REG_BINARY) {
			line = sname + L"=hex:" + BytesToString(data.get(), count);
		}
		else {
			line = sname + std::format(L"=hex({:x}):", type).c_str() + BytesToString(data.get(), count);
		}
		success = WriteString(hFile, line + L"\r\n");
		return success;
	});
	return success && WriteString(hFile, L"\r\n");
}

CString RegExportImport::EscapeString(CString text) {
	text.Replace(L"\\", L"\\\\");
	text.Replace(L"\"", L"\\\"");
	return text;
}

CString RegExportImport::BytesToString(BYTE const* data, DWORD count) {
	CString text, chars;
	for (DWORD i = 0; i < count; i++) {
		chars.Format(L"%02x,", data[i]);
		if (i % 25 == 24 && i < count - 1)
			chars += L"\\\r\n  ";
		text += chars;
	}
	return text.Left(text.GetLength() - 1);
}

bool RegExportImport::WriteString(HANDLE hFile, CString const& text) {
	DWORD bytes;
	return ::WriteFile(hFile, text.GetString(), text.GetLength() * sizeof(WCHAR), &bytes, nullptr);
}

bool RegExportImport::Parse(PCWSTR path, std::vector<RegFileKey>& keys, CString& error) {
	keys.clear();
	wil::unique_hfile hFile(::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
	if (!hFile) {
		error = Helpers::GetErrorText();
		return false;
	}

	LARGE_INTEGER size;
	if (!::GetFileSizeEx(hFile.get(), &size) || size.QuadPart > (1 << 30)) {
		error = L"File is too large";
		return false;
	}
	if (size.QuadPart == 0) {
		error = L"File is empty";
		return false;
	}
	std::vector<BYTE> buffer(size.LowPart);
	DWORD bytes;
	if (!::ReadFile(hFile.get(), buffer.data(), size.LowPart, &bytes, nullptr) || bytes != size.LowPart) {
		error = Helpers::GetErrorText();
		return false;
	}

	auto text = DecodeText(buffer);
	std::vector<CString> lines;
	int start = 0;
	while (start <= text.GetLength()) {
		auto end = text.Find(L'\n', start);
		if (end < 0)
			end = text.GetLength();
		lines.push_back(text.Mid(start, end - start).TrimRight(L"\r"));
		start = end + 1;
	}

	//
	// the first non-empty line determines the format
	//
	size_t i = 0;
	while (i < lines.size() && CString(lines[i]).Trim().IsEmpty())
		i++;
	if (i == lines.size()) {
		error = L"File is empty";
		return false;
	}
	auto header = lines[i].Trim();
	bool unicode;
	if (header == L"Windows Registry Editor Version 5.00")
		unicode = true;
	else if (header == L"REGEDIT4")
		unicode = false;
	else {
		error = L"Not a valid registry file (missing header)";
		return false;
	}

	RegFileKey* current = nullptr;
	for (i++; i < lines.size(); i++) {
		auto lineNumber = (unsigned)i + 1;
		auto line = lines[i].Trim();
		if (line.IsEmpty() || line[0] == L';')
			continue;

		if (line[0] == L'[') {
			auto end = line.ReverseFind(L']');
			if (end < 0) {
				error.Format(L"Line %u: missing ']'", lineNumber);
				return false;
			}
			RegFileKey key;
			auto keyPath = line.Mid(1, end - 1).Trim();
			if (!keyPath.IsEmpty() && keyPath[0] == L'-') {
				key.Delete = true;
				keyPath = keyPath.Mid(1).Trim();
			}
			keyPath.TrimRight(L'\\');
			if (!IsValidKeyPath(keyPath)) {
				error.Format(L"Line %u: invalid key path '%s'", lineNumber, (PCWSTR)keyPath);
				return false;
			}
			key.Path = keyPath;
			keys.push_back(std::move(key));
			current = &keys.back();
			continue;
		}

		if (!current) {
			error.Format(L"Line %u: value outside of a key", lineNumber);
			return false;
		}

		RegFileValue value;
		int pos = 0;
		if (line[0] == L'@') {
			pos = 1;
		}
		else if (line[0] != L'"' || !ParseQuoted(line, pos, value.Name)) {
			error.Format(L"Line %u: invalid value name", lineNumber);
			return false;
		}
		while (pos < line.GetLength() && iswspace(line[pos]))
			pos++;
		if (pos == line.GetLength() || line[pos] != L'=') {
			error.Format(L"Line %u: missing '='", lineNumber);
			return false;
		}
		auto data = line.Mid(pos + 1).Trim();
		if (data.Left(3).CompareNoCase(L"hex") == 0) {
			// hex data may continue on following lines
			while (data.Right(1) == L"\\" && i + 1 < lines.size()) {
				data = data.Left(data.GetLength() - 1) + lines[++i].Trim();
			}
		}
		if (!ParseData(data, unicode, value)) {
			error.Format(L"Line %u: invalid data for value '%s'", lineNumber, value.Name.IsEmpty() ? L"(Default)" : (PCWSTR)value.Name);
			return false;
		}
		// values in a deleted key are ignored, like RegEdit does
		if (!current->Delete)
			current->Values.push_back(std::move(value));
	}
	return true;
}

CString RegExportImport::DecodeText(std::vector<BYTE> const& buffer) {
	auto size = buffer.size();
	if (size >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE)
		return CString((PCWSTR)(buffer.data() + 2), int((size - 2) / sizeof(WCHAR)));

	// UTF-16LE without a BOM (written by older versions of this tool)
	if (size >= 2 && buffer[0] != 0 && buffer[1] == 0)
		return CString((PCWSTR)buffer.data(), int(size / sizeof(WCHAR)));

	UINT cp = CP_ACP;
	int skip = 0;
	if (size >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
		cp = CP_UTF8;
		skip = 3;
	}
	auto chars = (PCSTR)buffer.data() + skip;
	auto count = int(size - skip);
	CString text;
	auto len = ::MultiByteToWideChar(cp, 0, chars, count, nullptr, 0);
	if (len > 0) {
		::MultiByteToWideChar(cp, 0, chars, count, text.GetBuffer(len), len);
		text.ReleaseBuffer(len);
	}
	return text;
}

bool RegExportImport::ParseQuoted(CString const& text, int& pos, CString& result) {
	ATLASSERT(text[pos] == L'"');
	result.Empty();
	for (pos++; pos < text.GetLength(); pos++) {
		auto ch = text[pos];
		if (ch == L'"') {
			pos++;
			return true;
		}
		if (ch == L'\\' && pos + 1 < text.GetLength()) {
			auto next = text[++pos];
			if (next != L'\\' && next != L'"')
				result += ch;
			ch = next;
		}
		result += ch;
	}
	return false;
}

bool RegExportImport::ParseData(CString const& text, bool unicode, RegFileValue& value) {
	if (text == L"-") {
		value.Delete = true;
		return true;
	}

	if (!text.IsEmpty() && text[0] == L'"') {
		int pos = 0;
		CString str;
		if (!ParseQuoted(text, pos, str) || !text.Mid(pos).Trim().IsEmpty())
			return false;
		value.Type = REG_SZ;
		auto data = (BYTE const*)str.GetString();
		value.Data.assign(data, data + (str.GetLength() + 1) * sizeof(WCHAR));
		return true;
	}

	if (text.Left(6).CompareNoCase(L"dword:") == 0) {
		auto digits = text.Mid(6).Trim();
		if (digits.IsEmpty() || digits.GetLength() > 8 || digits.SpanIncluding(L"0123456789abcdefABCDEF") != digits)
			return false;
		auto number = wcstoul(digits, nullptr, 16);
		value.Type = REG_DWORD;
		value.Data.assign((BYTE const*)&number, (BYTE const*)&number + sizeof(DWORD));
		return true;
	}

	if (text.Left(3).CompareNoCase(L"hex") != 0)
		return false;

	int pos = 3;
	value.Type = REG_BINARY;
	if (pos < text.GetLength() && text[pos] == L'(') {
		auto end = text.Find(L')', pos);
		if (end < 0)
			return false;
		auto digits = text.Mid(pos + 1, end - pos - 1).Trim();
		if (digits.IsEmpty() || digits.GetLength() > 8 || digits.SpanIncluding(L"0123456789abcdefABCDEF") != digits)
			return false;
		value.Type = wcstoul(digits, nullptr, 16);
		pos = end + 1;
	}
	if (pos >= text.GetLength() || text[pos] != L':')
		return false;
	if (!ParseHex(text.Mid(pos + 1), value.Data))
		return false;

	if (!unicode && (value.Type == REG_EXPAND_SZ || value.Type == REG_MULTI_SZ) && !value.Data.empty()) {
		// REGEDIT4 files store these as ANSI
		auto chars = (PCSTR)value.Data.data();
		auto count = (int)value.Data.size();
		auto len = ::MultiByteToWideChar(CP_ACP, 0, chars, count, nullptr, 0);
		std::vector<WCHAR> wide(len);
		::MultiByteToWideChar(CP_ACP, 0, chars, count, wide.data(), len);
		value.Data.assign((BYTE const*)wide.data(), (BYTE const*)(wide.data() + len));
	}
	return true;
}

bool RegExportImport::ParseHex(CString text, std::vector<BYTE>& bytes) {
	bytes.clear();
	text.Remove(L' ');
	text.Remove(L'\t');
	if (text.IsEmpty())
		return true;

	int start = 0;
	while (start <= text.GetLength()) {
		auto end = text.Find(L',', start);
		if (end < 0)
			end = text.GetLength();
		auto token = text.Mid(start, end - start);
		if (token.IsEmpty() || token.GetLength() > 2 || token.SpanIncluding(L"0123456789abcdefABCDEF") != token)
			return false;
		bytes.push_back((BYTE)wcstoul(token, nullptr, 16));
		start = end + 1;
	}
	return true;
}

bool RegExportImport::IsValidKeyPath(CString const& path) {
	auto root = path.Left(path.Find(L'\\') < 0 ? path.GetLength() : path.Find(L'\\'));
	if (root.IsEmpty())
		return false;

	return std::ranges::any_of(Registry::Keys, [&](auto& k) {
		return _wcsicmp(k.text, root) == 0 || (*k.stext && _wcsicmp(k.stext, root) == 0);
		});
}

namespace {
	SnapshotValue MakeValue(CString const& name, DWORD type, BYTE const* data, DWORD size) {
		SnapshotValue value;
		value.Name = name;
		value.Type = type;
		value.Size = size;
		value.Preview.assign(data, data + std::min(size, SnapshotValue::PreviewSize));
		return value;
	}

	bool ReadValue(HKEY hKey, PCWSTR name, DWORD& type, std::vector<BYTE>& data) {
		DWORD size = 0;
		if (ERROR_SUCCESS != ::RegQueryValueEx(hKey, name, nullptr, &type, nullptr, &size))
			return false;
		data.resize(size);
		if (ERROR_SUCCESS != ::RegQueryValueEx(hKey, name, nullptr, &type, data.data(), &size))
			return false;
		data.resize(size);
		return true;
	}
}

std::vector<SnapshotChange> RegExportImport::Preview(std::vector<RegFileKey> const& keys) {
	std::vector<SnapshotChange> changes;
	// keys reported as created, so a later section under them doesn't report them again
	std::set<CString> created;
	auto isCreated = [&](CString const& path) {
		return created.contains(CString(path).MakeUpper());
	};

	for (auto& key : keys) {
		CString subKey;
		auto hRoot = Registry::GetRootKey(key.Path, subKey);
		if (!hRoot)
			continue;

		if (key.Delete) {
			// a symbolic link: only the link goes, not its target's contents
			if (auto bs = key.Path.ReverseFind(L'\\'); bs > 0) {
				auto parent = Registry::OpenKey(key.Path.Left(bs), KEY_READ);
				CString target;
				if (parent && Registry::IsKeyLink(parent, key.Path.Mid(bs + 1), target)) {
					changes.push_back({ SnapshotChangeType::KeyDeleted, key.Path });
					continue;
				}
			}
			// everything under the key goes
			RegistrySnapshot snapshot;
			if (snapshot.Take(key.Path)) {
				for (auto& k : snapshot.GetKeys()) {
					auto path = k.Path.IsEmpty() ? key.Path : key.Path + L"\\" + k.Path;
					changes.push_back({ SnapshotChangeType::KeyDeleted, path });
					for (auto& value : k.Values) {
						SnapshotChange change{ SnapshotChangeType::ValueDeleted, path, value.Name };
						change.Old = value;
						changes.push_back(std::move(change));
					}
				}
			}
			else if (::GetLastError() != ERROR_FILE_NOT_FOUND)
				changes.push_back({ SnapshotChangeType::NoAccess, key.Path });
			continue;
		}

		CRegKey hKey;
		auto error = hKey.Open(hRoot, subKey, KEY_READ);
		if (error == ERROR_FILE_NOT_FOUND || isCreated(key.Path)) {
			// the key, and any missing parents, are created
			auto path = key.Path.Left(key.Path.GetLength() - subKey.GetLength());
			path.TrimRight(L'\\');
			CString relative;
			int pos = 0;
			for (auto name = subKey.Tokenize(L"\\", pos); !name.IsEmpty(); name = subKey.Tokenize(L"\\", pos)) {
				path += L"\\" + name;
				relative += (relative.IsEmpty() ? L"" : L"\\") + name;
				CRegKey existing;
				if (!isCreated(path) && existing.Open(hRoot, relative, KEY_READ) == ERROR_FILE_NOT_FOUND) {
					created.insert(CString(path).MakeUpper());
					changes.push_back({ SnapshotChangeType::KeyAdded, path });
				}
			}
			for (auto& value : key.Values) {
				if (value.Delete)
					continue;
				SnapshotChange change{ SnapshotChangeType::ValueAdded, key.Path, value.Name };
				change.New = MakeValue(value.Name, value.Type, value.Data.data(), (DWORD)value.Data.size());
				changes.push_back(std::move(change));
			}
			continue;
		}
		if (error != ERROR_SUCCESS) {
			changes.push_back({ SnapshotChangeType::NoAccess, key.Path });
			continue;
		}

		std::vector<BYTE> data;
		for (auto& value : key.Values) {
			DWORD type;
			bool exists = ReadValue(hKey, value.Name, type, data);
			if (value.Delete) {
				if (exists) {
					SnapshotChange change{ SnapshotChangeType::ValueDeleted, key.Path, value.Name };
					change.Old = MakeValue(value.Name, type, data.data(), (DWORD)data.size());
					changes.push_back(std::move(change));
				}
				continue;
			}
			if (exists && type == value.Type && data == value.Data)
				continue;		// unchanged

			SnapshotChange change{ exists ? SnapshotChangeType::ValueChanged : SnapshotChangeType::ValueAdded, key.Path, value.Name };
			if (exists)
				change.Old = MakeValue(value.Name, type, data.data(), (DWORD)data.size());
			change.New = MakeValue(value.Name, value.Type, value.Data.data(), (DWORD)value.Data.size());
			changes.push_back(std::move(change));
		}
	}
	return changes;
}
