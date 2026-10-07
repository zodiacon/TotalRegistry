#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "RegExportImport.h"
#include "ImportRegFileCommand.h"

using namespace TestHelpers;

namespace {
	// "<change> <relative key> [<value>] <old> -> <new>", sorted
	std::vector<std::string> Describe(std::vector<SnapshotChange> const& changes, CString const& root) {
		std::vector<std::string> lines;
		for (auto& c : changes) {
			auto line = ToUtf8(RegistrySnapshot::GetChangeTypeName(c.Type));
			if (auto relative = c.Key.Mid(root.GetLength()); !relative.IsEmpty())
				line += " " + ToUtf8(relative);
			if (!RegistrySnapshot::IsKeyChange(c.Type))
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

	std::vector<std::string> Sorted(std::vector<std::string> lines) {
		std::sort(lines.begin(), lines.end());
		return lines;
	}

	std::vector<RegFileKey> Parse(TempDir const& dir, CString const& text) {
		auto file = dir / L"preview.reg";
		WriteUnicodeFile(file, text);
		std::vector<RegFileKey> keys;
		CString error;
		if (!RegExportImport::Parse(file, keys, error))
			FAIL("parse error: " << ToUtf8(error));
		return keys;
	}
}

TEST_CASE("Import preview lists what would change, and changes nothing", "[import][preview]") {
	ScratchKey scratch;
	TempDir dir;
	scratch.SetString(L"Existing", L"Same", L"same");
	scratch.SetString(L"Existing", L"Changes", L"old");
	scratch.SetString(L"Existing", L"Goes", L"x");
	scratch.SetString(L"Doomed\\Child", L"V", L"v");
	scratch.CreateKey(L"Locked");
	// readable by no one but its owner's right to change the permissions
	SetDacl(scratch / L"Locked", L"D:P(A;;RC;;;$USER)");

	CString text;
	text.Format(
		L"Windows Registry Editor Version 5.00\r\n\r\n"
		L"[%s]\r\n\"Same\"=\"same\"\r\n\"Changes\"=\"new\"\r\n\"Goes\"=-\r\n\"Missing\"=-\r\n\"Added\"=dword:00000001\r\n\r\n"
		L"[%s]\r\n\"V\"=\"x\"\r\n\r\n"
		L"[%s]\r\n\r\n"
		L"[-%s]\r\n\r\n"
		L"[-%s]\r\n\r\n"
		L"[%s]\r\n\"X\"=\"y\"\r\n",
		(PCWSTR)(scratch / L"Existing"), (PCWSTR)(scratch / L"New\\Deep"), (PCWSTR)(scratch / L"New\\Deep\\Deeper"),
		(PCWSTR)(scratch / L"Doomed"), (PCWSTR)(scratch / L"NeverExisted"), (PCWSTR)(scratch / L"Locked"));
	auto keys = Parse(dir, text);

	auto before = scratch.Dump();
	auto preview = RegExportImport::Preview(keys);
	CHECK(scratch.Dump() == before);

	auto expected = Sorted({
		"Key added \\New",
		"Key added \\New\\Deep",
		"Key added \\New\\Deep\\Deeper",
		"Value added \\New\\Deep [V] -> x",
		"Value added \\Existing [Added] -> 0x00000001 (1)",
		"Value changed \\Existing [Changes] old -> new",
		"Value deleted \\Existing [Goes] x",
		"Key deleted \\Doomed",
		"Key deleted \\Doomed\\Child",
		"Value deleted \\Doomed\\Child [V] v",
		"No access \\Locked",
	});
	CHECK(Describe(preview, scratch.Path()) == expected);

	SECTION("the preview matches what the import then does") {
		RegistrySnapshot snapshotBefore;
		REQUIRE(snapshotBefore.Take(scratch.Path()));
		ImportRegFileCommand cmd(dir / L"preview.reg", keys);
		REQUIRE(cmd.Execute());
		CHECK(cmd.GetErrors().size() == 1);		// the locked key
		RegistrySnapshot snapshotAfter;
		REQUIRE(snapshotAfter.Take(scratch.Path()));

		auto actual = Describe(RegistrySnapshot::Compare(snapshotBefore, snapshotAfter), scratch.Path());
		std::erase(expected, std::string("No access \\Locked"));
		CHECK(actual == expected);
	}
}

TEST_CASE("Import preview of a file that changes nothing is empty", "[import][preview]") {
	ScratchKey scratch;
	TempDir dir;
	scratch.SetString(L"Key", L"V", L"v");
	CString text;
	text.Format(L"Windows Registry Editor Version 5.00\r\n\r\n[%s]\r\n\"V\"=\"v\"\r\n\"Missing\"=-\r\n\r\n[-%s]\r\n",
		(PCWSTR)(scratch / L"Key"), (PCWSTR)(scratch / L"NeverExisted"));
	CHECK(RegExportImport::Preview(Parse(dir, text)).empty());
}
