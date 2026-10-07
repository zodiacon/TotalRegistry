#pragma once

#include "AppCommandBase.h"
#include "KeyBackup.h"

struct DeleteKeyCommand : RegAppCommandBase<DeleteKeyCommand> {
	DeleteKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteKeyCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

private:
	LSTATUS RestoreBackup(HKEY hParent);

	KeyBackup m_Backup;
	// a symbolic link is deleted on its own (RegDeleteTree would empty its target), and re-created by undo
	bool m_IsLink{ false };
	CString m_LinkTarget;
};
