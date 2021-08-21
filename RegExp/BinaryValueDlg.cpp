#include "pch.h"
#include "resource.h"
#include "BinaryValueDlg.h"
#include "Helpers.h"
#include "ThemeHelper.h"
#include "Theme.h"

CBinaryValueDlg::CBinaryValueDlg(RegistryKey& key, PCWSTR name, bool readOnly, IMainFrame* frame)
	: m_Key(key), m_Name(name), m_ReadOnly(readOnly), m_pFrame(frame) {
}

const std::vector<BYTE>& CBinaryValueDlg::GetValue() const {
	return m_Value;
}

bool CBinaryValueDlg::IsModified() const {
	return m_Modified;
}

void CBinaryValueDlg::BuildToolBar(CRect& rc) {
	CToolBarCtrl tb;
	tb.Create(m_hWnd, &rc, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
	tb.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);

	CImageList tbImages;
	tbImages.Create(16, 16, ILC_COLOR32, 8, 4);
	tb.SetImageList(tbImages);

	const struct {
		UINT id;
		int image;
		BYTE style = BTNS_BUTTON;
		BYTE state = TBSTATE_ENABLED;
		PCWSTR text = nullptr;
	} buttons[] = {
		{ ID_DATA_BYTE, IDI_NUM1, BTNS_BUTTON | BTNS_CHECKGROUP, TBSTATE_ENABLED | TBSTATE_CHECKED },
		{ ID_DATA_BYTE + 1, IDI_NUM2, BTNS_BUTTON | BTNS_CHECKGROUP },
		{ ID_DATA_BYTE + 2, IDI_NUM4, BTNS_BUTTON | BTNS_CHECKGROUP },
		{ ID_DATA_BYTE + 3, IDI_NUM8_2, BTNS_BUTTON | BTNS_CHECKGROUP },
		{ 0 },
		{ ID_LINE, IDI_OPTIONS, BTNS_BUTTON | BTNS_DROPDOWN, TBSTATE_ENABLED, L"Bytes / Line" },
	};
	for (auto& b : buttons) {
		if (b.id == 0)
			tb.AddSeparator(0);
		else {
			HICON hIcon = nullptr;
			int image = -1;
			if (b.image) {
				hIcon = AtlLoadIconImage(b.image, 0, 16, 16);
				ATLASSERT(hIcon);
				image = tbImages.AddIcon(hIcon);
			}
			tb.AddButton(b.id, b.style | (b.text ? BTNS_SHOWTEXT : 0), b.state, image, b.text, 0);
		}
	}
	UIAddToolBar(tb);
}

void CBinaryValueDlg::UpdateBufferSize() {
	CString text;
	auto bytes = (ULONG)m_Buffer.GetSize();
	text.Format(L"Size: %u bytes", bytes);
	SetDlgItemText(IDC_BUFFERSIZE, text);
}

LRESULT CBinaryValueDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_BINARY);

	CRect rc;
	GetDlgItem(IDC_BUTTON1).GetWindowRect(&rc);
	ScreenToClient(&rc);
	GetDlgItem(IDC_BUTTON1).ShowWindow(SW_HIDE);

	m_Hex.Create(m_hWnd, &rc, nullptr, 
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_CLIENTEDGE, IDC_HEX);
	rc.OffsetRect(0, -30);
	if (!ThemeHelper::IsDefault()) {
		auto theme = ThemeHelper::GetCurrentTheme();
		auto& colors = m_Hex.GetColors();
		colors.Offset = theme->TextColor;
		colors.Ascii = theme->TextColor;
	}

	BuildToolBar(rc);

	Helpers::RestoreWindowPosition(m_hWnd, L"BinaryValueDialog");
	PostMessage(WM_SIZE);

	ULONG bytes = 0;
	m_Key.QueryBinaryValue(m_Name, nullptr, &bytes);
	m_Value.resize(bytes);
	if (ERROR_SUCCESS != m_Key.QueryBinaryValue(m_Name, m_Value.data(), &bytes)) {
		EndDialog(IDRETRY);
		return 0;
	}

	m_Buffer.Init(m_Value.data(), (uint32_t)m_Value.size());

	UpdateBufferSize();

	m_Hex.SetReadOnly(m_ReadOnly);
	m_Hex.SetBufferManager(&m_Buffer);
	m_Hex.SetBytesPerLine(8);
	m_Hex.SetBiasOffset(0);
	m_Hex.SetFocus();

	SetDlgItemText(IDC_NAME, m_Name.IsEmpty() ? Helpers::DefaultValueName : m_Name);

	return 0;
}

LRESULT CBinaryValueDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	Helpers::SaveWindowPosition(m_hWnd, L"BinaryValueDialog");
	handled = FALSE;
	return 0;
}

LRESULT CBinaryValueDlg::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
	if (m_Hex) {
		CRect rc;
		GetDlgItem(IDC_BUTTON1).GetWindowRect(&rc);
		ScreenToClient(&rc);
		m_Hex.MoveWindow(&rc);
	}
	handled = FALSE;
	return 0;
}

LRESULT CBinaryValueDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (IDOK == wID && !m_ReadOnly) {
		if (m_Buffer.GetSize() != m_Value.size())
			m_Modified = true;
		else {
			m_Modified = 0 != ::memcmp(m_Value.data(), m_Buffer.GetRawData(0), m_Value.size());
		}
		if (m_Modified) {
			m_Value.resize(m_Buffer.GetSize());
			m_Buffer.GetData(0, m_Value.data(), (DWORD)m_Buffer.GetSize());
		}
	}
	EndDialog(wID);
	return 0;
}

LRESULT CBinaryValueDlg::OnHexBufferSizeChanged(int, LPNMHDR, BOOL&) {
	UpdateBufferSize();
	return 0;
}

LRESULT CBinaryValueDlg::OnDataSize(WORD, WORD id, HWND, BOOL&) {
	m_Hex.SetDataSize(1 << (id - ID_DATA_BYTE));

	return 0;
}

LRESULT CBinaryValueDlg::OnToolBarDropdown(int, LPNMHDR hdr, BOOL&) {
	auto tb = (NMTOOLBAR*)hdr;
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	CWindow win(hdr->hwndFrom);
	CRect rc(tb->rcButton);
	rc.OffsetRect(0, rc.Height());
	win.ClientToScreen(&rc);
	auto id = (UINT)m_pFrame->TrackPopupMenu(menu.GetSubMenu(4), TPM_RETURNCMD, rc.left, rc.top);
	if (id) {
		LRESULT result = 0;
		ProcessWindowMessage(m_hWnd, WM_COMMAND, id, 0, result);
	}
	return 0;
}

LRESULT CBinaryValueDlg::OnBytesPerLine(WORD, WORD id, HWND, BOOL&) {
	m_Hex.SetBytesPerLine(8 * (id - ID_HEX_8BYTES + 1));

	return 0;
}
