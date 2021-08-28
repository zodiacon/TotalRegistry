#pragma once

#include "ThemeHelper.h"

class CCustomRebar : public CWindowImpl<CCustomRebar, CReBarCtrl> {
public:
	void OnFinalMessage(HWND) override {
		delete this;
	}

	BEGIN_MSG_MAP(CCustomRebar)
		//MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnColorEdit)
	END_MSG_MAP()
	
	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM wp, LPARAM lParam, BOOL& bHandled) {
		if (ThemeHelper::IsDefault()) {
			bHandled = FALSE;
			return 0;
		}
		CDCHandle dc((HDC)wp);
		CRect rc;
		GetClientRect(&rc);
		dc.FillRect(&rc, ::GetSysColorBrush(COLOR_WINDOW));
		return 1;
	}

	LRESULT OnColorEdit(UINT /*uMsg*/, WPARAM wp, LPARAM lParam, BOOL& /*bHandled*/) {
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
