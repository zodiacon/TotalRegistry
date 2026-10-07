#pragma once

#include "AppCommandBase.h"

struct CreateKeyCommand : RegAppCommandBase<CreateKeyCommand> {
	// options: e.g. REG_OPTION_VOLATILE
	CreateKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<CreateKeyCommand> cb = nullptr, DWORD options = 0);

	bool Execute() override;
	bool Undo() override;

private:
	DWORD m_Options;
};

