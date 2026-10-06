#include "pch.h"
#include "ImportRegFileCommand.h"
#include "Registry.h"
#include "Helpers.h"

namespace {
	HKEY GetRootKey(CString const& path, CString& subKey) {
		auto bs = path.Find(L'\\');
		auto root = bs < 0 ? path : path.Left(bs);
		subKey = bs < 0 ? CString() : path.Mid(bs + 1);
		for (auto& k : Registry::Keys)
			if (_wcsicmp(k.text, root) == 0 || (*k.stext && _wcsicmp(k.stext, root) == 0))
				return k.hKey;
		return nullptr;
	}

	bool SplitPath(CString const& path, CString& parent, CString& name) {
		auto bs = path.ReverseFind(L'\\');
		if (bs < 0)
			return false;
		parent = path.Left(bs);
		name = path.Mid(bs + 1);
		return true;
	}

	//
	// creates the key one level at a time, so that the first key that did not exist is known,
	// and deleting it undoes the creation
	//
	LSTATUS CreateKey(CString const& path, CRegKey& key, CString& firstCreated, REGSAM access = KEY_READ | KEY_WRITE) {
		CString subKey;
		auto hRoot = GetRootKey(path, subKey);
		ATLASSERT(hRoot);
		auto error = key.Open(hRoot, subKey, access);
		if (error != ERROR_FILE_NOT_FOUND)
			return error;

		auto current = path.Left(path.GetLength() - subKey.GetLength() - 1);
		CRegKey parent;
		int pos = 0;
		for (auto name = subKey.Tokenize(L"\\", pos); !name.IsEmpty(); name = subKey.Tokenize(L"\\", pos)) {
			current += L"\\" + name;
			CRegKey child;
			DWORD disp;
			error = child.Create(parent ? parent.m_hKey : hRoot, name, REG_NONE, REG_OPTION_NON_VOLATILE, access, nullptr, &disp);
			if (error != ERROR_SUCCESS)
				return error;
			if (disp == REG_CREATED_NEW_KEY && firstCreated.IsEmpty())
				firstCreated = current;
			parent.Close();
			parent.Attach(child.Detach());
		}
		key.Attach(parent.Detach());
		return ERROR_SUCCESS;
	}

	bool ReadValue(HKEY hKey, PCWSTR name, DWORD& type, std::vector<BYTE>& data) {
		DWORD size = 0;
		if (ERROR_SUCCESS != ::RegQueryValueEx(hKey, name, nullptr, &type, nullptr, &size))
			return false;
		data.resize(size);
		if (ERROR_SUCCESS != ::RegQueryValueEx(hKey, name, nullptr, &type, data.data(), &size))
			return false;
		data.resize(size);
		return true;
	}
}

ImportRegFileCommand::ImportRegFileCommand(PCWSTR fileName, std::vector<RegFileKey> keys, AppCommandCallback<ImportRegFileCommand> cb)
	: AppCommandBase(L"Import " + CString(fileName).Mid(CString(fileName).ReverseFind(L'\\') + 1), cb), m_Keys(std::move(keys)) {
}

bool ImportRegFileCommand::Execute() {
	m_UndoLog.clear();
	m_Errors.clear();

	for (auto& key : m_Keys) {
		if (key.Delete)
			DeleteKey(key.Path);
		else
			ImportKey(key);
	}

	if (m_UndoLog.empty() && !m_Errors.empty())
		return false;

	return InvokeCallback(true);
}

bool ImportRegFileCommand::Undo() {
	DWORD firstError = ERROR_SUCCESS;
	std::vector<UndoEntry> failed;
	for (auto it = m_UndoLog.rbegin(); it != m_UndoLog.rend(); ++it) {
		auto& entry = *it;
		LSTATUS error = ERROR_SUCCESS;
		switch (entry.Type) {
			case UndoType::RestoreValue:
			{
				auto key = Registry::OpenKey(entry.Path, KEY_SET_VALUE);
				if (!key)
					error = ::GetLastError();
				else if (entry.Existed)
					error = key.SetValue(entry.Name, entry.ValueType, entry.Data.data(), (ULONG)entry.Data.size());
				else if ((error = key.DeleteValue(entry.Name)) == ERROR_FILE_NOT_FOUND)
					error = ERROR_SUCCESS;
				break;
			}

			case UndoType::DeleteCreatedKey:
			{
				CString parentPath, name;
				SplitPath(entry.Path, parentPath, name);
				auto parent = Registry::OpenKey(parentPath, MAXIMUM_ALLOWED);
				if (!parent)
					error = ::GetLastError();
				else
					error = ::RegDeleteTree(parent, name);
				if (error == ERROR_FILE_NOT_FOUND)
					error = ERROR_SUCCESS;
				break;
			}

			case UndoType::RestoreDeletedKey:
			{
				// Name holds the backup key path
				CRegKey key, backup;
				CString created;
				error = CreateKey(entry.Path, key, created, KEY_ALL_ACCESS);
				if (error == ERROR_SUCCESS)
					error = backup.Open(HKEY_CURRENT_USER, entry.Name, KEY_READ);
				if (error == ERROR_SUCCESS)
					error = ::RegCopyTree(backup, nullptr, key);
				if (error == ERROR_SUCCESS) {
					backup.Close();
					::RegDeleteTree(HKEY_CURRENT_USER, entry.Name);
				}
				break;
			}
		}
		if (error != ERROR_SUCCESS) {
			if (firstError == ERROR_SUCCESS)
				firstError = error;
			failed.insert(failed.begin(), std::move(entry));
		}
	}

	// keep what could not be undone, so a retry can attempt it again
	m_UndoLog = std::move(failed);
	InvokeCallback(false);
	::SetLastError(firstError);
	return firstError == ERROR_SUCCESS;
}

std::vector<CString> const& ImportRegFileCommand::GetErrors() const {
	return m_Errors;
}

void ImportRegFileCommand::ImportKey(RegFileKey const& regKey) {
	CRegKey key;
	CString created;
	auto error = CreateKey(regKey.Path, key, created);
	if (error != ERROR_SUCCESS) {
		AddError(regKey.Path, nullptr, error);
		return;
	}
	if (!created.IsEmpty())
		m_UndoLog.push_back({ UndoType::DeleteCreatedKey, created });

	for (auto& value : regKey.Values) {
		UndoEntry entry{ UndoType::RestoreValue, regKey.Path, value.Name };
		entry.Existed = ReadValue(key, value.Name, entry.ValueType, entry.Data);
		if (value.Delete) {
			if (!entry.Existed)
				continue;
			error = key.DeleteValue(value.Name);
		}
		else {
			error = key.SetValue(value.Name, value.Type, value.Data.data(), (ULONG)value.Data.size());
		}
		if (error != ERROR_SUCCESS)
			AddError(regKey.Path, value.Name.IsEmpty() ? L"(Default)" : (PCWSTR)value.Name, error);
		else if (created.IsEmpty())
			// no need to restore values of a new key, since undo deletes it
			m_UndoLog.push_back(std::move(entry));
	}
}

void ImportRegFileCommand::DeleteKey(CString const& path) {
	CString parentPath, name;
	if (!SplitPath(path, parentPath, name)) {
		// a root key cannot be deleted
		AddError(path, nullptr, ERROR_ACCESS_DENIED);
		return;
	}

	auto parent = Registry::OpenKey(parentPath, MAXIMUM_ALLOWED);
	if (!parent) {
		auto error = ::GetLastError();
		if (error != ERROR_FILE_NOT_FOUND)
			AddError(path, nullptr, error);
		return;
	}

	CRegKey key;
	auto error = key.Open(parent, name, KEY_READ);
	if (error == ERROR_FILE_NOT_FOUND)
		return;		// nothing to delete
	key.Close();

	//
	// back up the key so it can be restored by undo
	//
	LARGE_INTEGER li;
	::QueryPerformanceCounter(&li);
	CString backupPath;
	backupPath.Format(L"%sImport%llX", (PCWSTR)DeletedPathBackup, li.QuadPart);
	CRegKey backup;
	error = backup.Create(HKEY_CURRENT_USER, backupPath, nullptr, 0, KEY_ALL_ACCESS);
	if (error == ERROR_SUCCESS)
		error = ::RegCopyTree(parent, name, backup);
	if (error != ERROR_SUCCESS) {
		backup.Close();
		::RegDeleteTree(HKEY_CURRENT_USER, backupPath);
		AddError(path, nullptr, error);
		return;
	}

	error = ::RegDeleteTree(parent, name);
	if (error != ERROR_SUCCESS)
		AddError(path, nullptr, error);
	// even a failed delete may have deleted some of the tree, so keep the backup for undo
	m_UndoLog.push_back({ UndoType::RestoreDeletedKey, path, backupPath });
}

void ImportRegFileCommand::AddError(CString const& path, PCWSTR name, DWORD error) {
	CString text(path);
	if (name)
		text += CString(L" [") + name + L"]";
	m_Errors.push_back(text + L": " + Helpers::GetErrorText(error));
}
