#pragma once

#include "ThemeHelper.h"
#include "SizeGrip.h"

class CCustomStatusBar : public CWindowImpl<CCustomStatusBar, CStatusBarCtrl> {
public:
	BEGIN_MSG_MAP(CCustomStatusBar)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	void OnFinalMessage(HWND) override {
		delete this;
	}

	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
		auto theme = ThemeHelper::GetCurrentTheme();
		if (theme == nullptr || theme->IsDefault()) {
			bHandled = FALSE;
			return 0;
		}
		CDCHandle dc((HDC)wParam);
		CRect rc;
		GetClientRect(&rc);
		dc.FillSolidRect(&rc, theme->BackColor);

		return 1;
	}

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
		auto theme = ThemeHelper::GetCurrentTheme();
		if (theme == nullptr || theme->IsDefault()) {
			bHandled = FALSE;
			return 0;
		}
		DefWindowProc();
		CRect rc;
		GetClientRect(&rc);
		rc.left = rc.right - 20;
		rc.top = rc.bottom - 20;
		CSizeGrip::DrawSizeGrip(*this, rc);

		return 0;
	}
};
