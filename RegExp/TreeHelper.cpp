#include "pch.h"
#include "TreeHelper.h"

HTREEITEM TreeHelper::FindChild(HTREEITEM item, PCWSTR name) const {
	item = _tv.GetChildItem(item);
	while (item) {
		CString text;
		_tv.GetItemText(item, text);
		if (text.CompareNoCase(name) == 0)
			return item;
		item = _tv.GetNextSiblingItem(item);
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
		_tv.Expand(hParent, TVE_EXPAND);
		hItem = FindChild(hParent, name);
		if (!hItem)
			break;
		hParent = hItem;
	}
	return hItem;
}

int TreeHelper::DeleteChildren(HTREEITEM hItem) {
	int count = 0;
	hItem = _tv.GetChildItem(hItem);
	while (hItem) {
		auto hNext = _tv.GetNextSiblingItem(hItem);
		if (_tv.DeleteItem(hItem))
			count++;
		hItem = hNext;
	}
	return count;
}

void TreeHelper::DoForEachItem(HTREEITEM hRoot, DWORD mask, std::function<void(HTREEITEM, DWORD)> action) {
	CString text;
	auto hItem = _tv.GetChildItem(hRoot);
	while (hItem) {
#ifdef _DEBUG
		_tv.GetItemText(hItem, text);
		ATLTRACE(L"DoForEachExpanded hItem: 0x%p (%s)\n", hItem, text);
#endif
		auto state = _tv.GetItemState(hItem, mask);
		if (state) {
			action(hItem, state);
			DoForEachItem(hItem, mask, action);
		}
		hItem = _tv.GetNextSiblingItem(hItem);
	}
}

std::map<CString, HTREEITEM> TreeHelper::GetChildItems(HTREEITEM hItem) {
	std::map<CString, HTREEITEM> items;
	hItem = _tv.GetChildItem(hItem);
	while (hItem) {
		CString text;
		_tv.GetItemText(hItem, text);
		items.insert({ text, hItem });
		hItem = _tv.GetNextSiblingItem(hItem);
	}
	return items;
}

