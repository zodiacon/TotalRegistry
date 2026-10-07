#include "pch.h"
#include "TestHelpers.h"
#include <catch2/catch_test_macros.hpp>
#include "Registry.h"
#include "KeyBackup.h"
#include <wil\resource.h>
#include <sddl.h>
#include <algorithm>
#include <atomic>

std::string Catch::StringMaker<CString>::convert(CString const& value) {
	return "\"" + TestHelpers::ToUtf8(value) + "\"";
}

namespace {
	void GrantOwnerFullControl(HKEY hParent, PCWSTR name) {
		PSECURITY_DESCRIPTOR p = nullptr;
		if (!::ConvertStringSecurityDescriptorToSecurityDescriptor(L"D:P(A;;KA;;;OW)", SDDL_REVISION_1, &p, nullptr))
			return;
		wil::unique_hlocal_security_descriptor sd(p);

		CRegKey key;
		if (ERROR_SUCCESS == key.Open(hParent, name, WRITE_DAC))
			::RegSetKeySecurity(key, DACL_SECURITY_INFORMATION, sd.get());
		key.Close();
		if (ERROR_SUCCESS != key.Open(hParent, name, KEY_ENUMERATE_SUB_KEYS))
			return;

		std::vector<CString> names;
		Registry::EnumSubKeys(key, [&](auto subName, const auto&) {
			names.push_back(subName);
			return true;
			});
		for (auto& subName : names)
			GrantOwnerFullControl(key, subName);
	}

	CString CurrentUserSid() {
		wil::unique_handle hToken;
		if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, hToken.addressof()))
			return L"";
		BYTE buffer[256];
		DWORD len;
		if (!::GetTokenInformation(hToken.get(), TokenUser, buffer, sizeof(buffer), &len))
			return L"";
		PWSTR sid = nullptr;
		if (!::ConvertSidToStringSid(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, &sid))
			return L"";
		CString result(sid);
		::LocalFree(sid);
		return result;
	}

	void DumpKey(HKEY hKey, std::string const& relative, std::vector<std::string>& lines) {
		lines.push_back("[" + relative + "]");

		std::vector<std::string> values;
		auto name = std::make_unique<WCHAR[]>(16384);
		for (DWORD i = 0; ; i++) {
			DWORD nameLen = 16384, type, size = 0;
			if (ERROR_SUCCESS != ::RegEnumValue(hKey, i, name.get(), &nameLen, nullptr, &type, nullptr, &size))
				break;
			std::vector<BYTE> data(size);
			::RegQueryValueEx(hKey, name.get(), nullptr, &type, data.data(), &size);
			std::string line = TestHelpers::ToUtf8(name.get()) + "=" + std::to_string(type) + ":";
			char hex[4];
			for (DWORD b = 0; b < size; b++) {
				sprintf_s(hex, "%02x", data[b]);
				line += hex;
			}
			values.push_back(line);
		}
		std::sort(values.begin(), values.end());
		lines.insert(lines.end(), values.begin(), values.end());

		std::vector<CString> subkeys;
		Registry::EnumSubKeys(hKey, [&](auto subName, const auto&) {
			subkeys.push_back(subName);
			return true;
			});
		std::sort(subkeys.begin(), subkeys.end(), [](auto& a, auto& b) { return a.CompareNoCase(b) < 0; });
		for (auto& subName : subkeys) {
			CRegKey sub;
			if (ERROR_SUCCESS == sub.Open(hKey, subName, KEY_READ))
				DumpKey(sub, relative.empty() ? TestHelpers::ToUtf8(subName) : relative + "\\" + TestHelpers::ToUtf8(subName), lines);
			else
				lines.push_back("[" + relative + "\\" + TestHelpers::ToUtf8(subName) + "] (no access)");
		}
	}
}

CString const& TestHelpers::ProcessRoot() {
	static CString const root = [] {
		CString path;
		path.Format(L"Software\\TotalRegistryTests\\%u", ::GetCurrentProcessId());
		return path;
	}();
	return root;
}

void TestHelpers::Cleanup() {
	ForceDeleteTree(HKEY_CURRENT_USER, ProcessRoot());
	// only succeeds if no other test run is using it
	::RegDeleteKey(HKEY_CURRENT_USER, L"Software\\TotalRegistryTests");
	KeyBackup::DeleteProcessBackups();
}

LSTATUS TestHelpers::ForceDeleteTree(HKEY hParent, PCWSTR name) {
	auto error = ::RegDeleteTree(hParent, name);
	if (error == ERROR_ACCESS_DENIED) {
		GrantOwnerFullControl(hParent, name);
		error = ::RegDeleteTree(hParent, name);
	}
	return error;
}

void TestHelpers::SetDacl(CString const& path, CString sddl) {
	sddl.Replace(L"$USER", CurrentUserSid());
	PSECURITY_DESCRIPTOR p = nullptr;
	REQUIRE(::ConvertStringSecurityDescriptorToSecurityDescriptor(sddl, SDDL_REVISION_1, &p, nullptr));
	wil::unique_hlocal_security_descriptor sd(p);
	auto key = Registry::OpenKey(path, WRITE_DAC);
	REQUIRE(key);
	REQUIRE(ERROR_SUCCESS == ::RegSetKeySecurity(key, DACL_SECURITY_INFORMATION, sd.get()));
}

std::string TestHelpers::ToUtf8(PCWSTR text) {
	auto len = ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
	std::string result(len, '\0');
	::WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), len, nullptr, nullptr);
	result.resize(len - 1);
	return result;
}

std::vector<BYTE> TestHelpers::ReadFileBytes(CString const& path) {
	wil::unique_hfile hFile(::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
	REQUIRE(hFile);
	LARGE_INTEGER size;
	REQUIRE(::GetFileSizeEx(hFile.get(), &size));
	std::vector<BYTE> data(size.LowPart);
	DWORD bytes = 0;
	if (!data.empty())
		REQUIRE(::ReadFile(hFile.get(), data.data(), size.LowPart, &bytes, nullptr));
	return data;
}

void TestHelpers::WriteFileBytes(CString const& path, std::vector<BYTE> const& data) {
	wil::unique_hfile hFile(::CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	REQUIRE(hFile);
	DWORD bytes;
	REQUIRE(::WriteFile(hFile.get(), data.data(), (DWORD)data.size(), &bytes, nullptr));
}

void TestHelpers::WriteUnicodeFile(CString const& path, CString const& text, bool bom) {
	std::vector<BYTE> data;
	if (bom)
		data = { 0xFF, 0xFE };
	auto p = (BYTE const*)text.GetString();
	data.insert(data.end(), p, p + text.GetLength() * sizeof(WCHAR));
	WriteFileBytes(path, data);
}

void TestHelpers::WriteAnsiFile(CString const& path, CString const& text, UINT codePage, bool utf8Bom) {
	std::vector<BYTE> data;
	if (utf8Bom)
		data = { 0xEF, 0xBB, 0xBF };
	auto len = ::WideCharToMultiByte(codePage, 0, text, text.GetLength(), nullptr, 0, nullptr, nullptr);
	std::vector<char> chars(len);
	::WideCharToMultiByte(codePage, 0, text, text.GetLength(), chars.data(), len, nullptr, nullptr);
	data.insert(data.end(), chars.begin(), chars.end());
	WriteFileBytes(path, data);
}

ScratchKey::ScratchKey() {
	static std::atomic<int> counter;
	m_SubPath.Format(L"%s\\%d", (PCWSTR)TestHelpers::ProcessRoot(), ++counter);
	m_Path = L"HKEY_CURRENT_USER\\" + m_SubPath;
	CRegKey key;
	REQUIRE(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, m_SubPath));
}

ScratchKey::~ScratchKey() {
	TestHelpers::ForceDeleteTree(HKEY_CURRENT_USER, m_SubPath);
}

CString const& ScratchKey::Path() const {
	return m_Path;
}

CString ScratchKey::operator/(PCWSTR sub) const {
	return m_Path + L"\\" + sub;
}

void ScratchKey::CreateKey(PCWSTR sub) const {
	CRegKey key;
	REQUIRE(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, m_SubPath + L"\\" + sub));
}

void ScratchKey::SetValue(PCWSTR sub, PCWSTR name, DWORD type, void const* data, DWORD size) const {
	CRegKey key;
	REQUIRE(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, sub && *sub ? m_SubPath + L"\\" + sub : m_SubPath));
	REQUIRE(ERROR_SUCCESS == ::RegSetValueEx(key, name, 0, type, (BYTE const*)data, size));
}

void ScratchKey::SetString(PCWSTR sub, PCWSTR name, PCWSTR value, DWORD type) const {
	SetValue(sub, name, type, value, DWORD(wcslen(value) + 1) * sizeof(WCHAR));
}

void ScratchKey::SetDword(PCWSTR sub, PCWSTR name, DWORD value) const {
	SetValue(sub, name, REG_DWORD, &value, sizeof(value));
}

bool ScratchKey::Exists(PCWSTR sub) const {
	CRegKey key;
	return ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, sub && *sub ? m_SubPath + L"\\" + sub : m_SubPath, KEY_READ);
}

std::vector<std::string> ScratchKey::Dump(PCWSTR sub) const {
	std::vector<std::string> lines;
	CRegKey key;
	if (ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, sub && *sub ? m_SubPath + L"\\" + sub : m_SubPath, KEY_READ))
		return { "(missing)" };
	DumpKey(key, "", lines);
	return lines;
}

TempDir::TempDir() {
	static std::atomic<int> counter;
	WCHAR temp[MAX_PATH];
	::GetTempPath(_countof(temp), temp);
	m_Path.Format(L"%sTotalRegistryTests-%u-%d", temp, ::GetCurrentProcessId(), ++counter);
	REQUIRE(::CreateDirectory(m_Path, nullptr));
}

TempDir::~TempDir() {
	WIN32_FIND_DATA fd;
	wil::unique_hfind hFind(::FindFirstFile(m_Path + L"\\*", &fd));
	if (hFind) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				::DeleteFile(m_Path + L"\\" + fd.cFileName);
		} while (::FindNextFile(hFind.get(), &fd));
	}
	hFind.reset();
	::RemoveDirectory(m_Path);
}

CString TempDir::operator/(PCWSTR name) const {
	return m_Path + L"\\" + name;
}
