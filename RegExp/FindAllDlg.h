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

	static constexpr UINT WM_SEARCH_COMPLETE = WM_APP + 1;	// wParam: search ID
	static constexpr UINT WM_SEARCH_RESULT = WM_APP + 2;	// new results are pending

	explicit CFindAllDlg(IMainFrame* frame);

	void UpdateUI();
	void Cancel();
	void OnFinalMessage(HWND) override;

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int) const;
	void DoSort(const SortInfo* si);

	bool OnDoubleClickList(HWND, int row, int, const POINT&);

	BEGIN_MSG_MAP(CFindAllDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SEARCH_COMPLETE, OnSearchComplete)
		MESSAGE_HANDLER(WM_SEARCH_RESULT, OnSearchResult)
		COMMAND_ID_HANDLER(IDC_FIND, OnFind)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_CANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_SAVE, OnSaveResults)
		COMMAND_ID_HANDLER(IDC_LOAD, OnLoadResults)
		COMMAND_ID_HANDLER(IDC_DELETE, OnDelete)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnTextChanged)
		COMMAND_CODE_HANDLER(BN_CLICKED, OnClick)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CVirtualListView<CFindAllDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CFindAllDlg>)
		REFLECT_NOTIFICATIONS_EX()
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

	//
	// results found by the search thread, waiting to be added to the list by the UI thread
	// shared with the search thread, which never touches the dialog itself; each search gets its own
	//
	struct PendingResults {
		std::mutex Lock;
		std::vector<ListItem> Items;
	};

	void AddPendingResults();
	bool CheckButton(UINT id, FindOptions options, FindOptions value);
	FindOptions UpdateOptions();

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFind(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClick(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSearchComplete(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSearchResult(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSaveResults(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoadResults(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDelete(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
	RegistrySearcher m_Searcher;
	CListViewCtrl m_List;
	CProgressBarCtrl m_Progress;
	std::vector<ListItem> m_Items;
	std::shared_ptr<PendingResults> m_Pending;
	// identifies the current search, so a completion of an earlier one still in the message queue is ignored
	WPARAM m_SearchId{ 0 };
};
