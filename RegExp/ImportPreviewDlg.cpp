#include "pch.h"
#include "resource.h"
#include "ImportPreviewDlg.h"
#include "SortHelper.h"
#include <ListViewhelper.h>

CImportPreviewDlg::CImportPreviewDlg(CString const& fileName, std::vector<SnapshotChange> changes)
	: m_FileName(fileName), m_Changes(std::move(changes)) {
}

LRESULT CImportPreviewDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	CenterWindow(GetParent());
	SetDialogIcon(IDI_IMPORT);

	// a summary of the changes, by kind
	int counts[6]{};
	for (auto& change : m_Changes)
		counts[(int)change.Type]++;
	auto plural = [](size_t count, PCWSTR one, PCWSTR many) {
		CString text;
		text.Format(L"%zu %s", count, count == 1 ? one : many);
		return text;
	};
	auto total = m_Changes.size() - counts[(int)SnapshotChangeType::NoAccess];
	CString summary;
	summary.Format(L"Importing %s will make %s:", (PCWSTR)m_FileName.Mid(m_FileName.ReverseFind(L'\\') + 1),
		(PCWSTR)plural(total, L"change", L"changes"));
	static const struct { SnapshotChangeType Type; PCWSTR One, Many; } kinds[] = {
		{ SnapshotChangeType::KeyAdded, L"key added", L"keys added" },
		{ SnapshotChangeType::KeyDeleted, L"key deleted", L"keys deleted" },
		{ SnapshotChangeType::ValueAdded, L"value added", L"values added" },
		{ SnapshotChangeType::ValueChanged, L"value changed", L"values changed" },
		{ SnapshotChangeType::ValueDeleted, L"value deleted", L"values deleted" },
	};
	CString parts;
	for (auto& kind : kinds)
		if (auto count = counts[(int)kind.Type])
			parts += (parts.IsEmpty() ? L" " : L", ") + plural(count, kind.One, kind.Many);
	summary += parts + L".";
	if (auto noAccess = counts[(int)SnapshotChangeType::NoAccess])
		summary += L"\n" + plural(noAccess, L"key can't be read, so its changes aren't", L"keys can't be read, so their changes aren't")
			+ L" shown (importing them will likely fail).";
	SetDlgItemText(IDC_STATUS, summary);

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
	m_List.SetItemCount((int)m_Changes.size());

	return 0;
}

LRESULT CImportPreviewDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

CString CImportPreviewDlg::GetColumnText(HWND, int row, int col) const {
	return RegistrySnapshot::GetChangeText(m_Changes[row], col);
}

int CImportPreviewDlg::GetRowImage(HWND, int row, int) const {
	return RegistrySnapshot::IsKeyChange(m_Changes[row].Type) ? 0 : 1;
}

void CImportPreviewDlg::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	std::ranges::stable_sort(m_Changes, [&](SnapshotChange const& c1, SnapshotChange const& c2) {
		if (si->SortColumn == 0)
			return SortHelper::Sort((int)c1.Type, (int)c2.Type, si->SortAscending);
		return SortHelper::Sort(RegistrySnapshot::GetChangeText(c1, si->SortColumn), RegistrySnapshot::GetChangeText(c2, si->SortColumn), si->SortAscending);
		});
}
