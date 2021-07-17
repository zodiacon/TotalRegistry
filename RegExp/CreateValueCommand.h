#pragma once

#include "AppCommandBase.h"

struct CreateValueCommand : RegAppCommandBase<CreateValueCommand> {
	CreateValueCommand(PCWSTR path, PCWSTR name, DWORD type, AppCommandCallback<CreateValueCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

	DWORD GetType() const;
	DWORD GetSize() const;

private:
	DWORD _type;
	DWORD _size{ 0 };
};
