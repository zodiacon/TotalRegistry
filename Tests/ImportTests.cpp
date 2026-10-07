#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "RegExportImport.h"
#include "ImportRegFileCommand.h"

using namespace TestHelpers;

namespace {
	std::unique_ptr<ImportRegFileCommand> ParseCommand(TempDir const& dir, CString const& text, int* callbacks = nullptr) {
		auto file = dir / L"import.reg";
		WriteUnicodeFile(file, text);
		std::vector<RegFileKey> keys;
		CString error;
		if (!RegExportImport::Parse(file, keys, error))
			FAIL("parse error: " << ToUtf8(error));
		return std::make_unique<ImportRegFileCommand>(file, std::move(keys), [=](auto&, bool) {
			if (callbacks)
				++*callbacks;
			return true;
			});
	}
}

TEST_CASE("Import, undo and redo", "[import]") {
	ScratchKey scratch;
	TempDir dir;
	scratch.SetString(L"Existing", L"A", L"old");
	scratch.SetDword(L"Existing", L"B", 1);
	scratch.SetString(L"Existing", L"ToDelete", L"x");
	scratch.SetString(L"Gone\\Child", L"V", L"gone");
	auto before = scratch.Dump();

	CString text;
	text.Format(
		L"Windows Registry Editor Version 5.00\r\n\r\n"
		L"[%s]\r\n"
		L"@=\"default\"\r\n"
		L"\"A\"=\"new\"\r\n"
		L"\"B\"=dword:0000002a\r\n"
		L"\"ToDelete\"=-\r\n"
		L"\"Missing\"=-\r\n"
		L"\"Q\"=hex(b):89,67,45,23,01,00,00,00\r\n\r\n"
		L"[%s]\r\n"
		L"\"Multi\"=hex(7):61,00,00,00,62,00,00,00,00,00\r\n\r\n"
		L"[-%s]\r\n\r\n"
		L"[-%s]\r\n",
		(PCWSTR)(scratch / L"Existing"), (PCWSTR)(scratch / L"New\\Deep\\Key"), (PCWSTR)(scratch / L"Gone"), (PCWSTR)(scratch / L"NeverExisted"));
	int callbacks = 0;
	auto cmd = ParseCommand(dir, text, &callbacks);

	REQUIRE(cmd->Execute());
	CHECK(cmd->GetErrors().empty());
	auto after = scratch.Dump();
	CHECK(after == std::vector<std::string>{
		"[]",
		"[Existing]",
		"=1:640065006600610075006c0074000000",
		"A=1:6e00650077000000",
		"B=4:2a000000",
		"Q=11:8967452301000000",
		"[New]",
		"[New\\Deep]",
		"[New\\Deep\\Key]",
		"Multi=7:61000000620000000000",
	});

	REQUIRE(cmd->Undo());
	CHECK(scratch.Dump() == before);

	REQUIRE(cmd->Execute());
	CHECK(scratch.Dump() == after);

	REQUIRE(cmd->Undo());
	CHECK(scratch.Dump() == before);
	CHECK(callbacks == 4);
}

TEST_CASE("Import continues past failures and can still be undone", "[import]") {
	ScratchKey scratch;
	TempDir dir;
	scratch.CreateKey(L"ReadOnly");
	SetDacl(scratch / L"ReadOnly", L"D:P(A;;KR;;;$USER)");
	auto before = scratch.Dump();

	CString text;
	text.Format(
		L"Windows Registry Editor Version 5.00\r\n\r\n"
		L"[%s]\r\n\"X\"=\"y\"\r\n\r\n"
		L"[%s]\r\n\"P\"=\"ok\"\r\n",
		(PCWSTR)(scratch / L"ReadOnly"), (PCWSTR)(scratch / L"Writable"));
	auto cmd = ParseCommand(dir, text);

	REQUIRE(cmd->Execute());
	REQUIRE(cmd->GetErrors().size() == 1);
	CHECK(cmd->GetErrors()[0].Find(L"ReadOnly") >= 0);
	CHECK(scratch.Exists(L"Writable"));

	REQUIRE(cmd->Undo());
	CHECK(scratch.Dump() == before);
}

TEST_CASE("Importing only deletions of missing keys changes nothing", "[import]") {
	ScratchKey scratch;
	TempDir dir;
	auto before = scratch.Dump();

	CString text;
	text.Format(L"Windows Registry Editor Version 5.00\r\n\r\n[-%s]\r\n", (PCWSTR)(scratch / L"Nothing"));
	auto cmd = ParseCommand(dir, text);
	REQUIRE(cmd->Execute());
	CHECK(cmd->GetErrors().empty());
	CHECK(scratch.Dump() == before);
	REQUIRE(cmd->Undo());
	CHECK(scratch.Dump() == before);
}
