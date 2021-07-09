#pragma once

#include "FindOptions.h"
#include <wil\resource.h>
#include <mutex>

using RegistrySearcherCallback = std::function<void(PCWSTR, PCWSTR, void*)>;

struct RegistrySearcher {
	void SetStartKey(PCWSTR startKey);
	void SetOptions(FindOptions options);
	void SetText(PCWSTR text);
	bool Find(RegistrySearcherCallback callback);

	bool Cancel();
	bool Continue();

	bool IsRunning() const;
	bool IsCancelled() const;

protected:
	bool FindNextWorker(HKEY hKey, const CString& path);
	bool Notify(PCWSTR path, PCWSTR value, void* data);

private:
	DWORD DoSearch();
	wil::unique_handle _hThread;
	FindOptions _options{ FindOptions::None };
	RegistrySearcherCallback _cb;
	CString _searchText;
	std::mutex _lock;
	CString _startKey;
	wil::unique_handle _hCancelEvent, _hContinueEvent;
	std::atomic<bool> _inProgress{ false };
	std::atomic<bool> _cancel{ false };
};
