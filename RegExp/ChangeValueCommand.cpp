#include "pch.h"
#include "ChangeValueCommand.h"
#include "Registry.h"

ChangeValueCommand::ChangeValueCommand(PCWSTR path, PCWSTR name, DWORD type, const PVOID data, LONG size, AppCommandCallback<ChangeValueCommand> cb) 
	: RegAppCommandBase(L"Change Value " + CString(name), path, name, cb), m_Type(type), m_Size(size) {
	m_Data = std::make_unique<BYTE[]>(size);
	memcpy(m_Data.get(), data, size);
}

bool ChangeValueCommand::Execute() {
	auto key = Registry::OpenKey(GetPath(), KEY_SET_VALUE | KEY_QUERY_VALUE);
	if (!key)
		return false;

	//
	// read old value
	//
	ULONG size = 0;
	DWORD type;
	key.QueryValue(GetName(), &type, nullptr, &size);
	auto buffer = std::make_unique<BYTE[]>(size);
	auto error = key.QueryValue(GetName(), &type, buffer.get(), &size);
	if (ERROR_SUCCESS != error) {
		::SetLastError(error);
		return false;
	}

	//
	// make the change
	//
	error = key.SetValue(GetName(), m_Type, m_Data.get(), m_Size);
	if (ERROR_SUCCESS != error) {
		::SetLastError(error);
		return false;
	}

	m_Size = size;
	m_Type = type;
	m_Data = std::move(buffer);

	return InvokeCallback(false);
}
