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
	wil::unique_handle m_hThread;
	FindOptions m_Options{ FindOptions::None };
	RegistrySearcherCallback m_Callback;
	CString m_SearchText;
	std::mutex m_Lock;
	CString m_StartKey;
	wil::unique_handle m_hCancelEvent, m_hContinueEvent, m_hDoneEvent;
	std::atomic<bool> m_InProgress{ false };
	std::atomic<bool> m_Cancel{ false };
};
