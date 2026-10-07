#include "pch.h"
#include "KeyWatcher.h"
#include "Registry.h"

KeyWatcher::~KeyWatcher() {
	Stop();
}

bool KeyWatcher::Watch(CString const& path, std::function<void()> onChange) {
	Stop();
	if (path.Left(2) == L"\\\\") {
		::SetLastError(ERROR_NOT_SUPPORTED);
		return false;
	}

	m_Key = Registry::OpenKey(path, KEY_NOTIFY);
	if (!m_Key)
		return false;
	// a predefined handle (a root key) is never closed, so its notification could never be cancelled
	if (std::ranges::any_of(Registry::Keys, [&](auto& k) { return k.hKey == m_Key.Get(); })) {
		m_Key.Close();
		::SetLastError(ERROR_NOT_SUPPORTED);
		return false;
	}

	if (!m_Event.try_create(wil::EventOptions::ManualReset, nullptr)) {
		Stop();
		return false;
	}
	m_Wait = ::CreateThreadpoolWait(OnSignaled, this, nullptr);
	if (!m_Wait) {
		Stop();
		return false;
	}

	m_OnChange = std::move(onChange);
	m_Path = path;
	if (!Rearm()) {
		auto error = ::GetLastError();
		Stop();
		::SetLastError(error);
		return false;
	}
	return true;
}

bool KeyWatcher::Rearm() {
	if (!m_Wait)
		return false;

	m_Event.ResetEvent();
	// thread agnostic, so the notification survives the registering thread
	auto error = ::RegNotifyChangeKeyValue(m_Key.Get(), FALSE,
		REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC, m_Event.get(), TRUE);
	if (error != ERROR_SUCCESS) {
		::SetLastError(error);
		return false;
	}
	::SetThreadpoolWait(m_Wait, m_Event.get(), nullptr);
	return true;
}

void KeyWatcher::Stop() {
	if (m_Wait) {
		::SetThreadpoolWait(m_Wait, nullptr, nullptr);
		::WaitForThreadpoolWaitCallbacks(m_Wait, TRUE);
		::CloseThreadpoolWait(m_Wait);
		m_Wait = nullptr;
	}
	// closing the key ends the notification
	m_Key.Close();
	m_Event.reset();
	m_OnChange = nullptr;
	m_Path.Empty();
}

bool KeyWatcher::IsWatching() const {
	return m_Wait != nullptr;
}

CString const& KeyWatcher::GetPath() const {
	return m_Path;
}

void CALLBACK KeyWatcher::OnSignaled(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WAIT, TP_WAIT_RESULT) {
	auto watcher = static_cast<KeyWatcher*>(context);
	if (watcher->m_OnChange)
		watcher->m_OnChange();
}
