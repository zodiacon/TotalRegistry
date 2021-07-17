#pragma once

class CSecurityInformation : public ISecurityInformation {
public:
	CSecurityInformation(HANDLE hObject, const CString& name, bool readonly) : m_hObject(hObject), m_Name(name), m_ReadOnly(readonly) {}
	~CSecurityInformation();

	STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);

	// use as static or stack based object

	STDMETHOD_(ULONG, AddRef)() {
		return 2;
	}
	STDMETHOD_(ULONG, Release)() {
		return 1;
	}

	// ISecurityInformation methods
	STDMETHOD(GetObjectInformation) (PSI_OBJECT_INFO pObjectInfo);
	STDMETHOD(GetSecurity) (SECURITY_INFORMATION RequestedInformation, PSECURITY_DESCRIPTOR* ppSecurityDescriptor, BOOL fDefault);
	STDMETHOD(SetSecurity) (SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor);
	STDMETHOD(GetAccessRights) (const GUID* pguidObjectType,
		DWORD dwFlags, // SI_EDIT_AUDITS, SI_EDIT_PROPERTIES
		PSI_ACCESS* ppAccess, ULONG* pcAccesses, ULONG* piDefaultAccess);
	STDMETHOD(MapGeneric) (const GUID* pguidObjectType, UCHAR* pAceFlags, ACCESS_MASK* pMask);
	STDMETHOD(GetInheritTypes) (PSI_INHERIT_TYPE* ppInheritTypes, ULONG* pcInheritTypes);
	STDMETHOD(PropertySheetPageCallback)(HWND hwnd, UINT uMsg, SI_PAGE_TYPE uPage);

private:
	HANDLE m_hObject;
	CString m_Name;
	bool m_ReadOnly;
};

