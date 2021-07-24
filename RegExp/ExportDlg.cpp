#include "pch.h"
#include "resource.h"
#include "ExportDlg.h"

void CExportDlg::SetKeyPath(PCWSTR path) {
    m_Key = path;
}

CString CExportDlg::GetSelectedKey() const {
    return m_Key;
}

CString CExportDlg::GetFileName() const {
    return m_FileName;
}

LRESULT CExportDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
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
    CSimpleFileDialog dlg(FALSE, L"dat", nullptr, OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT,
        L"All Files\0*.*\0", m_hWnd);
    if (dlg.DoModal() == IDOK) {
        SetDlgItemText(IDC_PATH, dlg.m_szFileName);
    }
    return 0;
}

LRESULT CExportDlg::OnPathTextChanged(WORD, WORD wID, HWND, BOOL&) {
    GetDlgItem(IDOK).EnableWindow(GetDlgItem(IDC_PATH).GetWindowTextLength() > 0 
        && (IsDlgButtonChecked(IDC_EXPORT_REAL) == BST_CHECKED || GetDlgItem(IDC_KEY).GetWindowTextLength() > 0));
    return 0;
}
