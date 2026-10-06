#include "pch.h"
#include "RestoreKeyCommand.h"
#include "Registry.h"
#include "SecurityHelper.h"

RestoreKeyCommand::RestoreKeyCommand(PCWSTR path, PCWSTR fileName, AppCommandCallback<RestoreKeyCommand> cb)
	: RegAppCommandBase(L"Import " + CString(fileName).Mid(CString(fileName).ReverseFind(L'\\') + 1), path, fileName, cb) {
}

RestoreKeyCommand::~RestoreKeyCommand() {
	DeleteBackup();
}

bool RestoreKeyCommand::Execute() {
	BackupRestorePrivileges privileges;
	if (!privileges)
		return false;

	auto key = Registry::OpenKey(GetPath(), KEY_READ);
	if (!key)
		return false;

	//
	// save the current contents so the restore can be undone
	//
	WCHAR dir[MAX_PATH], file[MAX_PATH];
	if (!::GetTempPath(_countof(dir), dir) || !::GetTempFileName(dir, L"TRG", 0, file))
		return false;
	// RegSaveKeyEx fails if the file exists
	::DeleteFile(file);
	DeleteBackup();
	m_BackupFile = file;

	auto error = ::RegSaveKeyEx(key, m_BackupFile, nullptr, REG_LATEST_FORMAT);
	if (error == ERROR_SUCCESS)
		error = ::RegRestoreKey(key, GetName(), REG_FORCE_RESTORE);
	if (error != ERROR_SUCCESS) {
		DeleteBackup();
		::SetLastError(error);
		return false;
	}
	return InvokeCallback(true);
}

bool RestoreKeyCommand::Undo() {
	BackupRestorePrivileges privileges;
	if (!privileges)
		return false;

	auto key = Registry::OpenKey(GetPath(), KEY_READ);
	if (!key)
		return false;

	auto error = ::RegRestoreKey(key, m_BackupFile, REG_FORCE_RESTORE);
	if (error != ERROR_SUCCESS) {
		::SetLastError(error);
		return false;
	}
	DeleteBackup();
	return InvokeCallback(false);
}

void RestoreKeyCommand::DeleteBackup() {
	if (!m_BackupFile.IsEmpty()) {
		::DeleteFile(m_BackupFile);
		m_BackupFile.Empty();
	}
}
