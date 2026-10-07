#include "pch.h"
#include "resource.h"
#include "SnapshotDlg.h"
#include "SortHelper.h"
#include "Helpers.h"
#include <ListViewhelper.h>
#include <WTLHelper.h>
#include <ClipboardHelper.h>
#include <thread>

namespace {
	CString FormatTime(FILETIME const& ft) {
		return CTime(ft).Format(L"%x %X");
	}

	// a file name for a new snapshot: the key's name and the date
	CString DefaultFileName(CString const& key) {
		auto name = key.Mid(key.ReverseFind(L'\\') + 1);
		for (auto ch : L"/:*?\"<>|")
			if (ch)
				name.Remove(ch);
		if (name.IsEmpty())
			name = L"Registry";
		return name + CTime::GetCurrentTime().Format(L"-%Y-%m-%d-%H%M.trsnap");
	}
}

bool CSnapshotDlg::Job::Progress(size_t keys) {
	// a few updates a second are enough
	auto now = ::GetTickCount64();
	if (now - LastProgress >= 200) {
		LastProgress = now;
		::PostMessage(hWnd, WM_JOB_PROGRESS, Id, (LPARAM)keys);
	}
	return !Cancel;
}

CSnapshotDlg::CSnapshotDlg(IMainFrame* frame) : m_pFrame(frame) {
}

CSnapshotDlg::~CSnapshotDlg() {
	if (m_Job)
		m_Job->Cancel = true;
	if (m_Thread.joinable())
		m_Thread.join();
}

void CSnapshotDlg::SetKeyPath(CString const& path) {
	m_KeyPath = path;
	if (m_hWnd && !IsRunning())
		SetDlgItemText(IDC_KEY, path);
}

bool CSnapshotDlg::IsRunning() const {
	return m_Job != nullptr;
}

LRESULT CSnapshotDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	CenterWindow(GetParent());
	SetDialogIcon(IDI_EXPORT);
	AddIconToButton(IDC_CANCEL, IDI_DELETE);

	SetDlgItemText(IDC_KEY, m_KeyPath);

	m_Progress.Attach(GetDlgItem(IDC_PROGRESS));
	m_Progress.SetMarquee(TRUE);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	::SetWindowTheme(m_List, L"Explorer", L"");

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 2, 2);
	images.AddIcon(AtlLoadIconImage(IDI_FOLDER, 0, 16, 16));
	images.AddIcon(AtlLoadIconImage(IDI_TEXT, 0, 16, 16));
	m_List.SetImageList(images, LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Change", 0, 120);
	cm->AddColumn(L"Key", 0, 330);
	cm->AddColumn(L"Value", 0, 120);
	cm->AddColumn(L"Old Data", 0, 160);
	cm->AddColumn(L"New Data", 0, 160);
	cm->UpdateColumns();

	UpdateControls();
	Helpers::RestoreWindowPosition(m_hWnd, L"SnapshotsDlgRect");
	return 0;
}

LRESULT CSnapshotDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	if (m_Job)
		m_Job->Cancel = true;
	Helpers::SaveWindowPosition(m_hWnd, L"SnapshotsDlgRect");
	handled = FALSE;
	return 0;
}

LRESULT CSnapshotDlg::OnCloseCmd(WORD, WORD, HWND, BOOL&) {
	// hidden, not destroyed, so the results stay; a running job continues
	ShowWindow(SW_HIDE);
	return 0;
}

void CSnapshotDlg::UpdateControls() {
	auto running = IsRunning();
	for (auto id : { IDC_KEY, IDC_TAKE, IDC_COMPARE, IDC_TWOFILES })
		GetDlgItem(id).EnableWindow(!running);
	GetDlgItem(IDC_CANCEL).EnableWindow(running);
	GetDlgItem(IDC_SAVE).EnableWindow(!running && !m_Changes.empty());
	GetDlgItem(IDC_COPY).EnableWindow(!running && !m_Changes.empty());
	m_Progress.ShowWindow(running ? SW_SHOW : SW_HIDE);
}

bool CSnapshotDlg::PickFile(bool save, PCWSTR title, CString& path) {
	CSimpleFileDialog dlg(!save, L"trsnap", save ? (PCWSTR)path : nullptr,
		OFN_EXPLORER | OFN_ENABLESIZING | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST),
		L"Registry Snapshots (*.trsnap)\0*.trsnap\0All Files\0*.*\0", m_hWnd);
	dlg.m_ofn.lpstrTitle = title;
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal() == IDOK;
	WTLHelper::ResumeHook();
	if (ok)
		path = dlg.m_szFileName;
	return ok;
}

void CSnapshotDlg::StartJob(PCWSTR status, bool compare, std::function<void(Job&)> work) {
	// the previous job's thread has finished, or is about to
	if (m_Thread.joinable())
		m_Thread.join();

	auto job = std::make_shared<Job>();
	job->IsCompare = compare;
	job->Id = ++m_JobId;
	job->hWnd = m_hWnd;
	m_Job = job;
	SetDlgItemText(IDC_STATUS, status);
	UpdateControls();

	m_Thread = std::thread([job, work] {
		work(*job);
		::PostMessage(job->hWnd, WM_JOB_DONE, job->Id, 0);
		});
}

LRESULT CSnapshotDlg::OnTake(WORD, WORD, HWND, BOOL&) {
	CString key;
	GetDlgItemText(IDC_KEY, key);
	key.Trim();
	key.TrimRight(L'\\');
	if (key.IsEmpty()) {
		AtlMessageBox(m_hWnd, L"Enter the key to take a snapshot of, e.g. HKEY_LOCAL_MACHINE\\SOFTWARE, or \\REGISTRY for all hives.",
			IDS_APP_TITLE, MB_ICONINFORMATION);
		return 0;
	}

	auto file = DefaultFileName(key);
	if (!PickFile(true, L"Save Snapshot", file))
		return 0;

	StartJob(L"Taking a snapshot...", false, [key, file](Job& job) {
		RegistrySnapshot snapshot;
		if (!snapshot.Take(key, [&](size_t keys) { return job.Progress(keys); })) {
			auto error = ::GetLastError();
			if (job.Cancel)
				job.Message = L"Cancelled.";
			else
				job.Message.Format(L"Failed to read %s (%s)", (PCWSTR)key, (PCWSTR)Helpers::GetErrorText(error));
			return;
		}
		if (!snapshot.Save(file)) {
			job.Message.Format(L"Failed to save %s (%s)", (PCWSTR)file, (PCWSTR)Helpers::GetErrorText());
			return;
		}
		job.Success = true;
		job.Message.Format(L"Saved a snapshot of %s: %zu keys, %zu values", (PCWSTR)snapshot.GetRoot(),
			snapshot.GetKeys().size(), snapshot.GetValueCount());
		if (auto inaccessible = snapshot.GetInaccessibleKeys())
			job.Message.AppendFormat(L" (%zu keys could not be read)", inaccessible);
		});
	return 0;
}

LRESULT CSnapshotDlg::OnCompare(WORD, WORD, HWND, BOOL&) {
	CString first, second;
	auto twoFiles = IsDlgButtonChecked(IDC_TWOFILES) == BST_CHECKED;
	if (!PickFile(false, twoFiles ? L"Open the Earlier Snapshot" : L"Open a Snapshot to Compare with the Registry", first))
		return 0;
	if (twoFiles && !PickFile(false, L"Open the Later Snapshot", second))
		return 0;

	m_Changes.clear();
	m_List.SetItemCount(0);
	StartJob(L"Comparing...", true, [first, second](Job& job) {
		auto load = [&](RegistrySnapshot& snapshot, CString const& file) {
			if (snapshot.Load(file))
				return true;
			job.Message.Format(L"Failed to read %s (%s)", (PCWSTR)file,
				::GetLastError() == ERROR_INVALID_DATA ? L"not a snapshot file, or damaged" : (PCWSTR)Helpers::GetErrorText());
			return false;
		};

		RegistrySnapshot before, after;
		if (!load(before, first))
			return;
		if (!second.IsEmpty()) {
			if (!load(after, second))
				return;
			if (before.GetRoot().CompareNoCase(after.GetRoot()) != 0) {
				job.Message.Format(L"The snapshots are of different keys (%s and %s)", (PCWSTR)before.GetRoot(), (PCWSTR)after.GetRoot());
				return;
			}
		}
		else if (!after.Take(before.GetRoot(), [&](size_t keys) { return job.Progress(keys); })) {
			auto error = ::GetLastError();
			if (job.Cancel)
				job.Message = L"Cancelled.";
			else
				job.Message.Format(L"Failed to read %s (%s)", (PCWSTR)before.GetRoot(), (PCWSTR)Helpers::GetErrorText(error));
			return;
		}

		job.Changes = RegistrySnapshot::Compare(before, after);
		job.Success = true;
		job.Message.Format(L"%zu changes in %s from %s to %s", job.Changes.size(), (PCWSTR)before.GetRoot(),
			(PCWSTR)FormatTime(before.GetTime()), second.IsEmpty() ? L"now" : (PCWSTR)FormatTime(after.GetTime()));
		});
	return 0;
}

LRESULT CSnapshotDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	if (m_Job) {
		m_Job->Cancel = true;
		SetDlgItemText(IDC_STATUS, L"Cancelling...");
	}
	return 0;
}

LRESULT CSnapshotDlg::OnJobProgress(UINT, WPARAM id, LPARAM keys, BOOL&) {
	if (m_Job && id == m_Job->Id && !m_Job->Cancel) {
		CString text;
		text.Format(L"Reading the Registry... %zu keys", (size_t)keys);
		SetDlgItemText(IDC_STATUS, text);
	}
	return 0;
}

LRESULT CSnapshotDlg::OnJobDone(UINT, WPARAM id, LPARAM, BOOL&) {
	if (!m_Job || id != m_Job->Id)
		return 0;

	auto job = std::move(m_Job);
	// it only has to return now
	if (m_Thread.joinable())
		m_Thread.join();
	// a snapshot leaves the last comparison's results
	if (job->IsCompare) {
		m_Changes = std::move(job->Changes);
		m_List.SetItemCountEx((int)m_Changes.size(), LVSICF_NOSCROLL);
		DoSort(GetSortInfo(m_List));
		m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	}
	SetDlgItemText(IDC_STATUS, job->Message);
	UpdateControls();
	if (!job->Success && !job->Cancel)
		AtlMessageBox(m_hWnd, (PCWSTR)job->Message, IDS_APP_TITLE, MB_ICONERROR);
	return 0;
}

CString CSnapshotDlg::GetColumnText(HWND, int row, int col) const {
	return RegistrySnapshot::GetChangeText(m_Changes[row], col);
}

int CSnapshotDlg::GetRowImage(HWND, int row, int) const {
	return RegistrySnapshot::IsKeyChange(m_Changes[row].Type) ? 0 : 1;
}

void CSnapshotDlg::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	auto compare = [&](SnapshotChange const& c1, SnapshotChange const& c2) {
		switch (si->SortColumn) {
			case 0: return SortHelper::Sort((int)c1.Type, (int)c2.Type, si->SortAscending);
			case 1: return SortHelper::Sort(c1.Key, c2.Key, si->SortAscending);
			case 2: return SortHelper::Sort(c1.Value, c2.Value, si->SortAscending);
		}
		// data columns: by their text
		auto text = [&](SnapshotChange const& c) {
			auto& value = si->SortColumn == 3 ? c.Old : c.New;
			return value ? RegistrySnapshot::FormatData(*value) : CString();
		};
		return SortHelper::Sort(text(c1), text(c2), si->SortAscending);
	};
	std::ranges::stable_sort(m_Changes, compare);
}

bool CSnapshotDlg::OnDoubleClickList(HWND, int row, int, const POINT&) {
	if (row < 0)
		return false;

	auto& change = m_Changes[row];
	bool isValue = !RegistrySnapshot::IsKeyChange(change.Type);
	CWaitCursor wait;
	if (m_pFrame->GoToItem(change.Key, isValue ? (PCWSTR)change.Value : nullptr, nullptr))
		return true;

	// deleted since: show the nearest key that exists
	auto path = change.Key;
	for (int bs = path.ReverseFind(L'\\'); bs > 0; bs = path.ReverseFind(L'\\')) {
		path = path.Left(bs);
		if (m_pFrame->GoToItem(path, nullptr, nullptr)) {
			SetDlgItemText(IDC_STATUS, change.Key + L" no longer exists; showing " + path);
			return true;
		}
	}
	AtlMessageBox(m_hWnd, L"Unable to locate the key", IDS_APP_TITLE, MB_ICONERROR);
	return false;
}

LRESULT CSnapshotDlg::OnSaveResults(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(FALSE, L"txt", nullptr, OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT,
		L"Text Files (*.txt)\0*.txt\0All Files\0*.*\0", m_hWnd);
	WTLHelper::SuspendHook();
	if (dlg.DoModal() == IDOK) {
		CString text(L"Change\tKey\tValue\tOld Data\tNew Data\r\n");
		for (int i = 0; i < (int)m_Changes.size(); i++) {
			for (int col = 0; col < 5; col++)
				text += GetColumnText(m_List, i, col) + (col < 4 ? L"\t" : L"\r\n");
		}
		if (!Helpers::WriteToFile(dlg.m_szFileName, text))
			m_pFrame->DisplayError(L"Error saving results", m_hWnd);
	}
	WTLHelper::ResumeHook();
	return 0;
}

LRESULT CSnapshotDlg::OnCopy(WORD, WORD, HWND, BOOL&) {
	ClipboardHelper::CopyText(m_hWnd, m_List.GetSelectedCount() == 0 ?
		ListViewHelper::GetAllRowsAsString(m_List) : ListViewHelper::GetSelectedRowsAsString(m_List));
	return 0;
}
