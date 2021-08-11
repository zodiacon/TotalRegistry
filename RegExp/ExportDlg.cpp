#include "pch.h"
#include "resource.h"
#include "ExportDlg.h"
#include "ThemeHelper.h"

void CExportDlg::SetKeyPath(PCWSTR path) {
    m_Key = path;
}

const CString& CExportDlg::GetSelectedKey() const {
    return m_Key;
}

const CString& CExportDlg::GetFileName() const {
    return m_FileName;
}

LRESULT CExportDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    SetDialogIcon(IDI_EXPORT);
    SetDlgItemText(IDC_KEY, m_Key);
    CheckDlgButton(IDC_EXPORTKEY, BST_CHECKED);
    ::SHAutoComplete(GetDlgItem(IDC_PATH), SHACF_FILESYS_ONLY);

    return 0;
}

LRESULT CExportDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    if (wID == IDOK) {
        if (IsDlgButtonChecked(IDC_EXPORT_REAL))
            m_Key.Empty();
        else
            GetDlgItemText(IDC_KEY, m_Key);
        GetDlgItemText(IDC_PATH, m_FileName);
    }
    EndDialog(wID);
    return 0;
}

LRESULT CExportDlg::OnBrowse(WORD, WORD wID, HWND, BOOL&) {
    ThemeHelper::Suspend();
    CSimpleFileDialog dlg(FALSE, nullptr, nullptr, 
        OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT,
        L"REG format (*reg)\0*.reg\0Native Format\0*.*\0", m_hWnd);
    if (dlg.DoModal() == IDOK) {
        SetDlgItemText(IDC_PATH, dlg.m_szFileName);
    }
    ThemeHelper::Resume();
    return 0;
}

LRESULT CExportDlg::OnPathTextChanged(WORD, WORD wID, HWND, BOOL&) {
    GetDlgItem(IDOK).EnableWindow(GetDlgItem(IDC_PATH).GetWindowTextLength() > 0 
        && (IsDlgButtonChecked(IDC_EXPORT_REAL) == BST_CHECKED || GetDlgItem(IDC_KEY).GetWindowTextLength() > 0));
    return 0;
}
