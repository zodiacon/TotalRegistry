#pragma once

#include "ThemeHelper.h"

class CCustomDialog : public CWindowImpl<CCustomDialog> {
public:
	BEGIN_MSG_MAP(CCustomDialog)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnStaticColor)
		MESSAGE_HANDLER(WM_CTLCOLORDLG, OnStaticColor)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, OnStaticColor)
		MESSAGE_HANDLER(WM_CTLCOLORSCROLLBAR, OnScrollBarColor)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnEditColor)
	END_MSG_MAP()

	void Init() {
	}

	LRESULT OnScrollBarColor(UINT, WPARAM wp, LPARAM lp, BOOL&) {
		return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
	}

	LRESULT OnDialogColor(UINT, WPARAM wp, LPARAM lp, BOOL&) {
		if (ThemeHelper::IsDefault())
			return DefWindowProc();
		return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
	}

	LRESULT OnStaticColor(UINT, WPARAM wp, LPARAM lp, BOOL& handled) {
		auto theme = ThemeHelper::GetCurrentTheme();
		if (theme == nullptr || theme->IsDefault()) {
			return DefWindowProc();
		}
		CDCHandle dc((HDC)wp);
		dc.SetBkMode(OPAQUE);
		dc.SetTextColor(theme->TextColor);
		dc.SetBkColor(theme->BackColor);
		return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
	}

	LRESULT OnEditColor(UINT, WPARAM wp, LPARAM lp, BOOL&) {
		auto theme = ThemeHelper::GetCurrentTheme();
		if (theme == nullptr || theme->IsDefault()) {
			return DefWindowProc();
		}
		CDCHandle dc((HDC)wp);
		dc.SetBkMode(OPAQUE);
		dc.SetTextColor(theme->TextColor);
		dc.SetBkColor(theme->BackColor);
		return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
	}

};
