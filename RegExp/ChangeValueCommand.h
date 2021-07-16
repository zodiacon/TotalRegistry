#pragma once

#include "AppCommandBase.h"

struct ChangeValueCommand : RegAppCommandBase<ChangeValueCommand> {
public:
	ChangeValueCommand(PCWSTR path, PCWSTR name, DWORD type, const PVOID data, LONG size, AppCommandCallback<ChangeValueCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override {
		return Execute();
	}

private:
	LONG _size;
	std::unique_ptr<BYTE[]> _data;
	DWORD _type;
};
