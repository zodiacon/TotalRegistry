#include "pch.h"
#include "resource.h"
#include "KeyPropertiesDlg.h"
#include "RegistryMonitor.h"
#include "Helpers.h"
#include <ListViewhelper.h>
#include <ClipboardHelper.h>

namespace {
	// the kernel path, with the standard path after it when there is one
	CString DescribeKernelPath(CString const& kernel) {
		auto standard = RegistryMonitor::KernelPathToStandard(kernel, Helpers::GetCurrentUserSid());
		return standard == kernel ? kernel : standard + L"  (" + kernel + L")";
	}
}

CKeyPropertiesDlg::CKeyPropertiesDlg(CString const& path, KeyInfo const& info) : m_Path(path), m_Info(info) {
}

void CKeyPropertiesDlg::AddRow(PCWSTR name, CString const& value) {
	auto n = m_List.InsertItem(m_List.GetItemCount(), name);
	m_List.SetItemText(n, 1, value);
}

LRESULT CKeyPropertiesDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	CenterWindow(GetParent());
	SetDialogIcon(IDI_PROPERTIES);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	::SetWindowTheme(m_List, L"Explorer", L"");
	m_List.InsertColumn(0, L"Property", LVCFMT_LEFT, 110);
	m_List.InsertColumn(1, L"Value", LVCFMT_LEFT, 560);

	auto& info = m_Info;
	CString text;
	AddRow(L"Name", m_Path.Mid(m_Path.ReverseFind(L'\\') + 1));
	AddRow(L"Path", m_Path);
	if (!info.KernelPath.IsEmpty())
		AddRow(L"Stored at", info.KernelPath);

	text = info.IsLink ? L"Symbolic link" : L"Key";
	if (info.Volatile)
		text = L"Volatile " + text.MakeLower() + L" (not saved to disk)";
	AddRow(L"Type", text);
	if (info.IsLink)
		AddRow(L"Link target", DescribeKernelPath(info.LinkTarget));

	if (info.LastWrite.dwHighDateTime | info.LastWrite.dwLowDateTime)
		AddRow(L"Last written", CTime(info.LastWrite).Format(L"%x %X"));
	text.Format(L"%u", info.SubKeys);
	AddRow(L"Subkeys", text);
	text.Format(L"%u", info.Values);
	AddRow(L"Values", text);
	AddRow(L"Class", info.Class.IsEmpty() ? CString(L"(none)") : info.Class);
	AddRow(L"Owner", info.Owner.IsEmpty() ? CString(L"(unknown)") : info.Owner);

	if (info.HiveKey.IsEmpty())
		AddRow(L"Hive", info.KernelPath.IsEmpty() ? L"(unknown)" : L"(not in a hive)");
	else {
		AddRow(L"Hive", DescribeKernelPath(info.HiveKey) + (info.IsHiveRoot ? L"; this key is its root" : L""));
		AddRow(L"Hive file", info.HiveFile.IsEmpty() ? CString(L"(kept in memory only)") : info.HiveFile);
	}

	GetDlgItem(IDC_SHOWHIVE).EnableWindow(!info.HiveFile.IsEmpty());
	return TRUE;
}

LRESULT CKeyPropertiesDlg::OnClose(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

LRESULT CKeyPropertiesDlg::OnShowHive(WORD, WORD, HWND, BOOL&) {
	WCHAR explorer[MAX_PATH];
	::ExpandEnvironmentStrings(L"%systemroot%\\explorer.exe", explorer, _countof(explorer));
	::ShellExecute(nullptr, L"open", explorer, L"/select,\"" + m_Info.HiveFile + L"\"", nullptr, SW_SHOWNORMAL);
	return 0;
}

LRESULT CKeyPropertiesDlg::OnCopy(WORD, WORD, HWND, BOOL&) {
	ClipboardHelper::CopyText(m_hWnd, m_List.GetSelectedCount() == 0 ?
		ListViewHelper::GetAllRowsAsString(m_List) : ListViewHelper::GetSelectedRowsAsString(m_List));
	return 0;
}
