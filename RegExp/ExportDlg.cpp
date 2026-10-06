#include "pch.h"
#include "resource.h"
#include "ExportDlg.h"
#include "WTLHelper.h"

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
        CString key, fileName;
        if (!IsDlgButtonChecked(IDC_EXPORT_REAL))
            GetDlgItemText(IDC_KEY, key);
        GetDlgItemText(IDC_PATH, fileName);

        // .reg files can only refer to the standard HKEY_* keys, so the real Registry can only be exported as a hive
        bool real = key.IsEmpty() || (key[0] == L'\\' && key.Left(2) != L"\\\\");
        if (real && fileName.Right(4).CompareNoCase(L".reg") == 0) {
            AtlMessageBox(m_hWnd, L"Keys in the real Registry cannot be exported to a .reg file.\n\nUse a file name without the .reg extension to export a hive file instead.",
                IDS_APP_TITLE, MB_ICONWARNING);
            GetDlgItem(IDC_PATH).SetFocus();
            return 0;
        }
        m_Key = key;
        m_FileName = fileName;
    }
    EndDialog(wID);
    return 0;
}

LRESULT CExportDlg::OnBrowse(WORD, WORD wID, HWND, BOOL&) {
    WTLHelper::SuspendHook();
    CSimpleFileDialog dlg(FALSE, L"Reg", nullptr,
        OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        L"REG format (*reg)\0*.reg\0Native Format\0*.*\0", m_hWnd);
    if (dlg.DoModal() == IDOK) {
        SetDlgItemText(IDC_PATH, dlg.m_szFileName);
    }
    WTLHelper::ResumeHook();
    return 0;
}

LRESULT CExportDlg::OnPathTextChanged(WORD, WORD wID, HWND, BOOL&) {
    GetDlgItem(IDOK).EnableWindow(GetDlgItem(IDC_PATH).GetWindowTextLength() > 0 
        && (IsDlgButtonChecked(IDC_EXPORT_REAL) == BST_CHECKED || GetDlgItem(IDC_KEY).GetWindowTextLength() > 0));
    return 0;
}
