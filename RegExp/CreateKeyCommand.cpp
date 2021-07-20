#include "pch.h"
#include "CreateKeyCommand.h"
#include "Registry.h"

CreateKeyCommand::CreateKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<CreateKeyCommand> cb) 
    : RegAppCommandBase(L"Create Key " + CString(name), path, name, cb) {
}

bool CreateKeyCommand::Execute() {
    auto key = Registry::OpenKey(_path, KEY_CREATE_SUB_KEY);
    if (!key)
        return false;
   
    CRegKey newKey;
    DWORD disp;
    auto error = newKey.Create(key, _name, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &disp);
    if (error == ERROR_SUCCESS) {
        if (disp == REG_OPENED_EXISTING_KEY) {
            ::SetLastError(ERROR_OBJECT_ALREADY_EXISTS);
            return false;
        }
        return InvokeCallback(true);
    }
    return false;
}

bool CreateKeyCommand::Undo() {
    CRegKey key = Registry::OpenKey(_path, KEY_ENUMERATE_SUB_KEYS | DELETE | KEY_QUERY_VALUE);
    if(!key)
        return false;

    auto error = ::RegDeleteTree(key, _name);
    ::SetLastError(error);
    return ERROR_SUCCESS == error ? InvokeCallback(false) : false;
}

