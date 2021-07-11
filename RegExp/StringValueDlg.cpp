#include "pch.h"
#include "resource.h"
#include "StringValueDlg.h"

CStringValueDlg::CStringValueDlg(CRegKey& key, PCWSTR name, bool readOnly) : m_Key(key), m_Name(name), m_ReadOnly(readOnly) {
}

const CString& CStringValueDlg::GetValue() const {
	return m_Value;
}

DWORD CStringValueDlg::GetType() const {
	return m_Type;
}

bool CStringValueDlg::IsModified() const {
	return m_Modified;
}

LRESULT CStringValueDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_TEXT);

	ULONG chars = 0;
	m_Key.QueryStringValue(m_Name, nullptr, &chars);
	auto buffer = std::make_unique<WCHAR[]>(chars);
	if (ERROR_SUCCESS != m_Key.QueryStringValue(m_Name, buffer.get(), &chars)) {
		EndDialog(IDRETRY);
		return 0;
	}
	SetDlgItemText(IDC_VALUE, m_Value = buffer.get());
	m_Key.QueryValue(m_Name, &m_Type, nullptr, nullptr);
	ATLASSERT(m_Type == REG_SZ || m_Type == REG_EXPAND_SZ);
	CheckDlgButton(m_Type == REG_SZ ? IDC_STRING : IDC_EXPANDSTRING, BST_CHECKED);

	if (m_ReadOnly) {
		((CEdit)GetDlgItem(IDC_VALUE)).SetReadOnly(TRUE);
		GetDlgItem(IDC_STRING).EnableWindow(FALSE);
		GetDlgItem(IDC_EXPANDSTRING).EnableWindow(FALSE);
		GetDlgItem(IDC_FILE).EnableWindow(FALSE);
		GetDlgItem(IDC_FOLDER).EnableWindow(FALSE);
	}
	else {
		::SHAutoComplete(GetDlgItem(IDC_VALUE), SHACF_FILESYS_DIRS);
	}
	SetDlgItemText(IDC_NAME, m_Name);

	return 0;
}

LRESULT CStringValueDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		CString value;
		GetDlgItemText(IDC_VALUE, value);
		auto type = IsDlgButtonChecked(IDC_STRING) == BST_CHECKED ? REG_SZ : REG_EXPAND_SZ;
		if (type != m_Type || value != m_Value) {
			m_Value = value;
			m_Type = type;
			//
			// something changed
			//
			m_Modified = true;
		}
	}
	EndDialog(wID);
	return 0;
}

LRESULT CStringValueDlg::OnBrowseFile(WORD, WORD wID, HWND, BOOL&) {
	CFileDialog dlg(TRUE, nullptr, nullptr, OFN_EXPLORER | OFN_ENABLESIZING,
		L"All Files\0*.*\0", m_hWnd);
	if (dlg.DoModal() == IDOK) {
		SetDlgItemText(IDC_VALUE, dlg.m_szFileName);
	}
	return 0;
}

LRESULT CStringValueDlg::OnBrowseFolder(WORD, WORD wID, HWND, BOOL&) {
	CComPtr<IFileDialog> spFileDlg;
	auto hr = spFileDlg.CoCreateInstance(CLSID_FileOpenDialog);
	ATLASSERT(spFileDlg);
	if (FAILED(hr))
		return 0;

	FILEOPENDIALOGOPTIONS options;
	spFileDlg->GetOptions(&options);
	spFileDlg->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FORCESHOWHIDDEN | FOS_PICKFOLDERS);
	if (S_OK == spFileDlg->Show(m_hWnd)) {
		CComPtr<IShellItem> spItem;
		spFileDlg->GetResult(&spItem);
		ATLASSERT(spItem);
		PWSTR rpath;
		if (FAILED(spItem->GetDisplayName(SIGDN_FILESYSPATH, &rpath))) {
			AtlMessageBox(m_hWnd, L"Failed to retrieve selected folder", MB_ICONERROR);
		}
		else {
			SetDlgItemText(IDC_VALUE, rpath);
			::CoTaskMemFree(rpath);
		}
	}
	return 0;
}
