#include "pch.h"
#include "ChangeValueCommand.h"
#include "Registry.h"

ChangeValueCommand::ChangeValueCommand(PCWSTR path, PCWSTR name, DWORD type, const PVOID data, LONG size, AppCommandCallback<ChangeValueCommand> cb) 
	: RegAppCommandBase(L"Change Value " + CString(name), path, name, cb), _type(type), _size(size) {
	_data = std::make_unique<BYTE[]>(size);
	memcpy(_data.get(), data, size);
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
	error = key.SetValue(GetName(), _type, _data.get(), _size);
	if (ERROR_SUCCESS != error) {
		::SetLastError(error);
		return false;
	}

	_size = size;
	_type = type;
	_data = std::move(buffer);

	return InvokeCallback(false);
}
