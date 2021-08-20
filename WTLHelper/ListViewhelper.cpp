#include "pch.h"
#include "ListViewHelper.h"
#include <wil\resource.h>

bool ListViewHelper::SaveAll(PCWSTR path, CListViewCtrl& lv, bool includeHeaders) {
	wil::unique_handle hFile(::CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	if (!hFile)
		return false;

	auto count = lv.GetItemCount();
	auto header = lv.GetHeader();
	auto columns = header.GetItemCount();
	CString text;
	DWORD written;

	if (includeHeaders) {
		HDITEM hdi;
		WCHAR text[64] = { 0 };
		hdi.cchTextMax = _countof(text);
		hdi.pszText = text;
		hdi.mask = HDI_TEXT;
		for (int i = 0; i < columns; i++) {
			header.GetItem(i, &hdi);
			::wcscat_s(text, i == columns - 1 ? L"\n" : L"\t");
			::WriteFile(hFile.get(), text, (DWORD)::wcslen(text) * sizeof(WCHAR), &written, nullptr);
		}
	}

	for (int i = 0; i < count; i++) {
		for (int c = 0; c < columns; c++) {
			text.Empty();
			lv.GetItemText(i, c, text);
			text += c == columns - 1 ? L"\n" : L"\t";
			::WriteFile(hFile.get(), text.GetBuffer(), text.GetLength() * sizeof(WCHAR), &written, nullptr);
		}
	}

	return true;
}

CString ListViewHelper::GetRowAsString(CListViewCtrl& lv, int row, WCHAR separator) {
	auto count = lv.GetHeader().GetItemCount();
	if (count == 0)
		return L"";

	CString text, item;
	for (int c = 0; c < count; c++) {
		lv.GetItemText(row, c, item);
		text += item;
		if (c < count - 1)
			text += separator;
	}
	return text;
}

int ListViewHelper::FindItem(CListViewCtrl& lv, PCWSTR text, bool partial) {
	auto columns = lv.GetHeader().GetItemCount();
	CString stext(text);
	stext.MakeLower();
	for (int i = 0; i < lv.GetItemCount(); i++) {
		for (int c = 0; c < columns; c++) {
			CString text;
			lv.GetItemText(i, c, text);
			text.MakeLower();
			if (partial && text.Find(stext) >= 0)
				return i;
			if (!partial && text == stext)
				return i;
		}
	}

	return -1;
}
