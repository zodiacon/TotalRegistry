#include "pch.h"
#include "SecurityInformation.h"

#pragma comment(lib, "Aclui.lib")

CSecurityInformation::~CSecurityInformation() {
}

STDMETHODIMP CSecurityInformation::QueryInterface(REFIID riid, void** ppvObj) {
	if (riid == IID_IUnknown || riid == IID_ISecurityInformation)
		return *ppvObj = this, S_OK;
	return E_NOINTERFACE;
}

STDMETHODIMP CSecurityInformation::GetObjectInformation(PSI_OBJECT_INFO pInfo) {
	pInfo->dwFlags = SI_ADVANCED | SI_EDIT_OWNER | SI_CONTAINER | (m_ReadOnly ? (SI_READONLY | SI_OWNER_READONLY) : 0);
	pInfo->hInstance = nullptr;
	pInfo->pszPageTitle = nullptr;
	pInfo->pszObjectName = (LPWSTR)(LPCWSTR)m_Name;

	return S_OK;
}

STDMETHODIMP CSecurityInformation::GetAccessRights(const GUID* pguidObjectType,
	DWORD dwFlags, PSI_ACCESS* ppAccess, ULONG* pcAccesses, ULONG* piDefaultAccess) {
	static SI_ACCESS access[] = {
		{ &GUID_NULL, KEY_CREATE_SUB_KEY, L"Create", SI_ACCESS_SPECIFIC },
		{ &GUID_NULL, KEY_ENUMERATE_SUB_KEYS, L"Enumerate", SI_ACCESS_SPECIFIC },
		{ &GUID_NULL, KEY_SET_VALUE, L"Set Value", SI_ACCESS_SPECIFIC },
		{ &GUID_NULL, KEY_QUERY_VALUE, L"Query Value", SI_ACCESS_SPECIFIC },
		{ &GUID_NULL, KEY_WRITE, L"Write", SI_ACCESS_GENERAL },
		{ &GUID_NULL, KEY_READ, L"Read", SI_ACCESS_GENERAL },
	};

	*ppAccess = access;
	*pcAccesses = _countof(access);
	*piDefaultAccess = 0;

	return S_OK;
}

STDMETHODIMP CSecurityInformation::GetInheritTypes(PSI_INHERIT_TYPE* ppInheritTypes, ULONG* pcInheritTypes) {
	return S_OK;
}

STDMETHODIMP CSecurityInformation::SetSecurity(SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor) {
	return ::SetKernelObjectSecurity(m_hObject, SecurityInformation, pSecurityDescriptor) ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
}

STDMETHODIMP CSecurityInformation::MapGeneric(const GUID* pguidObjectType, UCHAR* pAceFlags, ACCESS_MASK* pMask) {
	return S_OK;
}

STDMETHODIMP CSecurityInformation::PropertySheetPageCallback(HWND hwnd, UINT uMsg, SI_PAGE_TYPE uPage) {
	return S_OK;
}

STDMETHODIMP CSecurityInformation::GetSecurity(SECURITY_INFORMATION RequestedInformation,
	PSECURITY_DESCRIPTOR* ppSecurityDescriptor, BOOL fDefault) {
	auto code = ::GetSecurityInfo(m_hObject, SE_REGISTRY_KEY, RequestedInformation,
		nullptr, nullptr, nullptr, nullptr, ppSecurityDescriptor);

	return HRESULT_FROM_WIN32(code);
}
