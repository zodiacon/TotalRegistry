#pragma once

#include <evntrace.h>
#include <evntcons.h>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <wil\resource.h>

enum class RegistryOperation {
	CreateKey = 1,
	OpenKey,
	DeleteKey,
	QueryKey,
	SetValue,
	DeleteValue,
	QueryValue,
	EnumerateKey,
	EnumerateValues,
	QueryMultipleValues,
	SetInformation,
	Flush,
	CloseKey,
	QuerySecurity,
	SetSecurity,
};

struct RegistryEvent {
	FILETIME Time;
	DWORD ProcessId;
	CString ProcessName;
	RegistryOperation Operation;
	CString Key;		// standard (HKEY_...) path where possible
	CString Value;
	LONG Status;		// NTSTATUS
};

//
// system-wide registry activity, from the kernel's system registry ETW provider (in a system logger session)
// needs administrator rights; events are collected on a background thread and taken in batches
//
class RegistryMonitor {
public:
	RegistryMonitor() = default;
	~RegistryMonitor();
	RegistryMonitor(RegistryMonitor const&) = delete;
	RegistryMonitor& operator=(RegistryMonitor const&) = delete;

	// changes (creating and deleting keys and values, security changes) and optionally reads too
	// notify is called on the monitoring thread when events become available after TakeEvents emptied the queue
	bool Start(bool includeReads, std::function<void()> notify = nullptr);
	void Stop();
	bool IsRunning() const;

	std::vector<RegistryEvent> TakeEvents();
	// events dropped since the queue grew too large (nobody took them)
	size_t GetDroppedEvents() const;

	static CString GetOperationName(RegistryOperation op);
	static bool IsChange(RegistryOperation op);
	// \REGISTRY\MACHINE\X -> HKEY_LOCAL_MACHINE\X, the current user's \REGISTRY\USER\<sid> -> HKEY_CURRENT_USER, etc.
	static CString KernelPathToStandard(CString const& path, CString const& currentUserSid);
	static CString GetCurrentUserSid();
	static CString FormatStatus(LONG status);

private:
	static void WINAPI OnEvent(PEVENT_RECORD record);
	static void WINAPI OnRundownEvent(PEVENT_RECORD record);
	static EVENT_TRACE_PROPERTIES* InitProperties(CString const& sessionName, std::vector<BYTE>& buffer);
	void RunDown();
	void HandleEvent(PEVENT_RECORD record);
	void HandleProcessEvent(PEVENT_RECORD record);
	CString GetProcessName(DWORD pid);

	CString m_SessionName;
	std::vector<BYTE> m_Properties;		// EVENT_TRACE_PROPERTIES and the session name
	TRACEHANDLE m_Session{ 0 };
	TRACEHANDLE m_Trace{ INVALID_PROCESSTRACE_HANDLE };
	std::thread m_Thread;
	std::function<void()> m_Notify;
	bool m_IncludeReads{ false };
	CString m_UserSid;
	DWORD m_SelfPid{ ::GetCurrentProcessId() };
	wil::unique_event m_NamesReady{ wil::EventOptions::ManualReset };	// the rundown filled the maps below

	// only used on the monitoring thread
	std::unordered_map<ULONGLONG, CString> m_KeyNames;		// key control block -> kernel name, from KCB create/rundown events
	std::unordered_map<DWORD, ULONGLONG> m_LastKcb;			// thread -> KCB just created for it, to fix its name's case
	std::unordered_map<DWORD, CString> m_ProcessNames;

	mutable std::mutex m_Lock;
	std::vector<RegistryEvent> m_Pending;
	size_t m_Dropped{ 0 };
};
