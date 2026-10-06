#include "pch.h"
#include "RestoreKeyCommand.h"
#include "Registry.h"
#include "SecurityHelper.h"

namespace {
	struct BackupRestorePrivileges {
		BackupRestorePrivileges() {
			m_Enabled = SecurityHelper::EnablePrivilege(SE_BACKUP_NAME, true) && SecurityHelper::EnablePrivilege(SE_RESTORE_NAME, true);
			if (!m_Enabled)
				::SetLastError(ERROR_PRIVILEGE_NOT_HELD);
		}
		~BackupRestorePrivileges() {
			// preserve the error of the operation for the caller
			auto error = ::GetLastError();
			SecurityHelper::EnablePrivilege(SE_BACKUP_NAME, false);
			SecurityHelper::EnablePrivilege(SE_RESTORE_NAME, false);
			::SetLastError(error);
		}
		explicit operator bool() const {
			return m_Enabled;
		}

	private:
		bool m_Enabled;
	};
}

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
