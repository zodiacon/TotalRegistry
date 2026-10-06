#pragma once

//
// a backup of a key's tree, so that deleting it can be undone
// with the Backup/Restore privileges (running elevated) the tree is saved to a temporary hive file,
// which keeps its security intact and out of the user's hive;
// otherwise it's copied under this process' backup key in HKEY_CURRENT_USER
//
class KeyBackup {
public:
	KeyBackup() = default;
	~KeyBackup();
	KeyBackup(KeyBackup const&) = delete;
	KeyBackup& operator=(KeyBackup const&) = delete;

	// allowFile should be false for remote keys, as hive files are saved on the remote machine
	LSTATUS Save(HKEY hParent, PCWSTR name, bool allowFile = true);
	LSTATUS Restore(HKEY hKey) const;
	void Discard();

	// deletes backups left by instances that are no longer running
	static void DeleteOrphans();

private:
	static CString const& GetProcessBackupPath();

	CString m_FileName;
	CString m_KeyPath;	// under HKEY_CURRENT_USER
};
