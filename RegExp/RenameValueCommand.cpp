#include "pch.h"
#include "RenameValueCommand.h"
#include "Registry.h"

RenameValueCommand::RenameValueCommand(PCWSTR path, PCWSTR name, PCWSTR newName, AppCommandCallback<RenameValueCommand> cb)
	: RegAppCommandBase(L"Rename Value", path, name, cb), _newName(newName) {
}

bool RenameValueCommand::Execute() {
	auto key = Registry::OpenKey(_path, KEY_ALL_ACCESS);
	if (!key)
		return false;

	if (Registry::RenameValue(key.Get(), nullptr, _name, _newName)) {
		if (!InvokeCallback(true))
			return false;
		std::swap(_name, _newName);
		return true;
	}

	return false;
}

bool RenameValueCommand::Undo() {
	return Execute();
}

const CString& RenameValueCommand::GetNewName() const {
	return _newName;
}

CString RenameValueCommand::GetCommandName() const {
	return L"Rename Value " + GetName();
}
