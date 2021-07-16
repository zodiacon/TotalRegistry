#include "pch.h"
#include "resource.h"
#include "BinaryValueDlg.h"

CBinaryValueDlg::CBinaryValueDlg(CRegKey& key, PCWSTR name, bool readOnly) : m_Key(key), m_Name(name), m_ReadOnly(readOnly) {
}

const std::vector<BYTE>& CBinaryValueDlg::GetValue() const {
	return m_Value;
}

bool CBinaryValueDlg::IsModified() const {
	return m_Modified;
}

LRESULT CBinaryValueDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_BINARY);

	CRect rc;
	GetDlgItem(IDC_BUTTON1).GetWindowRect(&rc);
	ScreenToClient(&rc);
	GetDlgItem(IDC_BUTTON1).ShowWindow(SW_HIDE);
	
	m_Hex.Create(m_hWnd, &rc, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_CLIENTEDGE, IDC_HEX);

	ULONG bytes = 0;
	m_Key.QueryBinaryValue(m_Name, nullptr, &bytes);
	m_Value.resize(bytes);
	if (ERROR_SUCCESS != m_Key.QueryBinaryValue(m_Name, m_Value.data(), &bytes)) {
		EndDialog(IDRETRY);
		return 0;
	}

	m_Buffer.Init(m_Value.data(), (uint32_t)m_Value.size());

	CString text;
	text.Format(L"Size: %u bytes", bytes);
	SetDlgItemText(IDC_BUFFERSIZE, text);

	m_Hex.SetReadOnly(m_ReadOnly);
	m_Hex.SetBufferManager(&m_Buffer);
	m_Hex.SetBytesPerLine(8);
	m_Hex.SetBiasOffset(0);
	m_Hex.SetFocus();

	SetDlgItemText(IDC_NAME, m_Name);

	return 0;
}

LRESULT CBinaryValueDlg::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
	if (m_Hex) {
		CRect rc;
		GetDlgItem(IDC_BUTTON1).GetWindowRect(&rc);
		ScreenToClient(&rc);
		m_Hex.MoveWindow(&rc);
	}
	handled = FALSE;
	return 0;
}

LRESULT CBinaryValueDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (IDOK == wID && !m_ReadOnly) {
		if (m_Buffer.GetSize() != m_Value.size())
			m_Modified = true;
		else {
			m_Modified = 0 != ::memcmp(m_Value.data(), m_Buffer.GetRawData(0), m_Value.size());
		}
		if (m_Modified) {
			m_Value.resize(m_Buffer.GetSize());
			m_Buffer.GetData(0, m_Value.data(), (DWORD)m_Buffer.GetSize());
		}
	}
	EndDialog(wID);
	return 0;
}
