#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "KeyWatcher.h"
#include <atomic>

namespace {
	// waits for the count to reach a value; false on timeout
	bool WaitFor(std::atomic<int> const& count, int value, DWORD timeout = 2000) {
		for (DWORD waited = 0; waited < timeout; waited += 10) {
			if (count >= value)
				return true;
			::Sleep(10);
		}
		return count >= value;
	}
}

TEST_CASE("KeyWatcher reports value and subkey changes", "[watcher]") {
	ScratchKey scratch;
	scratch.CreateKey(L"Watched\\Existing");
	std::atomic<int> changes = 0;
	KeyWatcher watcher;
	REQUIRE(watcher.Watch(scratch / L"Watched", [&] { changes++; }));
	CHECK(watcher.IsWatching());
	CHECK(watcher.GetPath() == scratch / L"Watched");

	SECTION("a value set") {
		scratch.SetString(L"Watched", L"v", L"1");
		CHECK(WaitFor(changes, 1));
	}

	SECTION("a subkey created") {
		scratch.CreateKey(L"Watched\\New");
		CHECK(WaitFor(changes, 1));
	}

	SECTION("a subkey deleted") {
		REQUIRE(ERROR_SUCCESS == ::RegDeleteTree(HKEY_CURRENT_USER, (scratch / L"Watched\\Existing").Mid(18)));
		CHECK(WaitFor(changes, 1));
	}

	SECTION("one callback per change until rearmed") {
		scratch.SetString(L"Watched", L"v", L"1");
		REQUIRE(WaitFor(changes, 1));
		scratch.SetString(L"Watched", L"v", L"2");
		::Sleep(300);
		CHECK(changes == 1);

		REQUIRE(watcher.Rearm());
		scratch.SetString(L"Watched", L"v", L"3");
		CHECK(WaitFor(changes, 2));
	}

	SECTION("changes deeper down are not reported") {
		scratch.SetString(L"Watched\\Existing", L"deep", L"x");
		::Sleep(300);
		CHECK(changes == 0);
	}

	SECTION("deleting the watched key is reported, and it can't be rearmed") {
		REQUIRE(ERROR_SUCCESS == ::RegDeleteTree(HKEY_CURRENT_USER, (scratch / L"Watched").Mid(18)));
		REQUIRE(WaitFor(changes, 1));
		CHECK_FALSE(watcher.Rearm());
	}

	SECTION("nothing is reported after Stop") {
		watcher.Stop();
		CHECK_FALSE(watcher.IsWatching());
		scratch.SetString(L"Watched", L"v", L"1");
		::Sleep(300);
		CHECK(changes == 0);
	}

	SECTION("watching another key stops watching the first") {
		scratch.CreateKey(L"Other");
		REQUIRE(watcher.Watch(scratch / L"Other", [&] { changes += 100; }));
		scratch.SetString(L"Watched", L"v", L"1");
		::Sleep(300);
		CHECK(changes == 0);
		scratch.SetString(L"Other", L"v", L"1");
		CHECK(WaitFor(changes, 100));
	}
}

TEST_CASE("KeyWatcher refuses keys it can't watch", "[watcher]") {
	ScratchKey scratch;
	KeyWatcher watcher;
	auto none = [] {};

	CHECK_FALSE(watcher.Watch(scratch / L"Missing", none));
	CHECK_FALSE(watcher.IsWatching());

	// root keys are predefined handles, which are never closed
	CHECK_FALSE(watcher.Watch(L"HKEY_CURRENT_USER", none));
	CHECK(::GetLastError() == ERROR_NOT_SUPPORTED);

	CHECK_FALSE(watcher.Watch(L"\\\\SOMEPC\\HKEY_LOCAL_MACHINE\\SOFTWARE", none));
	CHECK(::GetLastError() == ERROR_NOT_SUPPORTED);
}
