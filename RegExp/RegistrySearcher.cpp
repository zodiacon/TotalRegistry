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
    _hCancelEvent.reset(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
    _hContinueEvent.reset(::CreateEvent(nullptr, FALSE, FALSE, nullptr));

    _hThread.reset(::CreateThread(nullptr, 0, [](auto p) {
        return ((RegistrySearcher*)p)->DoSearch();
        }, this, 0, nullptr));
    ::SetThreadPriority(_hThread.get(), THREAD_PRIORITY_LOWEST);

    return true;
}

bool RegistrySearcher::Cancel() {
    _inProgress = false;
    if (IsRunning()) {
        ::SetEvent(_hCancelEvent.get());
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

bool RegistrySearcher::FindNextWorker(HKEY hKey, const CString& path) {
    if (WAIT_OBJECT_0 == ::WaitForSingleObject(_hCancelEvent.get(), 0)) {
        _cancel = true;
        return false;
    }

    bool searchValues = (_options & FindOptions::SearchValues) == FindOptions::SearchValues;
    bool searchKeys = (_options & FindOptions::SearchKeys) == FindOptions::SearchKeys;
    bool searchData = (_options & FindOptions::SearchData) == FindOptions::SearchData;
    bool caseSensitive = (_options & FindOptions::MatchCase) == FindOptions::MatchCase;

    {
        std::lock_guard locker(_lock);
        if (!caseSensitive)
            _searchText.MakeUpper();
    }

    if (searchValues || searchData) {
        Registry::EnumKeyValues(hKey, [&](auto type, auto name, auto size) {
            if (searchValues) {
                CString text(name);
                if (!caseSensitive)
                    text.MakeUpper();

                if (text.Find(_searchText) >= 0) {
                    if (Notify(path, name, nullptr))
                        return false;
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
                if(Notify(path + L"\\" + name, nullptr, nullptr))
                    return false;
            }
        }
        CRegKey subKey;
        subKey.Open(hKey, name, KEY_READ);
        if (subKey)
            FindNextWorker(subKey, path + L"\\" + name);
        if (_cancel)
            return false;
        return true;
        });

    if (_cancel)
        return false;

    return true;
}

bool RegistrySearcher::Notify(PCWSTR path, PCWSTR name, void* data) {
    if(!_cancel)
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
    if ((_options & (FindOptions::SearchRealRegistry | FindOptions::SearchSelected)) == FindOptions::SearchRealRegistry) {
        FindNextWorker(Registry::OpenRealRegistryKey(), L"\\REGISTRY");
    }
    _cb(nullptr, nullptr, nullptr);

    return 0;
}
