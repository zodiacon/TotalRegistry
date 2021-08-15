#pragma once

#include "ThemeHelper.h"

class CCustomEdit : public CWindowImpl<CCustomEdit, CEdit> {
public:
	void OnFinalMessage(HWND) override {
		delete this;
	}

	BEGIN_MSG_MAP(CCustomEdit)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
	END_MSG_MAP()

	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
		CClientDC dc(m_hWnd);
		CRect rc;
		GetClientRect(&rc);
		dc.FillSolidRect(&rc, ThemeHelper::GetCurrentTheme()->BackColor);
		return 1;
	}
};
