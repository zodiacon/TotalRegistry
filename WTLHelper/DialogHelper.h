#pragma once

template<typename T>
class DialogHelper {
public:
#ifdef IDI_OK
	void AdjustOKCancelButtons() {
		auto dlg = static_cast<T*>(this);
		CButton ok(dlg->GetDlgItem(IDOK));
		if (ok) {
			CString text;
			ok.GetWindowText(text);
			ok.SetWindowText(L"  " + text);
			ok.SetIcon(AtlLoadIconImage(IDI_OK, 0, 16, 16));
		}

		CButton cancel(dlg->GetDlgItem(IDCANCEL));
		if (cancel) {
			cancel.SetWindowText(L"  Cancel");
			cancel.SetIcon(AtlLoadIconImage(IDI_DELETE, 0, 16, 16));
		}
	}
#endif

	bool AddIconToButton(WORD id, WORD icon, int size = 16) {
		auto dlg = static_cast<T*>(this);
		CButton button(dlg->GetDlgItem(id));
		if (button) {
			button.SetIcon(AtlLoadIconImage(icon, 0, size, size));
			CString text;
			button.GetWindowText(text);
			button.SetWindowText(L"  " + text);
		}
		return (bool)button;
	}

	void SetDialogIcon(UINT icon) {
		auto dlg = static_cast<T*>(this);
		dlg->SetIcon(AtlLoadIconImage(icon, 0, 16, 16), FALSE);
		dlg->SetIcon(AtlLoadIconImage(icon, 0, 32, 32), TRUE);
	}
	void SetDialogIcon(HICON icon) {
		auto dlg = static_cast<T*>(this);
		dlg->SetIcon(icon, FALSE);
		dlg->SetIcon(icon, TRUE);
	}
};

