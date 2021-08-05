#pragma once

template<typename T>
class COwnerDrawnMenu {
public:
	BEGIN_MSG_MAP(COwnerDrawnMenu)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_MEASUREITEM, OnMeasureItem)
	END_MSG_MAP()

	enum { TopLevelMenu = 111, Separator = 100 };

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
		m_Images.Create(16, 16, ILC_COLOR32 | ILC_COLOR, 16, 8);
		m_Images.AddIcon(AtlLoadIconImage(IDI_CHECK, 0, 16, 16));
	}

	BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) {
		AddSubMenu(hMenu);
		return ::TrackPopupMenu(hMenu, flags, x, y, 0, m_pT->m_hWnd, nullptr);
	}

	void AddCommand(UINT id, HICON hIcon) {
		ItemData data;
		data.Image = m_Images.AddIcon(hIcon);
		auto it = m_Items.find(id);
		if (it != m_Items.end())
			it->second.Image = data.Image;
		else
			m_Items.insert({ id, data });
	}

	void AddCommand(UINT id, UINT iconId) {
		auto hIcon = AtlLoadIconImage(iconId, 64, 16, 16);
		ATLASSERT(hIcon);
		AddCommand(id, hIcon);
	}

	bool AddMenu(HMENU hMenu) {
		ATLASSERT(::IsMenu(hMenu));
		CMenuHandle menu(hMenu);
		UpdateMenu(menu, true);
		auto count = menu.GetMenuItemCount();
		MENUITEMINFO mii = { sizeof(mii) };
		WCHAR text[16];
		for (decltype(count) i = 0; i < count; i++) {
			mii.fMask = MIIM_SUBMENU | MIIM_ID;
			if (menu.GetMenuItemInfo(i, TRUE, &mii) && mii.hSubMenu) {
				AddSubMenu(mii.hSubMenu);
				mii.fMask = MIIM_TYPE | MIIM_DATA;
				mii.fType |= MFT_OWNERDRAW;
				mii.dwItemData = TopLevelMenu;		// top level menu
				ATLVERIFY(menu.SetMenuItemInfo(i, TRUE, &mii));
				if (menu.GetMenuString(i, text, _countof(text), MF_BYPOSITION)) {
					ItemData data;
					data.Text = text;
					data.Image = -1;
					m_Items.insert({ mii.wID, data });
				}
			}
		}
		return true;
	}

	void AddSubMenu(CMenuHandle menu) {
		UpdateMenu(menu);
		auto count = menu.GetMenuItemCount();
		MENUITEMINFO mii = { sizeof(mii) };
		WCHAR text[64];
		for (decltype(count) i = 0; i < count; i++) {
			mii.fMask = MIIM_TYPE | MIIM_DATA;
			if (menu.GetMenuItemInfoW(i, TRUE, &mii) && mii.fType == MFT_SEPARATOR)
				mii.dwItemData = 100;
			else
				mii.dwItemData = 0;
			mii.fType = MFT_OWNERDRAW;
			ATLVERIFY(menu.SetMenuItemInfo(i, TRUE, &mii));
			mii.fMask = MIIM_SUBMENU | MIIM_ID;
			if (menu.GetMenuItemInfo(i, TRUE, &mii) && mii.hSubMenu) {
				AddSubMenu(mii.hSubMenu);
			}
			else {
				if (menu.GetMenuString(i, text, _countof(text), MF_BYPOSITION)) {
					ATLASSERT(text[0]);
					ATLASSERT(mii.wID);
					auto it = m_Items.find(mii.wID);
					if (it != m_Items.end())
						it->second.Text = text;
					else {
						ItemData data;
						data.Text = text;
						data.Image = -1;
						m_Items.insert({ mii.wID, data });
					}
				}
			}
		}
	}

	void DrawSeparator(CDCHandle dc, CRect& rc) {
		dc.FillSolidRect(&rc, m_BackColor);
		CPen pen;
		pen.CreatePen(PS_SOLID, 1, m_SeparatorColor);
		dc.MoveTo(rc.left + 8, rc.top + rc.Height() / 2);
		dc.SelectPen(pen);
		dc.LineTo(rc.right - 8, rc.top + rc.Height() / 2);
	}

	void UpdateMenu(CMenuHandle menu, bool subMenus = false) {
		MENUINFO mi = { sizeof(mi) };
		mi.fMask = MIM_BACKGROUND | (subMenus ? MIM_APPLYTOSUBMENUS : 0);
		CBrush brush;
		brush.CreateSolidBrush(m_BackColor);
		mi.hbrBack = brush.Detach();
		ATLVERIFY(menu.SetMenuInfo(&mi));
	}

	void DrawItem(LPDRAWITEMSTRUCT dis) {
		if (dis->CtlType != ODT_MENU) {
			m_pT->SetMsgHandled(FALSE);
			return;
		}

		CDCHandle dc(dis->hDC);
		CRect rc(dis->rcItem);
		if (dis->itemData == Separator) {
			DrawSeparator(dc, rc);
			return;
		}

		dc.FillSolidRect(&rc, (dis->itemState & ODS_SELECTED) ? m_SelectionBackColor : m_BackColor);
		auto it = m_Items.find(dis->itemID);
		if (it != m_Items.end()) {
			auto& data = it->second;
			rc.OffsetRect(2, 2);
			rc.right = rc.left + 16;
			rc.bottom = rc.top + 16;
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
				m_Images.DrawEx(0, dis->hDC, rc, CLR_NONE, CLR_NONE, ILD_NORMAL);
			}
		}
		CMenuHandle menu((HMENU)dis->hwndItem);
		ATLASSERT(menu.IsMenu());
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_FTYPE | MIIM_STRING;
		mii.fType = MFT_STRING;
		WCHAR mtext[64];
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

	void SetTextColor(COLORREF color) {
		m_TextColor = color;
	}

	void SetBackColor(COLORREF color) {
		m_BackColor = color;
	}

	void SetSelectionTextColor(COLORREF color) {
		m_SelectionTextColor = color;
	}

	void SetSelectionBackColor(COLORREF color) {
		m_SelectionBackColor = color;
	}

	void SetSeparatorColor(COLORREF color) {
		m_SeparatorColor = color;
	}

private:
	int m_Width{ 0 };
	struct ItemData {
		CString Text;
		int Image;
	};
	std::unordered_map<UINT, ItemData> m_Items;
	CImageList m_Images;
	COLORREF m_TextColor{ RGB(0, 0, 0) }, m_BackColor{ RGB(240, 240, 240) };
	COLORREF m_SelectionBackColor{ RGB(0, 48, 192) }, m_SelectionTextColor{ RGB(255, 255, 255) };
	COLORREF m_SeparatorColor{ RGB(64, 64, 64) };
	int m_LastHeight{ 16 };
	T* m_pT;
};
