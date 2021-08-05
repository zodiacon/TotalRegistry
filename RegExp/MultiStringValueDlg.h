#pragma once

#include "DialogHelper.h"

class CMultiStringValueDlg :
	public CDialogImpl<CMultiStringValueDlg>,
	public CDialogHelper<CMultiStringValueDlg>,
	public CDynamicDialogLayout<CMultiStringValueDlg> {
public:
	enum { IDD = IDD_MULTISTRVALUE };

	CMultiStringValueDlg(CRegKey& key, PCWSTR name, bool readOnly);

	const CString& GetValue() const;
	bool IsModified() const;

	BEGIN_MSG_MAP(CMultiStringValueDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CMultiStringValueDlg>)
		REFLECT_NOTIFICATIONS_EX()
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CRegKey& m_Key;
	CString m_Name;
	CString m_Value;
	bool m_ReadOnly;
	bool m_Modified{ false };
};
