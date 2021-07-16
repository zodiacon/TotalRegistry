#pragma once

struct TreeHelper {
	explicit TreeHelper(CTreeViewCtrlEx& tv) : _tv(tv) {}
	HTREEITEM FindChild(HTREEITEM item, PCWSTR name) const;
	HTREEITEM FindItem(HTREEITEM hParent, PCWSTR name);
	int DeleteChildren(HTREEITEM hItem);
	void DoForEachItem(HTREEITEM hRoot, DWORD state, std::function<void(HTREEITEM, DWORD)> action);
	std::map<CString, HTREEITEM> GetChildItems(HTREEITEM hItem);

private:
	CTreeViewCtrlEx& _tv;
};
