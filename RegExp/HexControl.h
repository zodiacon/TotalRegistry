#pragma once

#include <map>
#include <string>

struct IBufferManager {
	virtual uint32_t GetData(int64_t offset, uint8_t* buffer, uint32_t count) = 0;
	virtual bool Insert(int64_t offset, const uint8_t* data, uint32_t count) = 0;
	virtual bool Delete(int64_t offset, size_t count) = 0;
	virtual bool SetData(int64_t offset, const uint8_t* data, uint32_t count) = 0;
	virtual int64_t GetSize() const = 0;
	virtual uint8_t* GetRawData(int64_t offset) = 0;
	virtual bool IsReadOnly() const = 0;
	virtual bool Increase(uint32_t size) {
		return false;
	}
	virtual ~IBufferManager() = default;
};

struct HexControlColors {
	COLORREF Text{ ::GetSysColor(COLOR_WINDOWTEXT) };
	COLORREF Background{ ::GetSysColor(COLOR_WINDOW) };
	COLORREF Ascii{ RGB(128, 0, 0) };
	COLORREF Offset{ RGB(0, 0, 128) };
	COLORREF SelectionText{ ::GetSysColor(COLOR_HIGHLIGHTTEXT) };
	COLORREF SelectionBackground{ ::GetSysColor(COLOR_HIGHLIGHT) };
};

enum class SelectionType {
	Simple,
	Box
};

class Selection {
public:
	void SetSimple(int64_t offset, int64_t len);
	void SetBox(int64_t offset, int bytesPerLine, int width, int height);
	void SetAnchor(int64_t offset);

	int64_t GetOffset() const;
	int64_t GetAnchor() const;
	bool IsSelected(int64_t offset) const;
	bool IsEmpty() const;
	SelectionType GetSelectionType() const;
	int64_t GetLength() const {
		return _length;
	}

	void Clear();

private:
	SelectionType _type{ SelectionType::Simple };
	int64_t _offset{ -1 }, _anchor{ -1 };
	int64_t _length{ 0 };
	int _width, _height, _bytesPerLine;
};

const UINT HCN_SIZECHANGED = NM_FIRST - 3000;

struct HCNOTIFY {
	NMHDR hdr;
};

class CHexControl :
	public CBufferedPaintWindowImpl<CHexControl> {
public:
	DECLARE_WND_CLASS_EX(L"WTLHexControl", CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, NULL)

	HWND GetHwnd() const;
	void SetBufferManager(IBufferManager* mgr);
	IBufferManager* GetBufferManager() const;
	void SetReadOnly(bool readonly);
	bool IsReadOnly() const;
	void SetAllowExtension(bool allow);
	bool IsAllowExtension() const;
	void SetSize(int64_t size);
	bool SetDataSize(int32_t size);
	int32_t GetDataSize() const;
	bool SetBytesPerLine(int32_t bytesPerLine);
	int32_t GetBytesPerLine() const;
	bool Copy(int64_t offset = -1, int64_t size = 0);
	bool Paste(int64_t offset = -1);
	bool CanCopy() const;
	bool CanPaste() const;
	bool Cut(int64_t offset = -1, int64_t size = 0);
	bool Delete(int64_t offset = -1, int64_t size = 0);
	bool CanCut() const;
	bool CanDelete() const;
	int64_t SetBiasOffset(int64_t offset);
	int64_t GetBiasOffset() const;
	HexControlColors& GetColors();
	std::wstring GetText(int64_t offset, int64_t size);
	void Refresh();

	BEGIN_MSG_MAP(CHexControl)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLeftButtonDown)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLeftButtonUp)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
		MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDialogCode)
		CHAIN_MSG_MAP(CBufferedPaintWindowImpl<CHexControl>)
	END_MSG_MAP()

	void DoPaint(CDCHandle dc, RECT& rect);

private:
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnHScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnVScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseWheel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnKillFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLeftButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnGetDialogCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnKeyDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLeftButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

private:
	void RecalcLayout();
	void InitFontMetrics();
	CPoint GetPointFromOffset(int64_t offset) const;
	int64_t GetOffsetFromPoint(const POINT& pt) const;
	void DrawNumber(CDCHandle dc, int64_t offset, uint64_t value, int editDigits);
	PCWSTR FormatNumber(ULONGLONG number) const;
	void SendNotify(HCNOTIFY* notify);

	void UpdateCaret();
	void RedrawWindow(RECT* = nullptr);
	void ClearSelection();
	void CommitValue(int64_t offset, uint64_t value);
	void ResetInput();
	int64_t NormalizeOffset(int64_t offset) const;
	void RedrawCaretLine();

private:
	HexControlColors m_Colors;
	CFont m_Font;
	int m_FontPointSize{ 100 };
	int m_Lines{ 1 };
	int m_CharWidth, m_CharHeight;
	IBufferManager* m_Buffer{ nullptr };
	std::vector<std::wstring> m_Text;
	int64_t m_StartOffset{ 0 }, m_EndOffset;
	int32_t m_DataSize{ 1 }, m_BytesPerLine{ 32 };
	int64_t m_CaretOffset{ 0 };
	int m_AddressDigits{ 4 };
	int m_EditDigits{ 0 }, m_LastDigits{ 0 };
	Selection m_Selection;
	HCNOTIFY m_NotifyData;
	uint64_t m_CurrentInput{ 0 }, m_OldValue;
	bool m_InsertMode{ false };
	bool m_ReadOnly{ false };
};

