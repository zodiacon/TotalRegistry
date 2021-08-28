#pragma once

#include "VirtualListView.h"
#include "Registry.h"
#include "AppSettings.h"
#include "CommandManager.h"
#include "FindDlg.h"
#include "IMainFrame.h"
#include "EnumStrings.h"
#include "DeleteKeyCommand.h"
#include "DeleteValueCommand.h"
#include "OwnerDrawnMenu.h"
#include "Theme.h"
#include "LocationManager.h"
#include "KeysHandlesDlg.h"

enum class NodeType {
	None = 0,
	Key = 1,
	Predefined = 2,
	RegistryRoot = 0x10,
	StandardRoot = 0x20,
	Hive = 0x40,
	Link = 0x80,
	Loaded = 0x100,
	Machine = 0x200,
	AccessDenied = 0x400,
	RemoteRegistry = 0x800,
};
DEFINE_ENUM_FLAG_OPERATORS(NodeType);

class CMainFrame :
	public CFrameWindowImpl<CMainFrame>,
	public CAutoUpdateUI<CMainFrame>,
	public CVirtualListView<CMainFrame>,
	public CCustomDraw<CMainFrame>,
	public COwnerDraw<CMainFrame>,
	public CDwmImpl<CMainFrame>,
	public IMainFrame,
	public CMessageFilter,
	public CIdleHandler {
public:
	DECLARE_FRAME_WND_CLASS(L"RegExpWndClass", IDR_MAINFRAME)

	CMainFrame() : m_FindDlg(this), m_HandlesDlg(this), m_AddressBar(this, 2), m_Menu(this) {}

	const UINT WM_BUILD_TREE = WM_APP + 11;
	const UINT WM_FIND_UPDATE = WM_APP + 12;
	const UINT WM_RUN = WM_APP + 13;
	const UINT TreeId = 123;

	BOOL PreTranslateMessage(MSG* pMsg) override;
	BOOL OnIdle() override;

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

	void RunOnUiThread(std::function<void()> f);
	void SetStartKey(const CString& key);
	void SetStatusText(PCWSTR text);

	// IMainFrame
	HWND GetHwnd() const override;
	AppSettings& GetSettings() override;
	void OnFindNext(PCWSTR path, PCWSTR name, PCWSTR data) override;
	void OnFindStart();
	void OnFindEnd(bool cancelled);
	bool GoToItem(PCWSTR path, PCWSTR name, PCWSTR data) override;
	BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y, HWND hWnd = nullptr) override;
	CString GetCurrentKeyPath() override;
	void DisplayError(PCWSTR msg, HWND hWnd = nullptr, DWORD error = ::GetLastError()) const override;
	bool AddMenu(HMENU hMenu) override;

	CString GetColumnText(HWND, int row, int col) const;

	int GetRowImage(HWND, int row) const;
	void DoSort(const SortInfo* si);
	bool IsSortable(HWND, int col) const;
	BOOL OnRightClickList(HWND, int row, int col, const POINT&);
	BOOL OnDoubleClickList(HWND, int row, int col, const POINT& pt);

	void DrawItem(LPDRAWITEMSTRUCT dis);

	enum {
		ID_LOCATION_FIRST = 1500
	};

	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		NOTIFY_CODE_HANDLER(NM_SETFOCUS, OnFocusChanged)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnTreeItemExpanding)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnTreeSelChanged)
		NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnListItemChanged)
		NOTIFY_CODE_HANDLER(TVN_BEGINLABELEDIT, OnTreeBeginEdit)
		NOTIFY_CODE_HANDLER(TVN_ENDLABELEDIT, OnTreeEndEdit)
		NOTIFY_CODE_HANDLER(LVN_ENDLABELEDIT, OnListEndEdit)
		NOTIFY_CODE_HANDLER(LVN_BEGINLABELEDIT, OnListBeginEdit)
		NOTIFY_HANDLER(TreeId, NM_RCLICK, OnTreeContextMenu)
		NOTIFY_CODE_HANDLER(TVN_KEYDOWN, OnTreeKeyDown)
		NOTIFY_CODE_HANDLER(LVN_KEYDOWN, OnListKeyDown)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_COPYDATA, OnGoToKeyExternal)
		MESSAGE_HANDLER(WM_BUILD_TREE, OnBuildTree)
		MESSAGE_HANDLER(WM_FIND_UPDATE, OnFindUpdate)
		MESSAGE_HANDLER(WM_RUN, OnRunOnUIThread)
		MESSAGE_HANDLER(WM_MENUSELECT, OnMenuSelect)
		COMMAND_ID_HANDLER(ID_FILE_RUNASADMIN, OnRunAsAdmin)
		COMMAND_ID_HANDLER(IDM_EXIT, OnExit)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		COMMAND_ID_HANDLER(ID_OPTIONS_SHOWEXTRAHIVES, OnShowExtraHives)
		COMMAND_ID_HANDLER(ID_EDIT_READONLY, OnEditReadOnly)
		COMMAND_ID_HANDLER(ID_OPTIONS_ALWAYSONTOP, OnAlwaysOnTop)
		COMMAND_ID_HANDLER(ID_VIEW_SHOWKEYSINLIST, OnShowKeysInList)
		COMMAND_ID_HANDLER(ID_NEW_KEY, OnNewKey)
		COMMAND_RANGE_HANDLER(ID_NEW_DWORDVALUE, ID_NEW_BINARYVALUE, OnNewValue)
		COMMAND_ID_HANDLER(ID_TOOLS_SCANKEYHANDLES, OnShowKeysHandles)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_EDIT_CUT, OnEditCut)
		COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnEditPaste)
		COMMAND_ID_HANDLER(ID_EDIT_FIND, OnEditFind)
		COMMAND_ID_HANDLER(ID_EDIT_RENAME, OnEditRename)
		COMMAND_ID_HANDLER(ID_EDIT_DELETE, OnEditDelete)
		COMMAND_ID_HANDLER(ID_TREE_REFRESH, OnTreeRefresh)
		COMMAND_ID_HANDLER(ID_SEARCH_FINDNEXT, OnSearchFindNext)
		COMMAND_ID_HANDLER(ID_EDIT_UNDO, OnEditUndo)
		COMMAND_ID_HANDLER(ID_EDIT_REDO, OnEditRedo)
		COMMAND_ID_HANDLER(ID_COPY_FULLNAME, OnCopyFullKeyName)
		COMMAND_ID_HANDLER(ID_COPY_NAME, OnCopyKeyName)
		COMMAND_ID_HANDLER(ID_SEARCH_FINDALL, OnFindAll)
		COMMAND_RANGE_HANDLER(ID_LOCATION_FIRST, ID_LOCATION_FIRST + 49, OnKnownLocation)
		COMMAND_ID_HANDLER(ID_KEY_PERMISSIONS, OnKeyPermissions)
		COMMAND_ID_HANDLER(ID_KEY_PROPERTIES, OnProperties)
		COMMAND_ID_HANDLER(ID_KEY_GOTO, OnGotoKey)
		COMMAND_ID_HANDLER(ID_EDIT_ADDRESSBAR, OnEditAddressBar)
		COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnExport)
		COMMAND_ID_HANDLER(ID_FILE_IMPORT, OnImport)
		COMMAND_ID_HANDLER(ID_FILE_CONNECTREMOTEREGISTRY, OnConnectRemote)
		COMMAND_ID_HANDLER(ID_FILE_DISCONNECT, OnDisconnectRemote)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAbout)
		COMMAND_ID_HANDLER(ID_OPTIONS_REPLACEREGEDIT, OnReplaceRegEdit)
		COMMAND_ID_HANDLER(ID_OPTIONS_DARKMODE, OnDarkMode)
		COMMAND_ID_HANDLER(ID_OPTIONS_ALLOWSINGLEINSTANCE, OnSingleInstance)
		COMMAND_ID_HANDLER(ID_FILE_LOADHIVE, OnLoadHive)
		COMMAND_ID_HANDLER(ID_FILE_UNLOADHIVE, OnUnloadHive)
		COMMAND_ID_HANDLER(ID_VIEW_ADDRESSBAR, OnViewAddressBar)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUSBAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_OPTIONS_FONT, OnOptionsFont)
		MESSAGE_HANDLER(WM_SHOWWINDOW, OnShowWindow)
		COMMAND_ID_HANDLER(ID_OPTIONS_RESTOREDEFAULTFONT, OnRestoreDefaultFont)
		CHAIN_MSG_MAP_MEMBER(m_Menu)
		CHAIN_MSG_MAP(CAutoUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CCustomDraw<CMainFrame>)
		CHAIN_MSG_MAP(COwnerDraw<CMainFrame>)
		CHAIN_MSG_MAP(CVirtualListView<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		REFLECT_NOTIFICATIONS_EX()
	ALT_MSG_MAP(2)
		MESSAGE_HANDLER(WM_KEYDOWN, OnEditKeyDown)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class StatusPane {
		Icon,
		Key
	};
	struct FindData {
		PCWSTR Path;
		PCWSTR Name;
		PCWSTR Data;
	};

	enum class ColumnType {
		Name, Type, Value, Details, Size, TimeStamp
	};
	enum class Operation {
		None,
		RenameKey,
		CreateKey,
		CreateValue,
		RenameValue,
	};
	enum class ClipboardOperation {
		Copy, Cut
	};
	struct ClipboardItem {
		CString Path;
		CString Name;
		bool Key;
	};

	struct LocalClipboard {
		ClipboardOperation Operation;
		std::vector<ClipboardItem> Items;
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnShowWindow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMenuSelect(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFindUpdate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBuildTree(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTreeSelChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRunAsAdmin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTreeItemExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnShowExtraHives(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShowKeysInList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAlwaysOnTop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFocusChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNewKey(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditReadOnly(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTreeBeginEdit(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTreeEndEdit(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnEditUndo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditRedo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTreeContextMenu(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTreeKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnListKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnEditFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRunOnUIThread(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSearchFindNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditCut(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditPaste(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTreeRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopyFullKeyName(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopyKeyName(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnKnownLocation(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFindAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnKeyPermissions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListEndEdit(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnListBeginEdit(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoadHive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnUnloadHive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnReplaceRegEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSingleInstance(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnGotoKey(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnGoToKeyExternal(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditKeyDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditAddressBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewAddressBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnConnectRemote(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDisconnectRemote(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOptionsFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRestoreDefaultFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShowKeysHandles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void InitCommandBar();
	void InitToolBar(CToolBarCtrl& tb, int size = 24);
	HTREEITEM BuildTree(HTREEITEM hRoot, HKEY hKey, PCWSTR name = nullptr);
	void InitTree();
	CString GetFullNodePath(HTREEITEM hItem) const;
	CString GetFullParentNodePath(HTREEITEM hItem) const;
	NodeType GetNodeData(HTREEITEM hItem) const;
	void SetNodeData(HTREEITEM hItem, NodeType type);
	void ExpandItem(HTREEITEM hItem);
	void RefreshFull(HTREEITEM hItem);
	HKEY GetKeyFromNode(HTREEITEM hItem) const;
	CTreeItem InsertKeyItem(HTREEITEM hParent, PCWSTR name, NodeType type = NodeType::Key);
	HTREEITEM FindItemByPath(PCWSTR path) const;
	void InvokeTreeContextMenu(const CPoint& pt);
	CString GetKeyDetails(const RegistryItem& item) const;
	CString GetValueDetails(const RegistryItem& item) const;
	bool RefreshItem(HTREEITEM hItem);
	void DisplayBackupRestorePrivilegeError();
	int GetKeyImage(const RegistryItem& item) const;
	INT_PTR ShowValueProperties(RegistryItem& item, int index);
	void SetDarkMode(bool dark);
	HTREEITEM GotoKey(const CString& path, bool knownToExist = false);
	void ShowBand(int index, bool show);
	void InitDarkTheme();
	void InitLocations();
	HTREEITEM BuildKeyPath(const CString& path, bool accessible);

	AppCommandCallback<DeleteKeyCommand> GetDeleteKeyCommandCallback();

	void UpdateUI();
	void UpdateList(bool force = false);

	CommandManager m_CmdMgr;
	LocationManager m_Locations;
	LocalClipboard m_Clipboard;
	mutable RegistryKey m_CurrentKey;
	CString m_CurrentPath;
	CSplitterWindow m_MainSplitter;
	CMultiPaneStatusBarCtrl m_StatusBar;
	CListViewCtrl m_List;
	CContainedWindowT<CEdit> m_AddressBar;
	COwnerDrawnMenu<CMainFrame> m_Menu;
	mutable CTreeViewCtrlEx m_Tree;
	CListViewCtrl m_Details;
	std::vector<RegistryItem> m_Items;
	CTreeItem m_hLocalRoot, m_hStdReg, m_hRealReg;
	NodeType m_CurrentNodeType{ NodeType::None };
	int m_CurrentSelectedItem{ -1 };
	AppSettings m_Settings;
	Operation m_CurrentOperation{ Operation::None };
	CFindDlg m_FindDlg;
	HANDLE m_hSingleInstMutex{ nullptr };
	CString m_StartKey;
	CComObject<CEnumStrings>* m_AutoCompleteStrings{ nullptr };
	Theme m_DarkTheme, m_DefaultTheme{ true };
	CFont m_Font;
	CKeysHandlesDlg m_HandlesDlg;
	CString m_StatusText;
	bool m_ReadOnly{ true };
	bool m_UpdateNoDelay{ false };
};
