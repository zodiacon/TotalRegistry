#pragma once

#include "AppCommandBase.h"

struct CopyKeyCommand : public RegAppCommandBase<CopyKeyCommand> {
	CopyKeyCommand(PCWSTR path, PCWSTR name, PCWSTR targetPath, AppCommandCallback<CopyKeyCommand> cb = nullptr);

	const CString& GetTargetPath() const;

	bool Execute() override;
	bool Undo() override;

private:
	CString _targetPath;
};
