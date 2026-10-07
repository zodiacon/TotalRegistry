#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "RegExportImport.h"
#include "ImportRegFileCommand.h"

using namespace TestHelpers;

namespace {
	// creates values that exercise every part of the export format
	void CreateTrickyValues(ScratchKey const& scratch, PCWSTR sub) {
		scratch.SetString(sub, L"", L"default \"quoted\"");
		scratch.SetString(sub, L"Path", L"C:\\Windows\\System32\\");
		scratch.SetString(sub, L"Quote\"And\\Slash", L"a\"b\\c");
		scratch.SetString(sub, L"Empty", L"");
		scratch.SetDword(sub, L"Dword", 0xDEADBEEF);
		ULONGLONG qword = 0x123456789;
		scratch.SetValue(sub, L"Qword", REG_QWORD, &qword, sizeof(qword));
		scratch.SetString(sub, L"Expand", L"%SystemRoot%\\x", REG_EXPAND_SZ);
		WCHAR multi[] = L"one\0tw\\o\0";
		scratch.SetValue(sub, L"Multi", REG_MULTI_SZ, multi, sizeof(multi));
		BYTE binary[100];
		for (int i = 0; i < 100; i++)
			binary[i] = (BYTE)i;
		scratch.SetValue(sub, L"Binary", REG_BINARY, binary, sizeof(binary));
		WCHAR embedded[] = L"ab\0cd";
		scratch.SetValue(sub, L"EmbeddedNull", REG_SZ, embedded, sizeof(embedded));
		WORD shortDword = 7;
		scratch.SetValue(sub, L"ShortDword", REG_DWORD, &shortDword, sizeof(shortDword));
		scratch.SetValue(sub, L"None", REG_NONE, nullptr, 0);
		scratch.SetString(CString(sub) + L"\\Sub\\Deeper", L"X", L"y");
	}

	std::vector<RegFileKey> ParseText(TempDir const& dir, CString const& text) {
		auto file = dir / L"test.reg";
		WriteUnicodeFile(file, text);
		std::vector<RegFileKey> keys;
		CString error;
		if (!RegExportImport::Parse(file, keys, error))
			FAIL("parse error: " << ToUtf8(error));
		return keys;
	}

	CString ParseError(TempDir const& dir, CString const& text) {
		auto file = dir / L"bad.reg";
		WriteUnicodeFile(file, text);
		std::vector<RegFileKey> keys;
		CString error;
		REQUIRE_FALSE(RegExportImport::Parse(file, keys, error));
		return error;
	}

	std::wstring AsString(std::vector<BYTE> const& data) {
		return std::wstring((PCWSTR)data.data(), data.size() / sizeof(WCHAR));
	}
}

TEST_CASE("Export writes a valid RegEdit 5 file", "[regfile][export]") {
	ScratchKey scratch;
	TempDir dir;
	CreateTrickyValues(scratch, L"Src");
	auto file = dir / L"out.reg";
	REQUIRE(RegExportImport().Export(scratch / L"Src", file));

	auto bytes = ReadFileBytes(file);
	REQUIRE(bytes.size() > 2);
	CHECK(bytes[0] == 0xFF);
	CHECK(bytes[1] == 0xFE);
	CString text((PCWSTR)(bytes.data() + 2), int((bytes.size() - 2) / sizeof(WCHAR)));

	CHECK(text.Find(L"Windows Registry Editor Version 5.00\r\n") == 0);
	CHECK(text.Find(L"@=\"default \\\"quoted\\\"\"\r\n") >= 0);
	CHECK(text.Find(L"\"Path\"=\"C:\\\\Windows\\\\System32\\\\\"\r\n") >= 0);
	CHECK(text.Find(L"\"Quote\\\"And\\\\Slash\"=\"a\\\"b\\\\c\"\r\n") >= 0);
	CHECK(text.Find(L"\"Dword\"=dword:deadbeef\r\n") >= 0);
	CHECK(text.Find(L"\"Qword\"=hex(b):89,67,45,23,01,00,00,00\r\n") >= 0);
	// strings with embedded NULLs and DWORDs of the wrong size are kept exactly, as hex
	CHECK(text.Find(L"\"EmbeddedNull\"=hex(1):") >= 0);
	CHECK(text.Find(L"\"ShortDword\"=hex(4):07,00\r\n") >= 0);
	// long hex data is wrapped with indented continuation lines
	CHECK(text.Find(L",\\\r\n  ") >= 0);
}

TEST_CASE("Export then import reproduces the key exactly", "[regfile][export][import]") {
	ScratchKey scratch;
	TempDir dir;
	CreateTrickyValues(scratch, L"Src");
	auto file = dir / L"out.reg";
	REQUIRE(RegExportImport().Export(scratch / L"Src", file));

	auto bytes = ReadFileBytes(file);
	CString text((PCWSTR)(bytes.data() + 2), int((bytes.size() - 2) / sizeof(WCHAR)));
	text.Replace(scratch / L"Src", scratch / L"Dst");
	WriteUnicodeFile(file, text);

	std::vector<RegFileKey> keys;
	CString error;
	REQUIRE(RegExportImport::Parse(file, keys, error));
	ImportRegFileCommand cmd(file, std::move(keys));
	REQUIRE(cmd.Execute());
	CHECK(cmd.GetErrors().empty());
	CHECK(scratch.Dump(L"Dst") == scratch.Dump(L"Src"));
}

TEST_CASE("Export of a missing key fails without creating a file", "[regfile][export]") {
	ScratchKey scratch;
	TempDir dir;
	auto file = dir / L"none.reg";
	CHECK_FALSE(RegExportImport().Export(scratch / L"Missing", file));
	CHECK(::GetFileAttributes(file) == INVALID_FILE_ATTRIBUTES);
}

TEST_CASE("Parse handles the .reg syntax", "[regfile][parse]") {
	TempDir dir;
	auto keys = ParseText(dir,
		L"Windows Registry Editor Version 5.00\r\n"
		L"\r\n"
		L"; a comment\r\n"
		L"[HKEY_CURRENT_USER\\Software\\A]\r\n"
		L"@=\"default \\\"q\\\"\"\r\n"
		L"\"Path\"=\"C:\\\\x\\\\\"\r\n"
		L"\"D\"=dword:0000002a\r\n"
		L"\"Gone\"=-\r\n"
		L"\"Q\"=hex(b):01,02,03,04,05,06,07,08\r\n"
		L"\"Bin\"=hex:00,01,02,\\\r\n"
		L"  03,04\r\n"
		L"\r\n"
		L"[-HKEY_CURRENT_USER\\Software\\B]\r\n"
		L"\"Ignored\"=\"in a deleted key\"\r\n"
		L"\r\n"
		L"[HKCU\\Software\\C]\r\n");

	REQUIRE(keys.size() == 3);
	CHECK(keys[0].Path == L"HKEY_CURRENT_USER\\Software\\A");
	CHECK_FALSE(keys[0].Delete);
	REQUIRE(keys[0].Values.size() == 6);

	auto& values = keys[0].Values;
	CHECK(values[0].Name.IsEmpty());
	CHECK(values[0].Type == REG_SZ);
	CHECK(AsString(values[0].Data) == std::wstring(L"default \"q\"\0", 12));
	CHECK(AsString(values[1].Data) == std::wstring(L"C:\\x\\\0", 6));
	CHECK(values[2].Type == REG_DWORD);
	CHECK(*(DWORD*)values[2].Data.data() == 42);
	CHECK(values[3].Name == L"Gone");
	CHECK(values[3].Delete);
	CHECK(values[4].Type == REG_QWORD);
	CHECK(values[4].Data.size() == 8);
	CHECK(values[5].Type == REG_BINARY);
	CHECK(values[5].Data == std::vector<BYTE>{ 0, 1, 2, 3, 4 });

	CHECK(keys[1].Delete);
	CHECK(keys[1].Values.empty());
	CHECK(keys[2].Path == L"HKCU\\Software\\C");
}

TEST_CASE("Parse reads every file encoding", "[regfile][parse]") {
	TempDir dir;
	auto file = dir / L"enc.reg";
	std::vector<RegFileKey> keys;
	CString error;

	SECTION("UTF-16 without a BOM (older TotalRegistry exports)") {
		WriteUnicodeFile(file, L"Windows Registry Editor Version 5.00\r\n[HKEY_CURRENT_USER\\x]\r\n\"N\"=dword:00000007\r\n", false);
		REQUIRE(RegExportImport::Parse(file, keys, error));
		REQUIRE(keys.size() == 1);
		CHECK(*(DWORD*)keys[0].Values[0].Data.data() == 7);
	}

	SECTION("UTF-8 with a BOM") {
		WriteAnsiFile(file, L"Windows Registry Editor Version 5.00\r\n[HKEY_CURRENT_USER\\x]\r\n\"U\"=\"\x05E9\x05DC\"\r\n", CP_UTF8, true);
		REQUIRE(RegExportImport::Parse(file, keys, error));
		CHECK(AsString(keys[0].Values[0].Data) == std::wstring(L"\x05E9\x05DC\0", 3));
	}

	SECTION("REGEDIT4 (ANSI), including ANSI hex(2) data") {
		WriteAnsiFile(file, L"REGEDIT4\r\n\r\n[HKEY_CURRENT_USER\\x]\r\n\"S\"=\"caf\x00E9\"\r\n\"E\"=hex(2):25,54,45,4d,50,25,00\r\n", 1252);
		REQUIRE(RegExportImport::Parse(file, keys, error));
		auto& values = keys[0].Values;
		CHECK(AsString(values[0].Data) == std::wstring(L"caf\x00E9\0", 5));
		CHECK(values[1].Type == REG_EXPAND_SZ);
		CHECK(AsString(values[1].Data) == std::wstring(L"%TEMP%\0", 7));
	}
}

TEST_CASE("Parse rejects malformed files with the line number", "[regfile][parse]") {
	TempDir dir;
	CHECK(ParseError(dir, L"hello") == L"Not a valid registry file (missing header)");
	CHECK(ParseError(dir, L"Windows Registry Editor Version 5.00\r\n[HKEY_NOPE\\x]\r\n") == L"Line 2: invalid key path 'HKEY_NOPE\\x'");
	CHECK(ParseError(dir, L"Windows Registry Editor Version 5.00\r\n[HKEY_CURRENT_USER\\x\r\n") == L"Line 2: missing ']'");
	CHECK(ParseError(dir, L"Windows Registry Editor Version 5.00\r\n[HKEY_CURRENT_USER\\x]\r\n\"X\"=dword:zz\r\n") == L"Line 3: invalid data for value 'X'");
	CHECK(ParseError(dir, L"Windows Registry Editor Version 5.00\r\n\"X\"=\"y\"\r\n") == L"Line 2: value outside of a key");
	CHECK(ParseError(dir, L"Windows Registry Editor Version 5.00\r\n[HKEY_CURRENT_USER\\x]\r\n\"X\"\"y\"\r\n") == L"Line 3: missing '='");
}
