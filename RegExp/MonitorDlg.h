#pragma once

#include "DialogHelper.h"
#include "IMainFrame.h"
#include "VirtualListView.h"
#include "RegistryMonitor.h"

//
// live registry activity of all processes (needs to run elevated)
// owned by the main frame (not deleting itself), as its window is destroyed with the frame's
//
class CMonitorDlg :
	public CDialogImpl<CMonitorDlg>,
	public CDynamicDialogLayout<CMonitorDlg>,
	public CVirtualListView<CMonitorDlg>,
	public CDialogHelper<CMonitorDlg> {
public:
	enum { IDD = IDD_MONITOR };

	static constexpr UINT WM_EVENTS = WM_APP + 1;	// new events are waiting

	explicit CMonitorDlg(IMainFrame* frame);

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int) const;
	void DoSort(const SortInfo*) {}		// always in time order
	bool OnDoubleClickList(HWND, int row, int, const POINT&);

	BEGIN_MSG_MAP(CMonitorDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_EVENTS, OnEvents)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_START, OnStart)
		COMMAND_ID_HANDLER(IDC_CLEAR, OnClear)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		COMMAND_HANDLER(IDC_FILTER, EN_CHANGE, OnFilterChanged)
		CHAIN_MSG_MAP(CVirtualListView<CMonitorDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CMonitorDlg>)
		REFLECT_NOTIFICATIONS_EX()
	END_MSG_MAP()

private:
	bool Matches(RegistryEvent const& e) const;
	void Refilter();
	void UpdateStatus();

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEvents(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStart(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClear(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFilterChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
	RegistryMonitor m_Monitor;
	CListViewCtrl m_List;
	std::vector<RegistryEvent> m_Events;	// all, oldest first
	std::vector<size_t> m_Shown;			// indices of the events that pass the filter
	CString m_Filter;						// upper case
};
