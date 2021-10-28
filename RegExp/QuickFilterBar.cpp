#include "pch.h"
#include "resource.h"
#include "QuickFilterBar.h"

LRESULT CQuickFilterBar::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    ((CButton)GetDlgItem(IDC_CLEAR)).SetIcon(AtlLoadIconImage(IDI_DELETE, 0, 16, 16));
    m_Edit.SubclassWindow(GetDlgItem(IDC_FILTER));

    return 0;
}

LRESULT CQuickFilterBar::OnDialogColor(UINT, WPARAM, LPARAM, BOOL&) {
    return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
}

LRESULT CQuickFilterBar::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
    if (id == 1) {
        KillTimer(id);
        CString text;
        GetDlgItemText(IDC_FILTER, text);
        m_pFrame->QuickFilter(text);
    }
    return 0;
}

LRESULT CQuickFilterBar::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
    m_Edit.SetSelAll();
    m_Edit.SetFocus();
    return 0;
}

LRESULT CQuickFilterBar::OnClearText(WORD, WORD, HWND, BOOL&) {
    m_Edit.SetWindowText(L"");
    m_pFrame->QuickFilter(L"");
    ::SetFocus(m_pFrame->GetHwnd());

    return 0;
}

LRESULT CQuickFilterBar::OnEditKeyUp(UINT, WPARAM wp, LPARAM, BOOL& handled) {
    if (wp == VK_TAB) {
        ::SetFocus(m_pFrame->GetHwnd());
        return 0;
    }
    if (wp == VK_ESCAPE) {
        SetDlgItemText(IDC_FILTER, L"");
        m_pFrame->QuickFilter(L"");
        return 0;
    }

    SetTimer(1, 300);
    return TRUE;
}

LRESULT CQuickFilterBar::OnEditChar(UINT, WPARAM wp, LPARAM, BOOL& handled) {
    if (wp == VK_ESCAPE || wp == VK_TAB)
        return 0;

    handled = FALSE;
    return 0;
}
