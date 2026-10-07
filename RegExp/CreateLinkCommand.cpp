#include "pch.h"
#include "CreateLinkCommand.h"
#include "Registry.h"

CreateLinkCommand::CreateLinkCommand(PCWSTR path, PCWSTR name, PCWSTR target, bool isVolatile, AppCommandCallback<CreateLinkCommand> cb)
	: RegAppCommandBase(L"Create Link " + CString(name), path, name, cb), m_Target(target), m_Volatile(isVolatile) {
}

bool CreateLinkCommand::Execute() {
	auto key = Registry::OpenKey(m_Path, KEY_CREATE_SUB_KEY | KEY_CREATE_LINK);
	if (!key)
		return false;

	auto error = Registry::CreateLinkKey(key.Get(), m_Name, m_Target, m_Volatile);
	if (error != ERROR_SUCCESS) {
		::SetLastError(error);
		return false;
	}
	return InvokeCallback(true);
}

bool CreateLinkCommand::Undo() {
	auto key = Registry::OpenKey(m_Path, MAXIMUM_ALLOWED);
	if (!key)
		return false;

	auto error = Registry::DeleteLinkKey(key.Get(), m_Name);
	if (error != ERROR_SUCCESS) {
		::SetLastError(error);
		return false;
	}
	return InvokeCallback(false);
}

CString const& CreateLinkCommand::GetTarget() const {
	return m_Target;
}
