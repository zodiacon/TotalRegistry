#include "pch.h"
#include "LocationManager.h"
#include "IniFile.h"
#include "RegistryKey.h"

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

    m_Items.clear();
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
        //if (name.Left(4) != L"HKEY")
        //    name = name.Mid(name.Find(L"/") + 1);

        m_Items.insert({ name, path });
        p = cr + 1;
    }

    return true;
}

bool LocationManager::SaveToRegistry(PCWSTR path) const {
    if (m_Items.empty())
        return true;

    path = GetPath(path);
    if (!path)
        return false;

    RegistryKey key;
    key.Create(HKEY_CURRENT_USER, path);
    if (!key)
        return false;

    CString text;
    for (const auto& [name, value] : m_Items) {
        text += name + L";" + value + L"\n";
    }
    return ERROR_SUCCESS == key.SetStringValue(L"Locations", text);
}

bool LocationManager::LoadFromFile(PCWSTR path) {
    path = GetPath(path);
    if (!path)
        return false;

    IniFile file(path);
    if (!file.IsValid())
        return false;

    auto data = file.ReadSection(L"Locations");
    for (auto& text : data) {
        int n = text.Find(L'=');
        m_Items.insert({ n < 0 ? text : text.Left(n), n < 0 ? text : text.Mid(n + 1) });
    }

    return true;
}

bool LocationManager::SaveToFile(PCWSTR path) const {
    path = GetPath(path);
    if (!path)
        return false;

    IniFile file(path);
    for (auto& [name, target] : m_Items) {
        file.WriteString(L"Locations", name, target);
    }
    return true;
}

bool LocationManager::Load(PCWSTR path) {
    WCHAR fullpath[MAX_PATH];
    ::GetModuleFileName(nullptr, fullpath, _countof(fullpath));
    auto dot = wcsrchr(fullpath, L'.');
    ATLASSERT(dot);
    if (!dot)
        return false;

    *dot = 0;
    wcscat_s(fullpath, L".ini");
    if (::GetFileAttributes(fullpath) == INVALID_FILE_ATTRIBUTES) {
        //
        // INIT file does not exist, load from Registry
        //
        return LoadFromRegistry(path);
    }
    //
    // load from file
    //
    return LoadFromFile(fullpath);
}

bool LocationManager::Save() const {
    if (m_Path.IsEmpty())
        return false;

    return m_Path[1] == L':' ? SaveToFile() : SaveToRegistry();
}

int LocationManager::GetCount() const {
    return (int)m_Items.size();
}

bool LocationManager::Replace(CString const& name, CString const& newName) {
    auto it = m_Items.find(name);
    if (it == m_Items.end())
        return false;

    auto path = it->second;
    m_Items.erase(it);
    m_Items.insert({ newName, path });
    return true;
}

void LocationManager::Add(CString const& name, CString const& path) {
    m_Items.insert({ name, path });
}

void LocationManager::Clear() {
    m_Items.clear();
}

CString LocationManager::GetPathByName(CString const& name) const {
    if (auto it = m_Items.find(name); it != m_Items.end())
        return it->second;

    for (auto& [n, path] : m_Items) {
        if (auto index = n.Find(L"/"); index >= 0)
            if (n.Mid(index + 1) == name)
                return path;
    }
    return L"";
}

PCWSTR LocationManager::GetPath(PCWSTR path) const {
    if (path == nullptr || *path == 0)
        path = m_Path;

    m_Path = path;
    if (path == nullptr || *path == 0)
        return nullptr;

    return path;
}
