#pragma once

#include "AppCommandBase.h"

struct CreateKeyCommand : RegAppCommandBase<CreateKeyCommand> {
	CreateKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<CreateKeyCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;
};

