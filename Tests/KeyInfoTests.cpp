#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "Registry.h"
#include "Helpers.h"

using namespace TestHelpers;

namespace {
	CString UserKernelPath() {
		return L"\\REGISTRY\\USER\\" + Helpers::GetCurrentUserSid();
	}
}

TEST_CASE("Key info of a regular key", "[keyinfo]") {
	ScratchKey scratch;
	scratch.SetString(L"Key", L"a", L"1");
	scratch.SetString(L"Key", L"b", L"2");
	scratch.CreateKey(L"Key\\Sub");

	KeyInfo info;
	REQUIRE(Registry::GetKeyInfo(scratch / L"Key", info));
	CHECK(info.KernelPath.CompareNoCase(UserKernelPath() + (scratch / L"Key").Mid(17)) == 0);
	CHECK(info.SubKeys == 1);
	CHECK(info.Values == 2);
	CHECK_FALSE(info.Volatile);
	CHECK_FALSE(info.IsLink);
	CHECK(info.Class.IsEmpty());
	CHECK_FALSE(info.Owner.IsEmpty());
	CHECK(info.HiveKey.CompareNoCase(UserKernelPath()) == 0);
	CHECK(info.HiveFile.Right(10).CompareNoCase(L"NTUSER.DAT") == 0);
	CHECK_FALSE(info.IsHiveRoot);

	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	auto age = ULARGE_INTEGER{ now.dwLowDateTime, now.dwHighDateTime }.QuadPart - ULARGE_INTEGER{ info.LastWrite.dwLowDateTime, info.LastWrite.dwHighDateTime }.QuadPart;
	CHECK(age < 60ULL * 10000000);	// written within the last minute
}

TEST_CASE("Key info: class name, volatile and link keys", "[keyinfo]") {
	ScratchKey scratch;
	auto sub = scratch.Path().Mid(18);

	SECTION("class name") {
		CRegKey key;
		WCHAR className[] = L"MyClass";
		REQUIRE(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, sub + L"\\Classy", className));
		KeyInfo info;
		REQUIRE(Registry::GetKeyInfo(scratch / L"Classy", info));
		CHECK(info.Class == L"MyClass");
	}

	SECTION("volatile") {
		CRegKey key;
		REQUIRE(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, sub + L"\\Volatile", REG_NONE, REG_OPTION_VOLATILE));
		KeyInfo info;
		REQUIRE(Registry::GetKeyInfo(scratch / L"Volatile", info));
		CHECK(info.Volatile);
	}

	SECTION("a link describes itself, not its target") {
		scratch.SetString(L"Target", L"t", L"x");
		auto parent = Registry::OpenKey(scratch.Path(), KEY_ALL_ACCESS);
		auto target = Registry::StdPathToKernelPath(scratch / L"Target");
		REQUIRE(ERROR_SUCCESS == Registry::CreateLinkKey(parent, L"Link", target));

		KeyInfo info;
		bool ok = Registry::GetKeyInfo(scratch / L"Link", info);
		Registry::DeleteLinkKey(parent, L"Link");
		REQUIRE(ok);
		CHECK(info.IsLink);
		CHECK(info.LinkTarget == target);
		CHECK(info.KernelPath.Right(5) == L"\\Link");
		CHECK(info.Values == 1);		// SymbolicLinkValue, not the target's value
	}
}

TEST_CASE("Key info of special keys", "[keyinfo]") {
	KeyInfo info;

	SECTION("an HKEY_CLASSES_ROOT key shows where it really is") {
		REQUIRE(Registry::GetKeyInfo(L"HKEY_CLASSES_ROOT\\.txt", info));
		CHECK(info.KernelPath.Left(10).CompareNoCase(L"\\REGISTRY\\") == 0);
	}

	SECTION("a hive root") {
		REQUIRE(Registry::GetKeyInfo(L"HKEY_CURRENT_USER", info));
		CHECK(info.KernelPath.CompareNoCase(UserKernelPath()) == 0);
		CHECK(info.IsHiveRoot);
		CHECK(info.HiveFile.Right(10).CompareNoCase(L"NTUSER.DAT") == 0);
	}

	SECTION("a hive kept only in memory has no file") {
		REQUIRE(Registry::GetKeyInfo(L"HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION", info));
		CHECK(info.HiveKey.CompareNoCase(L"\\REGISTRY\\MACHINE\\HARDWARE") == 0);
		CHECK(info.HiveFile.IsEmpty());
	}

	SECTION("a missing key") {
		CHECK_FALSE(Registry::GetKeyInfo(L"HKEY_CURRENT_USER\\Software\\TotalRegistryTests-NoSuchKey", info));
	}
}
