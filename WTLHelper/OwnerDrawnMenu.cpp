#include "pch.h"
#include "OwnerDrawnMenu.h"

void COwnerDrawnMenuBase::SetTextColor(COLORREF color) {
	m_TextColor = color;
}

void COwnerDrawnMenuBase::SetBackColor(COLORREF color) {
	m_BackColor = color;
}

void COwnerDrawnMenuBase::SetSelectionTextColor(COLORREF color) {
	m_SelectionTextColor = color;
}

void COwnerDrawnMenuBase::SetSelectionBackColor(COLORREF color) {
	m_SelectionBackColor = color;
}

void COwnerDrawnMenuBase::SetSeparatorColor(COLORREF color) {
	m_SeparatorColor = color;
}

void COwnerDrawnMenuBase::UpdateMenu(CMenuHandle menu, bool subMenus) {
	MENUINFO mi = { sizeof(mi) };
	mi.fMask = MIM_BACKGROUND | (subMenus ? MIM_APPLYTOSUBMENUS : 0);
	CBrush brush;
	brush.CreateSolidBrush(m_BackColor);
	mi.hbrBack = brush.Detach();
	ATLVERIFY(menu.SetMenuInfo(&mi));
}

void COwnerDrawnMenuBase::AddCommand(UINT id, HICON hIcon) {
	ItemData data;
	data.Image = m_Images.AddIcon(hIcon);
	auto it = m_Items.find(id);
	if (it != m_Items.end())
		it->second.Image = data.Image;
	else
		m_Items.insert({ id, data });
}

void COwnerDrawnMenuBase::AddCommand(UINT id, UINT iconId) {
	auto hIcon = AtlLoadIconImage(iconId, 64, 16, 16);
	ATLASSERT(hIcon);
	AddCommand(id, hIcon);
}

bool COwnerDrawnMenuBase::AddMenu(HMENU hMenu) {
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

void COwnerDrawnMenuBase::AddSubMenu(CMenuHandle menu) {
	UpdateMenu(menu);
	auto count = menu.GetMenuItemCount();
	MENUITEMINFO mii = { sizeof(mii) };
	WCHAR text[64];
	for (decltype(count) i = 0; i < count; i++) {
		mii.fMask = MIIM_TYPE | MIIM_DATA;
		mii.fType = 0;
		if (menu.GetMenuItemInfoW(i, TRUE, &mii) && mii.fType == MFT_SEPARATOR)
			mii.dwItemData = Separator;
		else
			mii.dwItemData = 0;
		if (mii.fType & MFT_OWNERDRAW)
			continue;

		mii.fType |= MFT_OWNERDRAW;
		ATLVERIFY(menu.SetMenuItemInfo(i, TRUE, &mii));
		if (mii.dwItemData == Separator)
			continue;

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

void COwnerDrawnMenuBase::SetCheckIcon(HICON hIcon) {
	m_CheckIcon = m_Images.AddIcon(hIcon);
}

