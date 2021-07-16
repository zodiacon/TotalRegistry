#include "pch.h"
#include "DeleteValueCommand.h"
#include "Registry.h"

DeleteValueCommand::DeleteValueCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteValueCommand> cb) 
	: RegAppCommandBase(L"Delete Value " + CString(name), path, name, cb) {
}

bool DeleteValueCommand::Execute() {
	return false;
}

bool DeleteValueCommand::Undo() {
	return false;
}
