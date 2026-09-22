#include "pch.h"
#include "RenameValueCommand.h"
#include "Registry.h"

RenameValueCommand::RenameValueCommand(PCWSTR path, PCWSTR name, PCWSTR newName, AppCommandCallback<RenameValueCommand> cb)
	: RegAppCommandBase(L"Rename Value", path, name, cb), m_NewName(newName) {
}

bool RenameValueCommand::Execute() {
	auto key = Registry::OpenKey(m_Path, KEY_ALL_ACCESS);
	if (!key)
		return false;

	if (Registry::RenameValue(key.Get(), nullptr, m_Name, m_NewName)) {
		if (!InvokeCallback(true))
			return false;
		std::swap(m_Name, m_NewName);
		return true;
	}

	return false;
}

bool RenameValueCommand::Undo() {
	return Execute();
}

const CString& RenameValueCommand::GetNewName() const {
	return m_NewName;
}

CString RenameValueCommand::GetCommandName() const {
	return L"Rename Value " + GetName();
}
