#include "pch.h"
#include "Settings.h"

bool Settings::Load(PCWSTR registryPath) {
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

	WCHAR name[64];
	BYTE value[1024];
	DWORD type;
	for (DWORD i = 0;; ++i) {
		DWORD lname = _countof(name), lvalue = _countof(value);
		if (ERROR_SUCCESS != ::RegEnumValue(key, i, name, &lname, nullptr, &type, value, &lvalue))
			break;
		auto it = _settings.find(name);
		if (it == _settings.end())
			_settings.insert({ name, Setting(name, (BYTE*)value, lvalue, (SettingType)type) });
		else
			it->second.Set(value, lvalue);
	}
	return true;
}

bool Settings::Save(PCWSTR registryPath) const {
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

void Settings::Set(PCWSTR name, int value) {
	return Set(name, value, SettingType::Int32);
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
