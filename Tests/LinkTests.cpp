#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "Registry.h"
#include "Helpers.h"
#include "CreateKeyCommand.h"
#include "CreateLinkCommand.h"
#include "DeleteKeyCommand.h"
#include "RegExportImport.h"
#include "ImportRegFileCommand.h"

using namespace TestHelpers;

namespace {
	CString Kernel(CString const& path) {
		return Registry::StdPathToKernelPath(path);
	}

	// the target of a link, or empty if the key is not a link
	CString LinkTarget(CString const& path) {
		auto bs = path.ReverseFind(L'\\');
		auto parent = Registry::OpenKey(path.Left(bs), KEY_READ);
		CString target;
		if (!parent || !Registry::IsKeyLink(parent, path.Mid(bs + 1), target))
			return L"";
		return target;
	}

	// reads a string value through a path (following links)
	CString ReadString(CString const& path, PCWSTR name) {
		auto key = Registry::OpenKey(path, KEY_READ);
		return key ? Registry::QueryStringValue(key, name) : CString(L"(no key)");
	}

	// a link left behind would make the scratch key's cleanup follow it
	struct LinkCleanup {
		CString Path;
		~LinkCleanup() {
			auto bs = Path.ReverseFind(L'\\');
			auto parent = Registry::OpenKey(Path.Left(bs), MAXIMUM_ALLOWED);
			if (parent)
				Registry::DeleteLinkKey(parent, Path.Mid(bs + 1));
		}
	};
}

TEST_CASE("Standard paths as kernel paths", "[links]") {
	auto sid = Helpers::GetCurrentUserSid();
	CHECK(Kernel(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\X") == L"\\REGISTRY\\MACHINE\\SOFTWARE\\X");
	CHECK(Kernel(L"HKLM") == L"\\REGISTRY\\MACHINE");
	CHECK(Kernel(L"HKEY_USERS\\.DEFAULT") == L"\\REGISTRY\\USER\\.DEFAULT");
	CHECK(Kernel(L"HKCU\\Software") == L"\\REGISTRY\\USER\\" + sid + L"\\Software");
	CHECK(Kernel(L"\\REGISTRY\\MACHINE\\SYSTEM") == L"\\REGISTRY\\MACHINE\\SYSTEM");
	// a merged view has no single kernel path
	CHECK(Kernel(L"HKEY_CLASSES_ROOT\\.txt").IsEmpty());
	CHECK(Kernel(L"BOGUS\\X").IsEmpty());
}

TEST_CASE("Creating and deleting symbolic links", "[links]") {
	ScratchKey scratch;
	scratch.SetString(L"Target", L"t", L"through the link");
	scratch.SetString(L"Target\\Child", L"c", L"child");
	auto link = scratch / L"Link";
	LinkCleanup cleanup{ link };

	int callbacks = 0;
	CreateLinkCommand cmd(scratch.Path(), L"Link", Kernel(scratch / L"Target"), false, [&](auto&, bool) { callbacks++; return true; });
	REQUIRE(cmd.Execute());
	CHECK(LinkTarget(link) == Kernel(scratch / L"Target"));
	CHECK(ReadString(link, L"t") == L"through the link");

	SECTION("undo deletes only the link") {
		REQUIRE(cmd.Undo());
		CHECK(LinkTarget(link).IsEmpty());
		CHECK_FALSE(scratch.Exists(L"Link"));
		CHECK(ReadString(scratch / L"Target", L"t") == L"through the link");
		CHECK(callbacks == 2);
	}

	SECTION("a second link with the same name fails") {
		CreateLinkCommand again(scratch.Path(), L"Link", Kernel(scratch / L"Target"), false);
		CHECK_FALSE(again.Execute());
		CHECK(::GetLastError() == ERROR_ALREADY_EXISTS);
	}

	SECTION("deleting a link key leaves its target alone, and undo re-creates the link") {
		auto before = scratch.Dump(L"Target");
		DeleteKeyCommand del(scratch.Path(), L"Link");
		REQUIRE(del.Execute());
		CHECK(LinkTarget(link).IsEmpty());
		CHECK(scratch.Dump(L"Target") == before);

		REQUIRE(del.Undo());
		CHECK(LinkTarget(link) == Kernel(scratch / L"Target"));
		CHECK(ReadString(link, L"t") == L"through the link");
		CHECK(scratch.Dump(L"Target") == before);
	}

	SECTION("a .reg file deleting a link removes only the link, and the preview says so") {
		TempDir dir;
		auto file = dir / L"link.reg";
		CString text;
		text.Format(L"Windows Registry Editor Version 5.00\r\n\r\n[-%s]\r\n", (PCWSTR)link);
		WriteUnicodeFile(file, text);
		std::vector<RegFileKey> keys;
		CString error;
		REQUIRE(RegExportImport::Parse(file, keys, error));

		auto preview = RegExportImport::Preview(keys);
		REQUIRE(preview.size() == 1);
		CHECK(preview[0].Type == SnapshotChangeType::KeyDeleted);
		CHECK(preview[0].Key == link);

		auto before = scratch.Dump(L"Target");
		ImportRegFileCommand import(file, keys);
		REQUIRE(import.Execute());
		CHECK(import.GetErrors().empty());
		CHECK(LinkTarget(link).IsEmpty());
		CHECK(scratch.Dump(L"Target") == before);

		REQUIRE(import.Undo());
		CHECK(LinkTarget(link) == Kernel(scratch / L"Target"));
	}
}

TEST_CASE("Volatile keys", "[links]") {
	ScratchKey scratch;
	CreateKeyCommand cmd(scratch.Path(), L"Volatile", nullptr, REG_OPTION_VOLATILE);
	CHECK(cmd.GetCommandName() == L"Create Volatile Key Volatile");
	REQUIRE(cmd.Execute());
	CHECK(scratch.Exists(L"Volatile"));

	// only volatile keys can be created under a volatile key, which shows the key is volatile
	CRegKey child;
	CHECK(ERROR_CHILD_MUST_BE_VOLATILE == child.Create(HKEY_CURRENT_USER, (scratch / L"Volatile\\Child").Mid(18)));

	REQUIRE(cmd.Undo());
	CHECK_FALSE(scratch.Exists(L"Volatile"));

	SECTION("creating an existing key fails") {
		scratch.CreateKey(L"Existing");
		CreateKeyCommand existing(scratch.Path(), L"Existing");
		CHECK_FALSE(existing.Execute());
		CHECK(::GetLastError() == ERROR_OBJECT_ALREADY_EXISTS);
	}
}
