#pragma once

#include "DialogHelper.h"

class CNumberValueDlg :
	public CDialogImpl<CNumberValueDlg>,
	public DialogHelper<CNumberValueDlg> {
public:
	enum { IDD = IDD_NUMBERVALUE };

	CNumberValueDlg(CRegKey& key, PCWSTR name, DWORD type, bool readOnly);

	DWORD64 GetValue() const;
	DWORD GetType() const;
	bool IsModified() const;

	BEGIN_MSG_MAP(CNumberValueDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_DECIMAL, OnClickBase)
		COMMAND_ID_HANDLER(IDC_HEX, OnClickBase)
		COMMAND_ID_HANDLER(IDC_BINARY, OnClickBase)
		COMMAND_ID_HANDLER(IDC_COLOR, OnClickColor)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class ValueFormat {
		Decimal = 10, Hex = 16, Binary = 2, Octal = 8
	};
	void DisplayValue(DWORD64 value, bool checkRadio = true);
	DWORD64 ParseValue(const CString& text, bool& error);
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClickBase(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClickColor(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CRegKey& m_Key;
	CString m_Name;
	DWORD64 m_Value;
	DWORD m_Type;
	bool m_ReadOnly;
	bool m_Modified{ false };
	inline static ValueFormat m_Format{ ValueFormat::Decimal };
};
