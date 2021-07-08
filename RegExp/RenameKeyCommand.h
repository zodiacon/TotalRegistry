#pragma once

#include "AppCommandBase.h"

struct RenameKeyCommand : RegAppCommandBase<RenameKeyCommand> {
	RenameKeyCommand(PCWSTR path, PCWSTR name, PCWSTR newName, AppCommandCallback<RenameKeyCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

	const CString& GetNewName() const;

private:
	CString _newName;
};
