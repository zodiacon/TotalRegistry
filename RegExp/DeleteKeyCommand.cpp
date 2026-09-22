#include "pch.h"
#include "DeleteKeyCommand.h"
#include "Registry.h"
#include "Helpers.h"

DeleteKeyCommand::DeleteKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteKeyCommand> cb)
	: RegAppCommandBase(L"Delete Key " + CString(name), path, name, cb) {
}

bool DeleteKeyCommand::Execute() {
	auto key = Registry::OpenKey(m_Path, MAXIMUM_ALLOWED);
	if (!key)
		return false;

	if (m_SavePath.IsEmpty()) {
		LARGE_INTEGER li;
		::QueryPerformanceCounter(&li);
		m_SavePath.Format(L"%llX", li.QuadPart);
	}
	CRegKey keyBackup;
	auto error = keyBackup.Create(HKEY_CURRENT_USER, DeletedPathBackup + m_SavePath, nullptr, 0, MAXIMUM_ALLOWED);
	::SetLastError(error);
	if (!keyBackup)
		return false;
	// BUG: RegCopyTree fails if key is one of the predefined keys
	::SetLastError(error = ::RegCopyTree(key.Get(), m_Name, keyBackup));
	if (ERROR_SUCCESS != error)
		return false;

	::SetLastError(error = ::RegDeleteTree(key.Get(), m_Name));
	if (ERROR_SUCCESS != error) {
		return false;
	}
	return InvokeCallback(true);
}

bool DeleteKeyCommand::Undo() {
	auto key = Registry::OpenKey(m_Path, KEY_CREATE_SUB_KEY);
	if (!key)
		return false;

	DWORD error;
	CRegKey keyBackup;
	::SetLastError(error = keyBackup.Open(HKEY_CURRENT_USER, DeletedPathBackup + m_SavePath, KEY_READ));
	if (!keyBackup)
		return false;

	CRegKey newKey;
	DWORD disp;
	error = newKey.Create(key.Get(), m_Name, nullptr, 0, KEY_ALL_ACCESS, nullptr, &disp);
	::SetLastError(error);
	if (error != ERROR_SUCCESS)
		return false;

	::SetLastError(error = ::RegCopyTree(keyBackup, nullptr, newKey));
	if (error != ERROR_SUCCESS)
		return false;

	return InvokeCallback(false);
}
