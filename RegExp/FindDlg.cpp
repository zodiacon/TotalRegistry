#include "pch.h"
#include "resource.h"
#include "FindDlg.h"

CFindDlg::CFindDlg(IMainFrame* frame) : m_pFrame(frame) {
}

void CFindDlg::UpdateUI() {
    auto options = m_pFrame->GetSettings().Find();
    CheckButton(IDC_SEARCH_KEYS, options, FindOptions::SearchKeys);
    CheckButton(IDC_SEARCH_VALUES, options, FindOptions::SearchValues);
    CheckButton(IDC_SEARCH_DATA, options, FindOptions::SearchData);
    if (!CheckButton(IDC_SEARCH_SELECTED, options, FindOptions::SearchSelected))
        CheckDlgButton(IDC_SEARCH_ALL, BST_CHECKED);
    CheckButton(IDC_SEARCH_MATCHWHOLE, options, FindOptions::MatchWholeWords);
    CheckButton(IDC_SEARCH_CASE, options, FindOptions::MatchCase);
    CheckButton(IDC_SEARCH_STD, options, FindOptions::SearchStdRegistry);
    CheckButton(IDC_SEARCH_REAL, options, FindOptions::SearchRealRegistry);
    GetDlgItem(IDC_CANCEL).EnableWindow(m_Searcher.IsRunning());

}

void CFindDlg::Cancel() {
    m_Searcher.Cancel();
}

void CFindDlg::Continue() {
    m_Searcher.Continue();
}

bool CFindDlg::IsFindNextAvailable() const {
    return IsWindow() && GetDlgItem(IDC_TEXT).GetWindowTextLength() > 0;
}

bool CFindDlg::CheckButton(UINT id, FindOptions options, FindOptions value) {
    bool check = (options & value) == value;
    CheckDlgButton(id, check ? BST_CHECKED : BST_UNCHECKED);
    return check;
}

FindOptions CFindDlg::UpdateOptions() {
    //
    // save options
    //
    auto options = FindOptions::None;
    if (IsDlgButtonChecked(IDC_SEARCH_KEYS))
        options |= FindOptions::SearchKeys;
    if (IsDlgButtonChecked(IDC_SEARCH_VALUES))
        options |= FindOptions::SearchValues;
    if (IsDlgButtonChecked(IDC_SEARCH_DATA))
        options |= FindOptions::SearchData;
    if (IsDlgButtonChecked(IDC_SEARCH_SELECTED))
        options |= FindOptions::SearchSelected;
    if (IsDlgButtonChecked(IDC_SEARCH_STD))
        options |= FindOptions::SearchStdRegistry;
    if (IsDlgButtonChecked(IDC_SEARCH_REAL))
        options |= FindOptions::SearchRealRegistry;
    if (IsDlgButtonChecked(IDC_SEARCH_MATCHWHOLE))
        options |= FindOptions::MatchWholeWords;
    if (IsDlgButtonChecked(IDC_SEARCH_CASE))
        options |= FindOptions::MatchCase;

    m_pFrame->GetSettings().Find(options);
    return options;
}

LRESULT CFindDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    CenterWindow(m_pFrame->GetHwnd());
    SetDialogIcon(IDI_FIND);
    AddIconToButton(IDC_CANCEL, IDI_DELETE);
    AddIconToButton(IDC_FIND, IDI_FIND_NEXT);
    AddIconToButton(IDC_NEW, IDI_FIND);

    m_Progress.Attach(GetDlgItem(IDC_PROGRESS));
    ::SetWindowTheme(m_Progress, L"Explorer", L"");
    m_Progress.SetMarquee(TRUE);
    UpdateUI();

    return 0;
}

LRESULT CFindDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    m_Searcher.Cancel();
    UpdateOptions();
    ShowWindow(SW_HIDE);
    return 0;
}

LRESULT CFindDlg::OnFind(WORD, WORD wID, HWND, BOOL&) {
    auto options = UpdateOptions();
    GetDlgItem(IDC_FIND).EnableWindow(FALSE);
    GetDlgItem(IDC_NEW).EnableWindow(FALSE);
    GetDlgItem(IDC_CANCEL).EnableWindow();
    m_Progress.ShowWindow(SW_SHOW);

    CString text;
    GetDlgItemText(IDC_TEXT, text);
    m_Searcher.SetText(text);
    m_Searcher.SetOptions(options);
    SetDlgItemText(IDC_STATUS, L"Searching...");
    if (m_Searcher.IsRunning()) {
        m_Searcher.Continue();
        return 0;
    }
  
    m_Searcher.SetStartKey(m_pFrame->GetCurrentKeyPath());
    m_Searcher.Find([&](auto path, auto name, auto data) {
        ResetUI();
        if (path == nullptr) {
            // search done
            m_pFrame->OnFindEnd(m_Searcher.IsCancelled());
        }
        else {
            m_pFrame->OnFindNext(path, name, data);
        }
        });

    return 0;
}

LRESULT CFindDlg::OnCancel(WORD, WORD wID, HWND, BOOL&) {
    if(m_Searcher.IsRunning())
        m_Searcher.Cancel();

    return 0;
}

LRESULT CFindDlg::OnNewSearch(WORD, WORD wID, HWND, BOOL&) {
    m_Searcher.Cancel();
    return 0;
}

LRESULT CFindDlg::OnTextChanged(WORD, WORD wID, HWND, BOOL&) {
    GetDlgItem(IDC_FIND).EnableWindow(GetDlgItem(IDC_TEXT).GetWindowTextLength() > 0);
    return 0;
}

LRESULT CFindDlg::OnClick(WORD, WORD wID, HWND, BOOL&) {
    if (GetDlgItem(IDC_FIND).IsWindowEnabled()) {
        if (IsDlgButtonChecked(IDC_SEARCH_STD) == BST_UNCHECKED && IsDlgButtonChecked(IDC_SEARCH_REAL) == BST_UNCHECKED)
            GetDlgItem(IDC_FIND).EnableWindow(FALSE);
    }
    return 0;
}

void CFindDlg::ResetUI() {
    GetDlgItem(IDC_FIND).EnableWindow(TRUE);
    GetDlgItem(IDC_CANCEL).EnableWindow(FALSE);
    GetDlgItem(IDC_NEW).EnableWindow();
    m_Progress.ShowWindow(SW_HIDE);
    SetDlgItemText(IDC_STATUS, L"");
}
