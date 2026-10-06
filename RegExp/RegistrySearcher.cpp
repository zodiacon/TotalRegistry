#include "pch.h"
#include "RegistrySearcher.h"
#include "Registry.h"

void RegistrySearcher::SetStartKey(PCWSTR startKey) {
	std::lock_guard locker(m_Lock);
	m_StartKey = startKey;
}

void RegistrySearcher::SetOptions(FindOptions options) {
	std::lock_guard locker(m_Lock);
	m_Options = options;
}

void RegistrySearcher::SetText(PCWSTR text) {
	std::lock_guard locker(m_Lock);
	m_SearchText = text;
}

RegistrySearcher::~RegistrySearcher() {
	std::lock_guard locker(m_Lock);
	if (m_Search) {
		m_Search->Abandoned = true;
		m_Search->Cancelled = true;
		::SetEvent(m_Search->hCancelEvent.get());
	}
}

bool RegistrySearcher::Find(RegistrySearcherCallback callback, bool pauseOnResult) {
	ATLASSERT(callback);
	auto search = std::make_shared<Search>();
	search->PauseOnResult = pauseOnResult;
	{
		std::lock_guard locker(m_Lock);
		search->RawText = m_SearchText;
		search->Options = m_Options;
		search->StartKey = m_StartKey;
	}
	search->Text = search->RawText;
	if ((search->Options & FindOptions::MatchCase) == FindOptions::None)
		search->Text.MakeUpper();
	search->Callback = callback;
	search->hCancelEvent.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
	search->hContinueEvent.reset(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
	search->hDoneEvent.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
	if (!search->hCancelEvent || !search->hContinueEvent || !search->hDoneEvent)
		return false;

	{
		std::lock_guard locker(m_Lock);
		if (m_Search) {
			// a previous search may still be running or waiting to continue
			m_Search->Abandoned = true;
			m_Search->Cancelled = true;
			::SetEvent(m_Search->hCancelEvent.get());
		}
		m_Search = search;
	}

	// the thread owns a reference to its search
	auto param = new std::shared_ptr<Search>(search);
	auto hThread = ::CreateThread(nullptr, 0, [](auto p) -> DWORD {
		std::unique_ptr<std::shared_ptr<Search>> search(static_cast<std::shared_ptr<Search>*>(p));
		return DoSearch(**search);
		}, param, 0, nullptr);
	if (!hThread) {
		delete param;
		search->Running = false;
		return false;
	}
	m_hThread.reset(hThread);
	::SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);

	return true;
}

bool RegistrySearcher::Cancel() {
	auto search = GetSearch();
	if (search && search->Running && !search->Cancelled) {
		search->Cancelled = true;
		::SetEvent(search->hCancelEvent.get());
		return true;
	}
	return false;
}

bool RegistrySearcher::Continue() {
	if (IsRunning()) {
		::SetEvent(GetSearch()->hContinueEvent.get());
		return true;
	}
	return false;
}

bool RegistrySearcher::CanContinue() const {
	auto search = GetSearch();
	if (!search || !search->Running || search->Cancelled)
		return false;

	std::lock_guard locker(m_Lock);
	return search->RawText == m_SearchText && search->Options == m_Options;
}

bool RegistrySearcher::IsRunning() const {
	auto search = GetSearch();
	return search && search->Running && !search->Cancelled;
}

bool RegistrySearcher::IsCancelled() const {
	auto search = GetSearch();
	return search && search->Cancelled;
}

bool RegistrySearcher::WaitForCompletion(DWORD timeout) {
	auto search = GetSearch();
	return !search || WAIT_OBJECT_0 == ::WaitForSingleObject(search->hDoneEvent.get(), timeout);
}

std::shared_ptr<RegistrySearcher::Search> RegistrySearcher::GetSearch() const {
	std::lock_guard locker(m_Lock);
	return m_Search;
}

bool RegistrySearcher::Matches(Search const& search, CString text) {
	auto len = search.Text.GetLength();
	if (len == 0)
		return false;

	if ((search.Options & FindOptions::MatchCase) == FindOptions::None)
		text.MakeUpper();
	bool wholeWords = (search.Options & FindOptions::MatchWholeWords) == FindOptions::MatchWholeWords;

	auto isWordChar = [](WCHAR ch) {
		return ch == L'_' || ::IsCharAlphaNumeric(ch);
	};

	for (int n = text.Find(search.Text); n >= 0; n = text.Find(search.Text, n + 1)) {
		if (!wholeWords)
			return true;

		auto end = n + len;
		if ((n == 0 || !isWordChar(text[n - 1])) && (end == text.GetLength() || !isWordChar(text[end])))
			return true;
	}
	return false;
}

bool RegistrySearcher::FindNextWorker(Search& search, HKEY hKey, const CString& path) {
	if (!hKey)
		return true;
	if (search.Cancelled)
		return false;

	bool searchValues = (search.Options & FindOptions::SearchValues) == FindOptions::SearchValues;
	bool searchKeys = (search.Options & FindOptions::SearchKeys) == FindOptions::SearchKeys;
	bool searchData = (search.Options & FindOptions::SearchData) == FindOptions::SearchData;

	if (searchValues || searchData) {
		Registry::EnumKeyValues(hKey, [&](auto type, auto name, auto size) {
			if (search.Cancelled)
				return false;

			if (searchValues && Matches(search, name)) {
				if (Notify(search, path, name, nullptr))
					return false;
			}
			if (searchData && (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)) {
				// extra zeroed characters guarantee NULL termination, even for improperly stored strings
				std::vector<WCHAR> buffer(size / sizeof(WCHAR) + 2);
				DWORD bytes = size;
				if (ERROR_SUCCESS == ::RegQueryValueEx(hKey, name, nullptr, nullptr, (BYTE*)buffer.data(), &bytes)) {
					if (type == REG_MULTI_SZ) {
						for (auto p = buffer.data(); *p; p += wcslen(p) + 1) {
							if (Matches(search, p)) {
								if (Notify(search, path, name, p))
									return false;
								break;
							}
						}
					}
					else if (Matches(search, buffer.data())) {
						if (Notify(search, path, name, buffer.data()))
							return false;
					}
				}
			}
			return true;
			});
	}
	if (search.Cancelled)
		return false;

	Registry::EnumSubKeys(hKey, [&](auto name, const auto&) {
		if (search.Cancelled)
			return false;

		auto subPath = path + (path.IsEmpty() ? L"" : L"\\") + name;
		if (searchKeys && Matches(search, name)) {
			if (Notify(search, subPath, nullptr, nullptr))
				return false;
		}
		RegistryKey subKey;
		subKey.Open(hKey, name, KEY_READ);
		if (subKey && !FindNextWorker(search, subKey.Get(), subPath))
			return false;
		return true;
		});

	return !search.Cancelled;
}

bool RegistrySearcher::Notify(Search& search, PCWSTR path, PCWSTR name, PCWSTR data) {
	if (search.Cancelled)
		return true;

	search.Callback(path, name, data);
	if (!search.PauseOnResult)
		return search.Cancelled;

	HANDLE h[]{ search.hCancelEvent.get(), search.hContinueEvent.get() };
	if (WAIT_OBJECT_0 == ::WaitForMultipleObjects(_countof(h), h, FALSE, INFINITE)) {
		search.Cancelled = true;
		return true;
	}
	return false;
}

DWORD RegistrySearcher::DoSearch(Search& search) {
	if ((search.Options & (FindOptions::SearchStdRegistry | FindOptions::SearchSelected)) == FindOptions::SearchStdRegistry) {
		for (auto key : Registry::Keys) {
			// the performance keys are not stored data, and enumerating them collects performance counters (slow);
			// HKEY_CURRENT_USER_LOCAL_SETTINGS is a view of HKCU\Software\Classes\Local Settings, already searched
			if (key.hKey == HKEY_PERFORMANCE_DATA || key.hKey == HKEY_PERFORMANCE_TEXT || key.hKey == HKEY_PERFORMANCE_NLSTEXT
				|| key.hKey == HKEY_CURRENT_USER_LOCAL_SETTINGS)
				continue;
			FindNextWorker(search, key.hKey, key.text);
			if (search.Cancelled)
				break;
		}
	}
	if (!search.Cancelled && (search.Options & (FindOptions::SearchRealRegistry | FindOptions::SearchSelected)) == FindOptions::SearchRealRegistry) {
		RegistryKey key(Registry::OpenRealRegistryKey());
		FindNextWorker(search, key.Get(), L"\\REGISTRY");
	}
	if (!search.Cancelled && (search.Options & FindOptions::SearchSelected) == FindOptions::SearchSelected) {
		auto key = Registry::OpenKey(search.StartKey, KEY_READ);
		FindNextWorker(search, key.Get(), search.StartKey);
	}

	search.Running = false;
	if (!search.Abandoned)
		search.Callback(nullptr, nullptr, nullptr);
	::SetEvent(search.hDoneEvent.get());

	return 0;
}
