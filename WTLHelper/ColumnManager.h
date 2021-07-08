#pragma once

#include <map>
#include <vector>

enum class ColumnFlags {
	None = 0,
	Visible = 1,
	Fixed = 2,
	Const = 4,		// currently unused
	Mandatory = 8,
	Numeric = 0x10,
	Modified = 0x80
};
DEFINE_ENUM_FLAG_OPERATORS(ColumnFlags);

class ColumnManager {
public:
	struct ColumnInfo {
		int DefaultWidth;
		int Format;
		CString Name;
		ColumnFlags Flags;
		CString Category;
		int Tag;

		bool IsVisible() const;
		bool IsMandatory() const;
		void SetVisible(bool);
	};

	ColumnManager(HWND hListView) : m_ListView(hListView) {}
	~ColumnManager();
	ColumnManager(const ColumnManager&) = default;
	ColumnManager& operator=(const ColumnManager&) = delete;

	HWND GetListView() const {
		return m_ListView;
	}

	bool CopyTo(ColumnManager& other) const;
	void AddFromControl(HWND hList = nullptr);
	void SetVisible(int column, bool visible);
	bool IsVisible(int column) const;
	bool IsModified(int column) const;
	void SetModified(int column, bool modified);
	bool IsConst(int column) const;

	template<typename T = int>
	int AddColumn(PCWSTR name, int format, int width, T tag = T(), ColumnFlags flags = ColumnFlags::Visible);
	template<typename T = int>
	T GetColumnTag(int index) const {
		return static_cast<T>(m_Columns[index].Tag);
	}

	void Clear();
	const ColumnInfo& GetColumn(int index) const;
	const std::vector<int>& GetColumnsByCategory(PCWSTR category) const;
	const std::vector<CString>& GetCategories() const;

	void UpdateColumns();
	int GetRealColumn(int index) const;

	int GetCount() const {
		return static_cast<int>(m_Columns.size());
	}

protected:
	void SetColumn(int i, const ColumnInfo& info);

private:
	CListViewCtrl m_ListView;
	std::vector<ColumnInfo> m_Columns;
	std::map<CString, std::vector<int>> m_ColumnsByCategory;
	std::vector<CString> m_Categories;
};

inline bool ColumnManager::ColumnInfo::IsVisible() const {
	return (Flags & ColumnFlags::Visible) == ColumnFlags::Visible;
}

inline bool ColumnManager::ColumnInfo::IsMandatory() const {
	return (Flags & ColumnFlags::Mandatory) == ColumnFlags::Mandatory;
}

inline void ColumnManager::ColumnInfo::SetVisible(bool visible) {
	bool old = (Flags & ColumnFlags::Visible) == ColumnFlags::Visible;
	if (old == visible)
		return;

	if (visible)
		Flags |= ColumnFlags::Visible;
	else
		Flags &= ~ColumnFlags::Visible;
	Flags |= ColumnFlags::Modified;
}

template<typename T>
int ColumnManager::AddColumn(PCWSTR name, int format, int width, T tag, ColumnFlags flags) {
	auto category = ::wcschr(name, L'\\');
	CString categoryName;
	if (category) {
		categoryName = CString(name, static_cast<int>(category - name));
		name = category + 1;
	}
	else {
		categoryName = L"General";
	}
	ColumnInfo info;
	info.Format = format;
	info.DefaultWidth = width;
	info.Flags = flags;
	info.Name = name;
	info.Category = categoryName;
	info.Tag = static_cast<int>(tag);

	if (m_ListView && ((flags & ColumnFlags::Visible) == ColumnFlags::Visible)) {
		auto header = m_ListView.GetHeader();
		int i = m_ListView.InsertColumn(header.GetItemCount(), name, format, width);
		HDITEM hdi;
		hdi.mask = HDI_LPARAM;
		hdi.lParam = m_Columns.size();
		header.SetItem(i, &hdi);
	}

	m_Columns.push_back(info);
	if (!categoryName.IsEmpty()) {
		if (std::find(m_Categories.begin(), m_Categories.end(), categoryName) == m_Categories.end())
			m_Categories.push_back(categoryName);
		m_ColumnsByCategory[categoryName].push_back(static_cast<int>(m_Columns.size() - 1));
	}

	return static_cast<int>(m_Columns.size());
}
