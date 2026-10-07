#pragma once

#include "DialogHelper.h"
#include "VirtualListView.h"
#include "RegistrySnapshot.h"

//
// shows what importing a .reg file would change, before importing it
//
class CImportPreviewDlg :
	public CDialogImpl<CImportPreviewDlg>,
	public CDynamicDialogLayout<CImportPreviewDlg>,
	public CVirtualListView<CImportPreviewDlg>,
	public CDialogHelper<CImportPreviewDlg> {
public:
	enum { IDD = IDD_IMPORTPREVIEW };

	CImportPreviewDlg(CString const& fileName, std::vector<SnapshotChange> changes);

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int) const;
	void DoSort(const SortInfo* si);

	BEGIN_MSG_MAP(CImportPreviewDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CVirtualListView<CImportPreviewDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CImportPreviewDlg>)
		REFLECT_NOTIFICATIONS_EX()
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CString m_FileName;
	std::vector<SnapshotChange> m_Changes;
	CListViewCtrl m_List;
};
