#pragma once

enum class SettingType {
	String = REG_SZ,
	Int32 = REG_DWORD,
	Int64 = REG_QWORD,
	Binary = REG_BINARY,
	Bool = REG_DWORD,
};

struct Setting {
	std::wstring Name;
	SettingType Type{ SettingType::String };
	std::unique_ptr<uint8_t[]> Buffer;
	int Size;

	Setting(std::wstring name, const std::wstring& value) : Name(std::move(name)) {
		Buffer = std::make_unique<uint8_t[]>(Size = (1 + (int)value.size()) * sizeof(wchar_t));
		::memcpy(Buffer.get(), value.data(), Size);
	}
	template<typename T>
	Setting(std::wstring name, const T& value, SettingType type) : Name(std::move(name)), Type(type) {
		Buffer = std::make_unique<uint8_t[]>(Size = sizeof(T));
		::memcpy(Buffer.get(), &value, Size);
	}

	Setting(std::wstring name, const void* value, int size, SettingType type = SettingType::Binary) : Name(std::move(name)), Type(type) {
		Buffer = std::make_unique<uint8_t[]>(Size = size);
		::memcpy(Buffer.get(), value, Size);
	}

	template<typename T>
	void Set(const T& value) {
		Buffer = std::make_unique<uint8_t[]>(sizeof(T));
		memcpy(Buffer.get(), &value, sizeof(T));
	}

	void SetString(PCWSTR value);

	void Set(const void* value, int size) {
		Buffer = std::make_unique<uint8_t[]>(Size = size);
		memcpy(Buffer.get(), value, size);
	}
};

#define BEGIN_SETTINGS(className)	\
	className() { InitSettings(); }	\
	void InitSettings() {	\

#define END_SETTINGS } 

#define SETTING_STRING(name, value)	_settings.insert({ L#name, Setting(L#name, value) })
#define SETTING(name, value, type)	_settings.insert({ L#name, Setting(L#name, value, type) })
#define DEF_SETTING_STRING(name) \
	std::wstring name() const { return GetString(L#name); }	\
	void name(const std::wstring& value) { SetString(L#name, value.c_str()); }

#define DEF_SETTING(name, type) \
	type name() const { return GetValueOrDefault<type>(L#name); }	\
	void name(const type& value) { Set<type>(L#name, value); }

class Settings {
public:
	Settings() = default;

	bool Load(PCWSTR registryPath = nullptr);
	bool Save(PCWSTR registryPath = nullptr) const;

	template<typename T>
	void Set(const std::wstring& name, const T& value, SettingType type = SettingType::Binary) {
		auto it = _settings.find(name);
		if (it != _settings.end()) {
			it->second.Set(value);
		}
		else {
			Setting s(name, value, type);
			_settings.insert({ name, std::move(s) });
		}
	}

	void Set(PCWSTR name, int value);
	void SetString(PCWSTR name, PCWSTR value);

	std::wstring GetString(PCWSTR name) const;

	template<typename T>
	T GetValue(PCWSTR name) const {
		auto it = _settings.find(name);
		ATLASSERT(it != _settings.end());
		ATLASSERT(it->second.Size == sizeof(T));
		return *(T*)it->second.Buffer.get();
	}

	template<typename T>
	T GetValueOrDefault(PCWSTR name, const T& def = T()) const {
		auto it = _settings.find(name);
		if (it == _settings.end())
			return def;
		ATLASSERT(it->second.Size == sizeof(T));
		return *(T*)it->second.Buffer.get();
	}

	int GetInt32(PCWSTR name) const;

	template<typename T>
	T* GetBinary(PCWSTR name) const {
		auto it = _settings.find(name);
		if (it == _settings.end())
			return nullptr;
		ATLASSERT(it->second.Size == sizeof(T));
		return (T*)it->second.Buffer.get();
	}

protected:
	struct LessNoCase {
		bool operator()(const std::wstring& s1, const std::wstring& s2) const {
			return ::_wcsicmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
	std::map<std::wstring, Setting, LessNoCase> _settings;
	std::wstring _path;
};

