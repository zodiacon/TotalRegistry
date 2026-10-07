#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "KeyBackup.h"
#include "DeleteKeyCommand.h"
#include "RestoreKeyCommand.h"
#include "SecurityHelper.h"
#include <wil\resource.h>
#include <sddl.h>

using namespace TestHelpers;

namespace {
	// number of backups this process currently keeps in the Registry
	DWORD RegistryBackupCount() {
		CRegKey key;
		if (ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, KeyBackup::GetProcessBackupPath(), KEY_READ))
			return 0;
		DWORD subkeys = 0;
		::RegQueryInfoKey(key, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		return subkeys;
	}

	CString GetDacl(CString const& path) {
		CRegKey key;
		CString sub = path.Mid(CString(L"HKEY_CURRENT_USER\\").GetLength());
		REQUIRE(ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, sub, READ_CONTROL));
		DWORD size = 0;
		::RegGetKeySecurity(key, DACL_SECURITY_INFORMATION, nullptr, &size);
		std::vector<BYTE> sd(size);
		REQUIRE(ERROR_SUCCESS == ::RegGetKeySecurity(key, DACL_SECURITY_INFORMATION, sd.data(), &size));
		PWSTR text = nullptr;
		REQUIRE(::ConvertSecurityDescriptorToStringSecurityDescriptor(sd.data(), SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &text, nullptr));
		CString result(text);
		::LocalFree(text);
		return result;
	}

	void CreateVictim(ScratchKey const& scratch) {
		scratch.SetString(L"Victim", L"v1", L"one");
		scratch.SetDword(L"Victim\\Sub1\\Deep", L"n", 7);
		WCHAR multi[] = L"a\0b\0";
		scratch.SetValue(L"Victim\\Sub2", L"m", REG_MULTI_SZ, multi, sizeof(multi));
		scratch.CreateKey(L"Keep");
	}
}

TEST_CASE("Delete key, undo and redo", "[backup][delete]") {
	ScratchKey scratch;
	CreateVictim(scratch);
	auto before = scratch.Dump();
	auto backups = RegistryBackupCount();
	int callbacks = 0;

	{
		DeleteKeyCommand cmd(scratch.Path(), L"Victim", [&](auto&, bool) { callbacks++; return true; });
		REQUIRE(cmd.Execute());
		CHECK_FALSE(scratch.Exists(L"Victim"));
		CHECK(scratch.Exists(L"Keep"));

		REQUIRE(cmd.Undo());
		CHECK(scratch.Dump() == before);
		// the backup is discarded once it has been restored
		CHECK(RegistryBackupCount() == backups);

		REQUIRE(cmd.Execute());
		CHECK_FALSE(scratch.Exists(L"Victim"));
		REQUIRE(cmd.Undo());
		CHECK(scratch.Dump() == before);

		REQUIRE(cmd.Execute());
		CHECK(callbacks == 5);
	}
	// destroying the command discards its backup
	CHECK(RegistryBackupCount() == backups);
}

TEST_CASE("Delete a key directly under a predefined key", "[backup][delete]") {
	CString name;
	name.Format(L"TotalRegistryTests-%u-TopLevel", ::GetCurrentProcessId());
	{
		CRegKey key;
		REQUIRE(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, name + L"\\Child"));
		REQUIRE(ERROR_SUCCESS == key.SetStringValue(L"x", L"y"));
	}
	auto cleanup = wil::scope_exit([&] { ForceDeleteTree(HKEY_CURRENT_USER, name); });

	DeleteKeyCommand cmd(L"HKEY_CURRENT_USER", name);
	REQUIRE(cmd.Execute());
	CRegKey key;
	CHECK(ERROR_FILE_NOT_FOUND == key.Open(HKEY_CURRENT_USER, name, KEY_READ));

	REQUIRE(cmd.Undo());
	CHECK(ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, name + L"\\Child", KEY_READ));
	CString value;
	ULONG chars = 16;
	CHECK(ERROR_SUCCESS == key.QueryStringValue(L"x", value.GetBuffer(chars), &chars));
	value.ReleaseBuffer();
	CHECK(value == L"y");
}

TEST_CASE("A delete that fails partway is rolled back", "[backup][delete]") {
	ScratchKey scratch;
	scratch.SetString(L"Partial\\A", L"a", L"a");
	scratch.CreateKey(L"Partial\\B\\Locked");
	scratch.CreateKey(L"Partial\\C");
	// deleting Locked is denied, so RegDeleteTree fails after deleting some of the tree
	SetDacl(scratch / L"Partial\\B\\Locked", L"D:(D;;SD;;;$USER)(A;;KA;;;$USER)");
	auto before = scratch.Dump();
	auto dacl = GetDacl(scratch / L"Partial\\B\\Locked");
	auto backups = RegistryBackupCount();

	{
		DeleteKeyCommand cmd(scratch.Path(), L"Partial");
		CHECK_FALSE(cmd.Execute());
		CHECK(::GetLastError() == ERROR_ACCESS_DENIED);
	}
	CHECK(scratch.Dump() == before);
	// restored with its original security
	CHECK(GetDacl(scratch / L"Partial\\B\\Locked") == dacl);
	// and the backup, which kept the "deny delete" permission, was still removed
	CHECK(RegistryBackupCount() == backups);
}

TEST_CASE("Backup owner detection", "[backup]") {
	FILETIME create, exit, kernel, user;
	::GetProcessTimes(::GetCurrentProcess(), &create, &exit, &kernel, &user);
	auto time = ULARGE_INTEGER{ create.dwLowDateTime, create.dwHighDateTime }.QuadPart;
	auto pid = ::GetCurrentProcessId();
	CString name;

	name.Format(L"%u-%llX", pid, time);
	CHECK(KeyBackup::IsOwnerRunning(name));
	CHECK(KeyBackup::GetProcessBackupPath().Right(name.GetLength()) == name);

	name.Format(L"%u-%llX", pid, time + 1);
	CHECK_FALSE(KeyBackup::IsOwnerRunning(name));		// a process that reused the PID

	name.Format(L"%u-%llX", 0xFFFFFFF0, time);
	CHECK_FALSE(KeyBackup::IsOwnerRunning(name));		// no such process

	name.Format(L"%u-%llXZ", pid, time);
	CHECK_FALSE(KeyBackup::IsOwnerRunning(name));		// not this format

	CHECK_FALSE(KeyBackup::IsOwnerRunning(L"DC4ABE6D73"));	// older versions' names
}

TEST_CASE("Elevated: backups are saved to hive files", "[backup][elevated]") {
	if (!SecurityHelper::IsRunningElevated())
		SKIP("requires running elevated");

	ScratchKey scratch;
	CreateVictim(scratch);
	auto before = scratch.Dump();
	auto backups = RegistryBackupCount();

	DeleteKeyCommand cmd(scratch.Path(), L"Victim");
	REQUIRE(cmd.Execute());
	// nothing was copied into the user's hive
	CHECK(RegistryBackupCount() == backups);
	REQUIRE(cmd.Undo());
	CHECK(scratch.Dump() == before);
}

TEST_CASE("Elevated: importing a hive file can be undone", "[restore][elevated]") {
	if (!SecurityHelper::IsRunningElevated())
		SKIP("requires running elevated");

	ScratchKey scratch;
	TempDir dir;
	scratch.SetString(L"Source", L"s", L"from hive");
	scratch.SetDword(L"Source\\Sub", L"d", 1);
	scratch.SetString(L"Target", L"t", L"original");
	scratch.CreateKey(L"Target\\OldSub");
	auto targetBefore = scratch.Dump(L"Target");

	auto hive = dir / L"source.hiv";
	{
		BackupRestorePrivileges privileges;
		REQUIRE(privileges);
		CRegKey source;
		REQUIRE(ERROR_SUCCESS == source.Open(HKEY_CURRENT_USER, (scratch / L"Source").Mid(18), KEY_READ));
		REQUIRE(ERROR_SUCCESS == ::RegSaveKeyEx(source, hive, nullptr, REG_LATEST_FORMAT));
	}

	RestoreKeyCommand cmd(scratch / L"Target", hive);
	REQUIRE(cmd.Execute());
	CHECK(scratch.Dump(L"Target") == scratch.Dump(L"Source"));
	REQUIRE(cmd.Undo());
	CHECK(scratch.Dump(L"Target") == targetBefore);
}
