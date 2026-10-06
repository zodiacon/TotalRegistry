#pragma once

struct SecurityHelper abstract final {
	static bool IsRunningElevated();
	static bool RunElevated();
	static bool EnablePrivilege(PCWSTR privName, bool enable);
	static HANDLE DupHandle(HANDLE hSource, DWORD sourcePid, DWORD access = 0);
};

//
// enables the Backup and Restore privileges for its lifetime
//
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
	BackupRestorePrivileges(BackupRestorePrivileges const&) = delete;
	BackupRestorePrivileges& operator=(BackupRestorePrivileges const&) = delete;

	explicit operator bool() const {
		return m_Enabled;
	}

private:
	bool m_Enabled;
};
