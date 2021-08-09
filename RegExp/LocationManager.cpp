#include "pch.h"
#include "LocationManager.h"

bool LocationManager::LoadFromRegistry(PCWSTR path) {
    path = GetPath(path);
    if (!path)
        return false;

    CRegKey key;
    key.Open(HKEY_CURRENT_USER, path, KEY_QUERY_VALUE);
    if (!key)
        return false;

    ULONG chars = 0;
    key.QueryStringValue(L"Locations", nullptr, &chars);
    if (chars == 0)
        return false;

    auto data = std::make_unique<WCHAR[]>(chars);
    if (ERROR_SUCCESS != key.QueryStringValue(L"Locations", data.get(), &chars))
        return false;

    _items.clear();
    for (auto p = data.get(); *p; ) {
        auto semi = wcschr(p, L';');
        if (semi == nullptr)
            return false;

        auto count = semi - p;
        CString name((PCWSTR)p, (int)count);
        auto cr = wcschr(semi + 1, L'\n');
        CString path(semi + 1, (int)(cr - semi - 1));
        if (name.IsEmpty())
            name = path;
        _items.insert({ name, path });
        p = cr + 1;
    }

    return true;
}

bool LocationManager::SaveToRegistry(PCWSTR path) const {
    if (_items.empty())
        return true;

    path = GetPath(path);
    if (!path)
        return false;

    CRegKey key;
    key.Create(HKEY_CURRENT_USER, path, nullptr, 0, KEY_WRITE);
    if (!key)
        return false;

    CString text;
    for (const auto& [name, value] : _items) {
        text += name + L";" + value + L"\n";
    }
    return ERROR_SUCCESS == key.SetStringValue(L"Locations", text);
}

int LocationManager::GetCount() const {
    return (int)_items.size();
}

bool LocationManager::Replace(CString const& name, CString const& newName) {
    auto it = _items.find(name);
    if (it == _items.end())
        return false;

    auto path = it->second;
    _items.erase(it);
    _items.insert({ newName, path });
    return true;
}

void LocationManager::Add(CString const& name, CString const& path) {
    _items.insert({ name, path });
}

void LocationManager::Clear() {
    _items.clear();
}

CString const& LocationManager::GetPathByName(CString const& name) const {
    auto& path = _items.at(name);
    return path.IsEmpty() ? name : path;
}

PCWSTR LocationManager::GetPath(PCWSTR path) const {
    if (path == nullptr || *path == 0)
        path = _path;

    _path = path;
    if (path == nullptr || *path == 0)
        return nullptr;

    return path;
}
