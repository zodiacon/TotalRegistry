#include "pch.h"
#include "DeleteKeyCommand.h"
#include "Registry.h"
#include "Helpers.h"

DeleteKeyCommand::DeleteKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<DeleteKeyCommand> cb)
	: RegAppCommandBase(L"Delete Key", path, name, cb) {
}

bool DeleteKeyCommand::Execute() {
    CRegKey key = Registry::OpenKey(_path, KEY_ENUMERATE_SUB_KEYS | DELETE | KEY_QUERY_VALUE);
    if (!key)
        return false;


    auto error = ::RegDeleteTree(key, _name);
    if (ERROR_SUCCESS != error) {
        return false;
    }
    ::SetLastError(error);
    return InvokeCallback(true);
}

bool DeleteKeyCommand::Undo() {
    auto key = Registry::OpenKey(_path, KEY_CREATE_SUB_KEY);
    if (!key)
        return false;

    CRegKey newKey;
    DWORD disp;
    auto error = newKey.Create(key, _name, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &disp);
    ::SetLastError(error);
    if (error != ERROR_SUCCESS)
        return false;

    return InvokeCallback(false);
}
