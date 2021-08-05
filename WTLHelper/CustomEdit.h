#pragma once

#include "ThemeHelper.h"

class CCustomEdit : public CWindowImpl<CCustomEdit, CEdit> {
public:
	void OnFinalMessage(HWND) override {
		delete this;
	}

	BEGIN_MSG_MAP(CCustomEdit)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
		CDCHandle dc((HDC)wParam);
		CRect rc;
		GetClientRect(&rc);
		dc.FillSolidRect(&rc, 0);
		return 1;
	}

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
		CPaintDC dc(m_hWnd);
		dc.SetTextColor(255);
		dc.SetBkMode(TRANSPARENT);
		CString text;
		GetWindowText(text);
		dc.TextOut(0, 0, text, text.GetLength());
		return 0;
	}

};
