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

	_size = 0;
	auto error = key.QueryValue(GetName(), &_type, nullptr, &_size);
	::SetLastError(error);
	if (error != ERROR_SUCCESS)
		return false;

	if (_size) {
		_data = std::make_unique<BYTE[]>(_size);
		if (!_data) {
			::SetLastError(ERROR_OUTOFMEMORY);
			return false;
		}
		error = key.QueryValue(GetName(), &_type, _data.get(), &_size);
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

	auto error = key.SetValue(GetName(), _type, _data.get(), _size);
	::SetLastError(error);
	if (ERROR_SUCCESS != error)
		return false;

	return InvokeCallback(false);
}
