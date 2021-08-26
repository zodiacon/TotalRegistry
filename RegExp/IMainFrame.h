#pragma once

struct AppSettings;

struct IMainFrame abstract {
	virtual AppSettings& GetSettings() = 0;
	virtual void OnFindStart() = 0;
	virtual void OnFindNext(PCWSTR path, PCWSTR name, PCWSTR data) = 0;
	virtual void OnFindEnd(bool cancelled) = 0;
	virtual bool GoToItem(PCWSTR path, PCWSTR name, PCWSTR data) = 0;
	virtual CString GetCurrentKeyPath() = 0;
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y, HWND hWnd = nullptr) = 0;
	virtual HWND GetHwnd() const = 0;
	virtual void DisplayError(PCWSTR text, HWND hWnd = nullptr, DWORD err = ::GetLastError()) const = 0;
	virtual bool AddMenu(HMENU hMenu) = 0;
	virtual HTREEITEM GotoKey(CString const&, bool knownToExist) = 0;
};
