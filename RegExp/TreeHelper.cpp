#include "pch.h"
#include "TreeHelper.h"

HTREEITEM TreeHelper::FindChild(HTREEITEM item, PCWSTR name) const {
    item = _tv.GetChildItem(item);
    while (item) {
        CString text;
        _tv.GetItemText(item, text);
        if (text == name)
            return item;
        item = _tv.GetNextSiblingItem(item);
    }
    return nullptr;
}
