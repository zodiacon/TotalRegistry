#include "pch.h"
#include "DriverHelper.h"
#include "SecurityHelper.h"
#include "resource.h"
#include <wil\resource.h>
#include <winioctl.h>
#include "..\KRegExp\KRegExpCommon.h"

wil::unique_hfile _hDevice;

bool DriverHelper::LoadDriver() {
	if (!SecurityHelper::IsRunningElevated())
		return false;

	wil::unique_schandle hScm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
	if (!hScm)
		return false;

	wil::unique_schandle hService(::OpenService(hScm.get(), L"KRegExp", SERVICE_START));
	if (!hService && !InstallDriver())
		return false;

	hService.reset(::OpenService(hScm.get(), L"KRegExp", SERVICE_START));
	if (!hService)
		return false;

	return ::StartService(hService.get(), 0, nullptr);
}

bool DriverHelper::InstallDriver() {
	if (!SecurityHelper::IsRunningElevated())
		return false;

	auto hRes = ::FindResource(nullptr, MAKEINTRESOURCE(IDR_DRIVER), L"BIN");
	if (!hRes)
		return false;

	auto hGlobal = ::LoadResource(nullptr, hRes);
	if (!hGlobal)
		return false;

	auto size = ::SizeofResource(nullptr, hRes);
	void* pBuffer = ::LockResource(hGlobal);

	WCHAR path[MAX_PATH];
	::GetSystemDirectory(path, MAX_PATH);
	::wcscat_s(path, L"\\Drivers\\KRegExp.sys");
	wil::unique_hfile hFile(::CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_SYSTEM, nullptr));
	if (!hFile)
		return false;

	DWORD bytes = 0;
	::WriteFile(hFile.get(), pBuffer, size, &bytes, nullptr);
	if (bytes != size)
		return false;

	wil::unique_schandle hScm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
	if (!hScm)
		return false;

	wil::unique_schandle hService(::CreateService(hScm.get(), L"KRegExp", nullptr, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
		SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path, nullptr, nullptr, nullptr, nullptr, nullptr));

	return hService != nullptr;
}

HANDLE DriverHelper::OpenKey(PCWSTR name, ACCESS_MASK access) {
	if (!OpenDevice())
		return nullptr;

	auto len = (ULONG)::wcslen(name);
	DWORD size = len * sizeof(WCHAR) + sizeof(KeyData);
	auto buffer = std::make_unique<BYTE[]>(size);
	if (!buffer)
		return nullptr;
	auto data = reinterpret_cast<KeyData*>(buffer.get());
	data->Access = access;
	data->Length = len;
	::wcscpy_s(data->Name, len + 1, name);

	DWORD bytes;
	HANDLE hObject = nullptr;
	::DeviceIoControl(_hDevice.get(), IOCTL_KREGEXP_OPEN_KEY, data, size,
		&hObject, sizeof(hObject), &bytes, nullptr);

	return hObject;
}

HANDLE DriverHelper::DupHandle(HANDLE hObject, ULONG pid, ACCESS_MASK access, DWORD flags) {
	HANDLE hTarget = nullptr;
	if (OpenDevice()) {
		DupHandleData data;
		data.AccessMask = access;
		data.Handle = hObject;
		data.SourcePid = pid;
		data.Flags = flags;

		DWORD bytes;
		::DeviceIoControl(_hDevice.get(), IOCTL_KREGEXP_DUP_HANDLE, &data, sizeof(data),
			&hTarget, sizeof(hTarget), &bytes, nullptr);
	}
	return hTarget;
}

bool DriverHelper::IsDriverLoaded() {
	wil::unique_schandle hScm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
	if (!hScm)
		return false;

	wil::unique_schandle hService(::OpenService(hScm.get(), L"KRegExp", SERVICE_QUERY_STATUS));
	if (!hService)
		return false;

	SERVICE_STATUS status;
	if (!::QueryServiceStatus(hService.get(), &status))
		return false;

	return status.dwCurrentState == SERVICE_RUNNING;
}

bool DriverHelper::OpenDevice() {
	if (!_hDevice) {
		_hDevice.reset(::CreateFile(L"\\\\.\\KRegExp", GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
			OPEN_EXISTING, 0, nullptr));
	}
	return (bool)_hDevice;
}
