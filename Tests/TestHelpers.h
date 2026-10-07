#pragma once

#include <catch2/catch_tostring.hpp>
#include <string>
#include <vector>

// lets Catch2 print CString values in assertion messages
template<>
struct Catch::StringMaker<CString> {
	static std::string convert(CString const& value);
};

namespace TestHelpers {
	// root of all test keys, under HKEY_CURRENT_USER; one subkey per test process
	CString const& ProcessRoot();
	// deletes everything this test process created in the Registry, including its KeyBackup key
	void Cleanup();

	// deletes a tree even if its permissions deny it, by giving the owner (the user who created it) full control
	LSTATUS ForceDeleteTree(HKEY hParent, PCWSTR name);
	// sets a key's DACL from SDDL, where $USER is replaced by the current user's SID
	void SetDacl(CString const& path, CString sddl);

	std::string ToUtf8(PCWSTR text);
	std::vector<BYTE> ReadFileBytes(CString const& path);
	void WriteFileBytes(CString const& path, std::vector<BYTE> const& data);
	// writes text as UTF-16LE with a BOM (the format of RegEdit 5 files)
	void WriteUnicodeFile(CString const& path, CString const& text, bool bom = true);
	void WriteAnsiFile(CString const& path, CString const& text, UINT codePage = CP_ACP, bool utf8Bom = false);
}

//
// a key used by a single test, deleted with its subkeys when the test ends
//
class ScratchKey {
public:
	ScratchKey();
	~ScratchKey();
	ScratchKey(ScratchKey const&) = delete;
	ScratchKey& operator=(ScratchKey const&) = delete;

	// full path, e.g. HKEY_CURRENT_USER\Software\TotalRegistryTests\1234\5
	CString const& Path() const;
	// full path of a subkey
	CString operator/(PCWSTR sub) const;

	void CreateKey(PCWSTR sub) const;
	void SetValue(PCWSTR sub, PCWSTR name, DWORD type, void const* data, DWORD size) const;
	void SetString(PCWSTR sub, PCWSTR name, PCWSTR value, DWORD type = REG_SZ) const;
	void SetDword(PCWSTR sub, PCWSTR name, DWORD value) const;
	bool Exists(PCWSTR sub = nullptr) const;

	// the whole tree as sorted lines ("[relative key]", "name=type:hex bytes"), to compare Registry states
	std::vector<std::string> Dump(PCWSTR sub = nullptr) const;

private:
	CString m_SubPath;	// under HKEY_CURRENT_USER
	CString m_Path;
};

//
// a directory under %TEMP% for test files, deleted when the test ends
//
class TempDir {
public:
	TempDir();
	~TempDir();
	TempDir(TempDir const&) = delete;
	TempDir& operator=(TempDir const&) = delete;

	CString operator/(PCWSTR name) const;

private:
	CString m_Path;
};
