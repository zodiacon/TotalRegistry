#include "pch.h"
#include "Resource.h"
#include "NumberValueDlg.h"
#include "Helpers.h"

CNumberValueDlg::CNumberValueDlg(RegistryKey& key, PCWSTR name, DWORD type, bool readOnly) 
	: m_Key(key), m_Name(name), m_Type(type), m_ReadOnly(readOnly) {
}

DWORD64 CNumberValueDlg::GetValue() const {
	return m_Value;
}

DWORD CNumberValueDlg::GetType() const {
	return m_Type;
}

bool CNumberValueDlg::IsModified() const {
	return m_Modified;
}

void CNumberValueDlg::DisplayValue(DWORD64 value, bool checkRadio) {
	CString text;
	switch (m_Format) {
		case ValueFormat::Decimal:
			text.Format(m_Type == REG_DWORD ? L"%u" : L"%llu", value);
			if(checkRadio)
				CheckDlgButton(IDC_DECIMAL, TRUE);
			break;

		case ValueFormat::Hex:
			text.Format(m_Type == REG_DWORD ? L"%X" : L"%llX", value);
			if (checkRadio)
				CheckDlgButton(IDC_HEX, TRUE);
			break;

		case ValueFormat::Binary:
			text = Helpers::ToBinary(value);
			if (checkRadio)
				CheckDlgButton(IDC_BINARY, TRUE);
			break;
	}
	SetDlgItemText(IDC_VALUE, text);
}

DWORD64 CNumberValueDlg::ParseValue(const CString& text, bool& error) {
	error = false;
	return _wcstoui64(text, nullptr, (int)m_Format);
}

LRESULT CNumberValueDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	ATLASSERT(m_Type == REG_DWORD || m_Type == REG_QWORD);
	m_Value = 0;
	ULONG bytes = m_Type == REG_DWORD ? 4 : 8;
	DWORD type;
	if (ERROR_SUCCESS != m_Key.QueryValue(m_Name, &type, &m_Value, &bytes)) {
		EndDialog(IDRETRY);
		return 0;
	}
	ATLASSERT(m_Type == type);
	SetDialogIcon(m_Type == REG_DWORD ? IDI_NUM4 : IDI_NUM8);
	SetWindowText(m_Type == REG_DWORD ? L"DWORD Value" : L"QWORD Value");
	SetDlgItemText(IDC_NAME, m_Name.IsEmpty() ? Helpers::DefaultValueName : m_Name);
	DisplayValue(m_Value);

	if (m_ReadOnly)
		((CEdit)GetDlgItem(IDC_VALUE)).SetReadOnly(TRUE);

	if (m_Type == REG_QWORD) {
		GetDlgItem(IDC_COLOR).ShowWindow(SW_HIDE);
		int extra = 120;
		CRect rc;
		GetWindowRect(&rc);
		SetWindowPos(nullptr, 0, 0, rc.Width() + extra, rc.Height(), SWP_NOMOVE | SWP_NOREPOSITION);
		GetDlgItem(IDC_VALUE).GetClientRect(&rc);
		GetDlgItem(IDC_VALUE).SetWindowPos(nullptr, 0, 0, rc.right + extra, rc.bottom, SWP_NOMOVE | SWP_NOREPOSITION);
	}

	return 0;
}

LRESULT CNumberValueDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (IDOK == wID) {
		CString text;
		GetDlgItemText(IDC_VALUE, text);
		bool error;
		auto value = ParseValue(text, error);
		if (error) {
			AtlMessageBox(m_hWnd, L"Error parsing value", IDS_APP_TITLE, MB_ICONERROR);
			return 0;
		}
		if (m_Value != value) {
			m_Value = value;
			m_Modified = true;
		}
	}
	EndDialog(wID);
	return 0;
}

LRESULT CNumberValueDlg::OnClickBase(WORD, WORD id, HWND, BOOL&) {
	CString text;
	GetDlgItemText(IDC_VALUE, text);
	bool error;
	auto value = ParseValue(text, error);

	switch (id) {
		case IDC_HEX: m_Format = ValueFormat::Hex; break;
		case IDC_DECIMAL: m_Format = ValueFormat::Decimal; break;
		case IDC_BINARY: m_Format = ValueFormat::Binary; break;
	}
	DisplayValue(value, false);
	return 0;
}

LRESULT CNumberValueDlg::OnClickColor(WORD, WORD id, HWND, BOOL&) {
	CString text;
	GetDlgItemText(IDC_VALUE, text);
	bool error;
	auto value = ParseValue(text, error);
	CColorDialog dlg((COLORREF)value, CC_RGBINIT);
	if (dlg.DoModal() == IDOK && !m_ReadOnly) {
		DisplayValue(dlg.GetColor(), false);
	}
	return 0;
}
