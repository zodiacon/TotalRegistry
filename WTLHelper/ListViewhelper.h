#pragma once

struct ListViewHelper {
	static bool SaveAll(PCWSTR path, CListViewCtrl& lv, bool includeHeaders = true);
	static CString GetRowAsString(CListViewCtrl& lv, int row, WCHAR separator = L'\t');
	static int FindItem(CListViewCtrl& lv, PCWSTR text, bool partial);
};

struct SelectedItemsView : std::ranges::view_interface<SelectedItemsView> {
	struct Iterator {
		Iterator(SelectedItemsView& view, bool end = false) : _view(view), _end(end) {
			if(!end)
				_index = view._lv.GetNextItem(-1, LVIS_SELECTED);
		}
		int operator*() const {
			return _index;
		}
		int operator++(int) {
			auto index = _index;
			_index = _view._lv.GetNextItem(_index, LVIS_SELECTED);
			if (_index = -1)
				_end = true;
			return index;
		}
		int operator++() {
			_index = _view._lv.GetNextItem(_index, LVIS_SELECTED);
			if (_index == -1)
				_end = true;
			return _index;
		}
		bool operator==(Iterator other) const {
			if (_end && other._end)
				return true;
			return false;
		}
		bool operator!=(Iterator other) const {
			return !(*this == other);
		}

		int _index{ -1 };
		SelectedItemsView& _view;
		bool _end;
	};

	SelectedItemsView() = default;
	explicit SelectedItemsView(CListViewCtrl& lv) : _lv(lv), _count(lv.GetSelectedCount()) {}

	Iterator begin() {
		return Iterator(*this);
	}
	Iterator end() {
		return Iterator(*this, true);
	}

	size_t size() const {
		return _count;
	}

private:
	CListViewCtrl& _lv;
	int _count;
};
