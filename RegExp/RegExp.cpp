
#include "pch.h"
#include "RegExp.h"
#include "MainFrame.h"
#include "ThemeHelper.h"
#include "DriverHelper.h"
#include "SecurityHelper.h"
#include "AppSettings.h"

CAppModule _Module;
AppSettings _Settings;

int Run(LPTSTR lpstrCmdLine = nullptr, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	CString cmdline(lpstrCmdLine);
	cmdline.Trim(L" \"");
	if (!cmdline.IsEmpty() && cmdline.Right(11).CompareNoCase(L"regedit.exe") != 0) 
		wndMain.SetStartKey(cmdline);

	if (wndMain.CreateEx() == nullptr) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
	HRESULT hRes = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES);

	hRes = _Module.Init(nullptr, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	ThemeHelper::Init();

	if (!DriverHelper::IsDriverLoaded() && SecurityHelper::IsRunningElevated()) {
		//auto loaded = DriverHelper::LoadDriver();
		//if (!loaded)
		//	AtlMessageBox(nullptr, L"Failed to load kernel driver. Some keys will be inaccesible", IDS_APP_TITLE, MB_ICONWARNING);
	}
	
	int nRet = Run(lpCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return 0;
}


