#pragma once

struct TreeHelper {
	explicit TreeHelper(CTreeViewCtrlEx& tv) : _tv(tv) {}
	HTREEITEM FindChild(HTREEITEM item, PCWSTR name) const;
	HTREEITEM FindItem(HTREEITEM hParent, PCWSTR name);
	int DeleteChildren(HTREEITEM hItem);

private:
	CTreeViewCtrlEx& _tv;
};
