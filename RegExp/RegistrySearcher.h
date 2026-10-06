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

	// true if a search is waiting to continue, and the text and options have not changed since it started
	bool CanContinue() const;
	bool IsRunning() const;
	bool IsCancelled() const;
	bool WaitForCompletion(DWORD timeout = INFINITE);

private:
	//
	// the state of a single search, shared with its worker thread
	// a new search gets a new instance, so a previous thread that is still winding down never sees it
	//
	struct Search {
		CString RawText;
		CString Text;	// upper-cased unless MatchCase
		CString StartKey;
		FindOptions Options{ FindOptions::None };
		RegistrySearcherCallback Callback;
		wil::unique_handle hCancelEvent, hContinueEvent, hDoneEvent;
		std::atomic<bool> Running{ true };
		std::atomic<bool> Cancelled{ false };
		std::atomic<bool> Abandoned{ false };	// replaced by a newer search, no more callbacks
	};

	static DWORD DoSearch(Search& search);
	static bool FindNextWorker(Search& search, HKEY hKey, const CString& path);
	static bool Notify(Search& search, PCWSTR path, PCWSTR name, PCWSTR data);
	static bool Matches(Search const& search, CString text);

	std::shared_ptr<Search> GetSearch() const;

	mutable std::mutex m_Lock;
	std::shared_ptr<Search> m_Search;
	wil::unique_handle m_hThread;
	FindOptions m_Options{ FindOptions::None };
	CString m_SearchText;
	CString m_StartKey;
};
