#include "pch.h"
#include <initguid.h>	// defines SystemRegistryProviderGuid and SystemProcessProviderGuid (from evntrace.h) here
#include "RegistryMonitor.h"
#include "Helpers.h"
#include "NtDll.h"
#include <wil\resource.h>
#include <tdh.h>

#pragma comment(lib, "tdh")

//
// The kernel's registry events come from the system registry provider, which only logs to a system logger session.
// (The Microsoft-Windows-Kernel-Registry manifest provider can be enabled in an ordinary session, but logs nothing to it.)
// Events are the classic kernel "Registry" events: the opcode is the operation, and the payload is
//   INT64 InitialTime, UINT32 Status, UINT32 Index, PVOID KeyHandle, WCHAR KeyName[]
// where KeyHandle is the kernel's key control block (KCB). Its name comes from KCB create events, and for the KCBs
// that already exist, from the rundown a system logger session logs when it stops (capture state isn't supported),
// which is why Start runs a short helper session first.
// For create/open, KeyName is relative to the KCB (or absolute if it's 0); for value operations, KeyName is the value name.
// Process names come from the system process provider's start and rundown events, since many processes
// (e.g. reg.exe) have exited by the time their events are delivered.
//
namespace {
	// the classic kernel event classes the system providers' events carry as their provider id
	const GUID RegistryGuid = { 0xae53722e, 0xc863, 0x11d2, { 0x86, 0x59, 0x00, 0xc0, 0x4f, 0xa3, 0x21, 0xa1 } };
	const GUID ProcessGuid = { 0x3d6fa8d0, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };

	enum Opcode : UCHAR {
		OpCreate = 10, OpOpen = 11, OpDelete = 12, OpQuery = 13, OpSetValue = 14, OpDeleteValue = 15, OpQueryValue = 16,
		OpEnumerateKey = 17, OpEnumerateValueKey = 18, OpQueryMultipleValue = 19, OpSetInformation = 20, OpFlush = 21,
		OpKcbCreate = 22, OpKcbDelete = 23, OpKcbRundownBegin = 24, OpKcbRundownEnd = 25, OpClose = 27,
		OpSetSecurity = 28, OpQuerySecurity = 29,
	};

	bool ToOperation(UCHAR opcode, RegistryOperation& op) {
		switch (opcode) {
			case OpCreate: op = RegistryOperation::CreateKey; return true;
			case OpOpen: op = RegistryOperation::OpenKey; return true;
			case OpDelete: op = RegistryOperation::DeleteKey; return true;
			case OpQuery: op = RegistryOperation::QueryKey; return true;
			case OpSetValue: op = RegistryOperation::SetValue; return true;
			case OpDeleteValue: op = RegistryOperation::DeleteValue; return true;
			case OpQueryValue: op = RegistryOperation::QueryValue; return true;
			case OpEnumerateKey: op = RegistryOperation::EnumerateKey; return true;
			case OpEnumerateValueKey: op = RegistryOperation::EnumerateValues; return true;
			case OpQueryMultipleValue: op = RegistryOperation::QueryMultipleValues; return true;
			case OpSetInformation: op = RegistryOperation::SetInformation; return true;
			case OpFlush: op = RegistryOperation::Flush; return true;
			case OpClose: op = RegistryOperation::CloseKey; return true;
			case OpSetSecurity: op = RegistryOperation::SetSecurity; return true;
			case OpQuerySecurity: op = RegistryOperation::QuerySecurity; return true;
		}
		return false;
	}

	struct EventData {
		LONG Status;
		ULONGLONG Kcb;
		CString Name;
	};

	bool ParseEvent(PEVENT_RECORD record, EventData& data) {
		auto p = (BYTE const*)record->UserData;
		size_t size = record->UserDataLength;
		size_t pointerSize = (record->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8;
		size_t nameOffset = 8 + 4 + 4 + pointerSize;
		if (!p || size < nameOffset)
			return false;
		memcpy(&data.Status, p + 8, sizeof(LONG));
		data.Kcb = 0;
		memcpy(&data.Kcb, p + 16, pointerSize);
		auto chars = (PCWSTR)(p + nameOffset);
		auto count = (size - nameOffset) / sizeof(WCHAR);
		data.Name.SetString(chars, (int)wcsnlen(chars, count));
		return true;
	}

	enum ProcessOpcode : UCHAR { OpProcessStart = 1, OpProcessRundown = 3, OpProcessRundownEnd = 4 };

	ULONGLONG GetNumber(PEVENT_RECORD record, PCWSTR name) {
		PROPERTY_DATA_DESCRIPTOR desc{ (ULONGLONG)name, ULONG_MAX, 0 };
		ULONGLONG value = 0;
		ULONG size = 0;
		if (ERROR_SUCCESS == ::TdhGetPropertySize(record, 0, nullptr, 1, &desc, &size) && size <= sizeof(value))
			::TdhGetProperty(record, 0, nullptr, 1, &desc, size, (PBYTE)&value);
		return value;
	}

	CString GetAnsiString(PEVENT_RECORD record, PCWSTR name) {
		PROPERTY_DATA_DESCRIPTOR desc{ (ULONGLONG)name, ULONG_MAX, 0 };
		ULONG size = 0;
		if (ERROR_SUCCESS != ::TdhGetPropertySize(record, 0, nullptr, 1, &desc, &size) || size == 0)
			return L"";
		std::vector<char> text(size + 1);
		if (ERROR_SUCCESS != ::TdhGetProperty(record, 0, nullptr, 1, &desc, size, (PBYTE)text.data()))
			return L"";
		return CString(text.data());
	}

	// more than this many events waiting means nobody is taking them
	const size_t MaxPending = 200000;
	const size_t MaxKeyNames = 1000000;

	bool StartsWithKey(CString const& path, CString const& prefix) {
		return path.GetLength() >= prefix.GetLength() && _wcsnicmp(path, prefix, prefix.GetLength()) == 0
			&& (path.GetLength() == prefix.GetLength() || path[prefix.GetLength()] == L'\\');
	}
}

RegistryMonitor::~RegistryMonitor() {
	Stop();
}

bool RegistryMonitor::Start(bool includeReads, std::function<void()> notify) {
	Stop();
	m_IncludeReads = includeReads;
	m_Notify = std::move(notify);
	m_UserSid = GetCurrentUserSid();
	m_KeyNames.clear();
	m_LastKcb.clear();
	m_ProcessNames.clear();
	{
		std::lock_guard locker(m_Lock);
		m_Pending.clear();
		m_Dropped = 0;
	}

	m_NamesReady.ResetEvent();
	m_SessionName.Format(L"TotalRegistry Monitor %u", ::GetCurrentProcessId());
	auto initProperties = [&] {
		return InitProperties(m_SessionName, m_Properties);
	};

	auto props = initProperties();
	auto error = ::StartTrace(&m_Session, m_SessionName, props);
	if (error == ERROR_ALREADY_EXISTS) {
		// left by an earlier run of this process that didn't stop it
		::ControlTrace(0, m_SessionName, props, EVENT_TRACE_CONTROL_STOP);
		props = initProperties();
		error = ::StartTrace(&m_Session, m_SessionName, props);
	}
	if (error != ERROR_SUCCESS) {
		m_Session = 0;
		::SetLastError(error);
		return false;
	}

	error = ::EnableTraceEx2(m_Session, &SystemRegistryProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION,
		SYSTEM_REGISTRY_KW_GENERAL, 0, 0, nullptr);
	if (error == ERROR_SUCCESS)
		error = ::EnableTraceEx2(m_Session, &SystemProcessProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION,
			SYSTEM_PROCESS_KW_GENERAL, 0, 0, nullptr);
	if (error != ERROR_SUCCESS) {
		Stop();
		::SetLastError(error);
		return false;
	}

	EVENT_TRACE_LOGFILE log{};
	log.LoggerName = m_SessionName.GetBuffer();
	log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
	log.EventRecordCallback = OnEvent;
	log.Context = this;
	m_Trace = ::OpenTrace(&log);
	m_SessionName.ReleaseBuffer();
	if (m_Trace == INVALID_PROCESSTRACE_HANDLE) {
		error = ::GetLastError();
		Stop();
		::SetLastError(error);
		return false;
	}

	m_Thread = std::thread([this] {
		::ProcessTrace(&m_Trace, 1, nullptr, nullptr);
		});

	// the names of existing keys and processes; the session's events wait for them, so every KCB created
	// after the session started is either in the rundown or has its own create event that follows
	RunDown();
	m_NamesReady.SetEvent();
	return true;
}

void RegistryMonitor::RunDown() {
	CString name = m_SessionName + L" Rundown";
	std::vector<BYTE> properties;
	TRACEHANDLE session = 0;
	auto props = InitProperties(name, properties);
	auto error = ::StartTrace(&session, name, props);
	if (error == ERROR_ALREADY_EXISTS) {
		::ControlTrace(0, name, props, EVENT_TRACE_CONTROL_STOP);
		props = InitProperties(name, properties);
		error = ::StartTrace(&session, name, props);
	}
	if (error != ERROR_SUCCESS)
		return;

	::EnableTraceEx2(session, &SystemRegistryProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION,
		SYSTEM_REGISTRY_KW_GENERAL, 0, 0, nullptr);
	::EnableTraceEx2(session, &SystemProcessProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION,
		SYSTEM_PROCESS_KW_GENERAL, 0, 0, nullptr);
	EVENT_TRACE_LOGFILE log{};
	log.LoggerName = name.GetBuffer();
	log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
	log.EventRecordCallback = OnRundownEvent;
	log.Context = this;
	auto trace = ::OpenTrace(&log);
	name.ReleaseBuffer();
	std::thread thread;
	if (trace != INVALID_PROCESSTRACE_HANDLE)
		thread = std::thread([&] { ::ProcessTrace(&trace, 1, nullptr, nullptr); });
	// stopping the session logs the rundown, which is delivered before ProcessTrace returns
	::ControlTrace(session, nullptr, InitProperties(name, properties), EVENT_TRACE_CONTROL_STOP);
	if (thread.joinable()) {
		thread.join();
		::CloseTrace(trace);
	}
}

void WINAPI RegistryMonitor::OnRundownEvent(PEVENT_RECORD record) {
	// the main session's events wait for the rundown, so the maps can be filled here
	auto monitor = static_cast<RegistryMonitor*>(record->UserContext);
	auto& provider = record->EventHeader.ProviderId;
	auto opcode = record->EventHeader.EventDescriptor.Opcode;
	if (IsEqualGUID(provider, RegistryGuid) && (opcode == OpKcbRundownBegin || opcode == OpKcbRundownEnd)) {
		EventData data;
		if (ParseEvent(record, data) && data.Kcb && !data.Name.IsEmpty())
			monitor->m_KeyNames.try_emplace(data.Kcb, data.Name);
	}
	else if (IsEqualGUID(provider, ProcessGuid))
		monitor->HandleProcessEvent(record);
}

void RegistryMonitor::HandleProcessEvent(PEVENT_RECORD record) {
	auto opcode = record->EventHeader.EventDescriptor.Opcode;
	if (opcode == OpProcessStart || opcode == OpProcessRundown || opcode == OpProcessRundownEnd) {
		auto pid = (DWORD)GetNumber(record, L"ProcessId");
		auto name = GetAnsiString(record, L"ImageFileName");
		if (pid && !name.IsEmpty())
			m_ProcessNames[pid] = name;
	}
}

EVENT_TRACE_PROPERTIES* RegistryMonitor::InitProperties(CString const& sessionName, std::vector<BYTE>& buffer) {
	auto size = sizeof(EVENT_TRACE_PROPERTIES) + (sessionName.GetLength() + 1) * sizeof(WCHAR);
	buffer.assign(size, 0);
	auto props = (EVENT_TRACE_PROPERTIES*)buffer.data();
	props->Wnode.BufferSize = (ULONG)size;
	props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
	props->Wnode.ClientContext = 1;		// QPC; ProcessTrace converts timestamps to FILETIME
	props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
	props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
	props->BufferSize = 256;			// KB; the kernel logs thousands of registry events a second
	props->MinimumBuffers = 32;
	props->MaximumBuffers = 128;
	props->FlushTimer = 1;				// deliver at least every second, even when buffers aren't full
	return props;
}

void RegistryMonitor::Stop() {
	if (m_Session) {
		auto props = (EVENT_TRACE_PROPERTIES*)m_Properties.data();
		::ControlTrace(m_Session, nullptr, props, EVENT_TRACE_CONTROL_STOP);
		m_Session = 0;
	}
	if (m_Trace != INVALID_PROCESSTRACE_HANDLE) {
		// ProcessTrace returns
		::CloseTrace(m_Trace);
		m_Trace = INVALID_PROCESSTRACE_HANDLE;
	}
	if (m_Thread.joinable())
		m_Thread.join();
}

bool RegistryMonitor::IsRunning() const {
	return m_Session != 0;
}

std::vector<RegistryEvent> RegistryMonitor::TakeEvents() {
	std::vector<RegistryEvent> events;
	std::lock_guard locker(m_Lock);
	events.swap(m_Pending);
	return events;
}

size_t RegistryMonitor::GetDroppedEvents() const {
	std::lock_guard locker(m_Lock);
	return m_Dropped;
}

void WINAPI RegistryMonitor::OnEvent(PEVENT_RECORD record) {
	static_cast<RegistryMonitor*>(record->UserContext)->HandleEvent(record);
}

void RegistryMonitor::HandleEvent(PEVENT_RECORD record) {
	if (!m_NamesReady.is_signaled())
		m_NamesReady.wait();

	auto& provider = record->EventHeader.ProviderId;
	if (IsEqualGUID(provider, ProcessGuid)) {
		HandleProcessEvent(record);
		return;
	}
	if (!IsEqualGUID(provider, RegistryGuid))
		return;

	auto opcode = record->EventHeader.EventDescriptor.Opcode;
	auto pid = record->EventHeader.ProcessId;
	auto tid = record->EventHeader.ThreadId;
	EventData data;

	// key names, by key control block
	switch (opcode) {
		case OpKcbCreate:
		case OpKcbRundownBegin:
		case OpKcbRundownEnd:
			if (ParseEvent(record, data) && data.Kcb && !data.Name.IsEmpty()) {
				if (m_KeyNames.size() >= MaxKeyNames)
					m_KeyNames.clear();
				m_KeyNames[data.Kcb] = data.Name;
				// the create/open that follows on this thread has the name's original casing (KCB names are upper case)
				if (opcode == OpKcbCreate)
					m_LastKcb[tid] = data.Kcb;
			}
			return;

		case OpKcbDelete:
			if (ParseEvent(record, data))
				m_KeyNames.erase(data.Kcb);
			return;
	}

	RegistryOperation op;
	if (!ToOperation(opcode, op) || !ParseEvent(record, data))
		return;
	bool isOpen = op == RegistryOperation::CreateKey || op == RegistryOperation::OpenKey;
	// unless reads are wanted, only changes are parsed further; opens are needed to fix the case of key names
	if (!m_IncludeReads && !IsChange(op) && !isOpen)
		return;

	auto nameOf = [&](ULONGLONG kcb) -> CString {
		auto it = m_KeyNames.find(kcb);
		return it == m_KeyNames.end() ? CString() : it->second;
	};
	CString keyName, valueName;
	if (isOpen) {
		// relative to the KCB's key, or absolute
		if (data.Kcb == 0 || (!data.Name.IsEmpty() && data.Name[0] == L'\\'))
			keyName = data.Name;
		else {
			auto base = nameOf(data.Kcb);
			keyName = base.IsEmpty() ? CString() : data.Name.IsEmpty() ? base : base + L"\\" + data.Name;
		}
		if (data.Status >= 0 && !keyName.IsEmpty()) {
			if (auto it = m_LastKcb.find(tid); it != m_LastKcb.end()) {
				auto& known = m_KeyNames[it->second];
				if (known.CompareNoCase(keyName) == 0)
					known = keyName;
				m_LastKcb.erase(it);
			}
		}
		if (!m_IncludeReads && op == RegistryOperation::OpenKey)
			return;
	}
	else {
		keyName = nameOf(data.Kcb);
		switch (op) {
			case RegistryOperation::SetValue:
			case RegistryOperation::DeleteValue:
			case RegistryOperation::QueryValue:
			case RegistryOperation::EnumerateValues:
			case RegistryOperation::QueryMultipleValues:
				valueName = data.Name;
				break;
		}
	}
	// our own activity (e.g. refreshing the view) would drown everything else
	if (pid == m_SelfPid)
		return;

	RegistryEvent event;
	event.Time = { record->EventHeader.TimeStamp.LowPart, (DWORD)record->EventHeader.TimeStamp.HighPart };
	event.ProcessId = pid;
	event.ProcessName = GetProcessName(pid);
	event.Operation = op;
	event.Key = keyName.IsEmpty() ? CString(L"(key opened before monitoring started)") : KernelPathToStandard(keyName, m_UserSid);
	event.Value = valueName;
	event.Status = data.Status;

	bool notify;
	{
		std::lock_guard locker(m_Lock);
		if (m_Pending.size() >= MaxPending) {
			m_Dropped++;
			return;
		}
		notify = m_Pending.empty();
		m_Pending.push_back(std::move(event));
	}
	// once per batch, so the UI thread's queue isn't flooded
	if (notify && m_Notify)
		m_Notify();
}

CString RegistryMonitor::GetProcessName(DWORD pid) {
	if (auto it = m_ProcessNames.find(pid); it != m_ProcessNames.end())
		return it->second;

	CString name;
	if (pid == 4)
		name = L"System";
	else if (pid == 0)
		name = L"Idle";
	else {
		wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
		WCHAR path[MAX_PATH];
		DWORD size = _countof(path);
		if (hProcess && ::QueryFullProcessImageName(hProcess.get(), 0, path, &size)) {
			name = path;
			name = name.Mid(name.ReverseFind(L'\\') + 1);
		}
	}
	m_ProcessNames[pid] = name;
	return name;
}

CString RegistryMonitor::GetOperationName(RegistryOperation op) {
	switch (op) {
		case RegistryOperation::CreateKey: return L"Create Key";
		case RegistryOperation::OpenKey: return L"Open Key";
		case RegistryOperation::DeleteKey: return L"Delete Key";
		case RegistryOperation::QueryKey: return L"Query Key";
		case RegistryOperation::SetValue: return L"Set Value";
		case RegistryOperation::DeleteValue: return L"Delete Value";
		case RegistryOperation::QueryValue: return L"Query Value";
		case RegistryOperation::EnumerateKey: return L"Enumerate Keys";
		case RegistryOperation::EnumerateValues: return L"Enumerate Values";
		case RegistryOperation::QueryMultipleValues: return L"Query Values";
		case RegistryOperation::SetInformation: return L"Set Information";
		case RegistryOperation::Flush: return L"Flush";
		case RegistryOperation::CloseKey: return L"Close Key";
		case RegistryOperation::QuerySecurity: return L"Query Security";
		case RegistryOperation::SetSecurity: return L"Set Security";
	}
	return L"";
}

bool RegistryMonitor::IsChange(RegistryOperation op) {
	switch (op) {
		case RegistryOperation::CreateKey:
		case RegistryOperation::DeleteKey:
		case RegistryOperation::SetValue:
		case RegistryOperation::DeleteValue:
		case RegistryOperation::SetInformation:
		case RegistryOperation::SetSecurity:
			return true;
	}
	return false;
}

CString RegistryMonitor::KernelPathToStandard(CString const& path, CString const& currentUserSid) {
	auto replace = [&](CString const& prefix, PCWSTR with, CString& result) {
		if (!StartsWithKey(path, prefix))
			return false;
		result = with + path.Mid(prefix.GetLength());
		return true;
	};

	CString result;
	if (!currentUserSid.IsEmpty()) {
		// the classes hive is mounted under the user's key
		if (replace(L"\\REGISTRY\\USER\\" + currentUserSid + L"_Classes", L"HKEY_CURRENT_USER\\Software\\Classes", result)
			|| replace(L"\\REGISTRY\\USER\\" + currentUserSid, L"HKEY_CURRENT_USER", result))
			return result;
	}
	if (replace(L"\\REGISTRY\\MACHINE", L"HKEY_LOCAL_MACHINE", result) || replace(L"\\REGISTRY\\USER", L"HKEY_USERS", result))
		return result;
	return path;
}

CString RegistryMonitor::GetCurrentUserSid() {
	return Helpers::GetCurrentUserSid();
}

CString RegistryMonitor::FormatStatus(LONG status) {
	if (status == 0)
		return L"Success";
	CString code;
	code.Format(L"0x%08X", (DWORD)status);
	auto error = ::RtlNtStatusToDosError(status);
	if (error == ERROR_MR_MID_NOT_FOUND)
		return code;
	auto text = Helpers::GetErrorText(error);
	return text.IsEmpty() ? code : text + L" (" + code + L")";
}
