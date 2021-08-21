#include "pch.h"
#include <ObjSel.h>
#include "resource.h"
#include "ConnectRegistryDlg.h"

const CString& CConnectRegistryDlg::GetComputerName() const {
	return m_Name;
}

LRESULT CConnectRegistryDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CenterWindow(GetParent());
	SetDialogIcon(IDI_REGREMOTE);

	return 0;
}

LRESULT CConnectRegistryDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK)
		GetDlgItemText(IDC_NAME, m_Name);

	EndDialog(wID);
	return 0;
}

LRESULT CConnectRegistryDlg::OnTextChanged(WORD, WORD wID, HWND, BOOL&) {
	GetDlgItem(IDOK).EnableWindow(GetDlgItem(IDC_NAME).GetWindowTextLength() > 0);

	return 0;
}

LRESULT CConnectRegistryDlg::OnBrowse(WORD, WORD wID, HWND, BOOL&) {
	IDsObjectPicker* pPicker;
	auto hr = ::CoCreateInstance(CLSID_DsObjectPicker, nullptr, CLSCTX_ALL, IID_IDsObjectPicker, reinterpret_cast<void**>(&pPicker));
	if (FAILED(hr)) {
		m_pFrame->DisplayError(L"Failed launch computer selection dialog", m_hWnd, hr & 0xffff);
		return 0;
	}

	DSOP_INIT_INFO info = { sizeof(info) };
	info.cDsScopeInfos = 1;
	DSOP_SCOPE_INIT_INFO scope = { sizeof(scope) };
	scope.flType = DSOP_SCOPE_TYPE_WORKGROUP | DSOP_SCOPE_TYPE_DOWNLEVEL_JOINED_DOMAIN | DSOP_SCOPE_TYPE_ENTERPRISE_DOMAIN;
	scope.flScope = DSOP_SCOPE_FLAG_DEFAULT_FILTER_COMPUTERS;
	scope.FilterFlags.flDownlevel = DSOP_DOWNLEVEL_FILTER_COMPUTERS;
	info.aDsScopeInfos = &scope;
	hr = pPicker->Initialize(&info);
	CComPtr<IDataObject> spData;
	hr = pPicker->InvokeDialog(m_hWnd, &spData);
	pPicker->Release();
	if (spData == nullptr || FAILED(hr))
		return 0;

	FORMATETC fmt{};
	fmt.cfFormat = ::RegisterClipboardFormat(CFSTR_DSOP_DS_SELECTION_LIST);
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.tymed = TYMED_HGLOBAL;
	fmt.lindex = -1;
	STGMEDIUM med;
	hr = spData->GetData(&fmt, &med);
	if (FAILED(hr)) {
		m_pFrame->DisplayError(L"Failed to retrieve computer name", m_hWnd, hr & 0xffff);
		return 0;
	}

	auto data = static_cast<DS_SELECTION_LIST*>(::GlobalLock(med.hGlobal));
	ATLASSERT(data);
	ATLASSERT(data->cItems >= 1);
	SetDlgItemText(IDC_NAME, data->aDsSelection[0].pwzName);
	::GlobalUnfix(med.hGlobal);
	::ReleaseStgMedium(&med);

	return 0;
}

