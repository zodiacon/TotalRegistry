#pragma once

#include "FindOptions.h"
#include "AppSettings.h"
#include "DialogHelper.h"
#include "RegistrySearcher.h"
#include "IMainFrame.h"

class CFindDlg : 
	public CDialogImpl<CFindDlg>,
	public CDialogHelper<CFindDlg> {
public:
	enum { IDD = IDD_FIND };

	CFindDlg(IMainFrame* frame);

	void UpdateUI();
	void Cancel();
	void Continue();
	bool IsFindNextAvailable() const;

	BEGIN_MSG_MAP(CFindDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_FIND, OnFind)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_CANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_NEW, OnNewSearch)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnTextChanged)
		COMMAND_CODE_HANDLER(BN_CLICKED, OnClick)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	bool CheckButton(UINT id, FindOptions options, FindOptions value);
	FindOptions UpdateOptions();
	void ResetUI();

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFind(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClick(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
	RegistrySearcher m_Searcher;
	CProgressBarCtrl m_Progress;
};
