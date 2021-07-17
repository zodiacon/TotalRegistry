#include "pch.h"
#include "CreateValueCommand.h"
#include "Registry.h"

CreateValueCommand::CreateValueCommand(PCWSTR path, PCWSTR name, DWORD type, AppCommandCallback<CreateValueCommand> cb) 
	: RegAppCommandBase(L"Create Value " + CString(name), path, name, cb), _type(type) {
}

bool CreateValueCommand::Execute() {
	auto key = Registry::OpenKey(GetPath(), KEY_SET_VALUE);
	if (!key)
		return false;

	DWORD size = 0;
	switch (GetType()) {
		case REG_DWORD: size = 4; break;
		case REG_QWORD: size = 8; break;
		case REG_SZ:
		case REG_EXPAND_SZ:
			size = 2;
			break;

		case REG_MULTI_SZ:
			size = 4;
			break;
	}
	BYTE dummy[8] = { 0 };
	auto error = key.SetValue(GetName(), GetType(), dummy, _size = size);
	::SetLastError(error);
	if (ERROR_SUCCESS != error)
		return false;

	return InvokeCallback(true);
}

bool CreateValueCommand::Undo() {
	auto key = Registry::OpenKey(GetPath(), KEY_WRITE);
	if (!key)
		return false;

	auto error = key.DeleteValue(GetName());
	::SetLastError(error);
	if (ERROR_SUCCESS != error)
		return false;

	return InvokeCallback(false);
}

DWORD CreateValueCommand::GetType() const {
	return _type;
}

DWORD CreateValueCommand::GetSize() const {
	return _size;
}
