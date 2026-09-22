#include "pch.h"
#include "TreeHelper.h"

HTREEITEM TreeHelper::FindChild(HTREEITEM item, PCWSTR name) const {
	item = m_TV.GetChildItem(item);
	while (item) {
		CString text;
		m_TV.GetItemText(item, text);
		if (text.CompareNoCase(name) == 0)
			return item;
		item = m_TV.GetNextSiblingItem(item);
	}
	return nullptr;
}

HTREEITEM TreeHelper::FindItem(HTREEITEM hParent, PCWSTR path) {
	int start = 0;
	CString spath(path);
	if (spath[0] == L'\\') {
		// skip first
		spath = spath.Mid(spath.Find(L'\\', 1));
	}
	HTREEITEM hItem = nullptr;
	while (hParent) {
		auto name = spath.Tokenize(L"\\", start);
		if (name.IsEmpty())
			break;
		m_TV.Expand(hParent, TVE_EXPAND);
		hItem = FindChild(hParent, name);
		if (!hItem)
			break;
		hParent = hItem;
	}
	return hItem;
}

int TreeHelper::DeleteChildren(HTREEITEM hItem) {
	int count = 0;
	hItem = m_TV.GetChildItem(hItem);
	while (hItem) {
		auto hNext = m_TV.GetNextSiblingItem(hItem);
		if (m_TV.DeleteItem(hItem))
			count++;
		hItem = hNext;
	}
	return count;
}

void TreeHelper::DoForEachItem(HTREEITEM hRoot, DWORD mask, std::function<void(HTREEITEM, DWORD)> action) {
	CString text;
	auto hItem = m_TV.GetChildItem(hRoot);
	while (hItem) {
#ifdef _DEBUG
		m_TV.GetItemText(hItem, text);
		ATLTRACE(L"DoForEachExpanded hItem: 0x%p (%s)\n", hItem, text);
#endif
		auto state = m_TV.GetItemState(hItem, mask);
		if (mask == 0 || state) {
			action(hItem, state);
			DoForEachItem(hItem, mask, action);
		}
		hItem = m_TV.GetNextSiblingItem(hItem);
	}
}

std::map<CString, HTREEITEM> TreeHelper::GetChildItems(HTREEITEM hItem) {
	std::map<CString, HTREEITEM> items;
	hItem = m_TV.GetChildItem(hItem);
	while (hItem) {
		CString text;
		m_TV.GetItemText(hItem, text);
		items.insert({ text, hItem });
		hItem = m_TV.GetNextSiblingItem(hItem);
	}
	return items;
}

