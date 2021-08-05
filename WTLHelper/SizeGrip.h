#pragma once

#include "ThemeHelper.h"

class CSizeGrip : public CWindowImpl<CSizeGrip, CScrollBar> {
public:
	BEGIN_MSG_MAP(CSizeGrip)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	void OnFinalMessage(HWND) override {
		delete this;
	}

	static void DrawSizeGrip(CWindow win, CRect& rc) {
		CClientDC dc(win);
		auto color = ThemeHelper::GetCurrentTheme()->TextColor;
		dc.FillSolidRect(&rc, ThemeHelper::GetCurrentTheme()->BackColor);
		CPoint start(rc.left + 5, rc.top + 5);

		for (int y = 0; y < 3; y++) {
			for (int x = 0; x < 3; x++) {
				if (x + y < 2)
					continue;
				CRect r(CPoint(start.x + 4 * x, start.y + 4 * y), CSize(2, 2));
				dc.FillSolidRect(&r, color);
			}
		}
		win.ValidateRect(nullptr);
	}

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
		auto theme = ThemeHelper::GetCurrentTheme();
		if (theme && theme->IsDefault()) {
			bHandled = FALSE;
			return 0;
		}

		if (GetStyle() & (SBS_SIZEBOX | SBS_SIZEGRIP)) {
			CRect rc;
			GetClientRect(&rc);
			DrawSizeGrip(m_hWnd, rc);
		}
		else {
			bHandled = FALSE;
		}

		return 0;
	}
};
