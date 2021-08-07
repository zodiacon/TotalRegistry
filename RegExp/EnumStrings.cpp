#include "pch.h"
#include "EnumStrings.h"
#include "Registry.h"

void CEnumStrings::SetRegistryPath(const CString& path) {
    _path = path;
}

bool CEnumStrings::GenerateStrings(const CString& path) {
    _strings.clear();
    _current = 0;
    if (path.IsEmpty()) {
        for (auto& key : Registry::Keys) {
            _strings.push_back(key.text);
        }
        return true;
    }
    if (path == L"\\") {
        _strings.push_back(L"\\REGISTRY");
        return true;
    }
    auto key = Registry::OpenKey(path, KEY_READ);
    if (!key)
        return false;
    Registry::EnumSubKeys(key.Get(), [&](auto name, const auto&) {
        _strings.push_back(path + name);
        return TRUE;
        });

    return true;
}

HRESULT __stdcall CEnumStrings::Next(ULONG celt, LPOLESTR* rgelt, ULONG* pceltFetched) {
    if (_strings.empty()) {
        if(pceltFetched)
            *pceltFetched = 0;

        // generate strings
        if (!GenerateStrings(_path))
            return E_FAIL;
    }
    if (celt == 0)
        return S_OK;

    ULONG i = 0;
    for (; i < celt; i++) {
        auto index = _current + i;
        if (index >= _strings.size())
            break;
        CString& str(_strings[index]);
        rgelt[i] = (PWSTR)::CoTaskMemAlloc((str.GetLength() + 1) * sizeof(WCHAR));
        wcscpy_s(rgelt[i], str.GetLength() + 1, str);
        ATLTRACE(L"Added string: %s\n", rgelt[i]);
    }
    if (pceltFetched)
        *pceltFetched = i;
    _current += i;
    return i < celt ? S_FALSE : S_OK;
}

HRESULT __stdcall CEnumStrings::Skip(ULONG celt) {
    _current += celt;
    return S_OK;
}

HRESULT __stdcall CEnumStrings::Reset(void) {
    _current = 0;
    return S_OK;
}

HRESULT __stdcall CEnumStrings::Clone(IEnumString** ppenum) {
    CComObject<CEnumStrings>* p;
    auto hr = p->CreateInstance(&p);
    if (FAILED(hr))
        return hr;

    p->SetRegistryPath(_path);
    p->_current = _current;
    p->_strings = _strings;

    return p->QueryInterface(ppenum);
}

HRESULT __stdcall CEnumStrings::Expand(PCWSTR pszExpand) {
    _path = pszExpand;
    _strings.clear();
    return S_OK;
}

