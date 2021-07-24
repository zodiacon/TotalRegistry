#pragma once

#include "AppCommandBase.h"

struct CopyValueCommand : RegAppCommandBase<CopyValueCommand> {
	CopyValueCommand(PCWSTR path, PCWSTR name, PCWSTR targetPath, AppCommandCallback<CopyValueCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

private:
	CString _targetPath;
	CString _targetName;
};
