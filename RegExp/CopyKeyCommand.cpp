#include "pch.h"
#include "CopyKeyCommand.h"
#include "Registry.h"

CopyKeyCommand::CopyKeyCommand(PCWSTR path, PCWSTR name, PCWSTR targetPath, AppCommandCallback<CopyKeyCommand> cb) 
	: RegAppCommandBase(L"Paste Key " + CString(name), path, name, cb), _targetPath(targetPath) {
}

const CString& CopyKeyCommand::GetTargetPath() const {
	return _targetPath;
}

bool CopyKeyCommand::Execute() {
	auto key = Registry::OpenKey(_path + L"\\" + _name, KEY_READ);
	if (!key)
		return false;

	auto targetKey = Registry::CreateKey(_targetPath + L"\\" + _name, KEY_ALL_ACCESS);
	if (!targetKey)
		return false;

	if (!Registry::CopyKey(key.Get(), nullptr, targetKey))
		return false;

	return InvokeCallback(true);
}

bool CopyKeyCommand::Undo() {
	auto key = Registry::OpenKey(_path, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | DELETE);
	if (!key)
		return false;

	LSTATUS error;
	if (ERROR_SUCCESS != (error = ::RegDeleteTree(key.Get(), _name))) {
		::SetLastError(error);
		return false;
	}
	return InvokeCallback(false);
}
