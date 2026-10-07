#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "Registry.h"

TEST_CASE("GetRootKey resolves root names", "[registry]") {
	CString sub;

	SECTION("full, short and PowerShell-style names, case-insensitive") {
		CHECK(Registry::GetRootKey(L"HKEY_CURRENT_USER\\Software", sub) == HKEY_CURRENT_USER);
		CHECK(sub == L"Software");
		CHECK(Registry::GetRootKey(L"hkey_local_machine\\SOFTWARE\\x", sub) == HKEY_LOCAL_MACHINE);
		CHECK(sub == L"SOFTWARE\\x");
		CHECK(Registry::GetRootKey(L"HKCU\\a", sub) == HKEY_CURRENT_USER);
		CHECK(Registry::GetRootKey(L"HKCU:\\a", sub) == HKEY_CURRENT_USER);
		CHECK(sub == L"a");
		CHECK(Registry::GetRootKey(L"HKU", sub) == HKEY_USERS);
		CHECK(sub.IsEmpty());
	}

	SECTION("keeps the subkey's case") {
		Registry::GetRootKey(L"HKCU\\Software\\MixedCase", sub);
		CHECK(sub == L"Software\\MixedCase");
	}

	SECTION("unknown roots and computers") {
		CHECK(Registry::GetRootKey(L"BOGUS\\x", sub) == nullptr);
		CHECK(Registry::GetRootKey(L"", sub) == nullptr);
		CHECK(Registry::GetRootKey(L"\\\\NOTCONNECTED\\HKEY_LOCAL_MACHINE\\x", sub) == nullptr);
	}
}

TEST_CASE("CreateKey", "[registry]") {
	ScratchKey scratch;
	auto subPath = scratch.Path().Mid(CString(L"HKEY_CURRENT_USER\\").GetLength());

	SECTION("creates keys through any root name, preserving case") {
		CHECK(Registry::CreateKey(L"hkey_current_user\\" + subPath + L"\\MixedCase", KEY_READ));
		CHECK(Registry::CreateKey(L"HKCU\\" + subPath + L"\\ShortName", KEY_READ));
		CHECK(Registry::CreateKey(L"HKCU:\\" + subPath + L"\\Colon", KEY_READ));
		CHECK(scratch.Dump() == std::vector<std::string>{ "[]", "[Colon]", "[MixedCase]", "[ShortName]" });
	}

	SECTION("fails cleanly for bad paths") {
		CHECK_FALSE(Registry::CreateKey(L"BOGUS\\x", KEY_READ));
		CHECK(::GetLastError() == ERROR_PATH_NOT_FOUND);
		CHECK_FALSE(Registry::CreateKey(L"HKEY_CURRENT_USER", KEY_READ));
		CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
		CHECK_FALSE(Registry::CreateKey(L"", KEY_READ));
		CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
		CHECK_FALSE(Registry::CreateKey(L"\\\\NOTCONNECTED\\HKEY_LOCAL_MACHINE\\x", KEY_READ));
	}
}

TEST_CASE("OpenKey", "[registry]") {
	ScratchKey scratch;
	scratch.CreateKey(L"Sub");

	CHECK(Registry::OpenKey(scratch / L"Sub", KEY_READ));
	CHECK_FALSE(Registry::OpenKey(scratch / L"Missing", KEY_READ));

	SECTION("unknown root fails instead of asserting") {
		CHECK_FALSE(Registry::OpenKey(L"BOGUS\\x", KEY_READ));
		CHECK(::GetLastError() == ERROR_PATH_NOT_FOUND);
	}

	SECTION("a root key opens as the predefined handle") {
		// NormalizePath appends a backslash, so a root is opened as an empty subkey of itself,
		// for which RegOpenKeyEx returns the predefined handle (never closed by RegistryKey)
		auto key = Registry::OpenKey(L"HKEY_CURRENT_USER", KEY_READ);
		CHECK(key.Get() == HKEY_CURRENT_USER);
		CRegKey software;
		CHECK(ERROR_SUCCESS == software.Open(key, L"Software", KEY_READ));
	}
}

TEST_CASE("EnumKeyValues", "[registry]") {
	ScratchKey scratch;

	SECTION("value names longer than 255 characters don't end the enumeration") {
		CString longName(L'L', 300);
		scratch.SetString(nullptr, longName, L"x");
		scratch.SetString(nullptr, L"after", L"x");

		auto key = Registry::OpenKey(scratch.Path(), KEY_READ);
		std::vector<CString> names;
		Registry::EnumKeyValues(key, [&](auto, auto name, auto) {
			names.push_back(name);
			return true;
			});
		REQUIRE(names.size() == 2);
		CHECK(std::ranges::find(names, longName) != names.end());
		CHECK(std::ranges::find(names, CString(L"after")) != names.end());
	}

	SECTION("returning false stops the enumeration") {
		for (int i = 0; i < 5; i++)
			scratch.SetDword(nullptr, CString(L"v") + (WCHAR)(L'0' + i), i);

		auto key = Registry::OpenKey(scratch.Path(), KEY_READ);
		int calls = 0;
		Registry::EnumKeyValues(key, [&](auto, auto, auto) {
			return ++calls < 2;
			});
		CHECK(calls == 2);
	}
}

TEST_CASE("StdRegPathToRealPath", "[registry]") {
	CHECK(Registry::StdRegPathToRealPath(L"HKEY_LOCAL_MACHINE\\SOFTWARE") == L"\\REGISTRY\\MACHINE\\SOFTWARE");
	CHECK(Registry::StdRegPathToRealPath(L"HKEY_USERS\\.DEFAULT") == L"\\REGISTRY\\USER\\.DEFAULT");
}
