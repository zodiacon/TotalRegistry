#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"

LRESULT CAboutDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CenterWindow(GetParent());

	return TRUE;
}

LRESULT CAboutDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EndDialog(wID);
	return 0;
}

LRESULT CAboutDlg::OnClickSyslink(int, LPNMHDR, BOOL&) const {
	::ShellExecute(nullptr, L"open", L"https://github.com/zodiacon/tasksched", nullptr, nullptr, SW_SHOWDEFAULT);
	return 0;
}
