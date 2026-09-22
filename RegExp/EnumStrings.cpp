#include "pch.h"
#include "EnumStrings.h"
#include "Registry.h"

void CEnumStrings::SetRegistryPath(const CString& path) {
    m_Path = path;
}

bool CEnumStrings::GenerateStrings(const CString& path) {
    m_Strings.clear();
    m_Current = 0;
    if (path.IsEmpty()) {
        for (auto& key : Registry::Keys) {
            m_Strings.push_back(key.text);
        }
        return true;
    }
    if (path == L"\\") {
        m_Strings.push_back(L"\\REGISTRY");
        return true;
    }
    auto key = Registry::OpenKey(path, KEY_READ);
    if (!key)
        return false;
    Registry::EnumSubKeys(key.Get(), [&](auto name, const auto&) {
        m_Strings.push_back(path + name);
        return TRUE;
        });

    return true;
}

HRESULT __stdcall CEnumStrings::Next(ULONG celt, LPOLESTR* rgelt, ULONG* pceltFetched) {
    if (m_Strings.empty()) {
        if(pceltFetched)
            *pceltFetched = 0;

        // generate strings
        if (!GenerateStrings(m_Path))
            return E_FAIL;
    }
    if (celt == 0)
        return S_OK;

    ULONG i = 0;
    for (; i < celt; i++) {
        auto index = m_Current + i;
        if (index >= m_Strings.size())
            break;
        CString& str(m_Strings[index]);
        rgelt[i] = (PWSTR)::CoTaskMemAlloc((str.GetLength() + 1) * sizeof(WCHAR));
        wcscpy_s(rgelt[i], str.GetLength() + 1, str);
        ATLTRACE(L"Added string: %s\n", rgelt[i]);
    }
    if (pceltFetched)
        *pceltFetched = i;
    m_Current += i;
    return i < celt ? S_FALSE : S_OK;
}

HRESULT __stdcall CEnumStrings::Skip(ULONG celt) {
    m_Current += celt;
    return S_OK;
}

HRESULT __stdcall CEnumStrings::Reset(void) {
    m_Current = 0;
    return S_OK;
}

HRESULT __stdcall CEnumStrings::Clone(IEnumString** ppenum) {
    CComObject<CEnumStrings>* p;
    auto hr = p->CreateInstance(&p);
    if (FAILED(hr))
        return hr;

    p->SetRegistryPath(m_Path);
    p->m_Current = m_Current;
    p->m_Strings = m_Strings;

    return p->QueryInterface(ppenum);
}

HRESULT __stdcall CEnumStrings::Expand(PCWSTR pszExpand) {
    m_Path = pszExpand;
    m_Strings.clear();
    return S_OK;
}

