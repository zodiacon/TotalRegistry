#pragma once

struct Theme {
	explicit Theme(bool def = false);

	bool IsDefault() const;

	CString Name{ L"Default" };
	COLORREF BackColor{ ::GetSysColor(COLOR_WINDOW) };
	COLORREF TextColor{ ::GetSysColor(COLOR_WINDOWTEXT) };

	struct {
		COLORREF TextColor{ ::GetSysColor(COLOR_WINDOWTEXT) };
		COLORREF BackColor{ ::GetSysColor(COLOR_MENU) };
		COLORREF SelectionTextColor{ ::GetSysColor(COLOR_MENUTEXT) };
		COLORREF SelectionBackColor{ ::GetSysColor(COLOR_MENUHILIGHT) };
		COLORREF SeparatorColor{ ::GetSysColor(COLOR_GRAYTEXT) };
	} Menu;

	COLORREF SysColors[32];

	HBRUSH GetSysBrush(int index) const;
	COLORREF GetSysColor(int index) const;

private:
	mutable CBrush _SysBrush[32];
	bool _default;
};
