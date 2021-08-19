#pragma once

#include "DialogHelper.h"
#include "VirtualListView.h"
#include "Registry.h"

class CKeysHandlesDlg :
	public CDialogImpl<CKeysHandlesDlg>,
	public CDynamicDialogLayout<CKeysHandlesDlg>,
	public CAutoUpdateUI<CKeysHandlesDlg>,
	public CVirtualListView<CKeysHandlesDlg>,
	public CIdleHandler,
	public CDialogHelper<CKeysHandlesDlg> {
public:
	enum { IDD = IDD_HANDLES };

	void Refresh();
	BOOL OnIdle() override;

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row) const;
	void DoSort(const SortInfo* si);

	BEGIN_MSG_MAP(CKeysHandlesDlg)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnListSelectionChanged)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, OnToolTipGetDisplay)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_HIDE_EMPTY, OnHideInaccessible)
		COMMAND_ID_HANDLER(ID_CLOSE_HANDLE, OnCloseHandle)
		COMMAND_ID_HANDLER(ID_KEY_PERMISSIONS, OnPermissions)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CVirtualListView<CKeysHandlesDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CKeysHandlesDlg>)
		CHAIN_MSG_MAP(CAutoUpdateUI<CKeysHandlesDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	void BuildToolBar();

	enum class ColumnType {
		ProcessName, PID, Handle, KeyName, Attributes, Access, DecodedAccess, Address
	};

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHideInaccessible(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseHandle (WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPermissions(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToolTipGetDisplay(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnListSelectionChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<HandleInfo> m_Handles;
	bool m_HideInaccesible{ false };
};
