#include "pch.h"
#include "CopyValueCommand.h"
#include "Registry.h"

CopyValueCommand::CopyValueCommand(PCWSTR path, PCWSTR name, PCWSTR targetPath, AppCommandCallback<CopyValueCommand> cb) 
	: RegAppCommandBase(L"Paste value " + CString(name), path, name, cb), _targetPath(targetPath) {
}

bool CopyValueCommand::Execute() {
	auto key = Registry::OpenKey(GetPath(), KEY_QUERY_VALUE);
	if (!key)
		return false;

	auto target = Registry::OpenKey(_targetPath, KEY_WRITE | KEY_READ);
	if (!target)
		return false;

	DWORD size = 0;
	int i = 1;
	auto tname = GetName();
	LSTATUS error;
	while (ERROR_SUCCESS == (error = target.QueryValue(tname, nullptr, nullptr, &size))) {
		// value already exists
		if (i++ == 1)
			tname = "Copy Of" + _name;
		else
			tname.Format(L"Copy(%d) Of %s", i, _name);
	}
	if (error != ERROR_FILE_NOT_FOUND) {
		::SetLastError(error);
		return false;
	}
	_targetName = tname;

	if (!Registry::CopyValue(key, target, GetName(), _targetName))
		return false;

	return InvokeCallback(true);
}

bool CopyValueCommand::Undo() {
	auto target = Registry::OpenKey(_targetPath, KEY_WRITE);
	if (!target)
		return false;

	auto error = target.DeleteValue(_targetName);
	::SetLastError(error);
	if (ERROR_SUCCESS != error)
		return false;

	return InvokeCallback(false);
}
