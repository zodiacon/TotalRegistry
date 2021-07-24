#include "pch.h"
#include "RegistrySearcher.h"
#include "Registry.h"

void RegistrySearcher::SetStartKey(PCWSTR startKey) {
	std::lock_guard locker(_lock);
	_startKey = startKey;
}

void RegistrySearcher::SetOptions(FindOptions options) {
	std::lock_guard locker(_lock);
	_options = options;
}

void RegistrySearcher::SetText(PCWSTR text) {
	std::lock_guard locker(_lock);
	_searchText = text;
}

bool RegistrySearcher::Find(RegistrySearcherCallback callback) {
	ATLASSERT(callback);
	_cb = callback;
	_inProgress = true;
	_cancel = false;
	_hCancelEvent.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
	_hContinueEvent.reset(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
	_hDoneEvent.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));

	_hThread.reset(::CreateThread(nullptr, 0, [](auto p) {
		return ((RegistrySearcher*)p)->DoSearch();
		}, this, 0, nullptr));
	::SetThreadPriority(_hThread.get(), THREAD_PRIORITY_LOWEST);

	return true;
}

bool RegistrySearcher::Cancel() {
	if (IsRunning()) {
		::SetEvent(_hCancelEvent.get());
		_inProgress = false;
		return true;
	}
	return false;
}

bool RegistrySearcher::Continue() {
	if (IsRunning()) {
		::SetEvent(_hContinueEvent.get());
		return true;
	}
	return false;
}

bool RegistrySearcher::IsRunning() const {
	return _inProgress.load();
}

bool RegistrySearcher::IsCancelled() const {
	return _cancel.load();
}

bool RegistrySearcher::WaitForCompletion(DWORD timeout) {
	return WAIT_OBJECT_0 == ::WaitForSingleObject(_hDoneEvent.get(), timeout);
}

bool RegistrySearcher::FindNextWorker(HKEY hKey, const CString& path) {
	if (WAIT_OBJECT_0 == ::WaitForSingleObject(_hCancelEvent.get(), 0)) {
		_cancel = true;
		return false;
	}

	bool searchValues = (_options & FindOptions::SearchValues) == FindOptions::SearchValues;
	bool searchKeys = (_options & FindOptions::SearchKeys) == FindOptions::SearchKeys;
	bool searchData = (_options & FindOptions::SearchData) == FindOptions::SearchData;
	bool caseSensitive = (_options & FindOptions::MatchCase) == FindOptions::MatchCase;
	bool wholeWords = (_options & FindOptions::MatchWholeWords) == FindOptions::MatchWholeWords;
	{
		std::lock_guard locker(_lock);
		if (!caseSensitive)
			_searchText.MakeUpper();
	}

	auto compare = [&](auto& text, auto& search) {
		int n = text.Find(search);
		if (n >= 0) {
			return !wholeWords || ((n == 0 || isspace(text[n - 1]) && (n + _searchText.GetLength() == text.GetLength() || isspace(text[n + _searchText.GetLength()]))));
		};
		return false;
	};

	if (searchValues || searchData) {
		Registry::EnumKeyValues(hKey, [&](auto type, auto name, auto size) {
			if (searchValues) {
				CString text(name);
				if (!caseSensitive)
					text.MakeUpper();

				if (compare(text, _searchText)) {
					if (Notify(path, name, nullptr))
						return false;
				}
			}
			if (searchData) {
				if (type == REG_SZ || type == REG_EXPAND_SZ) {
					auto buffer = std::make_unique<WCHAR[]>(size / sizeof(WCHAR));
					if (buffer) {
						DWORD bytes = size;
						if (ERROR_SUCCESS == ::RegQueryValueEx(hKey, name, nullptr, nullptr, (BYTE*)buffer.get(), &bytes)) {
							CString text(buffer.get());
							if (!caseSensitive)
								text.MakeUpper();
							if (compare(text, _searchText)) {
								if (Notify(path, name, buffer.get()))
									return false;
							}
						}
					}
				}
				else if (type == REG_MULTI_SZ) {
					auto buffer = std::make_unique<WCHAR[]>(size / sizeof(WCHAR));
					if (buffer) {
						DWORD bytes = size;
						if (ERROR_SUCCESS == ::RegQueryValueEx(hKey, name, nullptr, nullptr, (BYTE*)buffer.get(), &bytes)) {
							for (auto p = buffer.get(); *p; p += wcslen(p) + 1) {
								CString text(p);
								if (!caseSensitive)
									text.MakeUpper();
								if (compare(text, _searchText)) {
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
	if (_cancel)
		return false;

	Registry::EnumSubKeys(hKey, [&](auto name, const auto&) {
		if (WAIT_OBJECT_0 == ::WaitForSingleObject(_hCancelEvent.get(), 0)) {
			_cancel = true;
			return false;
		}
		if (searchKeys) {
			CString text(name);
			if (!caseSensitive)
				text.MakeUpper();
			if (text.Find(_searchText) >= 0) {
				if (Notify(path + (path.IsEmpty() ? L"" : L"\\") + name, nullptr, nullptr))
					return false;
			}
		}
		CRegKey subKey;
		subKey.Open(hKey, name, KEY_READ);
		if (subKey)
			FindNextWorker(subKey, path + (path.IsEmpty() ? L"" : L"\\") + name);
		if (_cancel)
			return false;
		return true;
		});

	if (_cancel)
		return false;

	return true;
}

bool RegistrySearcher::Notify(PCWSTR path, PCWSTR name, PCWSTR data) {
	if (!_cancel)
		_cb(path, name, data);
	HANDLE h[]{ _hCancelEvent.get(), _hContinueEvent.get() };
	if (WAIT_OBJECT_0 == ::WaitForMultipleObjects(_countof(h), h, FALSE, INFINITE)) {
		_cancel = true;
		return true;
	}
	if ((_options & FindOptions::MatchCase) == FindOptions::None)
		_searchText.MakeUpper();

	return false;
}

DWORD RegistrySearcher::DoSearch() {
	if ((_options & (FindOptions::SearchStdRegistry | FindOptions::SearchSelected)) == FindOptions::SearchStdRegistry) {
		for (auto key : Registry::Keys) {
			FindNextWorker(key.hKey, key.text);
			if (_cancel)
				break;
		}
	}
	if (!_cancel && (_options & (FindOptions::SearchRealRegistry | FindOptions::SearchSelected)) == FindOptions::SearchRealRegistry) {
		FindNextWorker(Registry::OpenRealRegistryKey(), L"\\REGISTRY");
	}
	if (!_cancel && (_options & FindOptions::SearchSelected) == FindOptions::SearchSelected) {
		FindNextWorker(Registry::OpenKey(_startKey, KEY_READ), _startKey);
	}

	_cb(nullptr, nullptr, nullptr);
	::SetEvent(_hDoneEvent.get());
	_inProgress = false;

	return 0;
}
