#include "pch.h"
#include "resource.h"
#include "MultiStringValueDlg.h"

CMultiStringValueDlg::CMultiStringValueDlg(CRegKey& key, PCWSTR name, bool readOnly) : m_Key(key), m_Name(name), m_ReadOnly(readOnly) {
}

const CString& CMultiStringValueDlg::GetValue() const {
	return m_Value;
}

bool CMultiStringValueDlg::IsModified() const {
	return m_Modified;
}

LRESULT CMultiStringValueDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_TEXT);

	ULONG chars = 0;
	m_Key.QueryMultiStringValue(m_Name, nullptr, &chars);
	chars++;
	auto buffer = std::make_unique<WCHAR[]>(chars);
	ZeroMemory(buffer.get(), chars * sizeof(WCHAR));
	DWORD type;
	auto len = chars * 2;
	if (ERROR_SUCCESS != ::RegQueryValueEx(m_Key, m_Name, nullptr, &type, (PBYTE)buffer.get(), &len)) {
		EndDialog(IDRETRY);
		return 0;
	}
	std::for_each(buffer.get(), buffer.get() + chars - 1, [](auto& ch) {
		if (ch == 0) ch = L'\n';
		});
		
	m_Value = buffer.get();
	m_Value.Replace(L"\n", L"\r\n");
	SetDlgItemText(IDC_VALUE, m_Value);

	if (m_ReadOnly) {
		((CEdit)GetDlgItem(IDC_VALUE)).SetReadOnly(TRUE);
	}
	SetDlgItemText(IDC_NAME, m_Name);

	return 0;
}

LRESULT CMultiStringValueDlg::OnCloseCmd(WORD, WORD id, HWND, BOOL&) {
	if (id == IDOK) {
		CString text;
		GetDlgItemText(IDC_VALUE, text);
		m_Modified = text != m_Value;
		if (m_Modified)
			m_Value = text;
	}
	EndDialog(id);
	return 0;
}
