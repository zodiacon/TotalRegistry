#pragma once

#include "AppCommandBase.h"

struct CopyValueCommand : RegAppCommandBase<CopyValueCommand> {
	CopyValueCommand(PCWSTR path, PCWSTR name, PCWSTR targetPath, AppCommandCallback<CopyValueCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

private:
	CString m_TargetPath;
	CString m_TargetName;
};
