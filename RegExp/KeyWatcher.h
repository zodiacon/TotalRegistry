#pragma once

#include "RegistryKey.h"
#include <wil\resource.h>

//
// watches a key for changes to its values and for subkeys added or removed (not deeper changes)
// the callback runs on a thread pool thread, once per change, until Rearm is called to watch for the next one
//
class KeyWatcher {
public:
	KeyWatcher() = default;
	~KeyWatcher();
	KeyWatcher(KeyWatcher const&) = delete;
	KeyWatcher& operator=(KeyWatcher const&) = delete;

	// stops watching any previous key; fails for remote keys (no asynchronous notifications) and root keys
	bool Watch(CString const& path, std::function<void()> onChange);
	// watches for the next change after the callback was invoked; fails if the key was deleted
	bool Rearm();
	// no callbacks are in progress or made after this returns
	void Stop();

	bool IsWatching() const;
	CString const& GetPath() const;

private:
	static void CALLBACK OnSignaled(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WAIT, TP_WAIT_RESULT);

	RegistryKey m_Key;
	wil::unique_event_nothrow m_Event;
	PTP_WAIT m_Wait{ nullptr };
	std::function<void()> m_OnChange;
	CString m_Path;
};
