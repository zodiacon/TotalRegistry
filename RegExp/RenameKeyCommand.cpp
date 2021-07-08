#include "pch.h"
#include "RenameKeyCommand.h"
#include "Registry.h"

RenameKeyCommand::RenameKeyCommand(PCWSTR path, PCWSTR name, PCWSTR newName, AppCommandCallback<RenameKeyCommand> cb)
	: RegAppCommandBase(L"Rename Key", path, name, cb), _newName(newName) {
}

bool RenameKeyCommand::Execute() {
	auto key = Registry::OpenKey(_path, KEY_WRITE);
	if (!key)
		return false;

	if (Registry::RenameKey(key, _name, _newName)) {
		std::swap(_name, _newName);
		return InvokeCallback(true);
	}

	return false;
}

bool RenameKeyCommand::Undo() {
	return Execute();
}

const CString& RenameKeyCommand::GetNewName() const {
	return _newName;
}
