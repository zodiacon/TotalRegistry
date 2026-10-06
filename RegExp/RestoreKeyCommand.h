#pragma once

#include "AppCommandBase.h"

//
// replaces a key's contents with a hive file (RegRestoreKey)
// path: the key to restore into, name: the hive file
//
struct RestoreKeyCommand : RegAppCommandBase<RestoreKeyCommand> {
	RestoreKeyCommand(PCWSTR path, PCWSTR fileName, AppCommandCallback<RestoreKeyCommand> cb = nullptr);
	~RestoreKeyCommand();

	bool Execute() override;
	bool Undo() override;

private:
	void DeleteBackup();

	CString m_BackupFile;
};
