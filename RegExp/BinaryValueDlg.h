#pragma once

#include "DialogHelper.h"
#include "HexControl.h"
#include "MemoryBuffer.h"

class CBinaryValueDlg :
	public CDialogImpl<CBinaryValueDlg>,
	public DialogHelper<CBinaryValueDlg>,
	public CDynamicDialogLayout<CBinaryValueDlg> {
public:
	enum { IDD = IDD_BINVALUE };

	CBinaryValueDlg(CRegKey& key, PCWSTR name, bool readOnly);

	const std::vector<BYTE>& GetValue() const;
	bool IsModified() const;

	BEGIN_MSG_MAP(CBinaryValueDlg)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		NOTIFY_CODE_HANDLER(HCN_SIZECHANGED, OnHexBufferSizeChanged)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CBinaryValueDlg>)
		FORWARD_NOTIFICATIONS()
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	void UpdateBufferSize();

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHexBufferSizeChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	CRegKey& m_Key;
	CString m_Name;
	CHexControl m_Hex;
	MemoryBuffer m_Buffer;
	std::vector<BYTE> m_Value;
	DWORD m_Type{ 0 };
	bool m_ReadOnly;
	bool m_Modified{ false };
};
