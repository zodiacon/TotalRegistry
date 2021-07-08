#pragma once

struct TreeHelper {
	explicit TreeHelper(const CTreeViewCtrlEx& tv) : _tv(tv) {}
	HTREEITEM FindChild(HTREEITEM item, PCWSTR name) const;

private:
	const CTreeViewCtrlEx& _tv;
};
