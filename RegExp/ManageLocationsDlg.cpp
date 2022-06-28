#include "pch.h"
#include "ManageLocationsDlg.h"
#include "ListViewhelper.h"
#include "Helpers.h"
#include "RegistryKey.h"
#include "Registry.h"
#include <ClipboardHelper.h>
#include "EnumStrings.h"

CManageLocationsDlg::CManageLocationsDlg(IMainFrame* frame, LocationManager& lm) : m_pFrame(frame), m_lm(lm) {
}

CString CManageLocationsDlg::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Items[row];
	switch (col) {
		case 0: return item.Name;
		case 1: return item.Path;
	}
	ATLASSERT(false);
	return L"";
}

void CManageLocationsDlg::DoSort(const SortInfo* si) {
}

void CManageLocationsDlg::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	UpdateUI();
}

void CManageLocationsDlg::UpdateUI() {
	auto selected = m_List.GetSelectedCount();
	GetDlgItem(IDC_SET).EnableWindow(selected == 1);
	if (selected == 1) {
		auto index = m_List.GetNextItem(-1, LVIS_SELECTED);
		ATLASSERT(index >= 0);
		SetDlgItemText(IDC_NAME, m_Items[index].Name);
		SetDlgItemText(IDC_PATH, m_Items[index].Path);
	}
	else {
		SetDlgItemText(IDC_NAME, L"");
		SetDlgItemText(IDC_PATH, L"");
	}
	GetDlgItem(IDC_DELETE).EnableWindow(selected > 0);
	GetDlgItem(IDC_COPY).EnableWindow(selected > 0);
}

LRESULT CManageLocationsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	Helpers::RestoreWindowPosition(m_hWnd, L"ManageLocationsDlg");

	SetDialogIcon(IDR_MAINFRAME);
	AddIconToButton(IDC_DELETE, IDI_DELETE);
	AddIconToButton(IDC_COPY, IDI_COPY);
	AddIconToButton(IDC_NEW, IDI_BOOKMARK_ADD);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", 0, 300);
	cm->AddColumn(L"Key Path", 0, 500);
	cm->UpdateColumns();

	for (auto& [name, path] : m_lm)
		m_Items.push_back({ name, path });

	m_List.SetItemCount((int)m_Items.size());
	UpdateUI();

	CComObject<CEnumStrings>* pEnum;
	pEnum->CreateInstance(&pEnum);
	CComPtr<IAutoComplete2> spAC;
	auto hr = spAC.CoCreateInstance(CLSID_AutoComplete);
	if (spAC) {
		spAC->Init(GetDlgItem(IDC_PATH), pEnum->GetUnknown(), nullptr, nullptr);
		spAC->SetOptions(ACO_AUTOSUGGEST | ACO_USETAB | ACO_AUTOAPPEND);
	}

	return 0;
}

LRESULT CManageLocationsDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	Helpers::SaveWindowPosition(m_hWnd, L"ManageLocationsDlg");
	handled = FALSE;

	return 0;
}

LRESULT CManageLocationsDlg::OnCloseCmd(WORD, WORD id, HWND, BOOL&) {
	if (id == IDOK) {
		m_lm.Clear();
		for (auto& item : m_Items)
			m_lm.Add(item.Name, item.Path);
	}
	EndDialog(id);
	return 0;
}

LRESULT CManageLocationsDlg::OnCopy(WORD, WORD wID, HWND, BOOL&) {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List));

	return 0;
}

LRESULT CManageLocationsDlg::OnDelete(WORD, WORD wID, HWND, BOOL&) {
	int n = m_List.GetNextItem(-1, LVIS_SELECTED);
	int offset = 0;
	for (; n != -1; n = m_List.GetNextItem(n, LVIS_SELECTED)) {
		m_Items.erase(m_Items.begin() + n + offset);
		offset--;
	}
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());

	return 0;
}

LRESULT CManageLocationsDlg::OnNewLocation(WORD, WORD wID, HWND, BOOL&) {
	Location newLoc{ L"New Name", L"HKEY_LOCAL_MACHINE" };
	m_Items.push_back(newLoc);
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	m_List.SelectItem(m_List.GetItemCount() - 1);
	GetDlgItem(IDC_NAME).SetFocus();

	return 0;
}

LRESULT CManageLocationsDlg::OnSet(WORD, WORD wID, HWND, BOOL&) {
	CString path, name;
	GetDlgItemText(IDC_PATH, path);
	auto key = Registry::OpenKey(path, KEY_QUERY_VALUE);
	if (!key && AtlMessageBox(m_hWnd, L"Key cannot be opened successfully. Continue?", 
		IDS_APP_TITLE, MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDCANCEL)
		return 0;

	GetDlgItemText(IDC_NAME, name);
	if (name.IsEmpty()) {
		AtlMessageBox(m_hWnd, L"Name cannot be empty.", IDS_APP_TITLE, MB_ICONWARNING);
		return 0;
	}

	if (auto it = std::ranges::find_if(m_Items, [&](auto& item) { return item.Name.CompareNoCase(name) == 0; }); it != m_Items.end()) {
		AtlMessageBox(m_hWnd, L"Name already exists.", IDS_APP_TITLE, MB_ICONWARNING);
		return 0;
	}

	int index = m_List.GetNextItem(-1, LVIS_SELECTED);
	ATLASSERT(index >= 0);
	auto& item = m_Items[index];
	item.Name = name;
	item.Path = path;
	m_List.RedrawItems(index, index);

	return 0;
}
