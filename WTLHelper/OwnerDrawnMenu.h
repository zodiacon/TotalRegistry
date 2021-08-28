#pragma once

#include <unordered_map>

struct COwnerDrawnMenuBase {
	void SetTextColor(COLORREF color);
	void SetBackColor(COLORREF color);
	void SetSelectionTextColor(COLORREF color);
	void SetSelectionBackColor(COLORREF color);
	void SetSeparatorColor(COLORREF color);
	void UpdateMenu(CMenuHandle menu, bool subMenus = false);
	void AddCommand(UINT id, HICON hIcon);
	void AddCommand(UINT id, UINT iconId);
	bool AddMenu(HMENU hMenu);
	void AddSubMenu(CMenuHandle menu);
	void SetCheckIcon(HICON hicon);

protected:
	int m_Width{ 0 };
	struct ItemData {
		CString Text;
		int Image;
	};
	std::unordered_map<UINT, ItemData> m_Items;
	CImageList m_Images;
	COLORREF m_TextColor{ RGB(0, 0, 0) }, m_BackColor{ ::GetSysColor(COLOR_MENUBAR) };
	COLORREF m_SelectionBackColor{ RGB(0, 48, 192) }, m_SelectionTextColor{ RGB(255, 255, 255) };
	COLORREF m_SeparatorColor{ RGB(64, 64, 64) };
	int m_LastHeight{ 16 };
	int m_CheckIcon{ -1 };
	enum { TopLevelMenu = 111, Separator = 100 };
};

template<typename T>
struct COwnerDrawnMenu : COwnerDrawnMenuBase {
	BEGIN_MSG_MAP(COwnerDrawnMenu)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_MEASUREITEM, OnMeasureItem)
	END_MSG_MAP()

	LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
		m_pT->SetMsgHandled(TRUE);
		DrawItem((LPDRAWITEMSTRUCT)lParam);
		bHandled = m_pT->IsMsgHandled();
		return TRUE;
	}

	LRESULT OnMeasureItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
		m_pT->SetMsgHandled(TRUE);
		MeasureItem((LPMEASUREITEMSTRUCT)lParam);
		bHandled = m_pT->IsMsgHandled();
		return TRUE;
	}

	explicit COwnerDrawnMenu(T* pT) : m_pT(pT) {
		m_Images.Create(16, 16, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 16, 8);
	}

	BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y, HWND hWnd = nullptr) {
		AddSubMenu(hMenu);
		return ::TrackPopupMenu(hMenu, flags, x, y, 0, hWnd ? hWnd : m_pT->m_hWnd, nullptr);
	}

	void DrawSeparator(CDCHandle dc, CRect& rc) {
		dc.FillSolidRect(&rc, m_BackColor);
		CPen pen;
		pen.CreatePen(PS_SOLID, 1, m_SeparatorColor);
		dc.MoveTo(rc.left + 8, rc.top + rc.Height() / 2);
		dc.SelectPen(pen);
		dc.LineTo(rc.right - 8, rc.top + rc.Height() / 2);
	}

	void DrawItem(LPDRAWITEMSTRUCT dis) {
		if (dis->CtlType != ODT_MENU || !::IsMenu((HMENU)dis->hwndItem)) {
			m_pT->SetMsgHandled(FALSE);
			return;
		}

		CDCHandle dc(dis->hDC);
		CRect rc(dis->rcItem);
		if (dis->itemData == Separator) {
			DrawSeparator(dc, rc);
			return;
		}

		bool enabled = (dis->itemState & (ODS_DISABLED | ODS_GRAYED)) == 0;

		dc.FillSolidRect(&rc, (dis->itemState & ODS_SELECTED) ? m_SelectionBackColor : m_BackColor);
		rc.OffsetRect(2, 2);
		rc.right = rc.left + 16;
		rc.bottom = rc.top + 16;

		auto it = m_Items.find(dis->itemID);
		if (it != m_Items.end()) {
			auto& data = it->second;
			if (data.Image >= 0) {
				m_Images.DrawEx(data.Image, dis->hDC, rc, CLR_NONE, CLR_NONE, ILD_NORMAL);
				if (dis->itemState & ODS_CHECKED) {
					rc.InflateRect(2, 2);
					CBrush brush;
					brush.CreateSolidBrush(m_TextColor);
					dc.FrameRect(&rc, brush);
				}
			}
			else if (dis->itemState & ODS_CHECKED) {
				// draw a checkmark
				m_Images.DrawEx(m_CheckIcon, dis->hDC, rc, CLR_NONE, CLR_NONE, ILD_NORMAL);
			}
		}
		else if (dis->itemState & ODS_CHECKED) {
			// draw a checkmark
			m_Images.DrawEx(0, dis->hDC, rc, CLR_NONE, CLR_NONE, ILD_NORMAL);
		}
		CMenuHandle menu((HMENU)dis->hwndItem);
		ATLASSERT(menu.IsMenu());
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_SUBMENU;
		if (dis->itemData != TopLevelMenu && menu.GetMenuItemInfo(dis->itemID, FALSE, &mii) && mii.hSubMenu) {
			CRect rc(dis->rcItem);
			rc.DeflateRect(10, 6);
			rc.left = rc.right - 10;
			if (enabled) {
				HBRUSH brush = ::GetSysColorBrush(COLOR_GRAYTEXT);
				POINT pt[] = { { rc.left, rc.top }, { rc.left, rc.bottom }, { rc.right, rc.top + rc.Height() / 2 } };
				CPen pen;
				pen.CreatePen(PS_SOLID, 1, m_TextColor);
				dc.SelectPen(pen);
				dc.SelectBrush(brush);
				dc.Polygon(pt, _countof(pt));
				rc.right = dis->rcItem.right;
			}
			dc.ExcludeClipRect(&rc);
		}

		WCHAR mtext[256];
		auto text = m_pT->UIGetText(dis->itemID);
		if (text == nullptr)
			if (menu.GetMenuString(dis->itemID, mtext, _countof(mtext), MF_BYCOMMAND))
				text = mtext;

		if (text && text[0]) {
			if (it != m_Items.end()) {
				it->second.Text = text;
			}
			rc = dis->rcItem;
			if (dis->itemData != TopLevelMenu) {
				rc.left += 24;
				rc.right -= 8;
			}
			if (dis->itemState & ODS_DISABLED)
				dc.SetTextColor(RGB(128, 128, 128));
			else
				dc.SetTextColor((dis->itemState & ODS_SELECTED) ? m_SelectionTextColor : m_TextColor);
			dc.SetBkMode(TRANSPARENT);
			if (dis->itemData == TopLevelMenu) {
				dc.DrawText(text, -1, &rc, DT_VCENTER | DT_SINGLELINE | DT_CENTER);
			}
			else {
				CString stext(text);
				auto tab = stext.Find(L'\t');
				if (tab >= 0)
					stext.SetAt(tab, 0);
				dc.DrawText(stext, -1, &rc, DT_VCENTER | DT_SINGLELINE);
				if (tab >= 0)
					dc.DrawText(stext.Mid(tab + 1), -1, &rc, DT_VCENTER | DT_SINGLELINE | DT_RIGHT);
			}
		}
	}

	void MeasureItem(LPMEASUREITEMSTRUCT mis) {
		if (mis->CtlType != ODT_MENU) {
			m_pT->SetMsgHandled(FALSE);
			return;
		}

		mis->itemWidth = 0;
		mis->itemHeight = m_LastHeight;
		if (mis->itemData == Separator)	// separator
			mis->itemHeight = 10;
		else if (mis->itemID) {
			auto text = m_pT->UIGetText(mis->itemID);
			CString stext;
			if (text == nullptr) {
				if (auto it = m_Items.find(mis->itemID); it != m_Items.end()) {
					stext = (it->second.Text);
				}
			}
			else {
				stext = text;
			}
			CClientDC dc(m_pT->m_hWnd);
			CSize size;
			stext.Remove(L'&');
			if (stext.IsEmpty())
				stext = L"M";
			if (dc.GetTextExtent(stext, stext.GetLength(), &size)) {
				mis->itemWidth = size.cx + (mis->itemData == TopLevelMenu ? -5 : 25);
				m_LastHeight = mis->itemHeight = size.cy + (mis->itemData == TopLevelMenu ? 10 : 6);
			}
		}

		if (mis->itemData != TopLevelMenu) {
			if (mis->itemWidth < 120)
				mis->itemWidth = 120;
		}
	}

private:
	T* m_pT;
};
