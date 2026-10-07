#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "RegistrySnapshot.h"
#include <wil\resource.h>
#include <sddl.h>

using namespace TestHelpers;

extern "C" NTSTATUS NTAPI NtDeleteKey(HANDLE key);

namespace {
	std::vector<std::string> Sorted(std::vector<std::string> lines) {
		std::sort(lines.begin(), lines.end());
		return lines;
	}

	// "<change> <relative key> [<value>] <old> -> <new>", sorted
	std::vector<std::string> Describe(std::vector<SnapshotChange> const& changes, CString const& root) {
		std::vector<std::string> lines;
		for (auto& c : changes) {
			auto line = ToUtf8(RegistrySnapshot::GetChangeTypeName(c.Type));
			if (auto relative = c.Key.Mid(root.GetLength()); !relative.IsEmpty())
				line += " " + ToUtf8(relative);
			if (c.Type != SnapshotChangeType::KeyAdded && c.Type != SnapshotChangeType::KeyDeleted)
				line += " [" + ToUtf8(c.Value) + "]";
			if (c.Old)
				line += " " + ToUtf8(RegistrySnapshot::FormatData(*c.Old));
			if (c.New)
				line += " -> " + ToUtf8(RegistrySnapshot::FormatData(*c.New));
			lines.push_back(line);
		}
		std::sort(lines.begin(), lines.end());
		return lines;
	}

	void CreateTree(ScratchKey const& scratch) {
		scratch.SetString(L"Root", L"", L"default");
		scratch.SetString(L"Root", L"Same", L"unchanged");
		scratch.SetString(L"Root", L"Changes", L"before");
		scratch.SetDword(L"Root", L"Number", 1);
		scratch.SetString(L"Root", L"Goes", L"away");
		scratch.SetString(L"Root\\Sub", L"X", L"x");
		scratch.SetString(L"Root\\Doomed\\Deeper", L"D", L"d");
	}
}

TEST_CASE("Snapshot records a tree", "[snapshot]") {
	ScratchKey scratch;
	CreateTree(scratch);
	RegistrySnapshot snapshot;
	REQUIRE(snapshot.Take(scratch / L"Root"));

	CHECK(snapshot.GetRoot() == scratch / L"Root");
	auto& keys = snapshot.GetKeys();
	REQUIRE(keys.size() == 4);
	CHECK(keys[0].Path.IsEmpty());
	CHECK(keys[1].Path == L"Doomed");
	CHECK(keys[2].Path == L"Doomed\\Deeper");
	CHECK(keys[3].Path == L"Sub");
	CHECK(snapshot.GetValueCount() == 7);
	CHECK(snapshot.GetInaccessibleKeys() == 0);
	// values sorted by name
	REQUIRE(keys[0].Values.size() == 5);
	CHECK(keys[0].Values[0].Name.IsEmpty());
	CHECK(keys[0].Values[1].Name == L"Changes");
}

TEST_CASE("Snapshot of a missing key fails", "[snapshot]") {
	ScratchKey scratch;
	RegistrySnapshot snapshot;
	CHECK_FALSE(snapshot.Take(scratch / L"Missing"));
}

TEST_CASE("Comparing snapshots finds every kind of change", "[snapshot]") {
	ScratchKey scratch;
	CreateTree(scratch);
	auto root = scratch / L"Root";
	RegistrySnapshot before;
	REQUIRE(before.Take(root));

	scratch.SetString(L"Root", L"Changes", L"after");
	scratch.SetDword(L"Root", L"Number", 2);
	REQUIRE(ERROR_SUCCESS == ::RegDeleteKeyValue(HKEY_CURRENT_USER, root.Mid(18), L"Goes"));
	scratch.SetString(L"Root", L"New", L"value");
	REQUIRE(ERROR_SUCCESS == ::RegDeleteTree(HKEY_CURRENT_USER, (root + L"\\Doomed").Mid(18)));
	scratch.SetString(L"Root\\Born", L"B", L"b");
	// a type change with the same bytes is a change too
	scratch.SetString(L"Root\\Sub", L"X", L"x", REG_EXPAND_SZ);

	RegistrySnapshot after;
	REQUIRE(after.Take(root));
	CHECK(Describe(RegistrySnapshot::Compare(before, after), root) == Sorted({
		"Key added \\Born",
		"Key deleted \\Doomed",
		"Key deleted \\Doomed\\Deeper",
		"Value added \\Born [B] -> b",
		"Value added [New] -> value",
		"Value changed [Changes] before -> after",
		"Value changed [Number] 0x00000001 (1) -> 0x00000002 (2)",
		"Value changed \\Sub [X] x -> x",
		"Value deleted [Goes] away",
		"Value deleted \\Doomed\\Deeper [D] d",
	}));

	SECTION("no changes, no differences") {
		RegistrySnapshot again;
		REQUIRE(again.Take(root));
		CHECK(RegistrySnapshot::Compare(after, again).empty());
	}

	SECTION("key and value names are compared case-insensitively") {
		RegistrySnapshot again;
		REQUIRE(again.Take(root));
		// re-created with names differing only in case: still the same key and value
		REQUIRE(ERROR_SUCCESS == ::RegDeleteTree(HKEY_CURRENT_USER, (root + L"\\Born").Mid(18)));
		scratch.SetString(L"Root\\BORN", L"b", L"b");
		RegistrySnapshot renamed;
		REQUIRE(renamed.Take(root));
		CHECK(RegistrySnapshot::Compare(again, renamed).empty());
	}
}

TEST_CASE("Large values are compared fully but kept short", "[snapshot]") {
	ScratchKey scratch;
	std::vector<BYTE> big(10000, 0xAB);
	scratch.SetValue(L"Root", L"Big", REG_BINARY, big.data(), (DWORD)big.size());
	RegistrySnapshot before;
	REQUIRE(before.Take(scratch / L"Root"));
	auto& value = before.GetKeys()[0].Values[0];
	CHECK(value.Size == 10000);
	CHECK(value.Preview.size() == SnapshotValue::PreviewSize);
	CHECK(value.IsTruncated());

	// a change past the preview is still found
	big[9999] = 0;
	scratch.SetValue(L"Root", L"Big", REG_BINARY, big.data(), (DWORD)big.size());
	RegistrySnapshot after;
	REQUIRE(after.Take(scratch / L"Root"));
	auto changes = RegistrySnapshot::Compare(before, after);
	REQUIRE(changes.size() == 1);
	CHECK(changes[0].Type == SnapshotChangeType::ValueChanged);
	CHECK(RegistrySnapshot::FormatData(*changes[0].New).Right(4) == L" ...");
}

TEST_CASE("Symbolic links are recorded, not followed", "[snapshot]") {
	ScratchKey scratch;
	scratch.SetString(L"Root\\Target\\Inside", L"V", L"v");
	// a link key pointing at its own parent would loop forever if followed
	HKEY hLink = nullptr;
	REQUIRE(ERROR_SUCCESS == ::RegCreateKeyEx(HKEY_CURRENT_USER, (scratch / L"Root\\Link").Mid(18), 0, nullptr,
		REG_OPTION_CREATE_LINK, KEY_ALL_ACCESS, nullptr, &hLink, nullptr));
	auto target = L"\\REGISTRY\\USER\\" + [] {
		CString sid;
		wil::unique_handle hToken;
		::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, hToken.addressof());
		BYTE buffer[256];
		DWORD len;
		::GetTokenInformation(hToken.get(), TokenUser, buffer, sizeof(buffer), &len);
		PWSTR str;
		::ConvertSidToStringSid(((TOKEN_USER*)buffer)->User.Sid, &str);
		sid = str;
		::LocalFree(str);
		return sid;
	}() + L"\\" + (scratch / L"Root").Mid(18);
	REQUIRE(ERROR_SUCCESS == ::RegSetValueEx(hLink, L"SymbolicLinkValue", 0, REG_LINK, (BYTE const*)target.GetString(), target.GetLength() * sizeof(WCHAR)));
	::RegCloseKey(hLink);
	auto cleanup = wil::scope_exit([&] {
		// a link key is deleted through a handle opened on the link itself
		HKEY h;
		if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_CURRENT_USER, (scratch / L"Root\\Link").Mid(18), REG_OPTION_OPEN_LINK, DELETE, &h)) {
			::NtDeleteKey(h);
			::RegCloseKey(h);
		}
	});

	RegistrySnapshot snapshot;
	REQUIRE(snapshot.Take(scratch / L"Root"));
	std::vector<CString> paths;
	for (auto& key : snapshot.GetKeys())
		paths.push_back(key.Path);
	CHECK(paths == std::vector<CString>{ L"", L"Link", L"Target", L"Target\\Inside" });
	auto& link = snapshot.GetKeys()[1];
	REQUIRE(link.Values.size() == 1);
	CHECK(link.Values[0].Type == REG_LINK);
}

TEST_CASE("Snapshots save and load", "[snapshot]") {
	ScratchKey scratch;
	TempDir dir;
	CreateTree(scratch);
	std::vector<BYTE> big(1000, 7);
	scratch.SetValue(L"Root", L"Big", REG_BINARY, big.data(), (DWORD)big.size());
	RegistrySnapshot original;
	REQUIRE(original.Take(scratch / L"Root"));
	auto file = dir / L"test.trsnap";
	REQUIRE(original.Save(file));

	RegistrySnapshot loaded;
	REQUIRE(loaded.Load(file));
	CHECK(loaded.GetRoot() == original.GetRoot());
	auto loadedTime = loaded.GetTime(), originalTime = original.GetTime();
	CHECK(::CompareFileTime(&loadedTime, &originalTime) == 0);
	CHECK(loaded.GetKeys().size() == original.GetKeys().size());
	CHECK(loaded.GetValueCount() == original.GetValueCount());
	CHECK(RegistrySnapshot::Compare(original, loaded).empty());
	CHECK(RegistrySnapshot::Compare(loaded, original).empty());

	SECTION("a loaded snapshot compares with the live Registry") {
		scratch.SetString(L"Root", L"Changes", L"after");
		RegistrySnapshot now;
		REQUIRE(now.Take(loaded.GetRoot()));
		auto changes = RegistrySnapshot::Compare(loaded, now);
		REQUIRE(changes.size() == 1);
		CHECK(changes[0].Value == L"Changes");
	}

	SECTION("damaged files are rejected, not trusted") {
		auto bytes = ReadFileBytes(file);
		for (size_t size : { size_t(0), size_t(4), size_t(20), bytes.size() / 2, bytes.size() - 1 }) {
			std::vector<BYTE> truncated(bytes.begin(), bytes.begin() + size);
			WriteFileBytes(dir / L"bad.trsnap", truncated);
			RegistrySnapshot bad;
			CHECK_FALSE(bad.Load(dir / L"bad.trsnap"));
		}
		auto wrongMagic = bytes;
		wrongMagic[0] = 'X';
		WriteFileBytes(dir / L"bad.trsnap", wrongMagic);
		RegistrySnapshot bad;
		CHECK_FALSE(bad.Load(dir / L"bad.trsnap"));
		// a failed load leaves the snapshot as it was
		CHECK(loaded.Load(dir / L"bad.trsnap") == false);
		CHECK(loaded.GetKeys().size() == original.GetKeys().size());
	}
}

// a real hive, to see time and size; run with: Tests.exe "[slow]"
TEST_CASE("Snapshot of HKLM\\SOFTWARE", "[snapshot][.][slow]") {
	TempDir dir;
	RegistrySnapshot snapshot;
	auto start = ::GetTickCount64();
	REQUIRE(snapshot.Take(L"HKEY_LOCAL_MACHINE\\SOFTWARE"));
	auto taken = ::GetTickCount64();
	REQUIRE(snapshot.Save(dir / L"software.trsnap"));
	auto saved = ::GetTickCount64();
	RegistrySnapshot loaded;
	REQUIRE(loaded.Load(dir / L"software.trsnap"));
	auto loadedTime = ::GetTickCount64();
	auto changes = RegistrySnapshot::Compare(snapshot, loaded);
	auto compared = ::GetTickCount64();
	CHECK(changes.empty());

	WARN(snapshot.GetKeys().size() << " keys, " << snapshot.GetValueCount() << " values, " << snapshot.GetInaccessibleKeys() << " inaccessible; "
		<< "file " << ReadFileBytes(dir / L"software.trsnap").size() / 1024 / 1024 << " MB; "
		<< "take " << taken - start << " ms, save " << saved - taken << " ms, load " << loadedTime - saved << " ms, compare " << compared - loadedTime << " ms");
}

TEST_CASE("Snapshots can be cancelled", "[snapshot]") {
	ScratchKey scratch;
	for (int i = 0; i < 600; i++)
		scratch.CreateKey(CString(L"Root\\k") + std::to_wstring(i).c_str());
	RegistrySnapshot snapshot;
	int calls = 0;
	CHECK_FALSE(snapshot.Take(scratch / L"Root", [&](size_t) { return ++calls < 2; }));
	CHECK(::GetLastError() == ERROR_CANCELLED);
	CHECK(snapshot.GetKeys().empty());
	CHECK(calls == 2);
}
