#pragma once

#include "DialogHelper.h"

//
// asks for the name and target of a new symbolic link key
//
class CCreateLinkDlg :
	public CDialogImpl<CCreateLinkDlg>,
	public CDialogHelper<CCreateLinkDlg> {
public:
	enum { IDD = IDD_CREATELINK };

	CString const& GetName() const;
	// a kernel path (\REGISTRY\...)
	CString const& GetTarget() const;
	bool IsVolatile() const;

	BEGIN_MSG_MAP(CCreateLinkDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CString m_Name, m_Target;
	bool m_Volatile{ false };
};
