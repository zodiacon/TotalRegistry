#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "RegistrySearcher.h"
#include <atomic>
#include <mutex>
#include <wil\resource.h>

using namespace TestHelpers;

namespace {
	void CreateSearchValues(ScratchKey const& scratch) {
		scratch.SetString(nullptr, CString(L'L', 300), L"x");
		scratch.SetString(nullptr, L"afterlong", L"x");
		scratch.SetString(nullptr, L"foo", L"x");
		scratch.SetString(nullptr, L"foobar", L"x");
		scratch.SetString(nullptr, L"d1", L"see foo.bar");
		scratch.SetString(nullptr, L"d2", L"xfoo");
		scratch.SetString(nullptr, L"d3", L"Foo", REG_EXPAND_SZ);
		WCHAR multi[] = L"alpha\0has foo here\0foo again\0";
		scratch.SetValue(nullptr, L"m", REG_MULTI_SZ, multi, sizeof(multi));
		scratch.CreateKey(L"FooKey");
		scratch.CreateKey(L"Sub\\foo");
	}

	// runs a search of the scratch key to completion; results are "relative key | value name | data", sorted
	std::vector<std::string> Search(ScratchKey const& scratch, PCWSTR text, FindOptions options) {
		RegistrySearcher searcher;
		std::mutex lock;
		std::vector<std::string> results;
		int finals = 0;
		auto rootLength = scratch.Path().GetLength();
		searcher.SetStartKey(scratch.Path());
		searcher.SetText(text);
		searcher.SetOptions(options | FindOptions::SearchSelected);
		searcher.Find([&](auto path, auto name, auto data) {
			std::lock_guard locker(lock);
			if (!path) {
				finals++;
				return;
			}
			results.push_back(ToUtf8(path + rootLength) + " | " + (name ? ToUtf8(name) : "-") + " | " + (data ? ToUtf8(data) : "-"));
			}, false);
		REQUIRE(searcher.WaitForCompletion(30000));
		CHECK(finals == 1);
		CHECK_FALSE(searcher.IsRunning());
		std::sort(results.begin(), results.end());
		return results;
	}

	auto const All = FindOptions::SearchKeys | FindOptions::SearchValues | FindOptions::SearchData;

	std::vector<std::string> Sorted(std::vector<std::string> lines) {
		std::sort(lines.begin(), lines.end());
		return lines;
	}
}

TEST_CASE("Search matching", "[search]") {
	ScratchKey scratch;
	CreateSearchValues(scratch);

	SECTION("whole words: word boundaries are any non-alphanumeric character") {
		CHECK(Search(scratch, L"foo", All | FindOptions::MatchWholeWords) == Sorted({
			"\\Sub\\foo | - | -",
			" | d1 | see foo.bar",
			" | d3 | Foo",
			" | foo | -",
			" | m | has foo here",
		}));
	}

	SECTION("substrings") {
		CHECK(Search(scratch, L"foo", All) == Sorted({
			"\\FooKey | - | -",
			"\\Sub\\foo | - | -",
			" | d1 | see foo.bar",
			" | d2 | xfoo",
			" | d3 | Foo",
			" | foo | -",
			" | foobar | -",
			" | m | has foo here",
		}));
	}

	SECTION("case sensitive") {
		CHECK(Search(scratch, L"Foo", All | FindOptions::MatchCase) == Sorted({
			"\\FooKey | - | -",
			" | d3 | Foo",
		}));
	}

	SECTION("a value after a name longer than 255 characters is found") {
		CHECK(Search(scratch, L"afterlong", FindOptions::SearchValues) == Sorted({ " | afterlong | -" }));
	}
}

TEST_CASE("Search pausing, superseding and cancelling", "[search]") {
	ScratchKey scratch;
	CreateSearchValues(scratch);
	RegistrySearcher searcher;
	searcher.SetStartKey(scratch.Path());
	searcher.SetOptions(All | FindOptions::SearchSelected);
	searcher.SetText(L"foo");

	std::atomic<int> oldHits = 0, oldFinals = 0;
	searcher.Find([&](auto path, auto, auto) { path ? oldHits++ : oldFinals++; });
	for (int i = 0; i < 500 && oldHits == 0; i++)
		::Sleep(10);
	REQUIRE(oldHits == 1);
	// paused on the first result
	CHECK(searcher.CanContinue());
	CHECK(searcher.IsRunning());

	searcher.SetText(L"alpha");
	CHECK_FALSE(searcher.CanContinue());

	SECTION("a new search replaces the paused one, which makes no more callbacks") {
		std::atomic<int> newHits = 0, newFinals = 0;
		searcher.Find([&](auto path, auto, auto) { path ? newHits++ : newFinals++; });
		for (int i = 0; i < 3000 && !searcher.WaitForCompletion(10); i++)
			searcher.Continue();
		::Sleep(100);
		CHECK(oldHits == 1);
		CHECK(oldFinals == 0);
		CHECK(newHits == 1);
		CHECK(newFinals == 1);
	}

	SECTION("cancel stops it at once, with a single final callback") {
		REQUIRE(searcher.Cancel());
		CHECK_FALSE(searcher.IsRunning());
		CHECK(searcher.IsCancelled());
		CHECK_FALSE(searcher.CanContinue());
		REQUIRE(searcher.WaitForCompletion(5000));
		CHECK(oldHits == 1);
		CHECK(oldFinals == 1);
	}
}

TEST_CASE("Destroying a searcher stops all callbacks", "[search]") {
	ScratchKey scratch;
	for (int i = 0; i < 2000; i++)
		scratch.SetDword(nullptr, CString(L"value") + std::to_wstring(i).c_str(), i);

	std::atomic<int> hits = 0, finals = 0;
	std::atomic<bool> destroyed = false;
	std::atomic<int> afterDestroy = 0;

	SECTION("while running") {
		auto searcher = std::make_unique<RegistrySearcher>();
		searcher->SetStartKey(scratch.Path());
		searcher->SetText(L"value");
		searcher->SetOptions(FindOptions::SearchValues | FindOptions::SearchSelected);
		searcher->Find([&](auto path, auto, auto) {
			if (destroyed)
				afterDestroy++;
			path ? hits++ : finals++;
			::Sleep(1);
			}, false);
		while (hits < 20)
			::Sleep(1);
		searcher.reset();
		destroyed = true;
		auto atDestroy = hits.load();
		::Sleep(500);
		// a callback already in progress may finish, but no new one starts
		CHECK(hits <= atDestroy + 1);
		CHECK(afterDestroy == 0);
		CHECK(finals == 0);
	}

	SECTION("while paused on a result") {
		auto searcher = std::make_unique<RegistrySearcher>();
		searcher->SetStartKey(scratch.Path());
		searcher->SetText(L"value");
		searcher->SetOptions(FindOptions::SearchValues | FindOptions::SearchSelected);
		searcher->Find([&](auto path, auto, auto) { path ? hits++ : finals++; });
		while (hits == 0)
			::Sleep(1);
		searcher.reset();
		::Sleep(500);
		CHECK(hits == 1);
		CHECK(finals == 0);
	}
}

// searches the whole standard Registry, so it's slow (~30s); run with: Tests.exe "[slow]"
TEST_CASE("Search skips the performance and local settings roots", "[search][.][slow]") {
	CString sub;
	sub.Format(L"Software\\Classes\\Local Settings\\TotalRegistryTests-%u", ::GetCurrentProcessId());
	CString name;
	name.Format(L"TotalRegistryTestsUnique%u", ::GetCurrentProcessId());
	{
		CRegKey key;
		REQUIRE(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, sub));
		REQUIRE(ERROR_SUCCESS == key.SetStringValue(name, L"x"));
	}
	auto cleanup = wil::scope_exit([&] { ::RegDeleteTree(HKEY_CURRENT_USER, sub); });

	RegistrySearcher searcher;
	std::mutex lock;
	std::vector<CString> paths;
	searcher.SetText(name);
	searcher.SetOptions(FindOptions::SearchValues | FindOptions::SearchStdRegistry);
	searcher.Find([&](auto path, auto, auto) {
		if (path) {
			std::lock_guard locker(lock);
			paths.push_back(path);
		}
		}, false);
	REQUIRE(searcher.WaitForCompletion(600000));

	CHECK_FALSE(paths.empty());
	for (auto& path : paths) {
		INFO(ToUtf8(path));
		CHECK(path.Find(L"HKEY_CURRENT_USER_LOCAL_SETTINGS") < 0);
		CHECK(path.Find(L"HKEY_PERFORMANCE") < 0);
	}
}
