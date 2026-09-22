#include "pch.h"
#include "DeleteValueCommand.h"
#include "Registry.h"

DeleteValueCommand::DeleteValueCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteValueCommand> cb) 
	: RegAppCommandBase(L"Delete Value " + CString(name), path, name, cb) {
}

bool DeleteValueCommand::Execute() {
	auto key = Registry::OpenKey(GetPath(), KEY_WRITE | KEY_READ);
	if (!key)
		return false;

	m_Size = 0;
	auto error = key.QueryValue(GetName(), &m_Type, nullptr, &m_Size);
	::SetLastError(error);
	if (error != ERROR_SUCCESS)
		return false;

	if (m_Size) {
		m_Data = std::make_unique<BYTE[]>(m_Size);
		if (!m_Data) {
			::SetLastError(ERROR_OUTOFMEMORY);
			return false;
		}
		error = key.QueryValue(GetName(), &m_Type, m_Data.get(), &m_Size);
		::SetLastError(error);
		if (ERROR_SUCCESS != error)
			return false;
	}

	error = key.DeleteValue(GetName());
	::SetLastError(error);
	if (ERROR_SUCCESS != error)
		return false;

	return InvokeCallback(true);
}

bool DeleteValueCommand::Undo() {
	auto key = Registry::OpenKey(GetPath(), KEY_WRITE);
	if (!key)
		return false;

	auto error = key.SetValue(GetName(), m_Type, m_Data.get(), m_Size);
	::SetLastError(error);
	if (ERROR_SUCCESS != error)
		return false;

	return InvokeCallback(false);
}
