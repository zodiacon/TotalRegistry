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

bool RegistrySearcher::Find(RegistrySearcherCallback callback) {
	ATLASSERT(callback);
	m_Callback = callback;
	m_InProgress = true;
	m_Cancel = false;
	m_hCancelEvent.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
	m_hContinueEvent.reset(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
	m_hDoneEvent.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));

	m_hThread.reset(::CreateThread(nullptr, 0, [](auto p) {
		return ((RegistrySearcher*)p)->DoSearch();
		}, this, 0, nullptr));
	::SetThreadPriority(m_hThread.get(), THREAD_PRIORITY_LOWEST);

	return true;
}

bool RegistrySearcher::Cancel() {
	if (IsRunning()) {
		::SetEvent(m_hCancelEvent.get());
		m_InProgress = false;
		return true;
	}
	return false;
}

bool RegistrySearcher::Continue() {
	if (IsRunning()) {
		::SetEvent(m_hContinueEvent.get());
		return true;
	}
	return false;
}

bool RegistrySearcher::IsRunning() const {
	return m_InProgress.load();
}

bool RegistrySearcher::IsCancelled() const {
	return m_Cancel.load();
}

bool RegistrySearcher::WaitForCompletion(DWORD timeout) {
	return WAIT_OBJECT_0 == ::WaitForSingleObject(m_hDoneEvent.get(), timeout);
}

bool RegistrySearcher::FindNextWorker(HKEY hKey, const CString& path) {
	if (WAIT_OBJECT_0 == ::WaitForSingleObject(m_hCancelEvent.get(), 0)) {
		m_Cancel = true;
		return false;
	}

	bool searchValues = (m_Options & FindOptions::SearchValues) == FindOptions::SearchValues;
	bool searchKeys = (m_Options & FindOptions::SearchKeys) == FindOptions::SearchKeys;
	bool searchData = (m_Options & FindOptions::SearchData) == FindOptions::SearchData;
	bool caseSensitive = (m_Options & FindOptions::MatchCase) == FindOptions::MatchCase;
	bool wholeWords = (m_Options & FindOptions::MatchWholeWords) == FindOptions::MatchWholeWords;
	{
		std::lock_guard locker(m_Lock);
		if (!caseSensitive)
			m_SearchText.MakeUpper();
	}

	auto compare = [&](auto& text, auto& search) {
		int n = text.Find(search);
		if (n >= 0) {
			return !wholeWords || ((n == 0 || isspace(text[n - 1]) && (n + m_SearchText.GetLength() == text.GetLength() || isspace(text[n + m_SearchText.GetLength()]))));
		};
		return false;
	};

	if (searchValues || searchData) {
		Registry::EnumKeyValues(hKey, [&](auto type, auto name, auto size) {
			if (WAIT_OBJECT_0 == ::WaitForSingleObject(m_hCancelEvent.get(), 0)) {
				m_Cancel = true;
				return false;
			}

			if (searchValues) {
				CString text(name);
				if (!caseSensitive)
					text.MakeUpper();

				if (compare(text, m_SearchText)) {
					if (Notify(path, name, nullptr))
						return false;
				}
			}
			if (searchData) {
				if (type == REG_SZ || type == REG_EXPAND_SZ) {
					size += 4 + (size % 2);	// just in case the value is not stored properly in the Registry
					ATLASSERT(size % 2 == 0);
					auto buffer = std::make_unique<WCHAR[]>(size / sizeof(WCHAR));
					if (buffer) {
						::ZeroMemory(buffer.get(), size);
						DWORD bytes = size;
						if (ERROR_SUCCESS == ::RegQueryValueEx(hKey, name, nullptr, nullptr, (BYTE*)buffer.get(), &bytes)) {
							CString text(buffer.get());
							if (!caseSensitive)
								text.MakeUpper();
							if (compare(text, m_SearchText)) {
								if (Notify(path, name, buffer.get()))
									return false;
							}
						}
					}
				}
				else if (type == REG_MULTI_SZ) {
					size += 4 + (size % 2);	// just in case the value is not stored properly in the Registry
					ATLASSERT(size % 2 == 0);
					auto buffer = std::make_unique<WCHAR[]>(size / sizeof(WCHAR));
					if (buffer) {
						::ZeroMemory(buffer.get(), size);
						DWORD bytes = size;
						if (ERROR_SUCCESS == ::RegQueryValueEx(hKey, name, nullptr, nullptr, (BYTE*)buffer.get(), &bytes)) {
							for (auto p = buffer.get(); *p; p += wcslen(p) + 1) {
								CString text(p);
								if (!caseSensitive)
									text.MakeUpper();
								if (compare(text, m_SearchText)) {
									if (Notify(path, name, buffer.get()))
										return false;
								}
							}
						}
					}
				}
			}
			return true;
			});
	}
	if (m_Cancel)
		return false;

	Registry::EnumSubKeys(hKey, [&](auto name, const auto&) {
		if (WAIT_OBJECT_0 == ::WaitForSingleObject(m_hCancelEvent.get(), 0)) {
			m_Cancel = true;
			return false;
		}
		if (searchKeys) {
			CString text(name);
			if (!caseSensitive)
				text.MakeUpper();
			if (text.Find(m_SearchText) >= 0) {
				if (Notify(path + (path.IsEmpty() ? L"" : L"\\") + name, nullptr, nullptr))
					return false;
			}
		}
		RegistryKey subKey;
		subKey.Open(hKey, name, KEY_READ);
		if (subKey)
			FindNextWorker(subKey.Get(), path + (path.IsEmpty() ? L"" : L"\\") + name);
		if (m_Cancel)
			return false;
		return true;
		});

	if (m_Cancel)
		return false;

	return true;
}

bool RegistrySearcher::Notify(PCWSTR path, PCWSTR name, PCWSTR data) {
	if (!m_Cancel)
		m_Callback(path, name, data);
	HANDLE h[]{ m_hCancelEvent.get(), m_hContinueEvent.get() };
	if (WAIT_OBJECT_0 == ::WaitForMultipleObjects(_countof(h), h, FALSE, INFINITE)) {
		m_Cancel = true;
		return true;
	}
	if ((m_Options & FindOptions::MatchCase) == FindOptions::None)
		m_SearchText.MakeUpper();

	return false;
}

DWORD RegistrySearcher::DoSearch() {
	if ((m_Options & (FindOptions::SearchStdRegistry | FindOptions::SearchSelected)) == FindOptions::SearchStdRegistry) {
		for (auto key : Registry::Keys) {
			FindNextWorker(key.hKey, key.text);
			if (m_Cancel)
				break;
		}
	}
	if (!m_Cancel && (m_Options & (FindOptions::SearchRealRegistry | FindOptions::SearchSelected)) == FindOptions::SearchRealRegistry) {
		FindNextWorker(Registry::OpenRealRegistryKey(), L"\\REGISTRY");
	}
	if (!m_Cancel && (m_Options & FindOptions::SearchSelected) == FindOptions::SearchSelected) {
		FindNextWorker(Registry::OpenKey(m_StartKey, KEY_READ).Get(), m_StartKey);
	}

	m_Callback(nullptr, nullptr, nullptr);
	::SetEvent(m_hDoneEvent.get());
	m_InProgress = false;

	return 0;
}
