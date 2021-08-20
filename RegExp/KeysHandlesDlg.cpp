#include "pch.h"
#include "resource.h"
#include "KeysHandlesDlg.h"
#include "Helpers.h"
#include "ImageIconCache.h"
#include "SortHelper.h"
#include "SecurityInformation.h"
#include "SecurityHelper.h"

CString CKeysHandlesDlg::GetColumnText(HWND hWnd, int row, int col) const {
	auto& item = m_Handles[row];
	switch (GetColumnManager(hWnd)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::PID: return std::format(L"{}", item.ProcessId).c_str();
		case ColumnType::Access: return std::format(L"0x{:08X}", item.Access).c_str();
		case ColumnType::Handle: return std::format(L"0x{:X}", item.Handle).c_str();
		case ColumnType::Attributes: return std::format(L"0x{:X}", item.Attributes).c_str();
		case ColumnType::KeyName: return item.Name;
		case ColumnType::ProcessName: return item.ProcessName;
		case ColumnType::Address: return std::format(L"0x{:p}", item.Object).c_str();
	}
	return L"";
}

int CKeysHandlesDlg::GetRowImage(HWND, int row) const {
	return ImageIconCache::Get().GetIconIndex(m_Handles[row].ProcessId);
}

void CKeysHandlesDlg::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);

	auto compare = [&](const auto& h1, const auto& h2) {
		switch (col) {
			case ColumnType::ProcessName: return SortHelper::Sort(h1.ProcessName, h2.ProcessName, si->SortAscending);
			case ColumnType::KeyName: return SortHelper::Sort(h1.Name, h2.Name, si->SortAscending);
			case ColumnType::PID: return SortHelper::Sort(h1.ProcessId, h2.ProcessId, si->SortAscending);
			case ColumnType::Handle: return SortHelper::Sort(h1.Handle, h2.Handle, si->SortAscending);
			case ColumnType::Access: return SortHelper::Sort(h1.Access, h2.Access, si->SortAscending);
			case ColumnType::Attributes: return SortHelper::Sort(h1.Attributes, h2.Attributes, si->SortAscending);
			case ColumnType::Address: return SortHelper::Sort(h1.Object, h2.Object, si->SortAscending);
		}
		return false;
	};
	std::ranges::sort(m_Handles, compare);
}

void CKeysHandlesDlg::Refresh() {
	m_Handles = Registry::EnumKeyHandles(m_HideInaccesible);
	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx(static_cast<int>(m_Handles.size()), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	SetDlgItemText(IDC_HANDLECOUNT, std::format(L"Handles: {}", m_Handles.size()).c_str());
}

BOOL CKeysHandlesDlg::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

void CKeysHandlesDlg::BuildToolBar() {
	CToolBarCtrl tb;
	CRect rc(8, 4, 350, 40);
	tb.Create(m_hWnd, &rc, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
	tb.GetToolTips().ModifyStyle(0, TTS_ALWAYSTIP);
	tb.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);

	CImageList tbImages;
	tbImages.Create(24, 24, ILC_COLOR32, 8, 4);
	tb.SetImageList(tbImages);

	const struct {
		UINT id;
		int image;
		BYTE style = BTNS_BUTTON;
		BYTE state = TBSTATE_ENABLED;
		PCWSTR text = nullptr;
	} buttons[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ 0 },
		{ ID_HIDE_EMPTY, IDI_FOLDER_ACCESSDENIED, BTNS_BUTTON | BTNS_CHECK, TBSTATE_ENABLED },
		{ 0 },
		{ ID_KEY_PERMISSIONS, IDI_PERM },
		{ ID_CLOSE_HANDLE, IDI_DELETE },
	};
	for (auto& b : buttons) {
		if (b.id == 0)
			tb.AddSeparator(0);
		else {
			HICON hIcon = nullptr;
			int image = -1;
			if (b.image) {
				hIcon = AtlLoadIconImage(b.image, 0, 24, 24);
				ATLASSERT(hIcon);
				image = tbImages.AddIcon(hIcon);
			}
			tb.AddButton(b.id, b.style | (b.text ? BTNS_SHOWTEXT : 0), b.state, image, b.text, 0);
		}
	}
	UIAddToolBar(tb);
}

LRESULT CKeysHandlesDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_REAL_REG);
	BuildToolBar();

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_List.SetImageList(ImageIconCache::Get().GetImageList(), LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Process Name", 0, 140, ColumnType::ProcessName);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 70, ColumnType::PID);
	cm->AddColumn(L"Handle", LVCFMT_RIGHT, 80, ColumnType::Handle);
	cm->AddColumn(L"Key", 0, 420, ColumnType::KeyName);
	cm->AddColumn(L"Object Address", LVCFMT_RIGHT, 140, ColumnType::Address);
	cm->AddColumn(L"Access", LVCFMT_RIGHT, 80, ColumnType::Access);
	cm->AddColumn(L"Decoded Access", 0, 160, ColumnType::DecodedAccess);
	cm->AddColumn(L"Attributes", LVCFMT_RIGHT, 90, ColumnType::Attributes);
	cm->UpdateColumns();

	_Module.GetMessageLoop()->AddIdleHandler(this);

	return 0;
}

LRESULT CKeysHandlesDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	ShowWindow(SW_HIDE);
	return 0;
}

LRESULT CKeysHandlesDlg::OnRefresh(WORD, WORD wID, HWND, BOOL&) {
	CWaitCursor wait;
	Refresh();
	return 0;
}

LRESULT CKeysHandlesDlg::OnHideInaccessible(WORD, WORD wID, HWND, BOOL&) {
	m_HideInaccesible = !m_HideInaccesible;
	UISetCheck(wID, m_HideInaccesible);
	Refresh();
	return 0;
}

LRESULT CKeysHandlesDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	handled = FALSE;
	_Module.GetMessageLoop()->RemoveIdleHandler(this);
	return 0;
}

LRESULT CKeysHandlesDlg::OnCloseHandle(WORD, WORD wID, HWND, BOOL&) {
	return 0;
}

LRESULT CKeysHandlesDlg::OnEditCopy(WORD, WORD wID, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CKeysHandlesDlg::OnPermissions(WORD, WORD wID, HWND, BOOL&) {
	auto index = m_List.GetSelectionMark();
	ATLASSERT(index >= 0);
	auto& item = m_Handles[index];
	auto hDup = SecurityHelper::DupHandle(ULongToHandle(item.Handle), item.ProcessId, READ_CONTROL | KEY_WRITE);
	if(!hDup)
		hDup = SecurityHelper::DupHandle(ULongToHandle(item.Handle), item.ProcessId, READ_CONTROL);
	if (hDup) {
		CSecurityInformation si(hDup, item.Name, false);
		::EditSecurity(m_hWnd, &si);
		::CloseHandle(hDup);
	}
	return 0;
}

LRESULT CKeysHandlesDlg::OnToolTipGetDisplay(int, LPNMHDR hdr, BOOL&) {
	auto tt = (NMTTDISPINFO*)hdr;
	CString text;
	text.LoadString((UINT)hdr->idFrom);
	text.Remove(L'\n');
	wcscpy_s(tt->szText, text);
	tt->uFlags = TTF_DI_SETITEM;

	return 0;
}

LRESULT CKeysHandlesDlg::OnListSelectionChanged(int, LPNMHDR, BOOL&) {
	auto count = m_List.GetSelectedCount();
	UIEnable(ID_KEY_PERMISSIONS, count == 1);
	UIEnable(ID_CLOSE_HANDLE, count > 0);
	UIEnable(ID_EDIT_COPY, count > 0);

	return 0;
}
