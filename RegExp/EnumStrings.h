#pragma once

class CEnumStrings : 
	public CComObjectRoot, 
	public IEnumString,
	public IACList {
public:
	BEGIN_COM_MAP(CEnumStrings)
		COM_INTERFACE_ENTRY(IEnumString)
		COM_INTERFACE_ENTRY(IACList)
	END_COM_MAP()

	void SetRegistryPath(const CString& path);

private:
	bool GenerateStrings(const CString& path);

	// Inherited via IEnumString
	HRESULT __stdcall Next(ULONG celt, LPOLESTR* rgelt, ULONG* pceltFetched) override;
	HRESULT __stdcall Skip(ULONG celt) override;
	HRESULT __stdcall Reset(void) override;
	HRESULT __stdcall Clone(IEnumString** ppenum) override;

	// Inherited via IACList
	HRESULT __stdcall Expand(PCWSTR pszExpand) override;

	CString _path;
	std::vector<CString> _strings;
	size_t _current{ 0 };

};
