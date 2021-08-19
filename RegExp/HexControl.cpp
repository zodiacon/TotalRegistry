#include "pch.h"
#include "HexControl.h"

void CHexControl::DoPaint(CDCHandle dc, RECT& rect) {
	auto back = m_ReadOnly ? RGB(192, 192, 192) : m_Colors.Background;
	dc.FillSolidRect(&rect, back);
	if (!m_Buffer)
		return;

	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS;
	GetScrollInfo(SB_HORZ, &si);
	int xstart = -si.nPos * m_CharWidth;

	dc.SelectFont(m_Font);
	dc.SetBkMode(OPAQUE);
	dc.SetBkColor(back);

	WCHAR str[20];
	int i = 0;
	uint8_t data[512];
	POLYTEXT poly[128] = { 0 };

	int addrLength = m_AddressDigits;
	const std::wstring addrFormat = L"%0" + std::to_wstring(addrLength) + L"X";
	int x = (addrLength + 1) * m_CharWidth;

	// data
	m_Text.clear();
	int lines = 1;
	std::wstring format(L"%0" + std::to_wstring(m_DataSize * 2) + L"llX ");
	int factor = m_CharWidth * (m_DataSize * 2 + 1);
	for (int y = 0;; y++) {
		auto offset = m_StartOffset + i;
		int count = m_Buffer->GetData(offset, data, m_BytesPerLine);
		if (count == 0)
			break;
		if (m_EditDigits > 0 && m_CaretOffset >= offset && m_CaretOffset < offset + m_BytesPerLine) {
			// changed data
			memcpy(data + m_CaretOffset - offset, &m_CurrentInput, m_DataSize);
		}
		int jcount = std::min(m_BytesPerLine / m_DataSize, count);
		for (int j = 0; j < jcount; j++) {
			if (offset + j * m_DataSize >= m_EndOffset)
				break;
			auto ds = int(m_EndOffset - offset - j * m_DataSize);
			ATLASSERT(ds >= 1);
			if (ds > m_DataSize)
				ds = m_DataSize;
			if (ds & (ds - 1))
				ds = m_DataSize >> 1;

			switch (ds) {
				case 1:	::StringCchPrintf(str, _countof(str), L"%02X", data[j * m_DataSize]); break;
				case 2:	::StringCchPrintf(str, _countof(str), L"%04X", *(WORD*)&data[j * m_DataSize]); break;
				case 4:	::StringCchPrintf(str, _countof(str), L"%08X", *(DWORD*)&data[j * m_DataSize]); break;
				case 8:	::StringCchPrintf(str, _countof(str), L"%0llX", *(DWORD64*)&data[j * m_DataSize]); break;
			}
			bool selected = m_Selection.IsSelected(offset + j);
			dc.SetTextColor(selected ? m_Colors.SelectionText : m_Colors.Text);
			dc.SetBkColor(selected ? m_Colors.SelectionBackground : back);

			dc.TextOut(x + xstart + j * factor, y * m_CharHeight, str, ds * 2);
		}
		if (y * m_CharHeight > rect.bottom)
			break;
		i += m_BytesPerLine;
		lines++;
	}

	// offsets
	dc.SetTextColor(m_Colors.Offset);
	dc.SetBkColor(m_Colors.Background);
	x = 0;
	m_Text.clear();
	int nlines = std::max(1, lines);
	m_Text.resize(nlines);
	for (int y = 0; y < nlines; y++) {
		std::wstring text;
		::StringCchPrintf(str, _countof(str), addrFormat.c_str(), m_StartOffset + y * m_BytesPerLine);
		text = str;

		auto& p = poly[y];
		p.x = x + xstart;
		p.y = y * m_CharHeight;
		p.n = (UINT)text.size();
		m_Text[y] = text;
		p.lpstr = m_Text[y].c_str();
	}
	::PolyTextOut(dc, poly, nlines);

	// ASCII
	dc.SetTextColor(m_Colors.Ascii);
	x = m_CharWidth * (10 + m_BytesPerLine * (m_DataSize * 2 + 1) / m_DataSize);
	m_Text.clear();
	i = 0;
	for (int y = 0;; y++) {
		auto count = m_Buffer->GetData(m_StartOffset + i, data, m_BytesPerLine);
		if (count == 0)
			break;
		if (m_EditDigits > 0 && m_CaretOffset >= m_StartOffset + i && m_CaretOffset < m_StartOffset + i + m_BytesPerLine) {
			// changed data
			memcpy(data + m_CaretOffset - m_StartOffset - i, &m_CurrentInput, m_DataSize);
		}
		std::wstring text;
		for (uint32_t j = 0; j < count; j++)
			text += data[j] < 32 || data[j] > 127 ? L'.' : (wchar_t)data[j];

		auto& p = poly[y];
		p.lpstr = text.c_str();
		p.x = x + xstart;
		p.y = y * m_CharHeight;
		p.n = (UINT)text.size();
		m_Text.push_back(std::move(text));
		if (y * m_CharHeight > rect.bottom)
			break;
		i += m_BytesPerLine;
	}
	if (!m_Text.empty())
		::PolyTextOut(dc, poly, (int)m_Text.size());

	UpdateCaret();
}

LRESULT CHexControl::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	CreateSolidCaret(m_InsertMode ? 2 : m_CharWidth, m_CharHeight);
	ShowCaret();
	UpdateCaret();

	return 0;
}

LRESULT CHexControl::OnKillFocus(UINT, WPARAM, LPARAM, BOOL&) {
	HideCaret();
	DestroyCaret();
	return 0;
}

LRESULT CHexControl::OnLeftButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&) {
	m_Selection.Clear();

	SetCapture();
	int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
	auto offset = GetOffsetFromPoint(CPoint(x, y));
	if (offset >= 0) {
		m_CaretOffset = offset;
		UpdateCaret();
	}
	RedrawWindow();
	return 0;
}

LRESULT CHexControl::OnGetDialogCode(UINT, WPARAM, LPARAM, BOOL&) {
	return DLGC_WANTALLKEYS;
}

LRESULT CHexControl::OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL&) {
	if (m_Buffer == nullptr)
		return 0;

	auto current = m_CaretOffset;
	bool abortEdit = false;
	bool shift = ::GetKeyState(VK_SHIFT) & 0x80;
	bool alt = ::GetKeyState(VK_CONTROL) & 0x80;

	bool redraw = false;

	if (shift) {
		if (m_Selection.IsEmpty()) {
			m_Selection.SetAnchor(m_CaretOffset);
		}
	}

	switch (wParam) {
		case VK_BACK:
			if (m_Selection.IsEmpty()) {
				m_Buffer->Delete(m_CaretOffset - 1, m_DataSize);
				m_CaretOffset--;
			}
			else {
				m_Buffer->Delete(m_Selection.GetOffset(), m_Selection.GetLength());
			}
			RedrawWindow();
			m_NotifyData.hdr.code = HCN_SIZECHANGED;
			SendNotify(&m_NotifyData);
			return 0;

		case VK_DELETE:
			if (m_Selection.IsEmpty()) {
				m_Buffer->Delete(m_CaretOffset, m_DataSize);
			}
			else {
				m_Buffer->Delete(m_Selection.GetOffset(), m_Selection.GetLength());
			}
			RedrawWindow();
			m_NotifyData.hdr.code = HCN_SIZECHANGED;
			SendNotify(&m_NotifyData);

			return 0;

		case VK_ESCAPE:
			if (m_EditDigits > 0) {
				abortEdit = true;
			}
			break;

		case VK_DOWN:
			if (m_CaretOffset + m_BytesPerLine < m_Buffer->GetSize())
				m_CaretOffset += m_BytesPerLine;
			else
				m_CaretOffset = m_Buffer->GetSize();
			break;

		case VK_UP:
			if (m_CaretOffset >= m_BytesPerLine)
				m_CaretOffset -= m_BytesPerLine;
			else
				m_CaretOffset = 0;
			break;

		case VK_RIGHT:
			m_CaretOffset += m_DataSize;
			if (m_CaretOffset > m_Buffer->GetSize())
				m_CaretOffset = m_Buffer->GetSize() + m_DataSize - 1;
			break;

		case VK_LEFT:
			m_CaretOffset -= m_DataSize;
			if (m_CaretOffset < 0)
				m_CaretOffset = 0;
			break;

		case VK_NEXT:
			if (m_CaretOffset + m_BytesPerLine * 20 <= m_Buffer->GetSize())
				m_CaretOffset += m_BytesPerLine * 20;
			break;

			//case VK_PRIOR:
			//	if (m_CaretOffset >= m_BytesPerLine * 20)
			//		m_CaretOffset -= m_BytesPerLine * 20;
			//	else
			//		m_CaretOffset = m_CaretOffset % m_BytesPerLine;
			//	break;
	}
	if (!shift && !m_Selection.IsEmpty()) {
		ClearSelection();
		redraw = true;
	}
	if (shift && m_CaretOffset != current) {
		if (alt) {
			m_Selection.SetBox(std::min(m_CaretOffset, m_Selection.GetAnchor()), m_BytesPerLine,
				(m_CaretOffset - m_Selection.GetAnchor()) % m_BytesPerLine, (int)(m_CaretOffset - m_Selection.GetAnchor()) / m_BytesPerLine);
		}
		else {
			m_Selection.SetSimple(std::min(m_CaretOffset, m_Selection.GetAnchor()), abs(m_CaretOffset - m_Selection.GetAnchor()));
		}
		redraw = true;
	}

	NormalizeOffset(m_CaretOffset);
	if (abortEdit)
		CommitValue(current, m_CurrentInput);

	//if (m_SelStart >= 0 && selLength != m_SelLength)
	//	DrawSelection(current);

	if (m_CaretOffset != current) {
		if (!abortEdit && m_EditDigits > 0)
			CommitValue(current, m_CurrentInput);

		if (m_CaretOffset > m_EndOffset - m_BytesPerLine * 3)
			SendMessage(WM_VSCROLL, SB_LINEDOWN);
		else if (m_CaretOffset < m_StartOffset + m_BytesPerLine * 3)
			SendMessage(WM_VSCROLL, SB_LINEUP);
		auto pt = GetPointFromOffset(m_CaretOffset);
		SetCaretPos(pt.x, pt.y);
	}
	if (redraw)
		RedrawWindow();
	return 0;
}

void CHexControl::ResetInput() {
	m_EditDigits = 0;
	m_CurrentInput = 0;
}

int64_t CHexControl::NormalizeOffset(int64_t offset) const {
	offset -= offset % m_DataSize;
	return offset;
}

void CHexControl::RedrawCaretLine() {
	auto pos = GetPointFromOffset(m_CaretOffset);
	RECT rcClient;
	GetClientRect(&rcClient);
	RECT rc = { pos.x - m_CharWidth, pos.y, rcClient.right, pos.y + m_CharHeight };
	RedrawWindow(&rc);
}

void CHexControl::CommitValue(int64_t offset, uint64_t value) {
	if (offset > m_EndOffset) {
		m_EndOffset = offset;
		RecalcLayout();
	}

	if (m_CaretOffset == m_Buffer->GetSize()) {
		m_Buffer->Increase(m_DataSize);
		m_NotifyData.hdr.code = HCN_SIZECHANGED;
		SendNotify(&m_NotifyData);
	}

	DWORD64 oldValue;
	m_Buffer->GetData(offset, (uint8_t*)&oldValue, m_DataSize);
	m_Buffer->SetData(offset, (const uint8_t*)&value, m_DataSize);

	ResetInput();
	if (m_CaretOffset > m_EndOffset - m_BytesPerLine * 2)
		SendMessage(WM_VSCROLL, SB_LINEDOWN);
}

LRESULT CHexControl::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	InitFontMetrics();
	m_NotifyData.hdr.hwndFrom = m_hWnd;
	m_NotifyData.hdr.idFrom = GetWindowLongPtr(GWLP_ID);
	SetCaretBlinkTime(500);

	return 0;
}

LRESULT CHexControl::OnHScroll(UINT, WPARAM wParam, LPARAM, BOOL&) {
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(SB_HORZ, &si);
	auto pos = si.nPos;

	switch (LOWORD(wParam)) {
		case SB_LINELEFT:
			si.nPos--;
			break;

		case SB_LINERIGHT:
			si.nPos++;
			break;

		case SB_PAGELEFT:
			si.nPos -= si.nPage;
			break;

		case SB_PAGERIGHT:
			si.nPos += si.nPage;
			break;

		case SB_THUMBTRACK:
			si.nPos = si.nTrackPos;
			break;

	}
	si.fMask = SIF_POS;
	SetScrollInfo(SB_HORZ, &si);
	GetScrollInfo(SB_HORZ, &si);
	if (si.nPos != pos) {
		RedrawWindow();
	}
	return 0;
}

LRESULT CHexControl::OnVScroll(UINT, WPARAM wParam, LPARAM, BOOL&) {
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(SB_VERT, &si);
	auto pos = si.nPos;

	switch (LOWORD(wParam)) {
		case SB_TOP:
			si.nPos = si.nMin;
			break;

		case SB_BOTTOM:
			si.nPos = si.nMax;
			break;

		case SB_LINEUP:
			si.nPos--;
			break;

		case SB_LINEDOWN:
			si.nPos++;
			break;

		case SB_PAGEUP:
			si.nPos -= si.nPage;
			break;

		case SB_PAGEDOWN:
			si.nPos += si.nPage;
			break;

		case SB_THUMBTRACK:
			si.nPos = si.nTrackPos;
			break;
	}
	if (si.nPos != pos) {
		si.fMask = SIF_POS;
		SetScrollInfo(SB_VERT, &si);
		GetScrollInfo(SB_VERT, &si);
		m_StartOffset = si.nPos * m_BytesPerLine;
		if (m_StartOffset + m_Lines * m_BytesPerLine >= m_Buffer->GetSize()) {
			m_StartOffset = m_Buffer->GetSize() - (m_Lines - 1) * m_BytesPerLine;
			if (m_StartOffset < 0)
				m_StartOffset = 0;
			m_StartOffset = m_StartOffset - m_StartOffset % m_BytesPerLine;
			ATLASSERT(m_StartOffset >= 0);
		}
		m_EndOffset = m_StartOffset + m_Lines * m_BytesPerLine;
		if (m_EndOffset > m_Buffer->GetSize())
			m_EndOffset = m_Buffer->GetSize();
		RedrawWindow();
	}
	return 0;
}

void CHexControl::RecalcLayout() {
	if (m_Buffer == nullptr) {
		return;
	}

	CRect rc;
	GetClientRect(&rc);

	auto lines = int((m_Buffer->GetSize() + (m_BytesPerLine - 1)) / m_BytesPerLine);
	if (m_Buffer->GetSize() % m_BytesPerLine != 0)
		lines++;

	m_Lines = rc.Height() / m_CharHeight;

	m_AddressDigits = m_Buffer->GetSize() < 1LL << 16 ? 4 : 8;

	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_PAGE | SIF_RANGE;
	si.nMin = 0;
	si.nMax = lines - 1;
	si.nPage = rc.bottom / m_CharHeight;
	SetScrollInfo(SB_VERT, &si);

	auto chars = m_AddressDigits + m_BytesPerLine * 2 + (m_BytesPerLine / m_DataSize) + m_BytesPerLine;

	si.nMax = static_cast<int>(chars) - 1;
	si.nPage = rc.right / m_CharWidth;
	SetScrollInfo(SB_HORZ, &si);

	si.fMask = SIF_POS;
	GetScrollInfo(SB_VERT, &si);

	m_StartOffset = si.nPos * m_BytesPerLine;
	ATLASSERT(m_StartOffset >= 0);
	if (m_StartOffset + m_Lines * m_BytesPerLine >= m_Buffer->GetSize()) {
		m_StartOffset = m_Buffer->GetSize() - (m_Lines - 1) * m_BytesPerLine;
		if (m_StartOffset < 0)
			m_StartOffset = 0;
		m_StartOffset = m_StartOffset - m_StartOffset % m_BytesPerLine;
		ATLASSERT(m_StartOffset >= 0);
	}
	m_EndOffset = m_StartOffset + m_Lines * m_BytesPerLine;
	if (m_EndOffset > m_Buffer->GetSize())
		m_EndOffset = m_Buffer->GetSize();
	RedrawWindow();
}

void CHexControl::InitFontMetrics() {
	if (m_Font)
		m_Font.DeleteObject();
	m_Font.CreatePointFont(m_FontPointSize, L"Consolas");
	CClientDC dc(*this);
	dc.SelectFont(m_Font);
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	m_CharHeight = tm.tmHeight;
	m_CharWidth = tm.tmAveCharWidth;
}

CPoint CHexControl::GetPointFromOffset(int64_t offset) const {
	if (offset < m_StartOffset || offset > m_EndOffset)
		return CPoint(-1, -1);

	int line = int((offset - m_StartOffset) / m_BytesPerLine);
	int b = (offset - m_StartOffset) % m_BytesPerLine;

	CPoint pt;
	pt.y = line * m_CharHeight;
	pt.x = (b * (m_DataSize * 2 + 1) + (m_AddressDigits + 1)) * m_CharWidth;
	ATLTRACE(L"GetPointFromOffset %llX: (%d,%d)\n", offset, pt.x, pt.y);

	return pt;
}

int64_t CHexControl::GetOffsetFromPoint(const POINT& pt) const {
	int line = pt.y / m_CharHeight;
	int b = pt.x / m_CharWidth - (m_AddressDigits + 1);

	b /= m_DataSize * 2 + 1;
	if (b < 0 || b >= m_BytesPerLine)
		return -1;

	return m_StartOffset + line * m_BytesPerLine + b;
}

void CHexControl::DrawNumber(CDCHandle dc, int64_t offset, uint64_t value, int digitsChanged) {
	auto pos = GetPointFromOffset(offset);
	CString temp = FormatNumber(value);
	HideCaret();
	bool selected = false;
	dc.SetTextColor(selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : m_Colors.Text);
	dc.SetBkColor(selected ? ::GetSysColor(COLOR_HIGHLIGHT) : ::GetSysColor(COLOR_WINDOW));
	if (digitsChanged < m_DataSize * 2)
		dc.TextOutW(pos.x, pos.y, temp, m_DataSize * 2);
	dc.SetTextColor(RGB(255, 0, 0));
	dc.TextOutW(pos.x + m_CharWidth * (m_DataSize * 2 - digitsChanged), pos.y, temp.Right(digitsChanged), digitsChanged);
	ShowCaret();
}

void CHexControl::UpdateCaret() {
	auto pt = GetPointFromOffset(m_CaretOffset);
	HideCaret();
	if (pt.y >= 0) {
		SetCaretPos(pt.x, pt.y);
		ShowCaret();
	}
	else {
		SetCaretPos(-1000, -1000);
		ShowCaret();
	}
}

void CHexControl::RedrawWindow(RECT* rc) {
	::RedrawWindow(m_hWnd, rc, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void CHexControl::ClearSelection() {
	m_Selection.Clear();
}

LRESULT CHexControl::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL&) {
	if (GetCapture() != m_hWnd)
		return 0;

	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	//bool asc;
	auto offset = GetOffsetFromPoint(pt);
	if (offset > m_EndOffset)
		offset = m_EndOffset;

	m_Selection.SetSimple(std::min(offset, m_CaretOffset), abs(offset - m_CaretOffset));
	RedrawWindow();

	return 0;
}

LRESULT CHexControl::OnLeftButtonUp(UINT, WPARAM, LPARAM, BOOL&) {
	ReleaseCapture();
	return 0;
}

LRESULT CHexControl::OnContextMenu(UINT, WPARAM, LPARAM, BOOL&) {
	m_NotifyData.hdr.code = HCN_RCLICK;
	return SendNotify(&m_NotifyData);
}

LRESULT CHexControl::OnMouseWheel(UINT, WPARAM wParam, LPARAM, BOOL&) {
	auto delta = GET_WHEEL_DELTA_WPARAM(wParam);
	if (delta == 0)
		return 0;
	int scroll = WHEEL_DELTA / 2;

	auto keys = GET_KEYSTATE_WPARAM(wParam);
	if (keys == 0) {
		if (delta > 0)
			while ((delta -= scroll) >= 0)
				SendMessage(WM_VSCROLL, SB_LINEUP);
		else
			while ((delta += scroll) <= 0)
				SendMessage(WM_VSCROLL, SB_LINEDOWN);
	}
	else if (keys == MK_CONTROL) {
		auto oldSize = m_FontPointSize;
		// change font size
		m_FontPointSize = static_cast<int>(m_FontPointSize * (delta > 0 ? 1.1 : (1 / 1.1)));
		if (m_FontPointSize < 70)
			m_FontPointSize = 70;
		else if (m_FontPointSize > 300)
			m_FontPointSize = 300;

		if (m_FontPointSize != oldSize) {
			InitFontMetrics();
			RecalcLayout();
		}
	}

	return 0;
}
LRESULT CHexControl::OnChar(UINT, WPARAM wParam, LPARAM, BOOL&) {
	if (IsReadOnly())
		return 0;

	if (wParam == VK_BACK)
		return 0;

	bool digit = wParam >= '0' && wParam <= '9';
	bool hexdigit = wParam >= 'A' && wParam <= 'F' || wParam >= 'a' && wParam <= 'f';

	if (!digit && !hexdigit) {
		::MessageBeep((UINT)-1);
		return 0;
	}

	uint8_t value;
	if (digit)
		value = static_cast<uint8_t>(wParam) - '0';
	else {
		if (wParam > 0x60)
			wParam -= 0x20;
		value = static_cast<uint8_t>(wParam) - 'A' + 10;
	}

	if (m_EditDigits == 0) {
		// save old value
		BYTE buffer[8] = { 0 };
		m_Buffer->GetData(m_CaretOffset, buffer, m_DataSize);
		m_OldValue = *(uint64_t*)buffer;
	}

	m_CurrentInput = (m_CurrentInput << 4) | value;
	m_EditDigits++;

	auto pos = GetPointFromOffset(m_CaretOffset);
	if (m_EditDigits == m_DataSize * 2) {
		CommitValue(m_CaretOffset, m_CurrentInput);
		// end of number
		m_CaretOffset += m_DataSize;
		if (m_CaretOffset >= m_EndOffset && m_CaretOffset % m_BytesPerLine == 0) {
			//			DrawOffset(m_CaretOffset);
		}
		SetCaretPos(pos.x, pos.y);
		RECT rcClient;
		GetClientRect(&rcClient);
		RECT rc = { pos.x - m_CharWidth, pos.y, rcClient.right, pos.y + m_CharHeight };
		RedrawWindow(&rc);
	}
	else {
		CClientDC dc(m_hWnd);
		DrawNumber(dc.m_hDC, m_CaretOffset, m_CurrentInput, m_EditDigits);
	}
	return 0;
}

PCWSTR CHexControl::FormatNumber(ULONGLONG number) const {
	static const struct {
		PCWSTR format;
		int bytes;
	} format[]{
		{ nullptr, 0 },
		{ L"%02X ", 1 },
		{ L"%04X ", 2 },
		{ nullptr, 0 },
		{ L"%08X ", 4 },
		{ nullptr, 0 },
		{ nullptr, 0 },
		{ nullptr, 0 },
		{ L"%016llX ", 8 },
	};

	static CString result;
	if (m_DataSize < 8)
		result.Format(format[m_DataSize].format, number & ((1LL << (8 * format[m_DataSize].bytes)) - 1));
	else
		result.Format(format[m_DataSize].format, number);
	return result.TrimRight();
}

LRESULT CHexControl::SendNotify(HCNOTIFY* notify) {
	return GetParent().SendMessage(WM_NOTIFY, notify->hdr.idFrom, reinterpret_cast<LPARAM>(notify));
}

LRESULT CHexControl::OnSize(UINT, WPARAM, LPARAM, BOOL&) {
	RecalcLayout();

	return 0;
}

HWND CHexControl::GetHwnd() const {
	return m_hWnd;
}

void CHexControl::SetBufferManager(IBufferManager* mgr) {
	m_Buffer = mgr;
	RecalcLayout();
}

IBufferManager* CHexControl::GetBufferManager() const {
	return m_Buffer;
}

void CHexControl::SetReadOnly(bool readonly) {
	m_ReadOnly = readonly;
	Invalidate();
}

bool CHexControl::IsReadOnly() const {
	return m_ReadOnly || (m_Buffer && m_Buffer->IsReadOnly());
}

void CHexControl::SetAllowExtension(bool allow) {
}

bool CHexControl::IsAllowExtension() const {
	return false;
}

void CHexControl::SetSize(int64_t size) {
}

bool CHexControl::SetDataSize(int32_t size) {
	if (size > 8 || size < 1 || (size & (size - 1)) != 0)
		return false;

	m_DataSize = size;
	RecalcLayout();

	return true;
}

int32_t CHexControl::GetDataSize() const {
	return m_DataSize;
}

bool CHexControl::SetBytesPerLine(int32_t bytesPerLine) {
	if (bytesPerLine % 8 != 0)
		return false;

	m_BytesPerLine = bytesPerLine;
	RecalcLayout();
	return true;
}

int32_t CHexControl::GetBytesPerLine() const {
	return int32_t();
}

bool CHexControl::Copy(int64_t offset, int64_t size) {
	return false;
}

bool CHexControl::Paste(int64_t offset) {
	return false;
}

bool CHexControl::CanCopy() const {
	return false;
}

bool CHexControl::CanPaste() const {
	return false;
}

bool CHexControl::Cut(int64_t offset, int64_t size) {
	return false;
}

bool CHexControl::Delete(int64_t offset, int64_t size) {
	return false;
}

bool CHexControl::CanCut() const {
	return false;
}

bool CHexControl::CanDelete() const {
	return false;
}

int64_t CHexControl::SetBiasOffset(int64_t offset) {
	return int64_t();
}

int64_t CHexControl::GetBiasOffset() const {
	return int64_t();
}

HexControlColors& CHexControl::GetColors() {
	return m_Colors;
}

std::wstring CHexControl::GetText(int64_t offset, int64_t size) {
	return std::wstring();
}

void CHexControl::Refresh() {
	RecalcLayout();
}

void Selection::SetSimple(int64_t offset, int64_t len) {
	_type = SelectionType::Simple;
	_offset = offset;
	_length = len;
}

void Selection::SetBox(int64_t offset, int bytesPerLine, int width, int height) {
	_type = SelectionType::Box;
	_offset = offset;
	_width = width;
	_height = height;
	_bytesPerLine = bytesPerLine;
}

void Selection::SetAnchor(int64_t offset) {
	_anchor = offset;
}

int64_t Selection::GetOffset() const {
	return _offset;
}

int64_t Selection::GetAnchor() const {
	return _anchor;
}

bool Selection::IsSelected(int64_t offset) const {
	switch (_type) {
		case SelectionType::Simple:
			return offset >= _offset && offset < _offset + _length;

		case SelectionType::Box:
			if (offset < _offset)
				return false;
			return (offset - _offset) % _bytesPerLine < _width && (offset - _offset) / _bytesPerLine < _height;
	}
	ATLASSERT(false);
	return false;
}

bool Selection::IsEmpty() const {
	return _offset < 0 || _length == 0;
}

SelectionType Selection::GetSelectionType() const {
	return _type;
}

void Selection::Clear() {
	_type = SelectionType::Simple;
	_offset = _anchor = -1;
	_length = 0;
}
