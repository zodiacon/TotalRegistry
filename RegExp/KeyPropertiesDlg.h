#pragma once

#include "DialogHelper.h"
#include "Registry.h"

//
// details of a key: its real location, type (volatile, link), times, counts, class, owner and hive
//
class CKeyPropertiesDlg :
	public CDialogImpl<CKeyPropertiesDlg>,
	public CDynamicDialogLayout<CKeyPropertiesDlg>,
	public CDialogHelper<CKeyPropertiesDlg> {
public:
	enum { IDD = IDD_KEYPROPERTIES };

	CKeyPropertiesDlg(CString const& path, KeyInfo const& info);

	BEGIN_MSG_MAP(CKeyPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		COMMAND_ID_HANDLER(IDC_SHOWHIVE, OnShowHive)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CKeyPropertiesDlg>)
	END_MSG_MAP()

private:
	void AddRow(PCWSTR name, CString const& value);

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShowHive(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CString m_Path;
	KeyInfo m_Info;
	CListViewCtrl m_List;
};
