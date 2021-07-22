#include "pch.h"
#include "Resource.h"
#include "MainFrame.h"
#include "SecurityHelper.h"
#include "AboutDlg.h"
#include "SortHelper.h"
#include "IconHelper.h"
#include "CreateKeyCommand.h"
#include "TreeHelper.h"
#include "FindAllDlg.h"
#include "RenameKeyCommand.h"
#include "CopyKeyCommand.h"
#include "ClipboardHelper.h"
#include "DeleteKeyCommand.h"
#include "StringValueDlg.h"
#include "ChangeValueCommand.h"
#include "MultiStringValueDlg.h"
#include "ListViewHelper.h"
#include "NumberValueDlg.h"
#include "BinaryValueDlg.h"
#include "SecurityInformation.h"
#include "CreateValueCommand.h"
#include "RenameValueCommand.h"

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (m_FindDlg.IsWindowVisible() && m_FindDlg.IsDialogMessage(pMsg))
		return TRUE;

	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return FALSE;
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return 0;
}

void CMainFrame::RunOnUiThread(std::function<void()> f) {
	SendMessage(WM_RUN, 0, reinterpret_cast<LPARAM>(&f));
}

AppSettings& CMainFrame::GetSettings() {
	return m_Settings;
}

LRESULT CMainFrame::OnFindUpdate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	TreeHelper th(m_Tree);
	HTREEITEM hItem;
	auto fd = reinterpret_cast<FindData*>(lParam);
	if (fd->Path[0] != L'\\') {
		hItem = th.FindItem(m_hStdReg, fd->Path);
	}
	else {
		hItem = th.FindItem(m_hRealReg, fd->Path);
	}
	ATLASSERT(hItem);
	m_Tree.SelectItem(hItem);
	m_Tree.EnsureVisible(hItem);
	if ((fd->Name == nullptr || *fd->Name == 0) && (fd->Data == nullptr || *fd->Data == 0)) {
		m_Tree.SetFocus();
	}
	else {
		UpdateList(true);
		int index = ListViewHelper::FindItem(m_List, fd->Data ? fd->Data : fd->Name, true);
		ATLASSERT(index >= 0);
		m_List.EnsureVisible(index, FALSE);
		m_List.SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
		m_List.SetSelectionMark(index);
		m_List.SetFocus();
	}
	return 0;
}

void CMainFrame::OnFindNext(PCWSTR path, PCWSTR name, PCWSTR data) {
	ATLTRACE(L"Found: %s, %s, %s\n", path, name, data);
	FindData fd{ path, name, data };
	SendMessage(WM_FIND_UPDATE, 0, reinterpret_cast<LPARAM>(&fd));
}

void CMainFrame::OnFindStart() {
	::SetCursor(AtlLoadSysCursor(IDC_APPSTARTING));
}

void CMainFrame::OnFindEnd(bool cancelled) {
	if (!cancelled) {
		RunOnUiThread([this]() {
			AtlMessageBox(m_hWnd, L"Finished searching the Registry.", IDS_APP_TITLE, MB_ICONINFORMATION);
			});
	}
}

bool CMainFrame::GoToItem(PCWSTR path, PCWSTR name, PCWSTR data) {
	TreeHelper th(m_Tree);
	auto hItem = th.FindItem(path[0] == L'\\' ? m_hRealReg : m_hStdReg, path);
	if (!hItem)
		return false;

	m_UpdateNoDelay = true;
	m_Tree.SelectItem(hItem);
	UpdateList();
	if (name && *name) {
		int index = m_List.FindItem(name, false);
		if (index >= 0) {
			m_List.SetSelectionMark(index);
			m_List.SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
			m_List.SetFocus();
		}
	}
	else {
		m_Tree.SetFocus();
	}
	return true;
}

BOOL CMainFrame::TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) {
	return m_CmdBar.TrackPopupMenu(hMenu, flags, x, y);
}

CString CMainFrame::GetCurrentKeyPath() {
	return GetFullNodePath(m_Tree.GetSelectedItem());
}

CString CMainFrame::GetColumnText(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	CString text;
	switch (static_cast<ColumnType>(GetColumnManager(h)->GetColumnTag(col))) {
		case ColumnType::Name:
			return item.Name.IsEmpty() ? CString(L"(Default)") : item.Name;

		case ColumnType::Type:
			return Registry::GetRegTypeAsString(item.Type);

		case ColumnType::Value:
			if (!item.Key) {
				if (item.Value.IsEmpty())
					item.Value = Registry::GetDataAsString(m_CurrentKey, item);
				return item.Value;
			}
			break;

		case ColumnType::Size:
			if (!item.Key) {
				if (item.Size == -1) {
					// recalculate
					item.Size = 0;
					m_CurrentKey.QueryValue(item.Name, nullptr, nullptr, &item.Size);
				}
				text.Format(L"%u", item.Size);
			}
			break;

		case ColumnType::TimeStamp:
			if (item.TimeStamp.dwHighDateTime + item.TimeStamp.dwHighDateTime)
				return CTime(item.TimeStamp).Format(L"%x %X");
			break;

		case ColumnType::Details:
			return item.Key ? GetKeyDetails(item) : GetValueDetails(item);
	}
	return text;
}


int CMainFrame::GetRowImage(HWND h, int row) const {
	switch (m_Items[row].Type) {
		case REG_KEY:
			return GetKeyImage(m_Items[row]);
		case REG_KEY_UP:
			return 8;
		case REG_DWORD:
			return 12;
		case REG_QWORD:
			return 13;
		case REG_SZ:
		case REG_EXPAND_SZ:
		case REG_MULTI_SZ:
		case REG_NONE:
			return 10;
	}
	return 9;
}

void CMainFrame::DoSort(const SortInfo* si) {
	if (si == nullptr || si->SortColumn < 0 || m_Items.empty())
		return;

	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;
	auto compare = [&](auto& d1, auto& d2) {
		switch (col) {
			case ColumnType::Name: return SortHelper::Sort(d1.Name, d2.Name, asc);
			case ColumnType::Size: return SortHelper::Sort(d1.Size, d2.Size, asc);
			case ColumnType::Type: return SortHelper::Sort(d1.Type, d2.Type, asc);
			case ColumnType::TimeStamp: return SortHelper::Sort(*(ULONGLONG*)&d1.TimeStamp, *(ULONGLONG*)&d2.TimeStamp, asc);
		}
		return false;
	};

	std::sort(m_Items.begin() + (m_Settings.ShowKeysInList() ? 1 : 0), m_Items.end(), compare);
}

bool CMainFrame::IsSortable(HWND h, int col) const {
	auto tag = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	return tag == ColumnType::Name || tag == ColumnType::Size || tag == ColumnType::Type || tag == ColumnType::TimeStamp;
}

BOOL CMainFrame::OnRightClickList(HWND h, int row, int col, const POINT& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	int index = 3;
	if (row >= 0) {
		index = m_Items[row].Key ? 1 : 2;
		if (m_Items[row].Type == REG_KEY_UP)
			return FALSE;
	}
	return m_CmdBar.TrackPopupMenu(menu.GetSubMenu(index), 0, pt.x, pt.y);
}

BOOL CMainFrame::OnDoubleClickList(HWND, int row, int col, const POINT& pt) {
	if (row < 0)
		return FALSE;

	auto& item = m_Items[row];
	if (item.Key) {
		if (item.Type == REG_KEY_UP) {
			m_UpdateNoDelay = true;
			m_Tree.SelectItem(m_Tree.GetParentItem(m_Tree.GetSelectedItem()));
			m_Tree.EnsureVisible(m_Tree.GetSelectedItem());
			m_List.SetSelectionMark(0);
			return TRUE;
		}
		ATLASSERT(item.Type == REG_KEY);
		m_Tree.Expand(m_Tree.GetSelectedItem(), TVE_EXPAND);
		TreeHelper th(m_Tree);
		auto hItem = th.FindChild(m_Tree.GetSelectedItem(), item.Name);
		ATLASSERT(hItem);
		m_UpdateNoDelay = true;
		m_Tree.SelectItem(hItem);
		m_Tree.EnsureVisible(hItem);
		return TRUE;
	}
	else {
		auto ret = ShowValueProperties(item);
	}
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	::RegDeleteTree(HKEY_CURRENT_USER, DeletedPathBackup.Left(DeletedPathBackup.GetLength() - 1));

	if (m_Settings.Load(L"Software\\ScorpioSoftware\\RegExp"))
		m_ReadOnly = m_Settings.ReadOnly();

	CMenuHandle menu = GetMenu();
	if (SecurityHelper::IsRunningElevated()) {
		auto fileMenu = menu.GetSubMenu(0);
		fileMenu.DeleteMenu(0, MF_BYPOSITION);
		fileMenu.DeleteMenu(0, MF_BYPOSITION);
		CString text;
		GetWindowText(text);
		text += L" (Administrator)";
		SetWindowText(text);
	}

	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, nullptr, ATL_SIMPLE_CMDBAR_PANE_STYLE);

	m_CmdBar.SetAlphaImages(true);
	m_CmdBar.AttachMenu(menu);
	InitCommandBar();

	UIAddMenu(menu);
	SetMenu(nullptr);

	CToolBarCtrl tb;
	tb.Create(m_hWnd, nullptr, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE, 0, ATL_IDW_TOOLBAR);
	InitToolBar(tb, 24);
	UIAddToolBar(tb);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(tb, nullptr, TRUE);

	CReBarCtrl rb(m_hWndToolBar);
	rb.LockBands(true);

	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP | SBT_TOOLTIPS);
	m_StatusBar.SubclassWindow(m_hWndStatusBar);
	int panes[] = { 24, 1300 };
	m_StatusBar.SetParts(_countof(panes), panes);
	::SetWindowTheme(m_StatusBar, L"Explorer", nullptr);

	m_hWndClient = m_MainSplitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

	m_Tree.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_EDITLABELS, 0, TreeId);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER | TVS_EX_RICHTOOLTIP | TVS_EX_FADEINOUTEXPANDOS, 0);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 8, 4);
	UINT icons[] = {
		IDR_MAINFRAME, IDI_COMPUTER, IDI_FOLDER, IDI_FOLDER_CLOSED, IDI_FOLDER_LINK,
		IDI_FOLDER_ACCESSDENIED, IDI_HIVE, IDI_HIVE_ACCESSDENIED, IDI_FOLDER_UP, IDI_BINARY,
		IDI_TEXT, IDI_REAL_REG, IDI_NUM4, IDI_NUM8
	};
	for (auto icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_Tree.SetImageList(images, TVSIL_NORMAL);
	::SetWindowTheme(m_Tree, L"Explorer", nullptr);

	m_List.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | LVS_EDITLABELS, 0);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_List.SetImageList(images, LVSIL_SMALL);
	::SetWindowTheme(m_List, L"Explorer", nullptr);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 220, ColumnType::Name);
	cm->AddColumn(L"Type", LVCFMT_LEFT, 110, ColumnType::Type);
	cm->AddColumn(L"Size", LVCFMT_RIGHT, 70, ColumnType::Size);
	cm->AddColumn(L"Value", LVCFMT_LEFT, 250, ColumnType::Value);
	cm->AddColumn(L"Last Write", LVCFMT_LEFT, 120, ColumnType::TimeStamp);
	cm->AddColumn(L"Details", LVCFMT_LEFT, 250, ColumnType::Details);
	cm->UpdateColumns();

	m_MainSplitter.SetSplitterPanes(m_Tree, m_List);
	m_MainSplitter.SetSplitterPosPct(25);
	m_MainSplitter.UpdateSplitterLayout();

	m_FindDlg.Create(m_hWnd);

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	//
	// update UI based on settings
	//
	UISetCheck(ID_OPTIONS_SHOWEXTRAHIVES, m_Settings.ShowExtraHives());
	UISetCheck(ID_VIEW_SHOWKEYSINLIST, m_Settings.ShowKeysInList());
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, m_Settings.AlwaysOnTop());
	if (m_Settings.AlwaysOnTop())
		SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	UISetCheck(ID_EDIT_READONLY, m_ReadOnly);
	UpdateLayout();
	PostMessage(WM_BUILD_TREE);
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);
	if (GetWindowPlacement(&wp)) {
		m_Settings.MainWindowPlacement(wp);
	}
	m_Settings.ReadOnly(m_ReadOnly);
	m_Settings.Save();
	m_FindDlg.Cancel();
	if (m_FindDlg)
		m_FindDlg.DestroyWindow();

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	m_Items.clear();

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 2) {
		KillTimer(2);
		UpdateList();
	}
	return 0;
}

LRESULT CMainFrame::OnShowWindow(UINT, WPARAM show, LPARAM, BOOL&) {
	static bool shown = false;
	if (show && !shown) {
		shown = true;
		auto wp = m_Settings.MainWindowPlacement();
		if (wp.showCmd != SW_HIDE) {
			SetWindowPlacement(&wp);
			UpdateLayout();
		}
	}
	return 0;
}

LRESULT CMainFrame::OnMenuSelect(UINT, WPARAM, LPARAM, BOOL&) {
	return 0;
}

LRESULT CMainFrame::OnExit(WORD, WORD, HWND, BOOL&) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnAbout(WORD, WORD, HWND, BOOL&) {
	CAboutDlg().DoModal();
	return 0;
}

LRESULT CMainFrame::OnBuildTree(UINT, WPARAM, LPARAM, BOOL&) {
	InitTree();
	auto showExtra = m_Settings.ShowExtraHives();

	int i = 0;
	for (auto& k : Registry::Keys) {
		if (!showExtra && ++i == 6)
			break;
		auto hItem = BuildTree(m_hStdReg, k.hKey, k.text);
		SetNodeData(hItem, NodeType::Key | NodeType::Predefined);
	}
	m_Tree.Expand(m_hStdReg, TVE_EXPAND);

	auto hKey = Registry::OpenRealRegistryKey();
	if (hKey) {
		CRegKey key(hKey);
		BuildTree(m_hRealReg, key);
	}
	m_Tree.SetFocus();

	return 0;
}

LRESULT CMainFrame::OnTreeSelChanged(int, LPNMHDR, BOOL&) {
	UpdateUI();
	if (m_UpdateNoDelay) {
		m_UpdateNoDelay = false;
		UpdateList();
	}
	else {
		//
		// short delay in case the user is scrolling fast
		//
		SetTimer(2, 200, nullptr);
	}
	return 0;
}

LRESULT CMainFrame::OnRunAsAdmin(WORD, WORD, HWND, BOOL&) {
	if (SecurityHelper::RunElevated())
		PostMessage(WM_CLOSE);

	return 0;
}

LRESULT CMainFrame::OnListItemChanged(int, LPNMHDR, BOOL&) {
	int selected = m_List.GetSelectionMark();
	if (selected != m_CurrentSelectedItem) {
		m_CurrentSelectedItem = selected;
		UpdateUI();
	}
	return 0;
}

LRESULT CMainFrame::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	RefreshFull(m_hStdReg);
	RefreshFull(m_hRealReg);
	return 0;
}

LRESULT CMainFrame::OnTreeItemExpanding(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	CString text;
	auto h = tv->itemNew.hItem;
	m_Tree.GetItemText(m_Tree.GetChildItem(h), text);
	if (text == L"\\\\") {
		m_Tree.DeleteItem(m_Tree.GetChildItem(h));
		//CWaitCursor wait;
		ExpandItem(h);
	}
	return FALSE;
}

LRESULT CMainFrame::OnShowExtraHives(WORD, WORD id, HWND, BOOL&) {
	auto show = m_Settings.ShowExtraHives();
	m_Settings.ShowExtraHives(!show);
	UISetCheck(id, !show);

	return 0;
}

LRESULT CMainFrame::OnShowKeysInList(WORD, WORD id, HWND, BOOL&) {
	m_Settings.ShowKeysInList(!m_Settings.ShowKeysInList());
	UISetCheck(id, m_Settings.ShowKeysInList());
	UpdateList();

	return 0;
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD, WORD id, HWND, BOOL&) {
	bool top = !(GetExStyle() & WS_EX_TOPMOST);
	SetWindowPos(top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	m_Settings.AlwaysOnTop(top);
	UISetCheck(id, top);

	return 0;
}

LRESULT CMainFrame::OnFocusChanged(int, LPNMHDR hdr, BOOL&) {
	if (hdr->hwndFrom == m_List || hdr->hwndFrom == m_Tree)
		UpdateUI();
	return 0;
}

LRESULT CMainFrame::OnNewKey(WORD, WORD, HWND, BOOL&) {
	m_Tree.GetSelectedItem().Expand(TVE_EXPAND);
	auto hItem = InsertKeyItem(m_Tree.GetSelectedItem(), L"(NewKey)");
	hItem.EnsureVisible();
	m_CurrentOperation = Operation::CreateKey;
	m_Tree.EditLabel(hItem);
	return 0;
}

LRESULT CMainFrame::OnEditReadOnly(WORD, WORD id, HWND, BOOL&) {
	m_ReadOnly = !m_ReadOnly;
	UISetCheck(id, m_ReadOnly);
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnTreeBeginEdit(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTVDISPINFO*)hdr;
	auto prevent = m_ReadOnly || (GetNodeData(tv->item.hItem) & (NodeType::Key | NodeType::Predefined)) != NodeType::Key;
	if (!prevent && m_CurrentOperation == Operation::None)
		m_CurrentOperation = Operation::RenameKey;

	return prevent;
}

LRESULT CMainFrame::OnTreeEndEdit(int, LPNMHDR hdr, BOOL&) {
	auto& item = ((NMTVDISPINFO*)hdr)->item;
	if (item.pszText == nullptr) {
		//
		// cancelled
		//
		if (m_CurrentOperation == Operation::CreateKey)
			m_Tree.DeleteItem(item.hItem);
		return FALSE;
	}
	switch (m_CurrentOperation) {
		case Operation::CreateKey:
		{
			auto hItem = item.hItem;
			auto hParent = m_Tree.GetParentItem(hItem);
			auto cmd = std::make_shared<CreateKeyCommand>(GetFullNodePath(hParent), item.pszText);
			if (!m_CmdMgr.AddCommand(cmd)) {
				DisplayError(L"Failed to create key");
				m_Tree.DeleteItem(hItem);
				return FALSE;
			}
			auto cb = [this](auto& cmd, bool execute) {
				if (execute) {
					auto hParent = FindItemByPath(cmd.GetPath());
					ATLASSERT(hParent);
					auto hItem = InsertKeyItem(hParent, cmd.GetName());
					m_Tree.EnsureVisible(hItem);
				}
				else {
					auto hItem = FindItemByPath(cmd.GetPath() + L"\\" + cmd.GetName());
					ATLASSERT(hItem);
					m_Tree.DeleteItem(hItem);
				}
				return true;
			};
			cmd->SetCallback(cb);
			if(AppSettings::Get().ShowKeysInList())
				UpdateList();
			return TRUE;
		}
		case Operation::RenameKey:
		{
			CString name;
			m_Tree.GetItemText(item.hItem, name);
			auto cb = [this](auto& cmd, bool) {
				auto hParent = FindItemByPath(cmd.GetPath());
				ATLASSERT(hParent);
				TreeHelper th(m_Tree);
				auto hItem = th.FindChild(hParent, cmd.GetName());
				ATLASSERT(hItem);
				m_Tree.SetItemText(hItem, cmd.GetNewName());
				return true;
			};
			auto hParent = m_Tree.GetParentItem(item.hItem);
			auto cmd = std::make_shared<RenameKeyCommand>(GetFullNodePath(hParent), name, item.pszText);
			if (!m_CmdMgr.AddCommand(cmd)) {
				DisplayError(L"Failed to rename keu");
				return FALSE;
			}
			cmd->SetCallback(cb);
			return TRUE;
		}
	}
	return FALSE;
}

LRESULT CMainFrame::OnEditUndo(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_CmdMgr.CanUndo());
	m_CmdMgr.Undo();
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnEditRedo(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_CmdMgr.CanRedo());
	m_CmdMgr.Redo();
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnTreeContextMenu(int, LPNMHDR hdr, BOOL&) {
	CPoint pt;
	::GetCursorPos(&pt);
	CPoint pt2(pt);
	m_Tree.ScreenToClient(&pt);
	UINT flags;
	auto hItem = m_Tree.HitTest(pt, &flags);
	if (hItem) {
		m_Tree.SelectItem(hItem);
		CMenu menu;
		menu.LoadMenu(IDR_CONTEXT);
		UpdateUI();
		m_CmdBar.TrackPopupMenu(menu.GetSubMenu(0), 0, pt2.x, pt2.y);
	}
	return 0;
}

LRESULT CMainFrame::OnTreeKeyDown(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTVKEYDOWN*)hdr;
	switch (tv->wVKey) {
		case VK_TAB:
			m_List.SetFocus();
			return TRUE;

		case VK_APPS:
			CRect rc;
			if (m_Tree.GetItemRect(m_Tree.GetSelectedItem(), &rc, TRUE)) {
				InvokeTreeContextMenu(rc.CenterPoint());
				return TRUE;
			}
			break;
	}
	return 0;
}

LRESULT CMainFrame::OnListKeyDown(int, LPNMHDR hdr, BOOL&) {
	auto lv = (NMLVKEYDOWN*)hdr;
	switch (lv->wVKey) {
		case VK_TAB:
			m_Tree.SetFocus();
			break;

		case VK_RETURN:
			OnDoubleClickList(m_List, m_List.GetSelectionMark(), 0, POINT());
			break;
	}
	return 0;
}

LRESULT CMainFrame::OnEditFind(WORD, WORD, HWND, BOOL&) {
	m_FindDlg.UpdateUI();
	m_FindDlg.ShowWindow(SW_SHOW);

	return 0;
}

LRESULT CMainFrame::OnRunOnUIThread(UINT, WPARAM, LPARAM lp, BOOL&) {
	auto f = reinterpret_cast<std::function<void()>*>(lp);
	(*f)();
	return 0;
}

LRESULT CMainFrame::OnSearchFindNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_FindDlg.ShowWindow(SW_SHOW);
	m_FindDlg.SetFocus();
	m_FindDlg.Continue();

	return 0;
}

LRESULT CMainFrame::OnEditRename(WORD, WORD, HWND, BOOL&) {
	if (::GetFocus() == m_Tree) {
		m_CurrentOperation = Operation::RenameKey;
		m_Tree.EditLabel(m_Tree.GetSelectedItem());
	}
	else if (::GetFocus() == m_List) {
		auto index = m_List.GetSelectionMark();
		if (index >= 0) {
			m_CurrentOperation = m_Items[index].Key ? Operation::RenameKey : Operation::RenameValue;
			m_List.EditLabel(index);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	if (::GetFocus() == m_Tree) {
		m_Clipboard.Key = true;
		m_Clipboard.Path = GetFullParentNodePath(m_Tree.GetSelectedItem());
		m_Tree.GetItemText(m_Tree.GetSelectedItem(), m_Clipboard.Name);
		m_Clipboard.Operation = ClipboardOperation::Copy;
	}
	return 0;
}

LRESULT CMainFrame::OnEditPaste(WORD, WORD, HWND, BOOL&) {
	if (::GetFocus() == m_Tree) {
		ATLASSERT(m_Clipboard.Key && !m_Clipboard.Path.IsEmpty());
		if (TreeHelper(m_Tree).FindChild(m_Tree.GetSelectedItem(), m_Clipboard.Name)) {
			AtlMessageBox(m_hWnd, (PCWSTR)(L"Key " + m_Clipboard.Name + L" already exists."), IDS_APP_TITLE, MB_ICONWARNING);
			return 0;
		}
		auto target = GetFullNodePath(m_Tree.GetSelectedItem());
		auto cb = [this](auto& cmd, bool execute) {
			TreeHelper th(m_Tree);
			auto real = cmd.GetTargetPath()[0] == L'\\';
			auto hParent = th.FindItem(real ? m_hRealReg : m_hStdReg, cmd.GetTargetPath());
			if (execute) {
				ATLASSERT(hParent);
				auto hItem = InsertKeyItem(hParent, cmd.GetName());
			}
			else {
				m_Tree.DeleteItem(th.FindChild(hParent, cmd.GetName()));
			}
			return true;
		};
		auto cmd = std::make_shared<CopyKeyCommand>(m_Clipboard.Path, m_Clipboard.Name, target, cb);
		if (!m_CmdMgr.AddCommand(cmd)) {
			DisplayError(L"Failed to paste key");
		}
	}
	return 0;
}

LRESULT CMainFrame::OnEditDelete(WORD, WORD, HWND, BOOL&) {
	if (::GetFocus() == m_Tree) {
		auto hItem = m_Tree.GetSelectedItem();
		auto path = GetFullParentNodePath(hItem);
		CString name;
		m_Tree.GetItemText(hItem, name);
		auto cb = [this](auto& cmd, bool execute) {
			TreeHelper th(m_Tree);
			auto real = cmd.GetPath()[0] == L'\\';
			auto hParent = th.FindItem(real ? m_hRealReg : m_hStdReg, cmd.GetPath());
			ATLASSERT(hParent);
			if (execute) {
				auto hItem = th.FindChild(hParent, cmd.GetName());
				ATLASSERT(hItem);
				m_Tree.DeleteItem(hItem);
			}
			else {
				//
				// create the item
				//
				auto hItem = InsertKeyItem(hParent, cmd.GetName());
			}
			return true;
		};
		auto cmd = std::make_shared<DeleteKeyCommand>(path, name, cb);
		if (!m_CmdMgr.AddCommand(cmd))
			DisplayError(L"Failed to delete key");
	}
	else {
	}
	return 0;
}

LRESULT CMainFrame::OnTreeRefresh(WORD, WORD, HWND, BOOL&) {
	RefreshItem(m_Tree.GetSelectedItem());
	return 0;
}

LRESULT CMainFrame::OnCopyFullKeyName(WORD, WORD, HWND, BOOL&) {
	ClipboardHelper::CopyText(m_hWnd, GetFullNodePath(m_Tree.GetSelectedItem()));
	return 0;
}

LRESULT CMainFrame::OnCopyKeyName(WORD, WORD, HWND, BOOL&) {
	CString text;
	m_Tree.GetItemText(m_Tree.GetSelectedItem(), text);
	ClipboardHelper::CopyText(m_hWnd, text);
	return 0;
}

LRESULT CMainFrame::OnKnownLocation(WORD, WORD id, HWND, BOOL&) {
	static const struct {
		UINT id;
		PCWSTR path;
	} locations[] = {
		{ ID_LOCATIONS_SERVICES, L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Services" },
		{ ID_LOCATIONS_HARDWARE, L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Enum" },
		{ ID_LOCATIONS_CLASS, L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Class" },
		{ ID_LOCATIONS_HIVELIST, L"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\hivelist" },
	};

	for (auto& loc : locations) {
		if (loc.id == id) {
			TreeHelper th(m_Tree);
			auto hItem = th.FindItem(m_hStdReg, loc.path);
			if (hItem) {
				m_Tree.EnsureVisible(hItem);
				m_Tree.SelectItem(hItem);
			}
			else {
				CString text;
				text.Format(L"Location %s not found", loc.path);
				AtlMessageBox(m_hWnd, (PCWSTR)text, IDS_APP_TITLE, MB_ICONWARNING);
			}
			return 0;
		}
	}
	AtlMessageBox(m_hWnd, L"Location not implemented", IDS_APP_TITLE, MB_ICONINFORMATION);
	return 0;
}

LRESULT CMainFrame::OnFindAll(WORD, WORD, HWND, BOOL&) {
	CFindAllDlg dlg(this);
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnKeyPermissions(WORD, WORD, HWND, BOOL&) {
	auto path = GetFullNodePath(m_Tree.GetSelectedItem());
	CRegKey key;
	if (::GetFocus() == m_Tree) {
		auto key2 = Registry::OpenKey(path, KEY_READ | (m_ReadOnly ? 0 : KEY_WRITE));
		key.Attach(key2.Detach());
	}
	else {
		ATLASSERT(::GetFocus() == m_List);
		auto& item = m_Items[m_List.GetSelectionMark()];
		ATLASSERT(item.Key);
		path += L"\\" + item.Name;
		auto key2 = Registry::OpenKey(path, KEY_READ | (m_ReadOnly ? 0 : KEY_WRITE));
		key.Attach(key2.Detach());
	}
	if (!key) {
		DisplayError(L"Failed to open key");
	}
	else {
		CSecurityInformation si((HANDLE)key.m_hKey, path, m_ReadOnly);
		::EditSecurity(m_hWnd, &si);
	}
	return 0;
}

LRESULT CMainFrame::OnNewValue(WORD, WORD id, HWND, BOOL&) {
	static const DWORD types[] = { REG_DWORD, REG_QWORD, REG_SZ, REG_MULTI_SZ, REG_EXPAND_SZ, REG_BINARY };
	ATLASSERT(id - ID_NEW_DWORDVALUE < _countof(types));
	RegistryItem item;
	item.Name = L"NewValue";
	item.Type = types[id - ID_NEW_DWORDVALUE];
	item.Key = false;
	m_Items.push_back(item);
	m_List.SetItemCountEx(m_List.GetItemCount() + 1, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	m_CurrentOperation = Operation::CreateValue;
	m_List.EditLabel((int)m_Items.size() - 1);
	return 0;
}

LRESULT CMainFrame::OnListBeginEdit(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	if (m_ReadOnly)
		return TRUE;

	if (m_CurrentOperation != Operation::None)
		return FALSE;

	auto lv = (NMLVDISPINFO*)pnmh;
	auto& item = m_Items[lv->item.iItem];
	m_CurrentOperation = item.Key ? Operation::RenameKey : Operation::RenameValue;
	return FALSE;
}

LRESULT CMainFrame::OnProperties(WORD, WORD, HWND, BOOL&) {
	if (::GetFocus() == m_List) {
		auto index = m_List.GetSelectionMark();
		if (index >= 0 && !m_Items[index].Key)
			return (LRESULT)ShowValueProperties(m_Items[index]);
	}
	return 0;
}

LRESULT CMainFrame::OnExport(WORD, WORD, HWND, BOOL&) {
	return 0;
}

LRESULT CMainFrame::OnListEndEdit(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	auto lv = (NMLVDISPINFO*)pnmh;
	if (lv->item.pszText == nullptr) {
		// cancelled
		return FALSE;
	}

	std::shared_ptr<AppCommand> cmd;
	auto hItem = m_Tree.GetSelectedItem();
	auto index = lv->item.iItem;
	auto& item = m_Items[index];
	auto path = GetFullNodePath(hItem);
	switch (m_CurrentOperation) {
		case Operation::RenameKey:
			cmd = std::make_shared<RenameKeyCommand>(path, item.Name, lv->item.pszText);
			break;

		case Operation::RenameValue:
			cmd = std::make_shared<RenameValueCommand>(GetFullNodePath(hItem), item.Name, lv->item.pszText);
			break;

		case Operation::CreateValue:
		{
			auto cb = [this](auto cmd, auto) {
				if (m_CurrentOperation == Operation::None && cmd.GetPath() == GetFullNodePath(m_Tree.GetSelectedItem())) {
					UpdateList(true);
				}
				return true;
			};
			cmd = std::make_shared<CreateValueCommand>(path, lv->item.pszText, item.Type, cb);
			break;
		}

		case Operation::CreateKey:
			cmd = std::make_shared<CreateKeyCommand>(path, lv->item.pszText);
			break;
	}
	LRESULT rv = FALSE;
	ATLASSERT(cmd);
	if (cmd) {
		auto op = m_CurrentOperation;
		m_CurrentOperation = Operation::None;
		if (!m_CmdMgr.AddCommand(cmd)) {
			DisplayError(L"Failed to " + cmd->GetCommandName());
		}
		else {
			switch (op) {
				case Operation::CreateValue:
					item.Size = std::static_pointer_cast<CreateValueCommand>(cmd)->GetSize();
					break;

				case Operation::RenameKey:
				case Operation::RenameValue:
					item.Name = lv->item.pszText;
					break;
			}

			m_List.RedrawItems(index, index);
			m_List.SetSelectionMark(index);
			m_List.EnsureVisible(index, FALSE);
			rv = TRUE;
		}
	}
	return rv;
}

void CMainFrame::InitCommandBar() {
	struct {
		UINT id, icon;
		HICON hIcon = nullptr;
	} cmds[] = {
		{ ID_FILE_RUNASADMIN, 0, IconHelper::GetShieldIcon() },
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_TREE_REFRESH, IDI_REFRESH },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ ID_EDIT_UNDO, IDI_UNDO },
		{ ID_EDIT_REDO, IDI_REDO },
		{ ID_EDIT_DELETE, IDI_DELETE },
		{ ID_EDIT_FIND, IDI_FIND },
		{ ID_SEARCH_FINDALL, IDI_FINDALL },
		{ ID_SEARCH_FINDNEXT, IDI_FIND_NEXT },
		{ ID_EDIT_READONLY, IDI_LOCK },
		{ ID_EDIT_RENAME, IDI_RENAME },
		{ ID_NEW_KEY, IDI_FOLDER_NEW },
		{ ID_VIEW_SHOWKEYSINLIST, IDI_FOLDER_VIEW },
		{ ID_KEY_PERMISSIONS, IDI_PERM },
		{ ID_FILE_EXPORT, IDI_EXPORT },
		{ ID_FILE_IMPORT, IDI_IMPORT },
		{ ID_KEY_PROPERTIES, IDI_PROPERTIES },
	};
	for (auto& cmd : cmds) {
		HICON hIcon = cmd.hIcon;
		if (!hIcon) {
			hIcon = AtlLoadIconImage(cmd.icon, 0, 16, 16);
			ATLASSERT(hIcon);
		}
		m_CmdBar.AddIcon(cmd.icon ? hIcon : cmd.hIcon, cmd.id);
	}
}

void CMainFrame::InitToolBar(CToolBarCtrl& tb, int size) {
	CImageList tbImages;
	tbImages.Create(size, size, ILC_COLOR32, 8, 4);
	tb.SetImageList(tbImages);

	const struct {
		UINT id;
		int image;
		BYTE style = BTNS_BUTTON;
		PCWSTR text = nullptr;
	} buttons[] = {
		{ ID_EDIT_READONLY, IDI_LOCK },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_VIEW_SHOWKEYSINLIST, IDI_FOLDER_VIEW },
		{ ID_KEY_PROPERTIES, IDI_PROPERTIES },
		{ 0 },
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ ID_EDIT_DELETE, IDI_DELETE },
		{ 0 },
		{ ID_EDIT_FIND, IDI_FIND },
		{ ID_SEARCH_FINDNEXT, IDI_FIND_NEXT },
		{ ID_SEARCH_FINDALL, IDI_FINDALL },
	};
	for (auto& b : buttons) {
		if (b.id == 0)
			tb.AddSeparator(0);
		else {
			auto hIcon = AtlLoadIconImage(b.image, 0, size, size);
			ATLASSERT(hIcon);
			int image = tbImages.AddIcon(hIcon);
			tb.AddButton(b.id, b.style, TBSTATE_ENABLED, image, b.text, 0);
		}
	}
}

HTREEITEM CMainFrame::BuildTree(HTREEITEM hRoot, HKEY hKey, PCWSTR name) {
	if (name) {
		hRoot = m_Tree.InsertItem(name, 3, 2, hRoot, TVI_LAST);
		auto path = GetFullNodePath(hRoot);
		if (Registry::IsHiveKey(path)) {
			m_Tree.SetItemImage(hRoot, 6, 6);
			SetNodeData(hRoot, GetNodeData(hRoot) | NodeType::Hive);
		}
		auto subkeys = Registry::GetSubKeyCount(hKey);
		if (subkeys) {
			m_Tree.InsertItem(L"\\\\", hRoot, TVI_LAST);
		}
	}
	else {
		CRegKey key(hKey);
		Registry::EnumSubKeys(key, [&](auto name, const auto& ft) {
			auto hItem = m_Tree.InsertItem(name, 3, 2, hRoot, TVI_LAST);
			SetNodeData(hItem, NodeType::Key);
			CRegKey subKey;
			auto error = subKey.Open(hKey, name, KEY_READ);
			if (error == ERROR_SUCCESS) {
				DWORD subkeys = Registry::GetSubKeyCount(subKey);
				if (subkeys) {
					m_Tree.InsertItem(L"\\\\", hItem, TVI_LAST);
				}
				subKey.Close();
				CString linkPath;
				if (Registry::IsKeyLink(key, name, linkPath)) {
					m_Tree.SetItemImage(hItem, 4, 4);
				}
			}
			else if (error == ERROR_ACCESS_DENIED) {
				SetNodeData(hItem, GetNodeData(hItem) | NodeType::AccessDenied);
				m_Tree.SetItemImage(hItem, 5, 5);
			}
			auto path = GetFullNodePath(hItem);
			if (Registry::IsHiveKey(path)) {
				int image = error == ERROR_ACCESS_DENIED ? 7 : 6;
				m_Tree.SetItemImage(hItem, image, image);
				SetNodeData(hItem, GetNodeData(hItem) | NodeType::Hive);
			}
			return true;
			});
		m_Tree.SortChildren(hRoot);
		key.Detach();
	}

	return hRoot;
}

void CMainFrame::InitTree() {
	WCHAR name[32];
	DWORD len = _countof(name);
	::GetComputerName(name, &len);
	m_hLocalRoot = m_Tree.InsertItem(name + CString(L" (Local)"), 1, 1, TVI_ROOT, TVI_LAST);
	m_hLocalRoot.SetData(static_cast<DWORD_PTR>(NodeType::Machine));
	m_hStdReg = m_Tree.InsertItem(L"Standard Registry", 0, 0, m_hLocalRoot, TVI_LAST);
	SetNodeData(m_hStdReg, NodeType::StandardRoot);
	m_hRealReg = m_Tree.InsertItem(L"REGISTRY", 11, 11, m_hLocalRoot, TVI_LAST);
	SetNodeData(m_hRealReg, NodeType::RegistryRoot | NodeType::Predefined);
	m_hLocalRoot.Expand(TVE_EXPAND);
	m_Tree.SelectItem(m_hStdReg);
}

CString CMainFrame::GetNodePath(HTREEITEM hItem, HKEY* pKey) const {
	CString path;
	CString text;
	while (hItem && ((GetNodeData(hItem) & (NodeType::Key | NodeType::Predefined)) == NodeType::Key || GetNodeData(hItem) == NodeType::None)) {
		ATLVERIFY(m_Tree.GetItemText(hItem, text));
		path = text + L"\\" + path;
		hItem = m_Tree.GetParentItem(hItem);
	}
	if (pKey)
		*pKey = GetKeyFromNode(hItem);
	path.TrimRight(L"\\");
	return path;
}

CString CMainFrame::GetParentNodePath(HTREEITEM hItem, HKEY* pKey) const {
	auto path = GetNodePath(hItem, pKey);
	auto index = path.ReverseFind(L'\\');
	if (index < 0)
		return L"";

	return path.Left(index);
}

CString CMainFrame::GetFullNodePath(HTREEITEM hItem) const {
	CString path;
	auto hPrev = hItem;
	CString text;
	while (hItem && ((GetNodeData(hItem) & (NodeType::Predefined | NodeType::Key)) != NodeType::None)) {
		ATLVERIFY(m_Tree.GetItemText(hItem, text));
		path = text + L"\\" + path;
		hPrev = hItem;
		hItem = m_Tree.GetParentItem(hItem);
	}
	path.TrimRight(L"\\");
	if (path.Left(8) == L"REGISTRY")
		path = L"\\" + path;
	return path;
}

CString CMainFrame::GetFullParentNodePath(HTREEITEM hItem) const {
	auto path = GetFullNodePath(hItem);
	auto index = path.ReverseFind(L'\\');
	if (index < 0)
		return L"";

	return path.Left(index);
}

NodeType CMainFrame::GetNodeData(HTREEITEM hItem) const {
	return static_cast<NodeType>(m_Tree.GetItemData(hItem));
}

void CMainFrame::SetNodeData(HTREEITEM hItem, NodeType type) {
	m_Tree.SetItemData(hItem, static_cast<DWORD_PTR>(type));
}

void CMainFrame::ExpandItem(HTREEITEM hItem) {
	CRegKey key;
	auto path = GetNodePath(hItem, &key.m_hKey);
	if (!key)
		return;

	CRegKey subkey;
	subkey.Open(key, path, KEY_READ);
	if (subkey) {
		m_Tree.SetRedraw(FALSE);
		BuildTree(hItem, subkey);
		m_Tree.SetRedraw(TRUE);
	}
}

void CMainFrame::RefreshFull(HTREEITEM hItem) {
	hItem = m_Tree.GetChildItem(hItem);
	TreeHelper th(m_Tree);
	while (hItem) {
		auto state = m_Tree.GetItemState(hItem, TVIS_EXPANDED | TVIS_EXPANDEDONCE);
		if (state) {
			if (state == TVIS_EXPANDEDONCE) {
				CString text;
				if (m_Tree.GetChildItem(hItem) && m_Tree.GetItemText(m_Tree.GetChildItem(hItem), text) && text != L"\\\\") {
					// not expanded now, delete all items and insert a dummy item
					th.DeleteChildren(hItem);
					m_Tree.InsertItem(L"\\\\", hItem, TVI_LAST);
				}
			}
			else {
				// really expanded
				RefreshFull(hItem);
				auto key = Registry::OpenKey(GetFullNodePath(hItem), KEY_READ);
				if (key) {
					auto keys = th.GetChildItems(hItem);
					Registry::EnumSubKeys(key, [&](auto name, const auto&) {
						if (!th.FindChild(hItem, name)) {
							// new sub key
							auto hChild = InsertKeyItem(hItem, name);
						}
						else {
							keys.erase(name);
						}
						return true;
						});
					for (auto& [name, h] : keys)
						m_Tree.DeleteItem(h);

					if (m_Tree.GetChildItem(hItem) == nullptr) {
						// remove children indicator
						TVITEM tvi;
						tvi.hItem = hItem;
						tvi.mask = TVIF_CHILDREN;
						tvi.cChildren = 0;
						m_Tree.SetItem(&tvi);
					}
					else {
						m_Tree.SortChildren(hItem);
					}
				}
			}
		}
		else if (m_Tree.GetChildItem(hItem) == nullptr && (GetNodeData(hItem) & NodeType::AccessDenied) == NodeType::None) {
			// no children - check if new exist
			auto key = Registry::OpenKey(GetFullNodePath(hItem), KEY_READ);
			if (Registry::GetSubKeyCount(key) > 0) {
				m_Tree.InsertItem(L"\\\\", hItem, TVI_LAST);
				TVITEM tvi;
				tvi.hItem = hItem;
				tvi.mask = TVIF_CHILDREN;
				tvi.cChildren = 1;
				m_Tree.SetItem(&tvi);
			}
		}
		hItem = m_Tree.GetNextSiblingItem(hItem);
	}
}

HKEY CMainFrame::GetKeyFromNode(HTREEITEM hItem) const {
	while (hItem && (GetNodeData(hItem) & NodeType::Predefined) != NodeType::Predefined)
		hItem = m_Tree.GetParentItem(hItem);

	CString text;
	m_Tree.GetItemText(hItem, text);
	if (text == L"REGISTRY")
		return Registry::OpenRealRegistryKey();
	for (auto& k : Registry::Keys)
		if (k.text == text)
			return k.hKey;
	return nullptr;
}

CTreeItem CMainFrame::InsertKeyItem(HTREEITEM hParent, PCWSTR name, NodeType type) {
	auto item = m_Tree.InsertItem(name, 3, 2, hParent, TVI_LAST);
	SetNodeData(item, type);
	auto key = Registry::OpenKey(GetFullNodePath(item), KEY_READ);
	if (key) {
		if (Registry::GetSubKeyCount(key) > 0)
			m_Tree.InsertItem(L"\\\\", item, TVI_LAST);
	}
	return item;
}

HTREEITEM CMainFrame::FindItemByPath(PCWSTR path) const {
	CTreeItem item;
	if (path[0] == L'\\') {
		// real registry
		item = m_hRealReg;
	}
	else {
		// standard registry
		item = m_hStdReg;
	}
	CString spath(path);
	int start = 0;
	while (true) {
		auto token = spath.Tokenize(L"\\", start);
		if (token.IsEmpty())
			break;
		TreeHelper th(m_Tree);
		item = th.FindChild(item, token);
	}
	return item;
}

void CMainFrame::InvokeTreeContextMenu(const CPoint& pt) {
	CPoint pt2(pt);
	m_Tree.ClientToScreen(&pt2);
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	UpdateUI();
	m_CmdBar.TrackPopupMenu(menu.GetSubMenu(0), 0, pt2.x, pt2.y);
}

CString CMainFrame::GetKeyDetails(const RegistryItem& item) const {
	CRegKey key;
	if (m_CurrentKey == nullptr)
		key.Attach(Registry::OpenKey(item.Name, KEY_QUERY_VALUE).Detach());
	else
		key.Open(m_CurrentKey, item.Name, KEY_QUERY_VALUE);
	CString text;
	if (key) {
		DWORD values = 0;
		auto subkeys = Registry::GetSubKeyCount(key, &values);
		text.Format(L"Subkeys: %u, Values: %u", subkeys, values);
	}
	return text;
}

CString CMainFrame::GetValueDetails(const RegistryItem& item) const {
	ATLASSERT(!item.Key);
	CString text;
	if (item.Value.IsEmpty())
		item.Value = Registry::GetDataAsString(m_CurrentKey, item);
	switch (item.Type) {
		case REG_EXPAND_SZ:
			if (item.Value.Find(L"%") >= 0) {
				text = Registry::ExpandStrings(item.Value);
			}
			break;

		case REG_SZ:
			if (item.Value[0] == L'@') {
				::RegLoadMUIString(m_CurrentKey, item.Name, text.GetBufferSetLength(512), 512, nullptr, REG_MUI_STRING_TRUNCATE, nullptr);
			}
			break;
	}
	return text;
}

bool CMainFrame::RefreshItem(HTREEITEM hItem) {
	auto expanded = m_Tree.GetItemState(hItem, TVIS_EXPANDED);
	m_Tree.LockWindowUpdate();
	m_Tree.Expand(hItem, TVE_COLLAPSE | TVE_COLLAPSERESET);
	TreeHelper th(m_Tree);
	th.DeleteChildren(hItem);
	m_Tree.InsertItem(L"\\\\", hItem, TVI_LAST);
	if (expanded)
		m_Tree.Expand(hItem, TVE_EXPAND);
	m_Tree.LockWindowUpdate(FALSE);
	UpdateList(true);
	return true;
}

void CMainFrame::DisplayError(PCWSTR msg) {
	CString text;
	text.Format(L"%s (%s)", msg, (PCWSTR)GetErrorText(::GetLastError()));
	AtlMessageBox(m_hWnd, (PCWSTR)text, IDS_APP_TITLE, MB_ICONERROR);
}

CString CMainFrame::GetErrorText(DWORD error) {
	ATLASSERT(error);
	PWSTR buffer;
	CString msg;
	if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, nullptr)) {
		msg = buffer;
		::LocalFree(buffer);
		msg.Trim(L"\n\r");
	}
	return msg;
}

int CMainFrame::GetKeyImage(const RegistryItem& item) const {
	int image = 3;
	if (m_CurrentKey == nullptr)
		return image;

	CString linkPath;
	if (Registry::IsKeyLink(m_CurrentKey, item.Name, linkPath))
		return 4;

	CRegKey key;
	auto error = key.Open(m_CurrentKey, item.Name, KEY_READ);
	if (Registry::IsHiveKey(GetFullNodePath(m_Tree.GetSelectedItem()) + L"\\" + item.Name))
		image = error == ERROR_ACCESS_DENIED ? 7 : 6;
	else if (error == ERROR_ACCESS_DENIED)
		image = 5;

	return image;
}

INT_PTR CMainFrame::ShowValueProperties(RegistryItem& item) {
	auto cb = [this](auto& cmd, bool) {
		if (GetFullNodePath(m_Tree.GetSelectedItem()) == cmd.GetPath()) {
			int index = m_List.FindItem(cmd.GetName(), false);
			ATLASSERT(index >= 0);
			if (index >= 0) {
				m_Items[index].Value.Empty();
				m_Items[index].Size = -1;
				m_List.RedrawItems(index, index);
				m_List.SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
			}
		}
		UpdateUI();
		return true;
	};
	bool success = false;
	bool result = false;
	switch (item.Type) {
		case REG_LINK:
			AtlMessageBox(m_hWnd, L"No special properties available for a symbolic link", IDS_APP_TITLE, MB_ICONINFORMATION);
			return 0;

		case REG_SZ:
		case REG_EXPAND_SZ:
		{
			CStringValueDlg dlg(m_CurrentKey, item.Name, m_ReadOnly);
			result = dlg.DoModal() == IDOK && dlg.IsModified();
			if (result) {
				auto hItem = m_Tree.GetSelectedItem();
				auto cmd = std::make_shared<ChangeValueCommand>(
					GetFullNodePath(hItem), item.Name, dlg.GetType(), (PVOID)(PCWSTR)dlg.GetValue(), (1 + dlg.GetValue().GetLength()) * (LONG)sizeof(WCHAR));
				success = m_CmdMgr.AddCommand(cmd);
				if (success)
					cmd->SetCallback(cb);
			}
			break;
		}

		case REG_MULTI_SZ:
		{
			CMultiStringValueDlg dlg(m_CurrentKey, item.Name, m_ReadOnly);
			result = dlg.DoModal() == IDOK && dlg.IsModified();
			if (result) {
				auto hItem = m_Tree.GetSelectedItem();
				auto value = dlg.GetValue();
				value.TrimRight(L"\r\n");
				value.Replace(L"\r\n", L"\n");
				auto len = value.GetLength();
				for (int i = 0; i < len; i++)
					if (value[i] == L'\n')
						value.SetAt(i, 0);

				auto cmd = std::make_shared<ChangeValueCommand>(
					GetFullNodePath(hItem), item.Name, REG_MULTI_SZ, (PVOID)(PCWSTR)value, (1 + len) * (LONG)sizeof(WCHAR));
				success = m_CmdMgr.AddCommand(cmd);
				if (success)
					cmd->SetCallback(cb);
			}
			break;
		}
		case REG_DWORD:
		case REG_QWORD:
		{
			CNumberValueDlg dlg(m_CurrentKey, item.Name, item.Type, m_ReadOnly);
			result = dlg.DoModal() == IDOK && dlg.IsModified();
			if (result) {
				auto hItem = m_Tree.GetSelectedItem();
				auto value = dlg.GetValue();
				auto cmd = std::make_shared<ChangeValueCommand>(
					GetFullNodePath(hItem), item.Name, item.Type, &value, dlg.GetType() == REG_DWORD ? 4 : 8);
				success = m_CmdMgr.AddCommand(cmd);
				if (success)
					cmd->SetCallback(cb);
			}
			break;
		}
		case REG_BINARY:
		case REG_FULL_RESOURCE_DESCRIPTOR:
		case REG_RESOURCE_REQUIREMENTS_LIST:
		case REG_RESOURCE_LIST:
		{
			CBinaryValueDlg dlg(m_CurrentKey, item.Name, m_ReadOnly, this);
			result = dlg.DoModal() == IDOK && dlg.IsModified();
			if (result) {
				auto hItem = m_Tree.GetSelectedItem();
				auto cmd = std::make_shared<ChangeValueCommand>(
					GetFullNodePath(hItem), item.Name, item.Type, (const PVOID)dlg.GetValue().data(), (LONG)dlg.GetValue().size());
				success = m_CmdMgr.AddCommand(cmd);
				if (success) {
					cmd->SetCallback(cb);
					item.Size = (DWORD)dlg.GetValue().size();
				}
			}
			break;
		}
	}
	if (!result)
		return 0;

	if (!success) {
		DisplayError(L"Failed to changed value");
	}
	else {
		item.Value.Empty();
		auto index = m_List.GetSelectionMark();
		m_List.RedrawItems(index, index);
		UpdateUI();
	}
	return 0;
}

void CMainFrame::UpdateUI() {
	UIEnable(ID_EDIT_UNDO, !m_ReadOnly && m_CmdMgr.CanUndo());
	if (m_CmdMgr.CanUndo())
		UISetText(ID_EDIT_UNDO, L"&Undo " + m_CmdMgr.GetUndoCommand()->GetCommandName() + L"\tCtrl+Z");
	UIEnable(ID_EDIT_REDO, !m_ReadOnly && m_CmdMgr.CanRedo());
	if (m_CmdMgr.CanRedo())
		UISetText(ID_EDIT_REDO, L"&Redo " + m_CmdMgr.GetRedoCommand()->GetCommandName() + L"\tCtrl+Y");

	int listItem = m_List.GetSelectionMark();
	bool listFocus = ::GetFocus() == m_List;
	bool treeFocus = ::GetFocus() == m_Tree;
	auto node = GetNodeData(m_Tree.GetSelectedItem());

	auto properKey = (node & (NodeType::Key | NodeType::Predefined | NodeType::AccessDenied)) == NodeType::Key;
	UIEnable(ID_NEW_KEY, !m_ReadOnly && (node & NodeType::Key) == NodeType::Key);
	if (treeFocus) {
		UIEnable(ID_EDIT_DELETE, !m_ReadOnly && properKey);
		UIEnable(ID_EDIT_CUT, !m_ReadOnly && properKey);
		UIEnable(ID_EDIT_RENAME, !m_ReadOnly && properKey);
		UIEnable(ID_EDIT_COPY, properKey);
		bool allowPaste = !m_ReadOnly && !m_Clipboard.Path.IsEmpty() && (node & NodeType::Key) == NodeType::Key;
		UIEnable(ID_EDIT_PASTE, allowPaste);
		ATLTRACE(L"Allow paste: %d\n", (int)allowPaste);
		UIEnable(ID_KEY_PERMISSIONS, properKey);
	}
	else if (listFocus) {
		UIEnable(ID_EDIT_DELETE, !m_ReadOnly && listItem >= 0);
		UIEnable(ID_EDIT_CUT, !m_ReadOnly && listItem >= 0);
		UIEnable(ID_EDIT_COPY, listItem >= 0);
		UIEnable(ID_KEY_PERMISSIONS, listItem >= 0 && m_Items[listItem].Key && m_Items[listItem].Type != REG_KEY_UP);
		UIEnable(ID_EDIT_RENAME, !m_ReadOnly && listItem >= 0);
	}
	else {
		UIEnable(ID_KEY_PERMISSIONS, FALSE);
	}
	for (auto id = ID_NEW_DWORDVALUE; id <= ID_NEW_BINARYVALUE; id++)
		UIEnable(id, !m_ReadOnly);
	UIEnable(ID_SEARCH_FINDNEXT, m_FindDlg.IsFindNextAvailable());
}

void CMainFrame::UpdateList(bool force) {
	m_Items.clear();
	m_List.SetItemCount(0);

	HKEY hKey;
	auto hItem = m_Tree.GetSelectedItem();
	m_CurrentPath = GetNodePath(hItem, &hKey);
	m_StatusBar.SetText((int)StatusPane::Key, GetFullNodePath(m_Tree.GetSelectedItem()));
	int image;
	m_Tree.GetItemImage(hItem, image, image);
	m_StatusBar.SetIcon((int)StatusPane::Icon, m_Tree.GetImageList(TVSIL_NORMAL).GetIcon(image));

	if (hItem == m_hStdReg && m_Settings.ShowKeysInList()) {
		// special case for root of registry
		for (hItem = m_Tree.GetChildItem(hItem); hItem; hItem = m_Tree.GetNextSiblingItem(hItem)) {
			RegistryItem item;
			CString name;
			m_Tree.GetItemText(hItem, name);
			item.Name = name;
			auto key = Registry::OpenKey(item.Name, KEY_READ);
			if (key) {
				Registry::GetSubKeyCount(key, nullptr, &item.TimeStamp);
			}
			item.Key = true;
			item.Type = REG_KEY;
			m_Items.push_back(std::move(item));
		}
		m_List.SetItemCount(static_cast<int>(m_Items.size()));
		DoSort(GetSortInfo(m_List));

		return;
	}

	if (!hKey)
		return;

	if (m_Settings.ShowKeysInList()) {
		//
		// insert up directory
		//
		RegistryItem up;
		up.Name = L"..";
		up.Type = REG_KEY_UP;
		up.Key = true;
		m_Items.push_back(up);

		CRegKey subKey;
		subKey.Open(hKey, m_CurrentPath, KEY_ENUMERATE_SUB_KEYS);
		if (subKey) {
			Registry::EnumSubKeys(subKey, [&](auto name, const auto& ft) {
				RegistryItem item;
				item.Name = name;
				item.TimeStamp = ft;
				item.Key = true;
				item.Type = REG_KEY;
				m_Items.push_back(std::move(item));
				return true;
				});
		}
	}

	m_CurrentKey.Close();
	m_CurrentKey.Open(hKey, m_CurrentPath, KEY_QUERY_VALUE);
	if (!m_CurrentKey)
		return;

	Registry::EnumKeyValues(m_CurrentKey, [&](auto type, auto name, auto size) {
		RegistryItem item;
		item.Name = name;
		item.Type = type;
		item.Size = size;
		m_Items.push_back(item);
		return true;
		});

	auto parentPath = GetFullParentNodePath(hItem);
	if (!parentPath.IsEmpty()) {
		auto temp = Registry::OpenKey(parentPath, KEY_READ);
		if (temp) {
			CString name, linkPath;
			m_Tree.GetItemText(hItem, name);
			if (Registry::IsKeyLink(temp, name, linkPath)) {
				RegistryItem item;
				item.Name = L"SymbolicLinkName";
				item.Type = REG_LINK;
				item.Size = (linkPath.GetLength() + 1) * sizeof(WCHAR);
				item.Value = linkPath;
				m_Items.push_back(item);
			}
		}
	}

	m_List.SetItemCount(static_cast<int>(m_Items.size()));
	DoSort(GetSortInfo(m_List));
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}
