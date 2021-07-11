#include "pch.h"
#include "DeleteKeyCommand.h"
#include "Registry.h"

DeleteKeyCommand::DeleteKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteKeyCommand> cb)
	: RegAppCommandBase(L"Delete Key", path, name, cb) {
}

bool DeleteKeyCommand::Execute() {
    CRegKey key = Registry::OpenKey(_path, KEY_ENUMERATE_SUB_KEYS | DELETE | KEY_QUERY_VALUE);
    if (!key)
        return false;

    auto error = ::RegDeleteTree(key, _name);
    ::SetLastError(error);
    return ERROR_SUCCESS == error ? InvokeCallback(true) : false;
}

bool DeleteKeyCommand::Undo() {
    auto key = Registry::OpenKey(_path, KEY_CREATE_SUB_KEY);
    if (!key)
        return false;

    CRegKey newKey;
    DWORD disp;
    auto error = newKey.Create(key, _name, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &disp);
    if (error == ERROR_SUCCESS) {
        return InvokeCallback(false);
    }
    return false;
}
