#include "pch.h"
#include "CopyValueCommand.h"
#include "Registry.h"

CopyValueCommand::CopyValueCommand(PCWSTR path, PCWSTR name, PCWSTR targetPath, AppCommandCallback<CopyValueCommand> cb) 
	: RegAppCommandBase(L"Paste value " + CString(name), path, name, cb), m_TargetPath(targetPath) {
}

bool CopyValueCommand::Execute() {
	auto key = Registry::OpenKey(GetPath(), KEY_QUERY_VALUE);
	if (!key)
		return false;

	auto target = Registry::OpenKey(m_TargetPath, KEY_WRITE | KEY_READ);
	if (!target)
		return false;

	DWORD size = 0;
	int i = 1;
	auto tname = GetName();
	LSTATUS error;
	while (ERROR_SUCCESS == (error = target.QueryValue(tname, nullptr, nullptr, &size))) {
		// value already exists
		if (i++ == 1)
			tname = "Copy Of" + m_Name;
		else
			tname.Format(L"Copy(%d) Of %s", i, m_Name);
	}
	if (error != ERROR_FILE_NOT_FOUND) {
		::SetLastError(error);
		return false;
	}
	m_TargetName = tname;

	if (!Registry::CopyValue(key.Get(), target.Get(), GetName(), m_TargetName))
		return false;

	return InvokeCallback(true);
}

bool CopyValueCommand::Undo() {
	auto target = Registry::OpenKey(m_TargetPath, KEY_WRITE);
	if (!target)
		return false;

	auto error = target.DeleteValue(m_TargetName);
	::SetLastError(error);
	if (ERROR_SUCCESS != error)
		return false;

	return InvokeCallback(false);
}
