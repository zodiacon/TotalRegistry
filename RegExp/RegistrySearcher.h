#pragma once

#include "FindOptions.h"
#include <wil\resource.h>
#include <mutex>

using RegistrySearcherCallback = std::function<void(PCWSTR, PCWSTR, PCWSTR)>;

struct RegistrySearcher {
	void SetStartKey(PCWSTR startKey);
	void SetOptions(FindOptions options);
	void SetText(PCWSTR text);
	bool Find(RegistrySearcherCallback callback);

	bool Cancel();
	bool Continue();

	bool IsRunning() const;
	bool IsCancelled() const;
	bool WaitForCompletion(DWORD timeout = INFINITE);

protected:
	bool FindNextWorker(HKEY hKey, const CString& path);
	bool Notify(PCWSTR path, PCWSTR value, PCWSTR data);

private:
	DWORD DoSearch();
	wil::unique_handle _hThread;
	FindOptions _options{ FindOptions::None };
	RegistrySearcherCallback _cb;
	CString _searchText;
	std::mutex _lock;
	CString _startKey;
	wil::unique_handle _hCancelEvent, _hContinueEvent, _hDoneEvent;
	std::atomic<bool> _inProgress{ false };
	std::atomic<bool> _cancel{ false };
};
