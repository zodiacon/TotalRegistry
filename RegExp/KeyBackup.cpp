#include "pch.h"
#include "KeyBackup.h"
#include "AppCommandBase.h"
#include "Registry.h"
#include "SecurityHelper.h"
#include <wil\resource.h>
#include <sddl.h>

namespace {
	// backup keys are named <pid>-<process creation time>, so live instances can be told apart from orphans
	bool IsOwnerRunning(PCWSTR name) {
		DWORD pid;
		ULONGLONG time;
		WCHAR end;
		if (swscanf_s(name, L"%u-%llX%c", &pid, &time, &end, 1) != 2)
			return false;	// older versions used other names, and deleted all backups on startup anyway

		wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
		if (!hProcess)
			return ::GetLastError() == ERROR_ACCESS_DENIED;	// can't tell, so assume it's alive

		FILETIME create, exit, kernel, user;
		if (!::GetProcessTimes(hProcess.get(), &create, &exit, &kernel, &user))
			return true;
		return ULARGE_INTEGER{ create.dwLowDateTime, create.dwHighDateTime }.QuadPart == time;
	}

	//
	// backups keep the security of the original keys, which may deny deleting them;
	// the owner (normally the user that made the backup) can always replace the DACL
	//
	void GrantOwnerFullControl(HKEY hParent, PCWSTR name) {
		static wil::unique_hlocal_security_descriptor sd = [] {
			PSECURITY_DESCRIPTOR sd = nullptr;
			::ConvertStringSecurityDescriptorToSecurityDescriptor(L"D:P(A;;KA;;;OW)", SDDL_REVISION_1, &sd, nullptr);
			return wil::unique_hlocal_security_descriptor(sd);
		}();
		if (!sd)
			return;

		CRegKey key;
		if (ERROR_SUCCESS == key.Open(hParent, name, WRITE_DAC))
			::RegSetKeySecurity(key, DACL_SECURITY_INFORMATION, sd.get());
		key.Close();
		if (ERROR_SUCCESS != key.Open(hParent, name, KEY_ENUMERATE_SUB_KEYS))
			return;

		std::vector<CString> names;
		Registry::EnumSubKeys(key, [&](auto subName, const auto&) {
			names.push_back(subName);
			return true;
			});
		for (auto& subName : names)
			GrantOwnerFullControl(key, subName);
	}

	LSTATUS DeleteBackupTree(HKEY hParent, PCWSTR name) {
		auto error = ::RegDeleteTree(hParent, name);
		if (error == ERROR_ACCESS_DENIED) {
			GrantOwnerFullControl(hParent, name);
			error = ::RegDeleteTree(hParent, name);
		}
		return error;
	}
}

KeyBackup::~KeyBackup() {
	Discard();
}

LSTATUS KeyBackup::Save(HKEY hParent, PCWSTR name, bool allowFile) {
	Discard();

	if (allowFile) {
		BackupRestorePrivileges privileges;
		if (privileges) {
			CRegKey key;
			WCHAR dir[MAX_PATH], file[MAX_PATH];
			if (ERROR_SUCCESS == ::RegOpenKeyEx(hParent, name, REG_OPTION_BACKUP_RESTORE, KEY_READ, &key.m_hKey)
				&& ::GetTempPath(_countof(dir), dir) && ::GetTempFileName(dir, L"TRK", 0, file)) {
				// RegSaveKeyEx fails if the file exists
				::DeleteFile(file);
				if (ERROR_SUCCESS == ::RegSaveKeyEx(key, file, nullptr, REG_LATEST_FORMAT)) {
					m_FileName = file;
					return ERROR_SUCCESS;
				}
				::DeleteFile(file);
			}
			// fall back to a copy in the Registry
		}
	}

	// copy from the key's own handle, rather than (parent, name), so the parent may be a predefined key
	CRegKey key;
	auto error = key.Open(hParent, name, KEY_READ);
	if (error != ERROR_SUCCESS)
		return error;

	LARGE_INTEGER li;
	::QueryPerformanceCounter(&li);
	CString path;
	path.Format(L"%s\\%llX", (PCWSTR)GetProcessBackupPath(), li.QuadPart);
	CRegKey backup;
	error = backup.Create(HKEY_CURRENT_USER, path, nullptr, 0, KEY_ALL_ACCESS);
	if (error == ERROR_SUCCESS)
		error = ::RegCopyTree(key, nullptr, backup);
	if (error != ERROR_SUCCESS) {
		backup.Close();
		DeleteBackupTree(HKEY_CURRENT_USER, path);
		return error;
	}
	m_KeyPath = path;
	return ERROR_SUCCESS;
}

LSTATUS KeyBackup::Restore(HKEY hKey) const {
	if (!m_FileName.IsEmpty()) {
		BackupRestorePrivileges privileges;
		if (!privileges)
			return ERROR_PRIVILEGE_NOT_HELD;
		return ::RegRestoreKey(hKey, m_FileName, REG_FORCE_RESTORE);
	}
	if (!m_KeyPath.IsEmpty()) {
		CRegKey backup;
		auto error = backup.Open(HKEY_CURRENT_USER, m_KeyPath, KEY_READ);
		if (error != ERROR_SUCCESS)
			return error;
		return ::RegCopyTree(backup, nullptr, hKey);
	}
	return ERROR_FILE_NOT_FOUND;
}

void KeyBackup::Discard() {
	if (!m_FileName.IsEmpty()) {
		::DeleteFile(m_FileName);
		m_FileName.Empty();
	}
	if (!m_KeyPath.IsEmpty()) {
		DeleteBackupTree(HKEY_CURRENT_USER, m_KeyPath);
		m_KeyPath.Empty();
	}
}

void KeyBackup::DeleteOrphans() {
	CRegKey key;
	if (ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, DeletedPathBackup.Left(DeletedPathBackup.GetLength() - 1), KEY_READ | DELETE))
		return;

	std::vector<CString> orphans;
	Registry::EnumSubKeys(key, [&](auto name, const auto&) {
		if (!IsOwnerRunning(name))
			orphans.push_back(name);
		return true;
		});
	for (auto& name : orphans)
		DeleteBackupTree(key, name);
}

CString const& KeyBackup::GetProcessBackupPath() {
	static CString const path = [] {
		FILETIME create, exit, kernel, user;
		::GetProcessTimes(::GetCurrentProcess(), &create, &exit, &kernel, &user);
		CString path;
		path.Format(L"%s%u-%llX", (PCWSTR)DeletedPathBackup, ::GetCurrentProcessId(),
			ULARGE_INTEGER{ create.dwLowDateTime, create.dwHighDateTime }.QuadPart);
		return path;
	}();
	return path;
}
