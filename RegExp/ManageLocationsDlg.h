#pragma once

#include "DialogHelper.h"
#include "VirtualListView.h"
#include "IMainFrame.h"
#include "LocationManager.h"
#include "resource.h"

class CManageLocationsDlg :
	public CDialogImpl<CManageLocationsDlg>,
	public CDynamicDialogLayout<CManageLocationsDlg>,
	public CVirtualListView<CManageLocationsDlg>,
	public CDialogHelper<CManageLocationsDlg> {
public:
	enum { IDD = IDD_LOCATIONS };

	CManageLocationsDlg(IMainFrame* frame, LocationManager& lm);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(const SortInfo* si);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	BEGIN_MSG_MAP(CManageLocationsDlg)
		COMMAND_ID_HANDLER(IDC_NEW, OnNewLocation)
		COMMAND_ID_HANDLER(IDC_SET, OnSet)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_DELETE, OnDelete)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CManageLocationsDlg>)
		CHAIN_MSG_MAP(CVirtualListView<CManageLocationsDlg>)
		REFLECT_NOTIFICATIONS_EX()
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	void UpdateUI();

	struct Location {
		CString Name;
		CString Path;
	};

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDelete(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewLocation(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
	CListViewCtrl m_List;
	LocationManager& m_lm;
	std::vector<Location> m_Items;
};
