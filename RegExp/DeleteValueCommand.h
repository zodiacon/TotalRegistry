#pragma once

#include "AppCommandBase.h"

struct DeleteValueCommand : RegAppCommandBase<DeleteValueCommand> {
	DeleteValueCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteValueCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;
};
