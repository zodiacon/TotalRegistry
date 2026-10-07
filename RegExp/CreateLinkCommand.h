#pragma once

#include "AppCommandBase.h"

//
// creates a symbolic link key; the target is a kernel path (\REGISTRY\...)
//
struct CreateLinkCommand : RegAppCommandBase<CreateLinkCommand> {
	CreateLinkCommand(PCWSTR path, PCWSTR name, PCWSTR target, bool isVolatile, AppCommandCallback<CreateLinkCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

	CString const& GetTarget() const;

private:
	CString m_Target;
	bool m_Volatile;
};
