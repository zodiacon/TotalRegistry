#include "pch.h"
#include "CreateKeyCommand.h"
#include "Registry.h"

CreateKeyCommand::CreateKeyCommand(PCWSTR path, PCWSTR name, AppCommandCallback<CreateKeyCommand> cb, DWORD options)
    : RegAppCommandBase((options & REG_OPTION_VOLATILE ? L"Create Volatile Key " : L"Create Key ") + CString(name), path, name, cb), m_Options(options) {
}

bool CreateKeyCommand::Execute() {
    auto key = Registry::OpenKey(m_Path, KEY_CREATE_SUB_KEY);
    if (!key)
        return false;
   
    CRegKey newKey;
    DWORD disp;
    auto error = newKey.Create(key.Get(), m_Name, nullptr, m_Options, KEY_READ | KEY_WRITE, nullptr, &disp);
    if (error == ERROR_SUCCESS) {
        if (disp == REG_OPENED_EXISTING_KEY) {
            ::SetLastError(ERROR_OBJECT_ALREADY_EXISTS);
            return false;
        }
        return InvokeCallback(true);
    }
    ::SetLastError(error);
    return false;
}

bool CreateKeyCommand::Undo() {
    auto key = Registry::OpenKey(m_Path, KEY_ENUMERATE_SUB_KEYS | DELETE | KEY_QUERY_VALUE);
    if(!key)
        return false;

    auto error = ::RegDeleteTree(key.Get(), m_Name);
    ::SetLastError(error);
    return ERROR_SUCCESS == error ? InvokeCallback(false) : false;
}

