#include "pch.h"
#include "RenameKeyCommand.h"
#include "Registry.h"

RenameKeyCommand::RenameKeyCommand(PCWSTR path, PCWSTR name, PCWSTR newName, AppCommandCallback<RenameKeyCommand> cb)
	: RegAppCommandBase(L"Rename Key", path, name, cb), m_NewName(newName) {
}

bool RenameKeyCommand::Execute() {
	auto key = Registry::OpenKey(m_Path, KEY_ALL_ACCESS);
	if (!key)
		return false;

	if (Registry::RenameKey(key.Get(), m_Name, m_NewName)) {
		if (!InvokeCallback(true))
			return false;
		std::swap(m_Name, m_NewName);
		return true;
	}

	return false;
}

bool RenameKeyCommand::Undo() {
	return Execute();
}

const CString& RenameKeyCommand::GetNewName() const {
	return m_NewName;
}

CString RenameKeyCommand::GetCommandName() const {
	return L"Rename Key " + GetName();
}
