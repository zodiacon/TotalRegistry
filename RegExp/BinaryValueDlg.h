#pragma once

#include "DialogHelper.h"
#include "HexControl.h"
#include "MemoryBuffer.h"
#include "IMainFrame.h"

class CBinaryValueDlg :
	public CDialogImpl<CBinaryValueDlg>,
	public CDialogHelper<CBinaryValueDlg>,
	public CAutoUpdateUI<CBinaryValueDlg>,
	public CDynamicDialogLayout<CBinaryValueDlg> {
public:
	enum { IDD = IDD_BINVALUE };
	enum { ID_DATA_BYTE = 500, ID_LINE = 520 };

	CBinaryValueDlg(CRegKey& key, PCWSTR name, bool readOnly, IMainFrame* frame);

	const std::vector<BYTE>& GetValue() const;
	bool IsModified() const;

	BEGIN_MSG_MAP(CBinaryValueDlg)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		NOTIFY_CODE_HANDLER(HCN_SIZECHANGED, OnHexBufferSizeChanged)
		NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnToolBarDropdown)
		COMMAND_RANGE_HANDLER(ID_DATA_BYTE, ID_DATA_BYTE + 3, OnDataSize)
		COMMAND_RANGE_HANDLER(ID_HEX_8BYTES, ID_HEX_32BYTES, OnBytesPerLine)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CBinaryValueDlg>)
		CHAIN_MSG_MAP(CAutoUpdateUI<CBinaryValueDlg>)
		REFLECT_NOTIFICATIONS_EX()
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	void BuildToolBar(CRect& rc);
	void UpdateBufferSize();

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHexBufferSizeChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnDataSize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToolBarDropdown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnBytesPerLine(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CRegKey& m_Key;
	CString m_Name;
	CHexControl m_Hex;
	MemoryBuffer m_Buffer;
	std::vector<BYTE> m_Value;
	DWORD m_Type{ 0 };
	IMainFrame* m_pFrame;
	bool m_ReadOnly;
	bool m_Modified{ false };
};
