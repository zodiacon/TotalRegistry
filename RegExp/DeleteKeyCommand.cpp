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

	bool remote = m_Path.Left(2) == L"\\\\";
	auto error = m_Backup.Save(key.Get(), m_Name, !remote);
	if (ERROR_SUCCESS != error) {
		::SetLastError(error);
		return false;
	}

	error = ::RegDeleteTree(key.Get(), m_Name);
	if (ERROR_SUCCESS != error) {
		// the delete may have removed part of the tree, put it back since this command won't be undoable
		RestoreBackup(key.Get());
		m_Backup.Discard();
		::SetLastError(error);
		return false;
	}
	return InvokeCallback(true);
}

bool DeleteKeyCommand::Undo() {
	auto key = Registry::OpenKey(m_Path, MAXIMUM_ALLOWED);
	if (!key)
		return false;

	auto error = RestoreBackup(key.Get());
	if (error != ERROR_SUCCESS) {
		::SetLastError(error);
		return false;
	}
	m_Backup.Discard();
	return InvokeCallback(false);
}

LSTATUS DeleteKeyCommand::RestoreBackup(HKEY hParent) {
	CRegKey newKey;
	DWORD disp;
	auto error = newKey.Create(hParent, m_Name, nullptr, 0, KEY_ALL_ACCESS, nullptr, &disp);
	if (error != ERROR_SUCCESS)
		return error;

	error = m_Backup.Restore(newKey);
	if (error != ERROR_SUCCESS && disp == REG_CREATED_NEW_KEY) {
		// don't leave an empty key behind
		newKey.Close();
		::RegDeleteTree(hParent, m_Name);
	}
	return error;
}
