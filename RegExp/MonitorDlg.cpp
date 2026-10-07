#include "pch.h"
#include "resource.h"
#include "MonitorDlg.h"
#include "Helpers.h"
#include "SecurityHelper.h"
#include <ListViewhelper.h>
#include <ClipboardHelper.h>

namespace {
	// the list keeps the latest events; older ones are dropped in chunks
	const size_t MaxEvents = 100000;
	const size_t DropChunk = 20000;

	bool ContainsNoCase(CString const& text, CString const& upperPart) {
		return CString(text).MakeUpper().Find(upperPart) >= 0;
	}
}

CMonitorDlg::CMonitorDlg(IMainFrame* frame) : m_pFrame(frame) {
}

LRESULT CMonitorDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	CenterWindow(GetParent());
	SetDialogIcon(IDI_FINDALL);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	::SetWindowTheme(m_List, L"Explorer", L"");

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 2, 2);
	images.AddIcon(AtlLoadIconImage(IDI_FOLDER, 0, 16, 16));
	images.AddIcon(AtlLoadIconImage(IDI_TEXT, 0, 16, 16));
	m_List.SetImageList(images, LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Time", 0, 90);
	cm->AddColumn(L"Process", 0, 120);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 55);
	cm->AddColumn(L"Operation", 0, 100);
	cm->AddColumn(L"Key", 0, 380);
	cm->AddColumn(L"Value", 0, 120);
	cm->AddColumn(L"Result", 0, 120);
	cm->UpdateColumns();

	if (!SecurityHelper::IsRunningElevated())
		SetDlgItemText(IDC_STATUS, L"Monitoring the Registry needs administrator rights: use File / Run as Administrator.");
	Helpers::RestoreWindowPosition(m_hWnd, L"MonitorDlgRect");
	return 0;
}

LRESULT CMonitorDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	m_Monitor.Stop();
	Helpers::SaveWindowPosition(m_hWnd, L"MonitorDlgRect");
	handled = FALSE;
	return 0;
}

LRESULT CMonitorDlg::OnCloseCmd(WORD, WORD, HWND, BOOL&) {
	// hidden, not destroyed: monitoring goes on, and the events stay
	ShowWindow(SW_HIDE);
	return 0;
}

LRESULT CMonitorDlg::OnStart(WORD, WORD, HWND, BOOL&) {
	if (m_Monitor.IsRunning()) {
		m_Monitor.Stop();
		// collect what arrived before stopping
		BOOL handled;
		OnEvents(0, 0, 0, handled);
	}
	else {
		auto hWnd = m_hWnd;
		if (!m_Monitor.Start(IsDlgButtonChecked(IDC_READS) == BST_CHECKED, [hWnd] { ::PostMessage(hWnd, WM_EVENTS, 0, 0); })) {
			auto error = ::GetLastError();
			if (error == ERROR_ACCESS_DENIED)
				AtlMessageBox(m_hWnd, L"Monitoring the Registry needs administrator rights. Use File / Run as Administrator.", IDS_APP_TITLE, MB_ICONWARNING);
			else
				m_pFrame->DisplayError(L"Failed to start monitoring", m_hWnd, error);
			return 0;
		}
	}
	auto running = m_Monitor.IsRunning();
	SetDlgItemText(IDC_START, running ? L"&Stop" : L"&Start");
	GetDlgItem(IDC_READS).EnableWindow(!running);
	UpdateStatus();
	return 0;
}

LRESULT CMonitorDlg::OnEvents(UINT, WPARAM, LPARAM, BOOL&) {
	auto events = m_Monitor.TakeEvents();
	if (events.empty())
		return 0;

	// follow new events only when already showing the last one
	auto count = (int)m_Shown.size();
	bool atEnd = count == 0 || m_List.GetTopIndex() + m_List.GetCountPerPage() >= count;

	if (m_Events.size() + events.size() > MaxEvents) {
		auto drop = std::min(m_Events.size(), std::max(DropChunk, m_Events.size() + events.size() - MaxEvents));
		m_Events.erase(m_Events.begin(), m_Events.begin() + drop);
		Refilter();
	}
	for (auto& e : events) {
		if (Matches(e))
			m_Shown.push_back(m_Events.size());
		m_Events.push_back(std::move(e));
	}
	m_List.SetItemCountEx((int)m_Shown.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
	if (atEnd && !m_Shown.empty())
		m_List.EnsureVisible((int)m_Shown.size() - 1, FALSE);
	UpdateStatus();
	return 0;
}

bool CMonitorDlg::Matches(RegistryEvent const& e) const {
	return m_Filter.IsEmpty() || ContainsNoCase(e.ProcessName, m_Filter) || ContainsNoCase(e.Key, m_Filter) || ContainsNoCase(e.Value, m_Filter);
}

void CMonitorDlg::Refilter() {
	m_Shown.clear();
	for (size_t i = 0; i < m_Events.size(); i++)
		if (Matches(m_Events[i]))
			m_Shown.push_back(i);
	m_List.SetItemCountEx((int)m_Shown.size(), LVSICF_NOSCROLL);
	m_List.Invalidate();
}

void CMonitorDlg::UpdateStatus() {
	CString text;
	text.Format(L"%s %zu events", m_Monitor.IsRunning() ? L"Monitoring:" : L"Stopped:", m_Events.size());
	if (m_Shown.size() != m_Events.size())
		text.AppendFormat(L" (%zu shown)", m_Shown.size());
	if (auto dropped = m_Monitor.GetDroppedEvents())
		text.AppendFormat(L"; %zu not shown, as they came too fast", dropped);
	SetDlgItemText(IDC_STATUS, text);
}

LRESULT CMonitorDlg::OnFilterChanged(WORD, WORD, HWND, BOOL&) {
	GetDlgItemText(IDC_FILTER, m_Filter);
	m_Filter.Trim();
	m_Filter.MakeUpper();
	Refilter();
	UpdateStatus();
	return 0;
}

LRESULT CMonitorDlg::OnClear(WORD, WORD, HWND, BOOL&) {
	m_Events.clear();
	m_Shown.clear();
	m_List.SetItemCount(0);
	UpdateStatus();
	return 0;
}

LRESULT CMonitorDlg::OnCopy(WORD, WORD, HWND, BOOL&) {
	ClipboardHelper::CopyText(m_hWnd, m_List.GetSelectedCount() == 0 ?
		ListViewHelper::GetAllRowsAsString(m_List) : ListViewHelper::GetSelectedRowsAsString(m_List));
	return 0;
}

CString CMonitorDlg::GetColumnText(HWND, int row, int col) const {
	auto& e = m_Events[m_Shown[row]];
	CString text;
	switch (col) {
		case 0:
		{
			FILETIME local;
			SYSTEMTIME st;
			::FileTimeToLocalFileTime(&e.Time, &local);
			::FileTimeToSystemTime(&local, &st);
			text.Format(L"%02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
			return text;
		}
		case 1: return e.ProcessName;
		case 2: text.Format(L"%u", e.ProcessId); return text;
		case 3: return RegistryMonitor::GetOperationName(e.Operation);
		case 4: return e.Key;
		case 5: return e.Value;
		case 6: return RegistryMonitor::FormatStatus(e.Status);
	}
	return text;
}

int CMonitorDlg::GetRowImage(HWND, int row, int) const {
	return m_Events[m_Shown[row]].Value.IsEmpty() ? 0 : 1;
}

bool CMonitorDlg::OnDoubleClickList(HWND, int row, int, const POINT&) {
	if (row < 0)
		return false;
	auto& e = m_Events[m_Shown[row]];
	CWaitCursor wait;
	if (!m_pFrame->GoToItem(e.Key, e.Value.IsEmpty() ? nullptr : (PCWSTR)e.Value, nullptr))
		SetDlgItemText(IDC_STATUS, e.Key + L" can't be shown (it may no longer exist)");
	return true;
}
