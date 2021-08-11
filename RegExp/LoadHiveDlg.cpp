#include "pch.h"
#include "Resource.h"
#include "LoadHiveDlg.h"
#include "ThemeHelper.h"

HKEY CLoadHiveDlg::GetSelectedKey() const {
    return m_hKey;
}

const CString& CLoadHiveDlg::GetFileName() const {
    return m_FileName;
}

const CString& CLoadHiveDlg::GetName() const {
    return m_Name;
}

LRESULT CLoadHiveDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    SetDialogIcon(IDI_FOLDER_LOAD);
    CheckDlgButton(IDC_MACHINE, BST_CHECKED);
    ::SHAutoComplete(GetDlgItem(IDC_PATH), SHACF_FILESYS_ONLY);

    return 0;
}

LRESULT CLoadHiveDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    if (wID == IDOK) {
        m_hKey = IsDlgButtonChecked(IDC_MACHINE) == BST_CHECKED ? HKEY_LOCAL_MACHINE : HKEY_USERS;
        GetDlgItemText(IDC_NAME, m_Name);
        GetDlgItemText(IDC_PATH, m_FileName);
    }
    EndDialog(wID);
    return 0;
}

LRESULT CLoadHiveDlg::OnBrowse(WORD, WORD wID, HWND, BOOL&) {
    ThemeHelper::Suspend();
    CSimpleFileDialog dlg(TRUE, L"dat", nullptr, OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST,
        L"All Files\0*.*\0", m_hWnd);
    if (dlg.DoModal() == IDOK) {
        SetDlgItemText(IDC_PATH, dlg.m_szFileName);
    }
    ThemeHelper::Resume();
    return 0;
}

LRESULT CLoadHiveDlg::OnTextChanged(WORD, WORD wID, HWND, BOOL&) {
    GetDlgItem(IDOK).EnableWindow(GetDlgItem(IDC_PATH).GetWindowTextLength() > 0 && GetDlgItem(IDC_NAME).GetWindowTextLength() > 0);
    return 0;
}
