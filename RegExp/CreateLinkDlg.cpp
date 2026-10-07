#include "pch.h"
#include "resource.h"
#include "CreateLinkDlg.h"
#include "Registry.h"

CString const& CCreateLinkDlg::GetName() const {
	return m_Name;
}

CString const& CCreateLinkDlg::GetTarget() const {
	return m_Target;
}

bool CCreateLinkDlg::IsVolatile() const {
	return m_Volatile;
}

LRESULT CCreateLinkDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CenterWindow(GetParent());
	SetDialogIcon(IDI_FOLDER_NEW);
	return TRUE;
}

LRESULT CCreateLinkDlg::OnOK(WORD, WORD wID, HWND, BOOL&) {
	CString name, target;
	GetDlgItemText(IDC_NAME, name);
	GetDlgItemText(IDC_TARGET, target);
	name.Trim();
	target.Trim();
	target.TrimRight(L'\\');

	if (name.IsEmpty() || name.Find(L'\\') >= 0) {
		AtlMessageBox(m_hWnd, L"Enter a name for the link (without backslashes).", IDS_APP_TITLE, MB_ICONWARNING);
		GetDlgItem(IDC_NAME).SetFocus();
		return 0;
	}
	auto kernel = Registry::StdPathToKernelPath(target);
	if (kernel.IsEmpty()) {
		AtlMessageBox(m_hWnd, L"The target must be under HKEY_LOCAL_MACHINE, HKEY_USERS, HKEY_CURRENT_USER or HKEY_CURRENT_CONFIG, "
			L"or a \\REGISTRY path. HKEY_CLASSES_ROOT is a merged view; use HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes or HKEY_CURRENT_USER\\Software\\Classes.",
			IDS_APP_TITLE, MB_ICONWARNING);
		GetDlgItem(IDC_TARGET).SetFocus();
		return 0;
	}
	// a link to a missing key is allowed (it may be created later), but likely a typo
	if (!Registry::OpenKey(kernel, KEY_QUERY_VALUE) && ::GetLastError() == ERROR_FILE_NOT_FOUND
		&& AtlMessageBox(m_hWnd, (PCWSTR)(L"The target " + kernel + L" does not exist. Create the link anyway?"), IDS_APP_TITLE, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
		return 0;

	m_Name = name;
	m_Target = kernel;
	m_Volatile = IsDlgButtonChecked(IDC_VOLATILE) == BST_CHECKED;
	EndDialog(wID);
	return 0;
}

LRESULT CCreateLinkDlg::OnCancel(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}
