#pragma once

class LocationManager {
public:
	bool LoadFromRegistry(PCWSTR path = nullptr);
	bool SaveToRegistry(PCWSTR path = nullptr) const;
	bool LoadFromFile(PCWSTR path = nullptr);
	bool SaveToFile(PCWSTR path = nullptr) const;
	bool Load(PCWSTR path);
	bool Save() const;

	int GetCount() const;

	auto begin() {
		return m_Items.begin();
	}

	auto end() {
		return m_Items.end();
	}

	bool Replace(CString const& name, CString const& newName);

	void Add(CString const& name, CString const& path);
	void Clear();
	CString GetPathByName(CString const& name) const;

private:
	PCWSTR GetPath(PCWSTR path) const;
	struct LessNoCase {
		bool operator()(CString const& s1, CString const& s2) const {
			return s1.CompareNoCase(s2) < 0;
		}
	};
	std::map<CString, CString, LessNoCase> m_Items;
	mutable CString m_Path;
};

