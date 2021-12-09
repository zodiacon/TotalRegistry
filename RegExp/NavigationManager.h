#pragma once

template<typename T>
struct NavigationManager {
	bool CanGoBack() const {
		return _current > 0;
	}

	T const& GoBack() {
		ATLASSERT(CanGoBack());
		return _items[--_current];
	}

	bool CanGoForward() const {
		return _current < _items.size() - 1;
	}

	T const& GoForward() {
		ATLASSERT(CanGoForward());
		return _items[++_current];
	}

	void Add(T const& address) {
		_current++;
		_items.resize(_current);
		_items.push_back(address);
	}

private:
	std::vector<T> _items;
	int _current{ -1 };
};

