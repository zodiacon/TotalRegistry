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
};
