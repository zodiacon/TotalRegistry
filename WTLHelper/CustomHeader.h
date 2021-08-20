#pragma once

#include "Theme.h"
#include "ThemeHelper.h"

class CCustomHeader : 
	public CWindowImpl<CCustomHeader, CHeaderCtrl> {
public:
	BEGIN_MSG_MAP(CCustomHeader)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()


	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
		if (ThemeHelper::IsDefault()) {
			bHandled = FALSE;
			return 0;
		}
		return 1;
	}

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
		if (ThemeHelper::IsDefault()) {
			bHandled = FALSE;
			return 0;
		}
		DefWindowProc();
		CClientDC dc(*this);
		CRect rc;
		GetClientRect(&rc);
		RECT rcItem;
		if (GetItemCount()) {
			GetItemRect(GetItemCount() - 1, &rcItem);
			rc.left = rcItem.right;
			if (rc.right > rc.left)
				dc.FillSolidRect(&rc, ThemeHelper::GetCurrentTheme()->BackColor);
		}
		return 0;
	}
};

class CCustomHeaderParent :
	public CWindowImpl<CCustomHeaderParent>,
	public CCustomDraw<CCustomHeaderParent> {
public:
	BEGIN_MSG_MAP(CCustomHeaderParent)
		CHAIN_MSG_MAP(CCustomDraw<CCustomHeaderParent>)
	END_MSG_MAP()

	void Init(HWND hWnd) {
		m_Header.SubclassWindow(hWnd);
	}

	void OnFinalMessage(HWND) override {
		delete this;
	}

	DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW cd) {
		if (ThemeHelper::IsDefault())
			return CDRF_DODEFAULT;

		if (cd->hdr.hwndFrom != m_Header) {
			SetMsgHandled(FALSE);
			return CDRF_DODEFAULT;
		}
		CDCHandle dc((HDC)cd->hdc);
		dc.FillSolidRect(&cd->rc, ThemeHelper::GetCurrentTheme()->BackColor);
		return CDRF_NOTIFYITEMDRAW;
	}

	DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW cd) {
		HDITEM item;
		item.mask = HDI_TEXT | HDI_FORMAT;
		WCHAR text[64];
		item.pszText = text;
		item.cchTextMax = _countof(text);
		m_Header.GetItem((int)cd->dwItemSpec, &item);

		CDCHandle dc(cd->hdc);
		dc.SelectStockPen(WHITE_PEN);
		CRect rc(cd->rc);
		rc.bottom -= 2;
		bool isSortinColumn = item.fmt & (HDF_SORTUP | HDF_SORTDOWN);
		auto color = ThemeHelper::GetCurrentTheme()->BackColor;
		dc.FillSolidRect(&rc, isSortinColumn ? RGB(32, 0, 0) : color);

		if (cd->dwItemSpec != 0) {
			CPen pen;
			pen.CreatePen(PS_SOLID, 1, RGB(64, 64, 64));
			auto hOldPen = dc.SelectPen(pen);
			dc.MoveTo(rc.left, rc.top);
			dc.LineTo(rc.left, rc.bottom);
			dc.SelectPen(hOldPen);
		}
		dc.MoveTo(rc.right, rc.top);
		dc.LineTo(rc.right, rc.bottom);

		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(ThemeHelper::GetCurrentTheme()->TextColor);
		rc = cd->rc;
		rc.left += 6;

		DWORD fmt = DT_LEFT;
		if ((item.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_RIGHT)
			fmt = DT_RIGHT;
		else if ((item.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_CENTER)
			fmt = DT_CENTER;

		rc.DeflateRect(4, 0);
		dc.DrawText(text, -1, &rc, fmt | DT_VCENTER | DT_SINGLELINE);

		// draw sort indicator (if any)
		if (item.fmt & (HDF_SORTUP | HDF_SORTDOWN)) {
			auto pt = rc.CenterPoint();
			auto up = item.fmt & HDF_SORTUP;
			pt.y = up ? rc.top : rc.top + 4;
			int offset = up ? 4 : -4;
			dc.MoveTo(pt);
			dc.LineTo(pt.x - offset, pt.y + offset);
			dc.MoveTo(pt);
			dc.LineTo(pt.x + offset, pt.y + offset);
		}
		return CDRF_SKIPDEFAULT;
	}

	CCustomHeader m_Header;
};
