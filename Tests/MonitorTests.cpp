#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "RegistryMonitor.h"
#include "SecurityHelper.h"

using namespace TestHelpers;

namespace {
	// runs reg.exe with the arguments and waits for it
	void Reg(CString const& args) {
		CString cmd = L"reg.exe " + args;
		STARTUPINFO si{ sizeof(si) };
		PROCESS_INFORMATION pi;
		REQUIRE(::CreateProcess(nullptr, cmd.GetBuffer(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi));
		::WaitForSingleObject(pi.hProcess, 10000);
		DWORD code = 1;
		::GetExitCodeProcess(pi.hProcess, &code);
		::CloseHandle(pi.hProcess);
		::CloseHandle(pi.hThread);
		REQUIRE(code == 0);
	}
}

TEST_CASE("Kernel registry paths become standard paths", "[monitor]") {
	CString sid = L"S-1-5-21-1-2-3-1001";
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\REGISTRY\\MACHINE\\SOFTWARE\\X", sid) == L"HKEY_LOCAL_MACHINE\\SOFTWARE\\X");
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\Registry\\Machine", sid) == L"HKEY_LOCAL_MACHINE");
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\REGISTRY\\USER\\S-1-5-21-1-2-3-1001\\Software", sid) == L"HKEY_CURRENT_USER\\Software");
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\REGISTRY\\USER\\S-1-5-21-1-2-3-1001_Classes\\.txt", sid) == L"HKEY_CURRENT_USER\\Software\\Classes\\.txt");
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\REGISTRY\\USER\\S-1-5-18\\Software", sid) == L"HKEY_USERS\\S-1-5-18\\Software");
	// another user whose SID starts like the current one's
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\REGISTRY\\USER\\S-1-5-21-1-2-3-10010", sid) == L"HKEY_USERS\\S-1-5-21-1-2-3-10010");
	// not mapped to a standard root
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\REGISTRY\\A\\{guid}\\X", sid) == L"\\REGISTRY\\A\\{guid}\\X");
	CHECK(RegistryMonitor::KernelPathToStandard(L"", sid).IsEmpty());
	CHECK(RegistryMonitor::KernelPathToStandard(L"\\REGISTRY\\USER\\X", L"") == L"HKEY_USERS\\X");
}

TEST_CASE("Monitor helpers", "[monitor]") {
	CHECK(RegistryMonitor::FormatStatus(0) == L"Success");
	CHECK(RegistryMonitor::FormatStatus((LONG)0xC0000034).Right(12) == L"(0xC0000034)");
	CHECK(RegistryMonitor::IsChange(RegistryOperation::SetValue));
	CHECK_FALSE(RegistryMonitor::IsChange(RegistryOperation::QueryValue));
	CHECK(RegistryMonitor::GetOperationName(RegistryOperation::DeleteValue) == L"Delete Value");
	CHECK(RegistryMonitor::GetCurrentUserSid().Left(4) == L"S-1-");
}

TEST_CASE("Monitoring needs elevation", "[monitor]") {
	if (SecurityHelper::IsRunningElevated())
		SKIP("running elevated");
	RegistryMonitor monitor;
	CHECK_FALSE(monitor.Start(false));
	CHECK(::GetLastError() == ERROR_ACCESS_DENIED);
	CHECK_FALSE(monitor.IsRunning());
}

TEST_CASE("Elevated: the monitor sees other processes' changes", "[monitor][elevated]") {
	if (!SecurityHelper::IsRunningElevated())
		SKIP("requires running elevated");

	ScratchKey scratch;
	std::atomic<int> notifications = 0;
	RegistryMonitor monitor;
	REQUIRE(monitor.Start(false, [&] { notifications++; }));
	CHECK(monitor.IsRunning());
	::Sleep(1000);

	auto key = scratch / L"Monitored";
	Reg(L"add \"" + key + L"\" /v Value /d data /f");
	Reg(L"delete \"" + key + L"\" /v Value /f");
	Reg(L"delete \"" + key + L"\" /f");

	// real-time events are delivered when buffers flush, about once a second
	std::vector<RegistryEvent> events;
	auto has = [&](RegistryOperation op, PCWSTR value) {
		return std::ranges::any_of(events, [&](auto& e) {
			return e.Operation == op && e.Key.CompareNoCase(key) == 0 && (value == nullptr || e.Value == value)
				&& e.ProcessName.CompareNoCase(L"reg.exe") == 0 && e.Status == 0;
			});
	};
	for (int i = 0; i < 100 && !(has(RegistryOperation::CreateKey, nullptr) && has(RegistryOperation::SetValue, L"Value")
		&& has(RegistryOperation::DeleteValue, L"Value") && has(RegistryOperation::DeleteKey, nullptr)); i++) {
		::Sleep(100);
		for (auto& e : monitor.TakeEvents())
			events.push_back(std::move(e));
	}
	monitor.Stop();
	CHECK_FALSE(monitor.IsRunning());

	INFO(events.size() << " events");
	// what reg.exe was seen doing, for when a check fails
	std::wstring seen;
	for (auto& e : events)
		if (e.ProcessName.CompareNoCase(L"reg.exe") == 0)
			seen += std::wstring(L"\n  ") + (PCWSTR)RegistryMonitor::GetOperationName(e.Operation) + L" " + (PCWSTR)e.Key
				+ L" [" + (PCWSTR)e.Value + L"] " + (PCWSTR)RegistryMonitor::FormatStatus(e.Status);
	INFO("expected key: " << CW2A(key) << "\nreg.exe events:" << CW2A(seen.c_str()));
	CHECK(has(RegistryOperation::CreateKey, nullptr));
	CHECK(has(RegistryOperation::SetValue, L"Value"));
	CHECK(has(RegistryOperation::DeleteValue, L"Value"));
	CHECK(has(RegistryOperation::DeleteKey, nullptr));
	CHECK(notifications > 0);
	// reads were not asked for
	CHECK_FALSE(std::ranges::any_of(events, [](auto& e) { return !RegistryMonitor::IsChange(e.Operation); }));
	// and none of this process' own activity
	CHECK_FALSE(std::ranges::any_of(events, [](auto& e) { return e.ProcessId == ::GetCurrentProcessId(); }));
}

TEST_CASE("Elevated: the monitor sees reads when asked to", "[monitor][elevated]") {
	if (!SecurityHelper::IsRunningElevated())
		SKIP("requires running elevated");

	ScratchKey scratch;
	auto key = scratch / L"Read";
	Reg(L"add \"" + key + L"\" /v Value /d data /f");

	RegistryMonitor monitor;
	REQUIRE(monitor.Start(true));
	Reg(L"query \"" + key + L"\" /v Value");

	std::vector<RegistryEvent> events;
	auto has = [&](RegistryOperation op, PCWSTR value) {
		return std::ranges::any_of(events, [&](auto& e) {
			return e.Operation == op && e.Key.CompareNoCase(key) == 0 && (value == nullptr || e.Value == value)
				&& e.ProcessName.CompareNoCase(L"reg.exe") == 0 && e.Status == 0;
			});
	};
	for (int i = 0; i < 100 && !(has(RegistryOperation::OpenKey, nullptr) && has(RegistryOperation::QueryValue, L"Value")); i++) {
		::Sleep(100);
		for (auto& e : monitor.TakeEvents())
			events.push_back(std::move(e));
	}
	monitor.Stop();

	INFO(events.size() << " events");
	CHECK(has(RegistryOperation::OpenKey, nullptr));
	CHECK(has(RegistryOperation::QueryValue, L"Value"));
	// a key the kernel opened before monitoring started still has its name
	CHECK_FALSE(std::ranges::any_of(events, [&](auto& e) {
		return e.ProcessName.CompareNoCase(L"reg.exe") == 0 && e.Key.Left(1) == L"(";
		}));
}
