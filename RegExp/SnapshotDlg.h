#pragma once

#include "DialogHelper.h"
#include "IMainFrame.h"
#include "VirtualListView.h"
#include "RegistrySnapshot.h"
#include <atomic>
#include <thread>

//
// takes snapshots of a key tree to files, and compares a snapshot with the live Registry or another snapshot
// owned by the main frame (not deleting itself), as its window is destroyed with the frame's
//
class CSnapshotDlg :
	public CDialogImpl<CSnapshotDlg>,
	public CDynamicDialogLayout<CSnapshotDlg>,
	public CVirtualListView<CSnapshotDlg>,
	public CDialogHelper<CSnapshotDlg> {
public:
	enum { IDD = IDD_SNAPSHOTS };

	static constexpr UINT WM_JOB_PROGRESS = WM_APP + 1;	// wParam: job ID, lParam: keys read so far
	static constexpr UINT WM_JOB_DONE = WM_APP + 2;		// wParam: job ID

	explicit CSnapshotDlg(IMainFrame* frame);
	// cancels a running job and waits for its thread, so it never outlives the dialog (and the CRT at exit)
	~CSnapshotDlg();

	// the key to take a snapshot of, unless a job is running
	void SetKeyPath(CString const& path);

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int) const;
	void DoSort(const SortInfo* si);
	bool OnDoubleClickList(HWND, int row, int, const POINT&);

	BEGIN_MSG_MAP(CSnapshotDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_JOB_PROGRESS, OnJobProgress)
		MESSAGE_HANDLER(WM_JOB_DONE, OnJobDone)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_TAKE, OnTake)
		COMMAND_ID_HANDLER(IDC_COMPARE, OnCompare)
		COMMAND_ID_HANDLER(IDC_CANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_SAVE, OnSaveResults)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		CHAIN_MSG_MAP(CVirtualListView<CSnapshotDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CSnapshotDlg>)
		REFLECT_NOTIFICATIONS_EX()
	END_MSG_MAP()

private:
	//
	// a snapshot or comparison running in the background
	// shared with its thread, which reports through messages and never touches the dialog
	//
	struct Job {
		WPARAM Id;
		HWND hWnd;
		std::atomic<bool> Cancel{ false };
		ULONGLONG LastProgress{ 0 };
		bool IsCompare{ false };
		bool Success{ false };
		CString Message;
		std::vector<SnapshotChange> Changes;

		// for RegistrySnapshot::Take
		bool Progress(size_t keys);
	};

	void StartJob(PCWSTR status, bool compare, std::function<void(Job&)> work);
	void UpdateControls();
	bool PickFile(bool save, PCWSTR title, CString& path);
	bool IsRunning() const;

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnJobProgress(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnJobDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTake(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCompare(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSaveResults(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
	CListViewCtrl m_List;
	CProgressBarCtrl m_Progress;
	std::vector<SnapshotChange> m_Changes;
	std::shared_ptr<Job> m_Job;
	std::thread m_Thread;
	WPARAM m_JobId{ 0 };
	CString m_KeyPath;
};
