#include "pch.h"
#include "resource.h"
#include "ExportDlg.h"

LRESULT CExportDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    return LRESULT();
}

LRESULT CExportDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    EndDialog(wID);
    return 0;
}
