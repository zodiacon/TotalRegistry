#include "pch.h"
#include "Resource.h"
#include "GotoKeyDlg.h"

const CString& CGotoKeyDlg::GetKey() const {
	return m_Key;
}

LRESULT CGotoKeyDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CenterWindow(GetParent());
	SetDialogIcon(IDI_GOTO);

	SetDlgItemText(IDC_PATH, m_Key);
	GetDlgItem(IDOK).EnableWindow(!m_Key.IsEmpty());

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
