#pragma once

#pragma once

#include "AppCommandBase.h"

struct RenameValueCommand : RegAppCommandBase<RenameValueCommand> {
	RenameValueCommand(PCWSTR path, PCWSTR name, PCWSTR newName, AppCommandCallback<RenameValueCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

	const CString& GetNewName() const;

	CString GetCommandName() const override;

private:
	CString _newName;
};
