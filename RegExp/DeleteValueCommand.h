#pragma once

#include "AppCommandBase.h"

struct DeleteValueCommand : RegAppCommandBase<DeleteValueCommand> {
	DeleteValueCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteValueCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

private:
	DWORD m_Type;
	std::unique_ptr<BYTE[]> m_Data;
	DWORD m_Size;
};
