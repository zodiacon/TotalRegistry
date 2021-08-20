#include "pch.h"
#include "Resource.h"
#include "GotoKeyDlg.h"
#include "EnumStrings.h"

const CString& CGotoKeyDlg::GetKey() const {
	return m_Key;
}

void CGotoKeyDlg::SetKey(const CString& key) {
	m_Key = key;
}

LRESULT CGotoKeyDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CenterWindow(GetParent());
	SetDialogIcon(IDI_GOTO);

	SetDlgItemText(IDC_PATH, m_Key);
	GetDlgItem(IDOK).EnableWindow(!m_Key.IsEmpty());

	CComObject<CEnumStrings>* pEnum;
	pEnum->CreateInstance(&pEnum);
	CComPtr<IAutoComplete2> spAC;
	auto hr = spAC.CoCreateInstance(CLSID_AutoComplete);
	if(spAC) {
		spAC->Init(GetDlgItem(IDC_PATH), pEnum->GetUnknown(), nullptr, nullptr);
		spAC->SetOptions(ACO_AUTOSUGGEST | ACO_USETAB | ACO_AUTOAPPEND);
	}

	return 0;
}

LRESULT CGotoKeyDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		GetDlgItemText(IDC_PATH, m_Key);
	}
	EndDialog(wID);
	return 0;
}

LRESULT CGotoKeyDlg::OnChangeText(WORD, WORD wID, HWND, BOOL&) {
	GetDlgItem(IDOK).EnableWindow(GetDlgItem(IDC_PATH).GetWindowTextLength() > 1);
	return 0;
}
