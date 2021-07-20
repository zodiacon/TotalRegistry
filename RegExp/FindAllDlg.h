#pragma once

#include "FindOptions.h"
#include "AppSettings.h"
#include "DialogHelper.h"
#include "RegistrySearcher.h"
#include "IMainFrame.h"
#include "VirtualListView.h"

class CFindAllDlg :
	public CDialogImpl<CFindAllDlg>,
	public CDynamicDialogLayout<CFindAllDlg>,
	public CVirtualListView<CFindAllDlg>,
	public CDialogHelper<CFindAllDlg> {
public:
	enum { IDD = IDD_FINDALL };

	CFindAllDlg(IMainFrame* frame);

	void UpdateUI();
	void Cancel();

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row) const;
	void DoSort(const SortInfo* si);

	BOOL OnDoubleClickList(HWND, int row, int, const POINT&);

	BEGIN_MSG_MAP(CFindAllDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_FIND, OnFind)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_CANCEL, OnCancel)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnTextChanged)
		COMMAND_CODE_HANDLER(BN_CLICKED, OnClick)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CVirtualListView<CFindAllDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CFindAllDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	struct ListItem {
		CString Path;
		CString Name;
		CString Data;
	};

	bool CheckButton(UINT id, FindOptions options, FindOptions value);
	FindOptions UpdateOptions();

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFind(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClick(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
	RegistrySearcher m_Searcher;
	CListViewCtrl m_List;
	CProgressBarCtrl m_Progress;
	std::vector<ListItem> m_Items;
};
