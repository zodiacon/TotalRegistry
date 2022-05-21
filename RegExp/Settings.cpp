#include "pch.h"
#include "Settings.h"
#include "IniFile.h"

bool Settings::LoadFromKey(PCWSTR registryPath) {
	if (registryPath == nullptr)
		registryPath = _path.c_str();
	else
		_path = registryPath;

	ATLASSERT(registryPath);
	if (registryPath == nullptr)
		return false;

	CRegKey key;
	if (ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, registryPath))
		return false;

	WCHAR name[128];
	auto size = 1 << 16;
	auto value = std::make_unique<BYTE[]>(size);
	DWORD type;
	for (DWORD i = 0;; ++i) {
		DWORD lname = _countof(name), lvalue = size;
		auto error = ::RegEnumValue(key, i, name, &lname, nullptr, &type, value.get(), &lvalue);
		if (ERROR_NO_MORE_ITEMS == error)
			break;

		if (error != ERROR_SUCCESS)
			continue;

		auto it = _settings.find(name);
		if (it == _settings.end())
			_settings.insert({ name, Setting(name, value.get(), lvalue, (SettingType)type) });
		else
			it->second.Set(value.get(), lvalue);
	}
	return true;
}

bool Settings::SaveToKey(PCWSTR registryPath) const {
	if (registryPath == nullptr)
		registryPath = _path.c_str();

	ATLASSERT(registryPath);
	if (registryPath == nullptr)
		return false;

	CRegKey key;
	key.Create(HKEY_CURRENT_USER, registryPath, nullptr, 0, KEY_WRITE);
	if (!key)
		return false;

	for (auto& [name, setting] : _settings) {
		key.SetValue(name.c_str(), (DWORD)setting.Type, setting.Buffer.get(), setting.Size);
	}
	return true;
}

bool Settings::LoadFromFile(PCWSTR path) {
	if (path == nullptr)
		path = _path.c_str();

	ATLASSERT(path);
	if (path == nullptr)
		return false;
	else
		_path = path;

	IniFile file(path);
	if (!file.IsValid())
		return false;

	PCWSTR section = L"General";
	for (auto& [name, setting] : _settings) {
		switch (setting.Type) {
			case SettingType::String:
				setting.SetString(file.ReadString(section, name.c_str()));
				break;

			case SettingType::Int32:
				setting.Set<int>(file.ReadInt(section, name.c_str()));
				break;

			default:
				unsigned size;
				auto data = file.ReadBinary(section, name.c_str(), size);
				if (data && size > 0)
					setting.Set(data.get(), size);
				break;
		}
	}

	return true;
}

bool Settings::SaveToFile(PCWSTR path) const {
	if (path == nullptr)
		path = _path.c_str();

	ATLASSERT(path);
	if (path == nullptr)
		return false;

	IniFile file(path);

	PCWSTR section = L"General";
	for (auto& [name, setting] : _settings) {
		switch (setting.Type) {
			case SettingType::String:
				file.WriteString(section, name.c_str(), (PCWSTR)setting.Buffer.get());
				break;

			case SettingType::Int32:
				file.WriteInt(section, name.c_str(), *(DWORD*)setting.Buffer.get());
				break;

			default:
				file.WriteBinary(section, name.c_str(), setting.Buffer.get(), setting.Size);
				break;
		}
	}
	return true;
}

bool Settings::Load(PCWSTR path) {
	WCHAR fullpath[MAX_PATH];
	::GetModuleFileName(nullptr, fullpath, _countof(fullpath));
	auto ch = fullpath[3];
	fullpath[3] = 0;
	if (::GetDriveType(fullpath) == DRIVE_FIXED)
		return LoadFromKey(path);
	fullpath[3] = ch;
	wcscpy_s(fullpath + wcslen(fullpath) - 3, _countof(fullpath), L"ini");
	return LoadFromFile(fullpath);
}

bool Settings::Save() const {
	if (_path.empty())
		return false;

	return _path[1] == L':' ? SaveToFile() : SaveToKey();
}

void Settings::Set(PCWSTR name, int value) {
	return Set(name, value, SettingType::Int32);
}

void Settings::Set(PCWSTR name, std::vector<CString> const& values) {
	Setting s(name, values);
	_settings.erase(name);
	_settings.insert({ name, std::move(s) });
}

void Settings::SetString(PCWSTR name, PCWSTR value) {
	auto it = _settings.find(name);
	if (it != _settings.end()) {
		it->second.SetString(value);
	}
	else {
		Setting s(name, value);
		_settings.insert({ name, std::move(s) });
	}
}

std::wstring Settings::GetString(PCWSTR name) const {
	auto it = _settings.find(name);
	if (it == _settings.end())
		return L"";
	return (PCWSTR)it->second.Buffer.get();
}

int Settings::GetInt32(PCWSTR name) const {
	return GetValue<int>(name);
}

void Setting::SetString(PCWSTR value) {
	Buffer = std::make_unique<uint8_t[]>(Size = (1 + (int)::wcslen(value)) * sizeof(wchar_t));
	::memcpy(Buffer.get(), value, Size);
	Type = SettingType::String;
}
