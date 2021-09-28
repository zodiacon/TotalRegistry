#pragma once

#pragma once

#include "ThemeHelper.h"

class CCustomButtonParent :
	public CWindowImpl<CCustomButtonParent>,
	public CCustomDraw<CCustomButtonParent> {
public:
	void OnFinalMessage(HWND) override {
		delete this;
	}

	BEGIN_MSG_MAP(CCustomButtonParent)
		CHAIN_MSG_MAP(CCustomDraw<CCustomButtonParent>)
	END_MSG_MAP()

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd) {
		return ThemeHelper::IsDefault() ? CDRF_DODEFAULT : CDRF_NOTIFYITEMDRAW;
	}

	DWORD OnPreErase(int, LPNMCUSTOMDRAW cd) {
		return ThemeHelper::IsDefault() ? CDRF_DODEFAULT : CDRF_NOTIFYPOSTERASE;
	}

	DWORD OnPostErase(int, LPNMCUSTOMDRAW cd) {
		CDCHandle dc(cd->hdc);
		auto theme = ThemeHelper::GetCurrentTheme();
		dc.FillSolidRect(&cd->rc, (cd->uItemState & (CDIS_DISABLED | CDIS_GRAYED)) ? ::GetSysColor(COLOR_GRAYTEXT) : theme->BackColor);
		dc.DrawEdge(&cd->rc, (cd->uItemState & CDIS_SELECTED) ? EDGE_BUMP : EDGE_SUNKEN, BF_RECT);

		return CDRF_DODEFAULT;
	}

};
