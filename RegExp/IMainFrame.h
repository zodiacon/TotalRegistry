#pragma once

struct AppSettings;

struct IMainFrame abstract {
	virtual AppSettings& GetSettings() = 0;
	virtual void OnFindStart() = 0;
	virtual void OnFindNext(PCWSTR path, PCWSTR name, PCWSTR data) = 0;
	virtual void OnFindEnd(bool cancelled) = 0;
	virtual bool GoToItem(PCWSTR path, PCWSTR name, PCWSTR data) = 0;
	virtual CString GetCurrentKeyPath() = 0;
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) = 0;
	virtual HWND GetHwnd() const = 0;
};
